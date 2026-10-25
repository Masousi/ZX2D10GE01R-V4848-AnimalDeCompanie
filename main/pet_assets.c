// ⚫ AJOUTÉ (25/10/2026) : descripteurs LVGL des images intégrées (voir pet_assets.h).
// Les .bin contiennent les pixels bruts (pas d'en-tête) : RGB565 (fond) ou RGB565 + alpha (sprites).
// ⚫ MODIFIÉ (25/10/2026) : réécrit pour le pack de la partition "assets" (plusieurs animaux) :
// - décor et accessoires : dessinés directement depuis la flash (partition projetée en mémoire) ;
// - animal choisi : 256 couleurs -> RGB565 + alpha en PSRAM, + copie miroir (tourné vers la gauche) ;
// - oiseaux (et toute autre image) : copie miroir créée à la première demande.
// ⚫ MODIFIÉ (25/10/2026) : l'image de l'animal est décodée À LA DEMANDE (à chaque changement de pose, ~1 ms)
// dans un tampon unique, au lieu de toutes les poses x 2 sens d'avance : 10 espèces et plus de poses sans
// remplir la PSRAM (un cheval décodé d'avance aurait pris ~9 Mo).
// ⚫ RETIRÉ (25/10/2026) : symboles _binary_<nom>_bin_start (EMBED_FILES) et table g_dog_mouth_dx/dy
// (positions de la gueule désormais dans PET_MOUTH, par espèce).
#include <string.h>
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_heap_caps.h"
#include "pet_assets.h"

#define TAG "ASSETS"

typedef struct __attribute__((packed)) {
    uint32_t off, size;
    uint16_t w, h;
    int16_t ox, oy;
    uint8_t cf, pad[3];
} pack_entry_t;

static const uint8_t *s_pack;
static const pack_entry_t *s_ent;
static pet_asset_t s_props[PET_ANIMAL_BASE];
static pet_asset_t *s_props_left[PET_ANIMAL_BASE];
// static pet_asset_t s_anim[PET_POSE_COUNT][2];
static pet_asset_t s_anim;           // ⚫ MODIFIÉ (25/10/2026) : image affichée (décodée à la demande)
static uint8_t *s_anim_buf;
static size_t s_anim_cap;
static int s_anim_pose = -1, s_anim_left;
static int s_species = -1;
static int s_age = PET_AGE_COUNT - 1;   // ⚫ AJOUTÉ (25/10/2026)
// ⚫ AJOUTÉ (25/10/2026) : première image de l'espèce à l'âge courant
#define ANIM_FIRST(sp, age) (PET_ANIMAL_BASE + ((age) * PET_SPECIES_COUNT + (sp)) * PET_POSE_COUNT)

static void dsc_init(pet_asset_t *a, const pack_entry_t *e, const uint8_t *data, bool alpha)
{
    a->img.header.cf = alpha ? LV_IMG_CF_TRUE_COLOR_ALPHA : LV_IMG_CF_TRUE_COLOR;
    a->img.header.w = e->w;
    a->img.header.h = e->h;
    a->img.data_size = (uint32_t)e->w * e->h * (alpha ? 3 : 2);
    a->img.data = data;
    a->ox = e->ox;
    a->oy = e->oy;
}

bool pet_assets_init(void)
{
    const esp_partition_t *part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, 0x40, "assets");
    if (!part) {
        ESP_LOGE(TAG, "Partition 'assets' absente : flasher avec idf.py flash (pack assets/pet_assets.bin)");
        return false;
    }
    const void *ptr;
    esp_partition_mmap_handle_t h;
    uint32_t size = PET_PACK_SIZE < part->size ? PET_PACK_SIZE : part->size;
    if (esp_partition_mmap(part, 0, size, ESP_PARTITION_MMAP_DATA, &ptr, &h) != ESP_OK) {
        ESP_LOGE(TAG, "Impossible de projeter la partition 'assets'");
        return false;
    }
    s_pack = ptr;
    if (*(const uint32_t *)s_pack != PET_PACK_MAGIC || *(const uint32_t *)(s_pack + 8) != PET_PACK_COUNT) {
        ESP_LOGE(TAG, "Pack d'images invalide ou d'une autre version : regénérer (npm run gen) puis idf.py flash");
        s_pack = NULL;
        return false;
    }
    s_ent = (const pack_entry_t *)(s_pack + 16);
    // for (int i = 0; i < PET_ANIMAL_BASE; i++) dsc_init(&s_props[i], &s_ent[i], s_pack + s_ent[i].off, s_ent[i].cf == 1);
    // ⚫ MODIFIÉ (25/10/2026) : les fonds plein écran (format 0, 450 Ko chacun) sont copiés en PSRAM : redessinés en
    // permanence (pluie, nuages, animal), lus depuis la flash ils saturaient le bus mémoire partagé avec l'écran
    // et l'image se décalait. La PSRAM (octale) est ~4 fois plus rapide que la flash.
    size_t in_ram = 0;
    for (int i = 0; i < PET_ANIMAL_BASE; i++) {
        const uint8_t *data = s_pack + s_ent[i].off;
        // if (s_ent[i].cf == 0) {
        // ⚫ MODIFIÉ (25/10/2026) : aussi les sprites du décor (abri, gestes, objets : ~1 Mo) tant qu'il reste de la PSRAM
        if (s_ent[i].cf == 0 || (s_ent[i].cf == 1 && heap_caps_get_free_size(MALLOC_CAP_SPIRAM) > 1200 * 1024)) {
            uint8_t *copy = heap_caps_malloc(s_ent[i].size, MALLOC_CAP_SPIRAM);
            if (copy) { memcpy(copy, data, s_ent[i].size); data = copy; in_ram += s_ent[i].size; }
        }
        dsc_init(&s_props[i], &s_ent[i], data, s_ent[i].cf == 1);
    }
    ESP_LOGI(TAG, "Fonds en PSRAM : %u Ko (PSRAM libre : %u Ko)", (unsigned)(in_ram / 1024),
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
    ESP_LOGI(TAG, "Pack : %d images, %lu Ko", PET_PACK_COUNT, (unsigned long)(size / 1024));
    return true;
}

// Copie miroir horizontale d'une image RGB565 (+ alpha)
static uint8_t *mirror(const uint8_t *src, int w, int h, int bpp)
{
    uint8_t *dst = heap_caps_malloc((size_t)w * h * bpp, MALLOC_CAP_SPIRAM);
    if (!dst) return NULL;
    for (int y = 0; y < h; y++) {
        const uint8_t *s = src + (size_t)y * w * bpp;
        uint8_t *d = dst + (size_t)y * w * bpp;
        for (int x = 0; x < w; x++) memcpy(d + (size_t)(w - 1 - x) * bpp, s + (size_t)x * bpp, bpp);
    }
    return dst;
}

// ⚫ MODIFIÉ (25/10/2026) : ne décode plus rien d'avance : retient l'espèce, réserve le tampon (taille de la
// plus grande pose de l'espèce)
bool pet_assets_load_species(int species)
{
    return pet_assets_load(species, PET_AGE_COUNT - 1);
}

// ⚫ AJOUTÉ (25/10/2026) : selon l'âge (tampon dimensionné pour la plus grande pose, tous âges confondus)
bool pet_assets_load(int species, int age)
{
    if (!s_pack || species < 0 || species >= PET_SPECIES_COUNT) return false;
    if (age < 0 || age >= PET_AGE_COUNT) age = PET_AGE_COUNT - 1;
    size_t need = 0;
    for (int p = 0; p < PET_POSE_COUNT * PET_AGE_COUNT; p++) {
        // const pack_entry_t *e = &s_ent[PET_ANIMAL_BASE + species * PET_POSE_COUNT + p];
        const pack_entry_t *e = &s_ent[ANIM_FIRST(species, p / PET_POSE_COUNT) + p % PET_POSE_COUNT];
        if ((size_t)e->w * e->h * 3 > need) need = (size_t)e->w * e->h * 3;
    }
    if (need > s_anim_cap) {
        heap_caps_free(s_anim_buf);
        s_anim_buf = heap_caps_malloc(need, MALLOC_CAP_SPIRAM);
        s_anim_cap = s_anim_buf ? need : 0;
        if (!s_anim_buf) { ESP_LOGE(TAG, "PSRAM insuffisante pour l'animal"); return false; }
    }
    s_species = species;
    s_age = age;
    s_anim_pose = -1;
    // ESP_LOGI(TAG, "Animal %d : %d poses, tampon %u Ko", species, PET_POSE_COUNT, (unsigned)(need / 1024));
    ESP_LOGI(TAG, "Animal %d, âge %d : %d poses, tampon %u Ko", species, age, PET_POSE_COUNT, (unsigned)(need / 1024));
    return true;
}

const pet_asset_t *pet_asset(int id)
{
    return &s_props[id];
}

const pet_asset_t *pet_asset_left(int id)
{
    if (!s_props_left[id]) {
        const pack_entry_t *e = &s_ent[id];
        int bpp = e->cf == 1 ? 3 : 2;
        pet_asset_t *a = heap_caps_calloc(1, sizeof(pet_asset_t), MALLOC_CAP_SPIRAM);
        uint8_t *d = a ? mirror(s_pack + e->off, e->w, e->h, bpp) : NULL;
        if (!d) { heap_caps_free(a); return &s_props[id]; }
        dsc_init(a, e, d, e->cf == 1);
        a->ox = -(e->ox + e->w);
        s_props_left[id] = a;
    }
    return s_props_left[id];
}

// ⚫ MODIFIÉ (25/10/2026) : décode la pose (256 couleurs -> RGB565 + alpha), en miroir si tournée à gauche
const pet_asset_t *pet_animal(int pose, bool left)
{
    if (s_species < 0 || !s_anim_buf) return &s_anim;
    if (pose == s_anim_pose && left == s_anim_left) return &s_anim;
    // const pack_entry_t *e = &s_ent[PET_ANIMAL_BASE + s_species * PET_POSE_COUNT + pose];
    const pack_entry_t *e = &s_ent[ANIM_FIRST(s_species, s_age) + pose];   // ⚫ MODIFIÉ (25/10/2026) : selon l'âge
    const uint8_t *pal = s_pack + e->off, *idx = pal + 1024;
    uint8_t lut[256][3];
    for (int k = 0; k < 256; k++) {   // palette B, G, R, A -> RGB565 little-endian + alpha
        uint16_t c = ((pal[k * 4 + 2] >> 3) << 11) | ((pal[k * 4 + 1] >> 2) << 5) | (pal[k * 4] >> 3);
        lut[k][0] = c & 0xFF;
        lut[k][1] = c >> 8;
        lut[k][2] = pal[k * 4 + 3];
    }
    if (e->cf == 3) {   // ⚫ AJOUTÉ (25/10/2026) : compressé : plages transparentes / suites d'index
        const uint8_t *q = idx, *end = s_pack + e->off + e->size;
        int x = 0, y = 0;
        while (q < end && y < e->h) {
            uint8_t b = *q++;
            int n = (b & 0x7F) + 1;
            while (n-- > 0 && y < e->h) {
                int k = (b & 0x80) ? 0 : *q++;
                memcpy(s_anim_buf + ((size_t)y * e->w + (left ? e->w - 1 - x : x)) * 3, lut[k], 3);
                if (++x == e->w) { x = 0; y++; }
            }
        }
    } else
    for (int y = 0; y < e->h; y++) {
        const uint8_t *src = idx + (size_t)y * e->w;
        uint8_t *dst = s_anim_buf + (size_t)y * e->w * 3;
        if (left) for (int x = 0; x < e->w; x++) memcpy(dst + (size_t)(e->w - 1 - x) * 3, lut[src[x]], 3);
        else      for (int x = 0; x < e->w; x++) memcpy(dst + (size_t)x * 3, lut[src[x]], 3);
    }
    dsc_init(&s_anim, e, s_anim_buf, true);
    if (left) s_anim.ox = -(e->ox + e->w);   // miroir autour de l'ancrage
    s_anim_pose = pose;
    s_anim_left = left;
    lv_img_cache_invalidate_src(&s_anim.img);
    return &s_anim;
}

int pet_asset_index_of(const void *src, bool *left)
{
    *left = false;
    for (int i = 0; i < PET_ANIMAL_BASE; i++) {
        if (src == &s_props[i].img) return i;
        if (s_props_left[i] && src == &s_props_left[i]->img) { *left = true; return i; }
    }
    if (s_species >= 0 && s_anim_pose >= 0 && src == &s_anim.img) {
        *left = s_anim_left;
        // return PET_ANIMAL_BASE + s_species * PET_POSE_COUNT + s_anim_pose;
        return ANIM_FIRST(s_species, s_age) + s_anim_pose;   // ⚫ MODIFIÉ (25/10/2026)
    }
    return -1;
}

const uint8_t *pet_pack_entry(int idx, uint32_t *size, uint16_t *w, uint16_t *h, int16_t *ox, int16_t *oy, uint8_t *cf)
{
    if (!s_pack || idx < 0 || idx >= PET_PACK_COUNT) return NULL;
    const pack_entry_t *e = &s_ent[idx];
    if (size) *size = e->size;
    if (w) *w = e->w;
    if (h) *h = e->h;
    if (ox) *ox = e->ox;
    if (oy) *oy = e->oy;
    if (cf) *cf = e->cf;
    return s_pack + e->off;
}

int pet_pack_count(void)
{
    return s_pack ? PET_PACK_COUNT : 0;
}
