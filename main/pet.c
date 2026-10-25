// ⚫ AJOUTÉ (24/10/2026) : modèle de l'animal de compagnie (voir pet.h).
// Règles du jeu (temps "de jeu" = temps réel x PET_TIME_SCALE) :
// - ⚫ 25/10/2026 : + soif : l'hydratation se vide en ~45 min (2x moins vite en dormant) ;
// - éveillé : la satiété se vide en ~1 h, l'énergie en ~1 h 30, le bonheur en ~45 min (2x plus vite s'il a faim
//   ou est épuisé), la discipline en ~3 h ;
// - endormi : l'énergie remonte en ~10 min, la satiété baisse 2x moins vite ; réveil spontané à 100 % ;
// - énergie à 0 : il s'endort d'épuisement ; satiété ou bonheur à 0 : la santé baisse (~30 min jusqu'à 0) ;
// - bêtises au hasard, d'autant plus souvent que la discipline est basse : gronder / punir les corrige.
//   Gronder ou punir sans raison le fait pleurer : il faut alors le réconforter.
// ⚫ AJOUTÉ (25/10/2026) :
// - besoins : la jauge se remplit en ~40 min (plus vite après manger / boire) ; à 70 % il demande à sortir,
//   à 100 % accident : il fait n'importe où (bêtise) ;
// - bêtises : trou dans la pelouse, gamelle de croquettes ou d'eau renversée, besoins n'importe où ; trous et
//   besoins restent jusqu'au nettoyage (les besoins font baisser la santé) ; une gamelle renversée est remise
//   droite en la remplissant (Manger / Boire) ou en nettoyant ;
// - rien ne se passe tant que l'animal n'est pas adopté (nom + espèce choisis sur la page web).
// ⚫ MODIFIÉ (25/10/2026) :
// - besoins au coin gravier (ils y restent jusqu'au nettoyage) ; assez discipliné (>= 45), il y va tout seul
//   quand la jauge atteint 90 % ; sinon accident à 100 % ; plus de bêtise "besoins n'importe où" au hasard ;
// - heure et météo réelles (pet_set_env) : il s'endort à la nuit tombée s'il est un peu fatigué, se réveille
//   avec le jour ; sous la pluie / la neige hors de l'abri, le bonheur baisse plus vite ;
// - jeux propres à chaque espèce (pet_games_mask).
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_random.h"
#include "nvs.h"
#include "sdkconfig.h"
#include <math.h>
#include <time.h>            // ⚫ AJOUTÉ (25/10/2026) : heure réelle
#include <stddef.h>          // ⚫ AJOUTÉ (25/10/2026) : offsetof            // ⚫ AJOUTÉ (25/10/2026) : lroundf
#include "pet.h"
#include "weather.h"         // ⚫ AJOUTÉ (25/10/2026) : froid, chaleur
#include "pet_assets_gen.h"   // ⚫ AJOUTÉ (25/10/2026) : espèces, zone des bêtises

#define TAG "PET"

#define NVS_NS        "pet"
#define NVS_KEY       "state"
// #define SAVE_MAGIC    0x50455431u   // "PET1" : change si la structure sauvegardée change
// #define SAVE_MAGIC    0x50455432u   // ⚫ MODIFIÉ (25/10/2026) : "PET2" (ajout de la soif)
// #define SAVE_MAGIC    0x50455433u   // ⚫ MODIFIÉ (25/10/2026) : "PET3" (adoption, besoins, dégâts de la cour)
#define SAVE_MAGIC    0x50455434u   // ⚫ MODIFIÉ (25/10/2026) : "PET4" (+ besoins au coin gravier)

// Vitesses en points par seconde de jeu
#define FOOD_DECAY      (100.0f / 3600)
#define WATER_DECAY     (100.0f / 2700)   // ⚫ AJOUTÉ (25/10/2026)
#define ENERGY_DECAY    (100.0f / 5400)
#define ENERGY_RECOVER  (100.0f / 600)
#define JOY_DECAY       (100.0f / 2700)
#define DISC_DECAY      (100.0f / 10800)
#define HEALTH_DECAY    (100.0f / 1800)
#define HEALTH_RECOVER  (100.0f / 3000)
#define POTTY_FILL      (100.0f / 2400)   // ⚫ AJOUTÉ (25/10/2026) : besoins pleins en ~40 min
#define POOP_HEALTH     (100.0f / 7200)   // ⚫ AJOUTÉ (25/10/2026) : santé perdue par besoin laissé dans la cour

#define NAUGHTY_CHECK_S 60      // tirage d'une bêtise toutes les 60 s de jeu
#define CRY_MAX_S       120     // pleure au plus 2 min s'il n'est pas consolé (puis boude)

// #define STAGE_CHILD_S   (3600)          // bébé pendant 1 h
// #define STAGE_ADULT_S   (6 * 3600)      // enfant jusqu'à 6 h
// ⚫ MODIFIÉ (25/10/2026) : durées réglables (menuconfig), en heures de jeu : bébé 1 jour, adolescent jusqu'à 4 jours
#define STAGE_CHILD_S   ((uint32_t)CONFIG_PET_BABY_HOURS * 3600)
#define STAGE_ADULT_S   ((uint32_t)CONFIG_PET_ADO_HOURS * 3600)

// ⚫ AJOUTÉ (25/10/2026) : rythme de vie selon l'âge (multiplicateurs)
//   bébé : petits repas fréquents, se fatigue vite et dort beaucoup, besoins fréquents et ne se retient pas
//          (il faut le sortir : jamais seul au coin), peu de bêtises, apprend lentement, fragile ;
//   adolescent : plein d'énergie, s'ennuie vite (veut jouer), fait le plus de bêtises, apprend moyennement ;
//   adulte : calme, autonome (va seul au coin plus tôt), peu de bêtises, apprend vite.
typedef struct {
    float food, water, energy, recover, joy, potty, naughty, learn, fragile;
    int auto_potty;   // discipline minimale pour aller seul au coin (> 100 = jamais)
    int cry_max;      // durée des pleurs sans consolation (s)
} age_k_t;
static const age_k_t AGE_K[3] = {
    [PET_STAGE_BABY]  = { 1.4f, 1.2f, 1.6f, 1.2f, 1.0f, 1.6f, 0.4f, 0.5f, 1.6f, 101, 180 },
    [PET_STAGE_CHILD] = { 1.1f, 1.1f, 0.8f, 1.0f, 1.4f, 1.1f, 1.6f, 0.8f, 1.0f, 45, 120 },
    [PET_STAGE_ADULT] = { 1.0f, 1.0f, 1.0f, 1.0f, 0.8f, 1.0f, 0.7f, 1.0f, 0.8f, 35, 90 },
};
#define AK (AGE_K[pet_stage()])

typedef struct {
    uint32_t magic;
    pet_t pet;
} pet_save_t;

static pet_t s_pet;
static uint32_t s_naughty_acc;   // secondes de jeu depuis le dernier tirage de bêtise
static uint32_t s_cry_s;         // durée des pleurs en cours
static uint32_t s_save_acc;      // secondes réelles depuis la dernière sauvegarde
static int s_save_in = -1;       // ⚫ AJOUTÉ (25/10/2026) : sauvegarde différée (secondes restantes, -1 = aucune)
// static char s_msg[96];
static char s_msg[160];   // ⚫ MODIFIÉ (25/10/2026) : résumé de l'absence plus long
static uint32_t s_mess_ver;      // ⚫ AJOUTÉ (25/10/2026)
static int16_t s_pos_x = 240, s_pos_y = 338;   // ⚫ AJOUTÉ (25/10/2026) : position de l'animal à l'écran
// ⚫ AJOUTÉ (25/10/2026) : environnement réel
static bool s_env_night, s_env_bad, s_env_sheltered, s_env_known, s_auto_potty;
static uint32_t s_since_action;   // secondes de jeu depuis la dernière action
// ⚫ AJOUTÉ (25/10/2026) : dernière perte de santé (affichée à l'écran et sur la page web)
static uint32_t s_hevt_seq;
static int s_hevt_delta;
static const char *s_hevt_why = "";

static float clampf(float v);   // ⚫ AJOUTÉ (25/10/2026) : défini plus bas
void pet_make_sick(int cause);   // ⚫ AJOUTÉ (25/10/2026)
// Perte de santé avec sa raison (multipliée par la fragilité de l'âge)
static void health_hit(float d, const char *why)
{
    float before = s_pet.s.health;
    s_pet.s.health = clampf(s_pet.s.health - d * AK.fragile);
    s_hevt_delta = (int)lroundf(s_pet.s.health - before);
    if (s_hevt_delta == 0) s_hevt_delta = -1;
    s_hevt_why = why;
    s_hevt_seq++;
}

// ⚫ AJOUTÉ (25/10/2026) : nom affiché et cri de chaque espèce (main/pet_assets_gen.h)
static const char *const s_species_label[PET_SPECIES_COUNT] = {
#define X(id, label, sound) label,
    PET_SPECIES(X)
#undef X
};
static const char *const s_species_sound[PET_SPECIES_COUNT] = {
#define X(id, label, sound) sound,
    PET_SPECIES(X)
#undef X
};

static float clampf(float v)
{
    return v < 0 ? 0 : (v > 100 ? 100 : v);
}

static void clamp_all(void)
{
    s_pet.s.food       = clampf(s_pet.s.food);
    s_pet.s.water      = clampf(s_pet.s.water);
    s_pet.s.energy     = clampf(s_pet.s.energy);
    s_pet.s.joy        = clampf(s_pet.s.joy);
    s_pet.s.discipline = clampf(s_pet.s.discipline);
    s_pet.s.health     = clampf(s_pet.s.health);
    s_pet.s.potty      = clampf(s_pet.s.potty);   // ⚫ AJOUTÉ (25/10/2026)
}

static void pet_new(void)
{
    memset(&s_pet, 0, sizeof(s_pet));
    s_pet.bone_hole = -1;   // ⚫ AJOUTÉ (25/10/2026)
    s_pet.s = (pet_stats_t){ .food = 70, .water = 70, .energy = 80, .joy = 70, .discipline = 30, .health = 100 };
}

// ⚫ AJOUTÉ (25/10/2026) : point au hasard dans la zone de bêtises de la cour
// ⚫ MODIFIÉ (25/10/2026) : toute la pelouse visible (voir pet_lawn_spot)
static pet_spot_t rand_spot(void)
{
    int x, y;
    pet_lawn_spot(&x, &y);
    return (pet_spot_t){ (int16_t)x, (int16_t)y };
}
static int s_dig_slot = -1;   // ⚫ AJOUTÉ (25/10/2026) : trou en cours de creusement (bêtise)

/* ---------- ⚫ AJOUTÉ (25/10/2026) : trous réalistes ---------- */

static int hole_find(int x, int y, int r)
{
    for (int i = 0; i < s_pet.n_holes; i++)
        if (abs(s_pet.holes[i].x - x) <= r && abs(s_pet.holes[i].y - y) <= r) return i;
    return -1;
}

static void hole_remove(int i)
{
    for (int k = i; k + 1 < s_pet.n_holes; k++) {
        s_pet.holes[k] = s_pet.holes[k + 1];
        s_pet.hole_dig[k] = s_pet.hole_dig[k + 1];
        s_pet.hole_filled[k] = s_pet.hole_filled[k + 1];
    }
    s_pet.n_holes--;
    if (s_pet.bone_hole == i) s_pet.bone_hole = -1;
    else if (s_pet.bone_hole > i) s_pet.bone_hole--;
    if (s_dig_slot == i) s_dig_slot = -1;
    else if (s_dig_slot > i) s_dig_slot--;
    s_mess_ver++;
}

// Nouveau trou (ouvert, profondeur dig) ; liste pleine : le plus ancien trou rebouché (sinon le plus ancien) part
static int hole_add(int x, int y, int dig)
{
    if (s_pet.n_holes >= PET_MAX_HOLES) {
        int victim = -1;
        for (int i = 0; i < s_pet.n_holes && victim < 0; i++) if (s_pet.hole_filled[i] && i != s_pet.bone_hole) victim = i;
        for (int i = 0; i < s_pet.n_holes && victim < 0; i++) if (i != s_pet.bone_hole) victim = i;
        hole_remove(victim < 0 ? 0 : victim);
    }
    int i = s_pet.n_holes++;
    s_pet.holes[i] = (pet_spot_t){ (int16_t)x, (int16_t)y };
    s_pet.hole_dig[i] = (uint8_t)dig;
    s_pet.hole_filled[i] = 0;
    s_mess_ver++;
    return i;
}

void pet_lawn_spot(int *x, int *y)
{
    for (int t = 0; t < 40; t++) {
        int px = 150 + (int)(esp_random() % 270), py = 296 + (int)(esp_random() % 108);
        float dx = px - 240, dy = py - 250;
        if (dx * dx + dy * dy > 190 * 190) continue;                                   // hors de l'écran rond
        if (abs(px - SCENE_FOOD_X) < 48 && abs(py - SCENE_FOOD_Y) < 30) continue;     // gamelles
        if (abs(px - SCENE_WATER_X) < 48 && abs(py - SCENE_WATER_Y) < 30) continue;
        if (abs(px - SCENE_TOILET_X) < 60 && abs(py - SCENE_TOILET_Y) < 30) continue; // gravier
        if (px < 175 && py < 332) continue;                                           // devant l'abri
        if (hole_find(px, py, 40) >= 0) continue;                                     // autre trou
        *x = px; *y = py;
        return;
    }
    *x = 260; *y = 360;
}

bool pet_bone_buried(int *x, int *y)
{
    if (!s_pet.bone_buried || s_pet.bone_hole < 0 || s_pet.bone_hole >= s_pet.n_holes) return false;
    *x = s_pet.holes[s_pet.bone_hole].x;
    *y = s_pet.holes[s_pet.bone_hole].y;
    return true;
}

void pet_bone_dig_up(void)
{
    if (s_pet.bone_hole >= 0 && s_pet.bone_hole < s_pet.n_holes) {
        s_pet.hole_filled[s_pet.bone_hole] = 0;     // le trou se rouvre
        s_pet.hole_dig[s_pet.bone_hole] = 100;
    }
    s_pet.bone_buried = false;
    s_mess_ver++;
}

int pet_hole_open(int x, int y)
{
    int i = s_pet.bone_hole >= 0 && s_pet.bone_hole < s_pet.n_holes ? s_pet.bone_hole : hole_find(x, y, 14);
    if (i < 0) i = hole_add(x, y, 70);
    s_pet.hole_filled[i] = 0;
    s_mess_ver++;
    return i;
}

void pet_bone_bury(int x, int y)
{
    int i = pet_hole_open(x, y);
    s_pet.hole_dig[i] = 100;
    s_pet.hole_filled[i] = s_pet.age_s ? s_pet.age_s : 1;   // terre fraîche par-dessus
    s_pet.bone_hole = (int8_t)i;
    s_pet.bone_buried = true;
    s_mess_ver++;
}

// Ajoute un trou / des besoins (le plus ancien est remplacé si la liste est pleine)
static void add_spot(pet_spot_t *list, uint8_t *n, uint8_t max, pet_spot_t p)
{
    if (*n < max) list[(*n)++] = p;
    else { memmove(list, list + 1, (max - 1) * sizeof(*list)); list[max - 1] = p; }
    s_mess_ver++;
}

// ⚫ AJOUTÉ (25/10/2026) : heure réelle connue (NTP) ? ; temps carte éteinte déjà rattrapé ?
static bool clock_ok(void) { return time(NULL) > 1700000000; }
static bool s_offline_done;

static void pet_save(void)
{
    // ⚫ AJOUTÉ (25/10/2026) : heure de la sauvegarde (seulement si l'heure est sûre et le rattrapage fait : sinon
    // on garderait une heure fausse et l'absence serait perdue)
    if (clock_ok() && s_offline_done) s_pet.saved_epoch = (uint32_t)time(NULL);
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    pet_save_t sv = { .magic = SAVE_MAGIC, .pet = s_pet };
    esp_err_t err = nvs_set_blob(h, NVS_KEY, &sv, sizeof(sv));
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    if (err != ESP_OK) ESP_LOGW(TAG, "Sauvegarde impossible (%s)", esp_err_to_name(err));
    s_save_acc = 0;
}

void pet_init(void)
{
    pet_new();
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        pet_save_t sv;
        size_t len = sizeof(sv);
        // if (nvs_get_blob(h, NVS_KEY, &sv, &len) == ESP_OK && len == sizeof(sv) && sv.magic == SAVE_MAGIC) {
        // ⚫ MODIFIÉ (25/10/2026) : une sauvegarde d'avant les trous réalistes (plus courte) est reprise
        memset(&sv, 0, sizeof(sv));
        const size_t old_len = offsetof(pet_save_t, pet) + offsetof(pet_t, hole_dig);
        if (nvs_get_blob(h, NVS_KEY, &sv, &len) == ESP_OK && len >= old_len && len <= sizeof(sv) && sv.magic == SAVE_MAGIC) {
            // if (len < sizeof(sv)) {   // anciens trous : ouverts et finis ; pas d'os enterré
            //     memset((uint8_t *)&sv.pet + offsetof(pet_t, hole_dig), 0, sizeof(pet_t) - offsetof(pet_t, hole_dig));
            //     for (int i = 0; i < PET_MAX_HOLES; i++) sv.pet.hole_dig[i] = 100;
            //     sv.pet.bone_hole = -1;
            // }
            // ⚫ MODIFIÉ (25/10/2026) : champs absents d'une ancienne sauvegarde = 0 (cônes debout, pas malade…)
            if (len < sizeof(sv)) memset((uint8_t *)&sv + len, 0, sizeof(sv) - len);
            if (len <= offsetof(pet_save_t, pet) + offsetof(pet_t, hole_dig) + 1) {   // d'avant les trous réalistes
                for (int i = 0; i < PET_MAX_HOLES; i++) { sv.pet.hole_dig[i] = 100; sv.pet.hole_filled[i] = 0; }
                sv.pet.bone_buried = false;
                sv.pet.bone_hole = -1;
            }
            s_pet = sv.pet;
            clamp_all();
            ESP_LOGI(TAG, "Animal rechargé (âge %lu s)", (unsigned long)s_pet.age_s);
        }
        nvs_close(h);
    }
    // if (s_pet.age_s == 0) ESP_LOGI(TAG, "Nouvel animal : %s", CONFIG_PET_NAME);
    // ⚫ MODIFIÉ (25/10/2026) : nom choisi à l'adoption (page web)
    if (!s_pet.adopted) ESP_LOGI(TAG, "Aucun animal adopté : en attente (page web)");
    else if (s_pet.age_s == 0) ESP_LOGI(TAG, "Nouvel animal : %s", s_pet.name);
    if (s_pet.species >= PET_SPECIES_COUNT) s_pet.species = 0;
    s_mess_ver++;
}

static void set_msg(const char *fmt, const char *name)
{
    snprintf(s_msg, sizeof(s_msg), fmt, name);
}

// ⚫ MODIFIÉ (25/10/2026) : ancienne version du rattrapage (jauges "au repos", au plus 6 h), remplacée par la
// simulation complète ci-dessous :
// // ⚫ AJOUTÉ (25/10/2026) : rattrape le temps passé carte éteinte (une fois, dès que l'heure est synchronisée) :
// // âge complet (il grandit), trous qui se recouvrent de gazon, maladie qui guérit ; jauges comme s'il s'était
// // reposé (au plus 6 h simulées : il ne revient pas mourant après un week-end)
// static bool offline_catch_up(void)
// {
//     s_offline_done = true;
//     uint32_t now = (uint32_t)time(NULL);
//     if (!s_pet.saved_epoch || now <= s_pet.saved_epoch + 60) return false;
//     uint32_t away = now - s_pet.saved_epoch;
//     if (away > 400u * 86400) away = 400u * 86400;   // horloge aberrante : borne
//     uint32_t game = away * CONFIG_PET_TIME_SCALE;
//     s_pet.age_s += game;
//     // (trous rebouchés : la repousse se calcule sur age_s, qui vient d'avancer ; ceux qui ont fini de repousser
//     // disparaissent au prochain tick)
//     if (s_pet.sick) {
//         s_pet.sick_s = s_pet.sick_s > game ? s_pet.sick_s - game : 0;
//         if (!s_pet.sick_s) s_pet.sick = PET_SICK_NONE;
//     }
//     float sim = game > 6 * 3600 ? 6 * 3600 : (float)game;
//     pet_stats_t *s = &s_pet.s;
//     s->food   -= FOOD_DECAY / 2 * sim;
//     s->water  -= WATER_DECAY / 2 * sim;
//     s->joy    -= JOY_DECAY * 0.3f * sim;
//     s->potty  += POTTY_FILL / 2 * sim;
//     s->energy += ENERGY_RECOVER * 0.2f * sim;
//     s->discipline -= DISC_DECAY * sim;
//     if (s->food < 8) s->food = 8;
//     if (s->water < 8) s->water = 8;
//     if (s->potty > 90) s->potty = 90;
//     s_pet.crying = false;
//     s_pet.naughty = false;
//     clamp_all();
//     s_mess_ver++;
//     char t[40];
//     if (away >= 86400) snprintf(t, sizeof(t), "%lu j %lu h", (unsigned long)(away / 86400), (unsigned long)(away % 86400 / 3600));
//     else if (away >= 3600) snprintf(t, sizeof(t), "%lu h %02lu", (unsigned long)(away / 3600), (unsigned long)(away % 3600 / 60));
//     else snprintf(t, sizeof(t), "%lu min", (unsigned long)(away / 60));
//     snprintf(s_msg, sizeof(s_msg), "%s a vécu %s sans toi : il a grandi, il a faim et soif !", s_pet.name, t);
//     ESP_LOGI(TAG, "Carte éteinte pendant %lu s : âge %lu s (%s)", (unsigned long)away, (unsigned long)s_pet.age_s,
//              pet_stage_text(pet_stage()));
//     return true;
// }

// ⚫ AJOUTÉ (25/10/2026) : rattrape le temps passé carte éteinte, de façon réaliste : l'animal a vécu sans
// personne, par pas de 10 min de jeu, avec le jour et la nuit du lieu :
// - la nuit il dort (énergie), le jour il vit (faim, soif, ennui : il se sent seul) ;
// - il mange et boit dans ses gamelles tant qu'il en reste (2 repas, 3 lapées ; rien si elle est renversée) ;
// - besoins : au coin gravier s'il est assez discipliné et que le gravier n'est pas plein, sinon sur la pelouse ;
// - bêtises (selon discipline et âge) : creuser (ou finir un trou), renverser une gamelle, renverser un cône ;
// - santé : baisse à jeun, assoiffé ou au milieu de ses besoins (jamais sous 5 %) ; maladie qui guérit ;
// - âge complet (il grandit), trous rebouchés qui se recouvrent de gazon (sur l'âge).
static uint32_t s_offline_wait;   // secondes depuis la synchro de l'heure (on attend la météo pour le soleil)

static bool offline_catch_up(void)
{
    s_offline_done = true;
    uint32_t now = (uint32_t)time(NULL);
    if (!s_pet.saved_epoch || now <= s_pet.saved_epoch + 60) return false;
    uint32_t away = now - s_pet.saved_epoch;
    if (away > 60u * 86400) away = 60u * 86400;   // borne (horloge aberrante, très longue absence)
    weather_t w;
    weather_get(&w);
    int rise = w.valid ? w.sunrise_min : 7 * 60, set = w.valid ? w.sunset_min : 20 * 60;
    int32_t utc = w.valid ? w.utc_offset : 0;
    const int DT = 600;   // pas de 10 min de jeu
    uint32_t steps = away * CONFIG_PET_TIME_SCALE / DT;
    pet_stats_t *s = &s_pet.s;
    int food_res = s_pet.food_tipped ? 0 : 2, water_res = s_pet.water_tipped ? 0 : 3;
    int meals = 0, drinks = 0, new_holes = 0, tipped_f = 0, tipped_w = 0, cones = 0, lawn = 0, gravel = 0;
    for (uint32_t k = 0; k < steps; k++) {
        const age_k_t *ak = &AK;
        uint32_t real = s_pet.saved_epoch + (uint32_t)((uint64_t)k * DT / CONFIG_PET_TIME_SCALE);
        int minute = (int)(((int64_t)real + utc) % 86400 / 60);
        bool night = minute < rise || minute > set;
        bool sleep = night || s->energy < 15;
        if (sleep) {
            s->food -= FOOD_DECAY / 2 * ak->food * DT;
            s->water -= WATER_DECAY / 2 * ak->water * DT;
            s->potty += POTTY_FILL / 2 * ak->potty * DT;
            s->energy += ENERGY_RECOVER * ak->recover * DT;
        } else {
            s->food -= FOOD_DECAY * ak->food * DT;
            s->water -= WATER_DECAY * ak->water * DT;
            s->potty += POTTY_FILL * ak->potty * DT;
            s->energy -= ENERGY_DECAY * ak->energy * DT;
            s->joy -= JOY_DECAY * ak->joy * 1.3f * DT;   // seul : il s'ennuie
            s->discipline -= DISC_DECAY * DT;
            // gamelles
            if (s->food < 40 && food_res > 0 && !s_pet.food_tipped) { s->food += 30; s->potty += 10; food_res--; meals++; }
            if (s->water < 40 && water_res > 0 && !s_pet.water_tipped) { s->water += 35; s->potty += 15; water_res--; drinks++; }
            // bêtises (probabilité par pas : comme en jeu, une par minute au plus sur 10 min tirée une fois)
            uint32_t pm = (uint32_t)((50 + (100 - s->discipline) * 4) * ak->naughty);
            if (!s_pet.sick && esp_random() % 1000 < pm) {
                uint32_t r = esp_random() % 100;
                if (r < 20 && !s_pet.food_tipped) { s_pet.food_tipped = true; food_res = 0; tipped_f++; }
                else if (r < 35 && !s_pet.water_tipped) { s_pet.water_tipped = true; water_res = 0; tipped_w++; }
                else if (r < 45 && s_pet.cones_down != 3) { s_pet.cones_down |= (s_pet.cones_down & 1) ? 2 : 1; cones++; }
                else {   // creuse : finit un trou commencé, sinon en commence un (4 au plus)
                    int i = -1;
                    for (int j = 0; j < s_pet.n_holes; j++) if (!s_pet.hole_filled[j] && s_pet.hole_dig[j] < 100 && j != s_pet.bone_hole) i = j;
                    if (i < 0 && s_pet.n_holes < PET_MAX_HOLES) { pet_spot_t sp = rand_spot(); i = hole_add(sp.x, sp.y, 10); new_holes++; }
                    if (i >= 0) s_pet.hole_dig[i] = s_pet.hole_dig[i] + 45 > 100 ? 100 : s_pet.hole_dig[i] + 45;
                    s->health -= 1;   // terre mangée
                }
            }
        }
        // besoins
        if (s->potty >= 90) {
            s->potty = 0;
            if (s->discipline >= ak->auto_potty && s_pet.toilet_poops < 3) { s_pet.toilet_poops++; gravel++; }
            else {
                pet_spot_t sp = rand_spot();
                add_spot(s_pet.poops, &s_pet.n_poops, PET_MAX_POOPS, sp);
                s->discipline -= 3;
                lawn++;
            }
        }
        // santé
        s->health -= POOP_HEALTH * s_pet.n_poops * DT;
        if (s->food <= 0 || s->water <= 0) s->health -= HEALTH_DECAY * 0.15f * DT;
        else if (s->food > 40 && s->water > 40 && s->joy > 40) s->health += HEALTH_RECOVER * DT;
        if (s_pet.sick) {
            uint32_t d = sleep ? 2 * DT : DT;
            s_pet.sick_s = s_pet.sick_s > d ? s_pet.sick_s - d : 0;
            if (!s_pet.sick_s) s_pet.sick = PET_SICK_NONE;
        }
        clamp_all();
        if (s->health < 5) s->health = 5;
        s_pet.age_s += DT;
        // trous rebouchés qui ont fini de repousser
        for (int i = s_pet.n_holes - 1; i >= 0; i--)
            if (s_pet.hole_filled[i] && s_pet.age_s - s_pet.hole_filled[i] >= PET_REGROW_S && i != s_pet.bone_hole) hole_remove(i);
    }
    s_pet.age_s += away * CONFIG_PET_TIME_SCALE % DT;
    s_pet.crying = false;
    s_pet.naughty = false;
    int minute_now = (int)(((int64_t)now + utc) % 86400 / 60);
    s_pet.sleeping = (minute_now < rise || minute_now > set) && s->energy < 95;
    s_mess_ver++;
    // résumé de l'absence
    char t[24];
    if (away >= 86400) snprintf(t, sizeof(t), "%lu j %lu h", (unsigned long)(away / 86400), (unsigned long)(away % 86400 / 3600));
    else if (away >= 3600) snprintf(t, sizeof(t), "%lu h %02lu", (unsigned long)(away / 3600), (unsigned long)(away % 3600 / 60));
    else snprintf(t, sizeof(t), "%lu min", (unsigned long)(away / 60));
    int o = 0;
    // ajout borné (snprintf renvoie la longueur voulue, pas celle écrite)
#define ADD(...) do { if (o < (int)sizeof(s_msg) - 1) { o += snprintf(s_msg + o, sizeof(s_msg) - o, __VA_ARGS__); if (o > (int)sizeof(s_msg) - 1) o = sizeof(s_msg) - 1; } } while (0)
    ADD("%s d'absence :", t);
    if (meals || drinks) ADD(" gamelles %s,", (food_res || water_res) ? "entamées" : "vidées");
    if (new_holes) ADD(" %d trou%s,", new_holes, new_holes > 1 ? "s" : "");
    if (tipped_f || tipped_w) ADD(" gamelle renversée,");
    if (cones) ADD(" cône à terre,");
    if (lawn) ADD(" %d besoin%s dans la cour,", lawn, lawn > 1 ? "s" : "");
    if (o > 0 && s_msg[o - 1] == ',') s_msg[o - 1] = '.';
    else ADD(" %s t'attendait.", s_pet.name);
#undef ADD
    ESP_LOGI(TAG, "Carte éteinte %lu s : %lu pas, repas %d, eau %d, trous %d, besoins cour %d / gravier %d, âge %lu s (%s)",
             (unsigned long)away, (unsigned long)steps, meals, drinks, new_holes, lawn, gravel, (unsigned long)s_pet.age_s,
             pet_stage_text(pet_stage()));
    ESP_LOGI(TAG, "%s", s_msg);
    return true;
}

bool pet_tick(uint32_t dt_s)
{
    bool event = false;
    if (!s_pet.adopted) return false;   // ⚫ AJOUTÉ (25/10/2026) : le temps ne passe qu'après l'adoption
    // if (!s_offline_done && clock_ok()) {   // ⚫ AJOUTÉ (25/10/2026)
    weather_t w0;
    weather_get(&w0);
    if (!s_offline_done && clock_ok() && (w0.valid || ++s_offline_wait > 90)) {   // ⚫ MODIFIÉ (25/10/2026) : + soleil du lieu
        event = offline_catch_up();
        if (event) s_save_in = 3;          // sauvegarde de l'état rattrapé (et de la nouvelle heure)
    }
    for (uint32_t n = 0; n < dt_s * CONFIG_PET_TIME_SCALE; n++) {   // pas de 1 s de jeu
        pet_stats_t *s = &s_pet.s;
        s_pet.age_s++;

        const age_k_t *k = &AK;   // ⚫ AJOUTÉ (25/10/2026) : rythme de l'âge
        if (s_pet.sleeping) {
            // s->food   -= FOOD_DECAY / 2;
            // s->water  -= WATER_DECAY / 2;
            // s->potty  += POTTY_FILL / 2;   // ⚫ AJOUTÉ (25/10/2026)
            // s->energy += ENERGY_RECOVER;
            s->food   -= FOOD_DECAY / 2 * k->food;     // ⚫ MODIFIÉ (25/10/2026) : selon l'âge
            s->water  -= WATER_DECAY / 2 * k->water;
            s->potty  += POTTY_FILL / 2 * k->potty;
            s->energy += ENERGY_RECOVER * k->recover;
            // if (s->energy >= 100) {
            // ⚫ MODIFIÉ (25/10/2026) : la nuit (heure réelle), il dort jusqu'au matin
            if (s->energy >= 100 && !(s_env_known && s_env_night)) {
                s_pet.sleeping = false;
                set_msg("%s se réveille en pleine forme !", s_pet.name);
                event = true;
            }
        } else {
            // s->food       -= FOOD_DECAY;
            // s->water      -= WATER_DECAY;
            // s->potty      += POTTY_FILL;   // ⚫ AJOUTÉ (25/10/2026)
            // s->energy     -= ENERGY_DECAY;
            // s->joy        -= JOY_DECAY * ((s->food < 20 || s->energy < 15) ? 2 : 1);
            // s->joy        -= JOY_DECAY * ((s->food < 20 || s->water < 20 || s->energy < 15) ? 2 : 1);
            // ⚫ MODIFIÉ (25/10/2026) : selon l'âge
            s->food       -= FOOD_DECAY * k->food;
            s->water      -= WATER_DECAY * k->water;
            s->potty      += POTTY_FILL * k->potty;
            s->energy     -= ENERGY_DECAY * k->energy;
            s->joy        -= JOY_DECAY * k->joy * ((s->food < 20 || s->water < 20 || s->energy < 15) ? 2 : 1);
            s->discipline -= DISC_DECAY * (s_pet.naughty ? 3 : 1);   // une bêtise ignorée s'installe

            if (s->energy <= 0) {
                s_pet.sleeping = true;
                s_pet.crying = false;
                s->health -= 5;
                set_msg("%s s'endort d'épuisement...", s_pet.name);
                event = true;
            }

            // ⚫ AJOUTÉ (25/10/2026) : la nuit tombe : il va se coucher s'il est un peu fatigué et qu'on ne joue pas
            s_since_action++;
            if (s_env_known && s_env_night && s->energy < 90 && s_since_action > 300 && !s_pet.naughty) {
                s_pet.sleeping = true;
                s_pet.crying = false;
                set_msg("La nuit tombe : %s va se coucher.", s_pet.name);
                event = true;
            }
            // sous la pluie / la neige, hors de l'abri : moins heureux
            if (s_env_bad && !s_env_sheltered) s->joy -= JOY_DECAY;

            // if (s_pet.crying && ++s_cry_s >= CRY_MAX_S) {
            if (s_pet.crying && ++s_cry_s >= (uint32_t)k->cry_max) {   // ⚫ MODIFIÉ (25/10/2026) : selon l'âge   // personne ne l'a consolé : il boude
                s_pet.crying = false;
                s->joy -= 10;
                set_msg("%s boude dans son coin.", s_pet.name);
                event = true;
            }

            // ⚫ AJOUTÉ (25/10/2026) : assez discipliné, il va seul au coin besoins
            // if (s->potty >= 90 && s->discipline >= 45) {
            if (s->potty >= 90 && s->discipline >= k->auto_potty) {   // ⚫ MODIFIÉ (25/10/2026) : jamais seul quand il est bébé
                s->potty = 0;
                if (s_pet.toilet_poops < 3) s_pet.toilet_poops++;
                s_auto_potty = true;
                s_mess_ver++;
                set_msg("%s va faire ses besoins tout seul. Bravo !", s_pet.name);
                event = true;
            }
            // ⚫ AJOUTÉ (25/10/2026) : il n'a pas pu se retenir : besoins là où il se trouve
            if (s->potty >= 100) {
                s->potty = 0;
                pet_spot_t at = { s_pos_x, s_pos_y };
                add_spot(s_pet.poops, &s_pet.n_poops, PET_MAX_POOPS, at);
                s_pet.naughty = true;
                s_pet.naughty_kind = PET_NAUGHTY_POOP;
                s_pet.naughty_at = (pet_spot_t){ -1000, -1000 };   // déjà déposé là où il est
                s->discipline -= 5;
                health_hit(3, "besoins dans la cour (hygiène)");   // ⚫ AJOUTÉ (25/10/2026)
                // set_msg("%s n'a pas pu se retenir...", s_pet.name);
                set_msg(pet_stage() == PET_STAGE_BABY ? "%s est trop petit pour se retenir : il faut le sortir !"
                                                       : "%s n'a pas pu se retenir...", s_pet.name);
                event = true;
            }

            if (++s_naughty_acc >= NAUGHTY_CHECK_S) {
                s_naughty_acc = 0;
                // 5 %/min si discipline à 100, 45 %/min si discipline à 0
                // uint32_t p = 50 + (uint32_t)((100 - s->discipline) * 4);   // pour mille
                uint32_t p = (uint32_t)((50 + (100 - s->discipline) * 4) * k->naughty);   // ⚫ MODIFIÉ (25/10/2026) : selon l'âge
                // if (!s_pet.naughty && !s_pet.crying && (esp_random() % 1000) < p) {
                if (!s_pet.naughty && !s_pet.crying && !s_pet.sick && (esp_random() % 1000) < p) {   // ⚫ 25/10/2026 : pas malade
                    s_pet.naughty = true;
                    // set_msg("%s fait une bêtise !", CONFIG_PET_NAME);
                    // ⚫ MODIFIÉ (25/10/2026) : plusieurs sortes de bêtises, avec des dégâts dans la cour
                    uint32_t k = esp_random() % 100;
                    if (k >= 45 && k < 58 && s_pet.cones_down != 3) {   // ⚫ AJOUTÉ (25/10/2026) : renverse un cône
                        s_pet.naughty = false;
                        pet_start_naughty(4);
                    } else
                    if (k < 25 && !s_pet.food_tipped) {
                        s_pet.naughty_kind = PET_NAUGHTY_TIP_FOOD;
                        s_pet.food_tipped = true;
                        s_pet.naughty_at = (pet_spot_t){ SCENE_FOOD_X, SCENE_FOOD_Y };
                        set_msg("%s renverse sa gamelle de croquettes !", s_pet.name);
                        health_hit(1, "bêtise : gamelle de croquettes renversée");   // ⚫ AJOUTÉ (25/10/2026)
                    } else if (k < 45 && !s_pet.water_tipped) {
                        s_pet.naughty_kind = PET_NAUGHTY_TIP_WATER;
                        s_pet.water_tipped = true;
                        s_pet.naughty_at = (pet_spot_t){ SCENE_WATER_X, SCENE_WATER_Y };
                        set_msg("%s renverse sa gamelle d'eau !", s_pet.name);
                        health_hit(1, "bêtise : gamelle d'eau renversée");   // ⚫ AJOUTÉ (25/10/2026)
                    // ⚫ RETIRÉ (25/10/2026) : bêtise "besoins n'importe où" au hasard (les besoins se font au coin
                    // gravier ; reste seulement l'accident quand on ne l'a pas sorti à temps)
                    } else {
                        s_pet.naughty_kind = PET_NAUGHTY_DIG;
                        // s_pet.naughty_at = rand_spot();
                        // ⚫ MODIFIÉ (25/10/2026) : un trou commencé (ouvert, pas fini) est repris, pas recommencé ailleurs
                        int cont = -1;
                        for (int i = 0; i < s_pet.n_holes; i++)
                            if (!s_pet.hole_filled[i] && s_pet.hole_dig[i] < 100 && i != s_pet.bone_hole) cont = i;
                        s_pet.naughty_at = cont >= 0 ? s_pet.holes[cont] : rand_spot();
                        set_msg(cont >= 0 ? "%s retourne finir son trou !" : "%s creuse un trou dans la pelouse !", s_pet.name);
                        health_hit(2, "bêtise : il a mangé de la terre");   // ⚫ AJOUTÉ (25/10/2026)
                    }
                    // les trous et besoins apparaissent quand il arrive sur place (main/pet_ui.c, pet_set_pos)
                    s_mess_ver++;
                    event = true;
                }
            }
        }

        // ⚫ AJOUTÉ (25/10/2026) : malade : il s'affaiblit, guérit avec le temps (2 fois plus vite s'il dort ou est
        // à l'abri) ; dehors sous la pluie plus de 10 min : il prend froid ; en plein soleil > 30 °C : coup de chaleur
        if (s_pet.sick) {
            s->health -= 100.0f / 9000;
            s->joy -= JOY_DECAY * 0.5f;
            uint32_t k2 = (s_pet.sleeping || s_env_sheltered) ? 2 : 1;
            s_pet.sick_s = s_pet.sick_s > k2 ? s_pet.sick_s - k2 : 0;
            if (!s_pet.sick_s) {
                s_pet.sick = PET_SICK_NONE;
                set_msg("%s est guéri, il reprend des forces !", s_pet.name);
                event = true;
            }
        } else if (!s_pet.sleeping) {
            static uint32_t wet_s, hot_s;
            weather_t w;
            weather_get(&w);
            wet_s = (s_env_bad && !s_env_sheltered) ? wet_s + 1 : (wet_s > 2 ? wet_s - 2 : 0);
            hot_s = (w.valid && w.temp > 30 && weather_tod() == TOD_DAY && !s_env_sheltered) ? hot_s + 1 : (hot_s ? hot_s - 1 : 0);
            if (wet_s > 600) { wet_s = 0; pet_make_sick(PET_SICK_COLD); event = true; }
            if (hot_s > 900) { hot_s = 0; pet_make_sick(PET_SICK_HEAT); event = true; }
        }
        // ⚫ AJOUTÉ (25/10/2026) : il creuse : le trou se creuse peu à peu (~30 s pour un trou fini) ; il le
        // reprend s'il a été interrompu
        if (s_pet.naughty && s_pet.naughty_kind == PET_NAUGHTY_DIG && s_dig_slot >= 0 && s_dig_slot < s_pet.n_holes &&
            abs(s_pos_x - s_pet.holes[s_dig_slot].x) < 10 && abs(s_pos_y - s_pet.holes[s_dig_slot].y) < 10 &&
            s_pet.hole_dig[s_dig_slot] < 100) {
            uint8_t before = s_pet.hole_dig[s_dig_slot];
            s_pet.hole_dig[s_dig_slot] = before + 3 > 100 ? 100 : before + 3;
            if (before / 10 != s_pet.hole_dig[s_dig_slot] / 10) s_mess_ver++;
        }
        if (!s_pet.naughty) s_dig_slot = -1;
        // trou rebouché : le gazon le recouvre, puis il disparaît (sauf celui de l'os : il reste sous la pelouse)
        for (int i = s_pet.n_holes - 1; i >= 0; i--)
            if (s_pet.hole_filled[i] && s_pet.age_s - s_pet.hole_filled[i] >= PET_REGROW_S && i != s_pet.bone_hole) hole_remove(i);
        // if (s->food <= 0 || s->joy <= 0) {
        // ⚫ AJOUTÉ (25/10/2026) : besoins laissés dans la cour : mauvais pour la santé
        s->health -= POOP_HEALTH * s_pet.n_poops;
        if (s->food <= 0 || s->water <= 0 || s->joy <= 0) {
            s->health -= HEALTH_DECAY;
        // } else if (s->food > 40 && s->joy > 40 && s->energy > 20) {
        } else if (s->food > 40 && s->water > 40 && s->joy > 40 && s->energy > 20) {
            s->health += HEALTH_RECOVER;
        }
        clamp_all();
    }

    s_save_acc += dt_s;
    // if (event || s_save_acc >= CONFIG_PET_SAVE_PERIOD_S) pet_save();
    // ⚫ MODIFIÉ (25/10/2026) : actions et événements programment une sauvegarde 2 s plus tard (regroupée) :
    // chaque écriture flash perturbe l'écran une fraction de seconde, on en fait le moins possible.
    // if (event && s_save_in < 0) s_save_in = 2;
    // ⚫ MODIFIÉ (25/10/2026) : une écriture flash perturbe l'écran (le rafraîchissement des bounce buffers est
    // suspendu) : plus aucune sauvegarde déclenchée par les appuis. Sauvegarde toutes les PET_SAVE_PERIOD_S
    // (5 min par défaut) et 4 s après l'endormissement, quand la nuit est tombée (écran sombre).
    static bool was_sleeping;
    if (s_pet.sleeping && !was_sleeping && s_save_in < 0) s_save_in = 4;
    was_sleeping = s_pet.sleeping;
    if (s_save_in > 0) s_save_in -= (int)dt_s;
    if (s_save_in == 0 || s_save_in < -1 || s_save_acc >= CONFIG_PET_SAVE_PERIOD_S) {
        pet_save();          // remet s_save_acc à 0
        s_save_in = -1;
    }
    return event;
}

pet_react_t pet_do(pet_action_t act, const char **msg)
{
    pet_stats_t *s = &s_pet.s;
    // const char *name = CONFIG_PET_NAME;
    const char *name = s_pet.name;   // ⚫ MODIFIÉ (25/10/2026) : nom choisi à l'adoption
    pet_react_t r = PET_REACT_NONE;

    // ⚫ AJOUTÉ (25/10/2026) : malade, il ne veut pas jouer
    bool game = act == PET_ACT_PLAY || act == PET_ACT_FETCH || act == PET_ACT_FRISBEE || act == PET_ACT_BIRDS ||
                (act >= PET_ACT_YARN && act <= PET_ACT_GRAZE);
    if (s_pet.sick && game && !s_pet.sleeping) {
        set_msg("%s est malade : il n'a pas envie de jouer, il a besoin de repos.", name);
        if (msg) *msg = s_msg;
        return PET_REACT_REFUSE;
    }

    switch (act) {
    // ⚫ AJOUTÉ (25/10/2026) : médicament
    case PET_ACT_HEAL:
        if (s_pet.sick) {
            if (s_pet.sick_s > 240) s_pet.sick_s = 240;   // il ira mieux dans quelques minutes (repos)
            s->health += 8;
            set_msg("%s prend son médicament. Du repos, et il ira mieux.", name);
            r = PET_REACT_HEAL;
        } else {
            set_msg("%s n'est pas malade.", name);
            r = PET_REACT_REFUSE;
        }
        break;

    case PET_ACT_FEED:
        if (s_pet.food_tipped) {   // ⚫ AJOUTÉ (25/10/2026) : remplir la gamelle la remet droite
            s_pet.food_tipped = false;
            s_mess_ver++;
        }
        // if (s->food >= 90) {
        if (s->food >= 95) {   // ⚫ MODIFIÉ (25/10/2026) : il ne refuse que s'il est vraiment plein
            s->joy -= 3;
            set_msg("%s n'a plus faim !", name);
            r = PET_REACT_REFUSE;
        } else {
            float before = s->food;   // ⚫ AJOUTÉ (25/10/2026)
            s->food += 30;
            s->joy += 3;
            s->health += 1;
            s->potty += 10;   // ⚫ AJOUTÉ (25/10/2026)
            set_msg("Miam ! %s se régale.", name);
            r = PET_REACT_EAT;
            // ⚫ AJOUTÉ (25/10/2026) : nourri alors qu'il n'avait plus faim : il a trop mangé (indigestion)
            if (before >= 70 && !s_pet.sick && (esp_random() % 100) < (pet_stage() == PET_STAGE_BABY ? 70u : 50u)) {
                pet_make_sick(PET_SICK_STOMACH);
                set_msg("%s a trop mangé : il a mal au ventre...", name);
            }
        }
        break;

    // ⚫ AJOUTÉ (25/10/2026) : boire à la gamelle d'eau
    case PET_ACT_DRINK:
        if (s_pet.water_tipped) {
            s_pet.water_tipped = false;
            s_mess_ver++;
        }
        if (s->water >= 90) {
            set_msg("%s n'a pas soif.", name);
            r = PET_REACT_REFUSE;
        } else {
            s->water += 35;
            s->health += 1;
            s->potty += 15;   // ⚫ AJOUTÉ (25/10/2026)
            set_msg("Slurp ! %s boit à grandes lapées.", name);
            r = PET_REACT_DRINK;
        }
        break;

    case PET_ACT_SLEEP:
        if (s->energy >= 80) {
            s->joy -= 2;
            set_msg("%s n'a pas sommeil !", name);
            r = PET_REACT_REFUSE;
        } else {
            s_pet.sleeping = true;
            s_pet.crying = false;
            set_msg("Bonne nuit %s...", name);
            r = PET_REACT_SLEEP;
        }
        break;

    case PET_ACT_PLAY:
        if (s_pet.crying) {
            set_msg("%s pleure... console-le d'abord.", name);
            r = PET_REACT_REFUSE;
        } else if (s->energy < 15) {
            set_msg("%s est trop fatigué pour jouer.", name);
            r = PET_REACT_REFUSE;
        } else if (s->food < 10) {
            set_msg("%s a trop faim pour jouer.", name);
            r = PET_REACT_REFUSE;
        } else if (s->water < 10) {   // ⚫ AJOUTÉ (25/10/2026)
            set_msg("%s a trop soif pour courir.", name);
            r = PET_REACT_REFUSE;
        } else {
            s->joy += 20;
            s->energy -= 12;
            s->food -= 6;
            s->water -= 10;   // ⚫ AJOUTÉ (25/10/2026) : courir donne soif
            // set_msg("Youpi ! %s adore jouer.", name);
            set_msg("Youpi ! %s court après la balle !", name);
            r = PET_REACT_JOY;
        }
        break;

    // ⚫ AJOUTÉ (25/10/2026) : nouveaux jeux
    case PET_ACT_FETCH:
    case PET_ACT_FRISBEE: {
        bool frisbee = (act == PET_ACT_FRISBEE);
        float need = frisbee ? 20 : 12;   // le frisbee (course + saut) fatigue davantage
        if (s_pet.crying) {
            set_msg("%s pleure... console-le d'abord.", name);
            r = PET_REACT_REFUSE;
        } else if (s->energy < need) {
            set_msg("%s est trop fatigué pour courir.", name);
            r = PET_REACT_REFUSE;
        } else if (s->food < 10 || s->water < 10) {
            set_msg("%s a trop faim ou trop soif.", name);
            r = PET_REACT_REFUSE;
        } else {
            s->joy        += frisbee ? 22 : 15;
            s->energy     -= frisbee ? 14 : 8;
            s->water      -= frisbee ? 8 : 6;
            s->food       -= frisbee ? 4 : 3;
            s->discipline += 3;               // il obéit et rapporte : bonne habitude
            if (s_pet.naughty) s_pet.naughty = false;   // le jeu le détourne de sa bêtise
            set_msg(frisbee ? "Hop ! %s attrape le frisbee au vol !" : "%s rapporte la balle, fier de lui !", name);
            r = PET_REACT_JOY;
        }
        break;
    }
    case PET_ACT_BONE:
        // jeu calme : possible même fatigué ; ne le récompense pas pendant une bêtise
        if (s_pet.crying) {
            s->joy += 6;
            s_pet.crying = false;
            set_msg("%s oublie son chagrin en rongeant son os.", name);
        } else if (s_pet.naughty) {
            s_pet.naughty = false;
            s->discipline -= 3;
            s->joy += 8;
            set_msg("%s lâche sa bêtise pour l'os... malin !", name);
        } else {
            s->joy += 12;
            s->food += 5;
            set_msg("%s ronge son os, tout content.", name);
        }
        r = PET_REACT_JOY;
        break;

    // ⚫ AJOUTÉ (25/10/2026) : courir après les oiseaux
    case PET_ACT_BIRDS:
        if (s_pet.crying || s->energy < 12) {
            set_msg(s_pet.crying ? "%s pleure... console-le d'abord." : "%s est trop fatigué pour courir.", name);
            r = PET_REACT_REFUSE;
        } else {
            s->joy += 18;
            s->energy -= 10;
            s->water -= 6;
            set_msg("%s court après les oiseaux !", name);
            r = PET_REACT_JOY;
        }
        break;

    // ⚫ AJOUTÉ (25/10/2026) : jeux propres à certaines espèces
    case PET_ACT_YARN: case PET_ACT_FEATHER: case PET_ACT_HURDLE: case PET_ACT_CARROT:
    case PET_ACT_MUD: case PET_ACT_CLIMB: case PET_ACT_BRUSH: case PET_ACT_GRAZE: {
        // énergie dépensée, bonheur gagné, faim / soif ; "calme" = possible même fatigué
        static const struct { int8_t energy, joy, food, water; bool calm; const char *ok; } G[] = {
            [PET_ACT_YARN - PET_ACT_YARN]    = { -8, 16, -3, -4, false, "%s fait rouler la pelote dans tous les sens !" },
            [PET_ACT_FEATHER - PET_ACT_YARN] = { -9, 18, -3, -4, false, "%s bondit sur la plume !" },
            [PET_ACT_HURDLE - PET_ACT_YARN]  = { -14, 20, -5, -8, false, "%s saute l'obstacle avec élégance !" },
            [PET_ACT_CARROT - PET_ACT_YARN]  = { 0, 12, 8, 0, true, "Crac ! %s croque sa carotte." },
            [PET_ACT_MUD - PET_ACT_YARN]     = { -4, 22, 0, 0, true, "%s se roule dans la boue, ravi !" },
            [PET_ACT_CLIMB - PET_ACT_YARN]   = { -10, 18, -3, -5, false, "%s grimpe sur la botte de foin !" },
            [PET_ACT_BRUSH - PET_ACT_YARN]   = { 0, 14, 0, 0, true, "%s adore être brossé." },
            [PET_ACT_GRAZE - PET_ACT_YARN]   = { 0, 10, 12, 0, true, "%s broute l'herbe fraîche." },
        };
        const typeof(G[0]) *g = &G[act - PET_ACT_YARN];
        if (!(pet_games_mask(s_pet.species) & (1u << act))) {
            set_msg("%s ne connaît pas ce jeu.", name);
            r = PET_REACT_REFUSE;
        } else if (s_pet.crying && !g->calm) {
            set_msg("%s pleure... console-le d'abord.", name);
            r = PET_REACT_REFUSE;
        } else if (!g->calm && s->energy < 12) {
            set_msg("%s est trop fatigué pour jouer.", name);
            r = PET_REACT_REFUSE;
        } else {
            s->energy += g->energy;
            s->joy += g->joy;
            s->food += g->food;
            s->water += g->water;
            if (s_pet.crying && g->calm) s_pet.crying = false;
            if (s_pet.naughty) s_pet.naughty = false;   // le jeu le détourne de sa bêtise
            set_msg(g->ok, name);
            r = PET_REACT_JOY;
        }
        break;
    }

    // ⚫ AJOUTÉ (25/10/2026) : sortir faire ses besoins au bon endroit
    case PET_ACT_POTTY:
        if (s->potty < 25) {
            set_msg("%s n'a pas envie.", name);
            r = PET_REACT_REFUSE;
        } else {
            s->potty = 0;
            s->discipline += 4;
            s->joy += 3;
            if (s_pet.toilet_poops < 3) s_pet.toilet_poops++;   // ⚫ AJOUTÉ (25/10/2026) : reste dans le gravier
            s_mess_ver++;
            if (s_pet.naughty_kind == PET_NAUGHTY_POOP && s_pet.naughty) s_pet.naughty = false;
            set_msg("%s fait ses besoins au bon endroit. Bravo !", name);
            r = PET_REACT_POTTY;
        }
        break;

    // ⚫ AJOUTÉ (25/10/2026) : nettoyer la cour
    case PET_ACT_CLEAN:
        // if (!s_pet.n_holes && !s_pet.n_poops && !s_pet.food_tipped && !s_pet.water_tipped) {
    {
        // ⚫ MODIFIÉ (25/10/2026) : les trous sont rebouchés (terre fraîche, le gazon repousse en quelques heures),
        // ils ne disparaissent pas d'un coup
        int open_holes = 0;
        for (int i = 0; i < s_pet.n_holes; i++) if (!s_pet.hole_filled[i]) open_holes++;
        if (s_pet.naughty && s_pet.naughty_kind == PET_NAUGHTY_DIG) { s_pet.naughty = false; s_dig_slot = -1; }
        // if (!s_pet.n_holes && !s_pet.n_poops && !s_pet.food_tipped && !s_pet.water_tipped && !s_pet.toilet_poops) {
        if (!open_holes && !s_pet.n_poops && !s_pet.food_tipped && !s_pet.water_tipped && !s_pet.toilet_poops && !s_pet.cones_down) {
            set_msg("La cour est déjà propre.", name);
            r = PET_REACT_REFUSE;
        } else {
            // s_pet.n_holes = s_pet.n_poops = 0;
            for (int i = 0; i < s_pet.n_holes; i++) if (!s_pet.hole_filled[i]) s_pet.hole_filled[i] = s_pet.age_s ? s_pet.age_s : 1;
            s_pet.n_poops = 0;
            s_pet.cones_down = 0;   // ⚫ AJOUTÉ (25/10/2026) : cônes relevés
            s_pet.toilet_poops = 0;   // ⚫ AJOUTÉ (25/10/2026)
            s_pet.food_tipped = s_pet.water_tipped = false;
            s->joy += 5;
            s->health += 2;
            s_mess_ver++;
            // set_msg("La cour est toute propre. %s est content !", name);
            set_msg(open_holes ? "Trous rebouchés : le gazon repoussera dans quelques heures. %s est content !"
                               : "La cour est toute propre. %s est content !", name);
            r = PET_REACT_CLEAN;
        }
        break;
    }

    case PET_ACT_PET:
        if (s_pet.crying) {
            s->joy += 4;
            set_msg("%s renifle un peu...", name);
        } else if (s_pet.naughty) {
            // une caresse pendant une bêtise l'encourage à recommencer
            s->joy += 8;
            s->discipline -= 6;
            // set_msg("%s ronronne... sans regretter sa bêtise.", name);
            // set_msg("%s remue la queue... sans regretter sa bêtise.", name);   // ⚫ 25/10/2026 : c'est un chien
            set_msg("%s en redemande... sans regretter sa bêtise.", name);   // ⚫ 25/10/2026 : toutes espèces
        } else {
            s->joy += 10;
            // set_msg("%s ronronne de bonheur.", name);
            // set_msg("%s se roule de bonheur.", name);   // ⚫ 25/10/2026 : c'est un chien
            // ⚫ MODIFIÉ (25/10/2026) : son de l'espèce (Ouaf, Miaou, Bêêê…)
            snprintf(s_msg, sizeof(s_msg), "%s ! %s adore les caresses.", s_species_sound[s_pet.species], name);
        }
        r = PET_REACT_LOVE;
        break;

    case PET_ACT_SCOLD:
        if (s_pet.naughty) {
            s_pet.naughty = false;
            // s->discipline += 15;
            s->discipline += 15 * AK.learn;   // ⚫ MODIFIÉ (25/10/2026) : un bébé apprend lentement
            s->joy -= 4;
            set_msg("%s baisse les oreilles. Compris !", name);
            r = PET_REACT_SORRY;
        } else {
            s->joy -= 12;
            s_pet.crying = true;
            s_cry_s = 0;
            health_hit(2, "grondé sans raison (stress)");   // ⚫ AJOUTÉ (25/10/2026)
            set_msg("%s ne comprend pas pourquoi...", name);
            r = PET_REACT_HURT;
        }
        break;

    case PET_ACT_PUNISH:
        // ⚫ AJOUTÉ (25/10/2026) : un bébé est trop petit pour comprendre une punition : il a juste peur
        if (s_pet.naughty && pet_stage() == PET_STAGE_BABY) {
            s_pet.naughty = false;
            s->discipline += 8;
            s->joy -= 15;
            s_pet.crying = true;
            s_cry_s = 0;
            health_hit(2, "puni trop petit (il ne comprend pas, il a peur)");
            set_msg("%s est trop petit pour comprendre... il a peur. Grondez-le plutôt.", name);
            r = PET_REACT_HURT;
        } else if (s_pet.naughty) {
            s_pet.naughty = false;
            // s->discipline += 30;
            s->discipline += 30 * AK.learn;   // ⚫ MODIFIÉ (25/10/2026) : selon l'âge
            s->joy -= 12;
            s_pet.crying = true;          // sanction méritée, mais il a quand même du chagrin
            // s_cry_s = CRY_MAX_S - 40;     // chagrin court
            s_cry_s = AK.cry_max - 40;    // ⚫ MODIFIÉ (25/10/2026) : chagrin court (durée selon l'âge)
            // set_msg("%s file au coin. Il ne recommencera plus.", name);
            // set_msg("%s file à la niche. Il ne recommencera plus.", name);   // ⚫ 25/10/2026
            set_msg("%s file au coin. Il ne recommencera plus.", name);   // ⚫ 25/10/2026 : plus de niche
            r = PET_REACT_SORRY;
        } else {
            s->joy -= 25;
            // s->health -= 5;
            health_hit(6, "puni sans raison");   // ⚫ MODIFIÉ (25/10/2026) : perte affichée avec sa raison
            s_pet.crying = true;
            s_cry_s = 0;
            set_msg("%s est puni sans raison... il pleure.", name);
            r = PET_REACT_HURT;
        }
        break;

    case PET_ACT_COMFORT:
        if (s_pet.crying) {
            s_pet.crying = false;
            s->joy += 18;
            set_msg("%s sèche ses larmes.", name);
            r = PET_REACT_LOVE;
        } else if (s->joy < 40) {
            s->joy += 12;
            set_msg("%s se blottit contre toi.", name);
            r = PET_REACT_LOVE;
        } else {
            s->joy += 4;
            set_msg("%s va déjà très bien !", name);
            r = PET_REACT_JOY;
        }
        break;

    case PET_ACT_WAKE:
        s_pet.sleeping = false;
        s_since_action = 0;   // ⚫ AJOUTÉ (25/10/2026) : réveillé la nuit, il reste éveillé un moment
        if (s->energy < 40) {
            s->joy -= 8;
            set_msg("%s est réveillé trop tôt : grognon !", name);
            r = PET_REACT_REFUSE;
        } else {
            set_msg("%s s'étire et se réveille.", name);
            r = PET_REACT_WAKE;
        }
        break;

    default:
        break;
    }

    s_since_action = 0;   // ⚫ AJOUTÉ (25/10/2026)
    // ⚫ AJOUTÉ (25/10/2026) : jouer dehors par grand froid (< 2 °C) ou forte chaleur (> 30 °C) : il peut tomber
    // malade (plus souvent quand il est bébé)
    if (game && r == PET_REACT_JOY && !s_pet.sick) {
        weather_t w;
        weather_get(&w);
        bool cold = w.valid && w.temp < 2, hot = w.valid && w.temp > 30 && weather_tod() == TOD_DAY;
        if ((cold || hot) && (esp_random() % 100) < (pet_stage() == PET_STAGE_BABY ? 55u : 30u)) {
            pet_make_sick(cold ? PET_SICK_COLD : PET_SICK_HEAT);
            snprintf(s_msg, sizeof(s_msg), cold ? "%s a joué dans le froid : il a pris froid !" : "%s a trop joué en pleine chaleur : coup de chaleur !", name);
        }
    }
    clamp_all();
    // pet_save();
    // s_save_in = 2;   // ⚫ MODIFIÉ (25/10/2026) : sauvegarde différée de 2 s (voir pet_tick), regroupe les appuis rapprochés
    // ⚫ MODIFIÉ (25/10/2026) : plus de sauvegarde à l'action (voir pet_tick)
    // ⚫ MODIFIÉ (25/10/2026) : + soif
    ESP_LOGI(TAG, "Action %d -> %s (faim %.0f, soif %.0f, énergie %.0f, joie %.0f, discipline %.0f, santé %.0f)",
             act, s_msg, s->food, s->water, s->energy, s->joy, s->discipline, s->health);
    if (msg) *msg = s_msg;
    return r;
}

const pet_t *pet_get(void)
{
    return &s_pet;
}

pet_mood_t pet_mood(void)
{
    const pet_stats_t *s = &s_pet.s;
    if (s_pet.sleeping)   return PET_MOOD_SLEEPING;
    if (s_pet.crying)     return PET_MOOD_CRYING;
    if (s_pet.sick)       return PET_MOOD_SICK;   // ⚫ AJOUTÉ (25/10/2026)
    if (s_pet.naughty)    return PET_MOOD_NAUGHTY;
    if (s->potty >= 70)   return PET_MOOD_POTTY;   // ⚫ AJOUTÉ (25/10/2026)
    if (s->health < 35)   return PET_MOOD_SICK;
    if (s->food < 20)     return PET_MOOD_HUNGRY;
    if (s->water < 20)    return PET_MOOD_THIRSTY;   // ⚫ AJOUTÉ (25/10/2026)
    if (s->energy < 20)   return PET_MOOD_TIRED;
    if (s->joy < 30)      return PET_MOOD_SAD;
    if (s->joy >= 70 && s->food >= 40 && s->energy >= 30) return PET_MOOD_HAPPY;
    return PET_MOOD_NEUTRAL;
}

pet_stage_t pet_stage(void)
{
    if (s_pet.age_s < STAGE_CHILD_S) return PET_STAGE_BABY;
    if (s_pet.age_s < STAGE_ADULT_S) return PET_STAGE_CHILD;
    return PET_STAGE_ADULT;
}

int pet_wellbeing(void)
{
    const pet_stats_t *s = &s_pet.s;
    // float w = (s->food + s->energy + s->joy + s->health + s->discipline * 0.5f) / 4.5f;
    float w = (s->food + s->water + s->energy + s->joy + s->health + s->discipline * 0.5f) / 5.5f;   // ⚫ 25/10/2026 : + soif
    if (s_pet.naughty) w -= 10;
    if (s_pet.crying)  w -= 10;
    return (int)clampf(w);
}

const char *pet_event_text(void)
{
    return s_msg;
}

const char *pet_mood_text(pet_mood_t mood)
{
    static const char *const t[PET_MOOD_COUNT] = {
        [PET_MOOD_SLEEPING] = "dort",
        [PET_MOOD_CRYING]   = "pleure",
        [PET_MOOD_NAUGHTY]  = "fait une bêtise",
        [PET_MOOD_POTTY]    = "doit faire ses besoins",
        [PET_MOOD_SICK]     = "est malade",
        [PET_MOOD_HUNGRY]   = "a faim",
        [PET_MOOD_THIRSTY]  = "a soif",
        [PET_MOOD_TIRED]    = "est fatigué",
        [PET_MOOD_SAD]      = "s'ennuie",
        [PET_MOOD_NEUTRAL]  = "va bien",
        [PET_MOOD_HAPPY]    = "est heureux",
    };
    return (mood < PET_MOOD_COUNT) ? t[mood] : "";
}

const char *pet_stage_text(pet_stage_t stage)
{
    switch (stage) {
    case PET_STAGE_BABY:  return "Bébé";
    // case PET_STAGE_CHILD: return "Enfant";
    case PET_STAGE_CHILD: return "Adolescent";   // ⚫ MODIFIÉ (25/10/2026)
    default:              return "Adulte";
    }
}

/* ---------- ⚫ AJOUTÉ (25/10/2026) : adoption, réinitialisation, utilitaires ---------- */

uint32_t pet_health_event(int *delta, const char **why)
{
    if (delta) *delta = s_hevt_delta;
    if (why) *why = s_hevt_why;
    return s_hevt_seq;
}

bool pet_adopt(const char *name, int species)
{
    if (s_pet.adopted || !name || !name[0] || species < 0 || species >= PET_SPECIES_COUNT) return false;
    pet_new();
    s_pet.adopted = true;
    s_offline_done = true;   // ⚫ AJOUTÉ (25/10/2026) : nouvel animal : rien à rattraper
    s_pet.species = (uint8_t)species;
    strlcpy(s_pet.name, name, sizeof(s_pet.name));
    s_mess_ver++;
    pet_save();
    ESP_LOGI(TAG, "Adoption : %s (%s)", s_pet.name, s_species_label[species]);
    return true;
}

void pet_reset_all(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_erase_all(h);
        nvs_commit(h);
        nvs_close(h);
    }
    pet_new();
    s_mess_ver++;
    ESP_LOGW(TAG, "Toutes les données de l'animal ont été effacées");
}

const char *pet_name(void)
{
    return s_pet.adopted ? s_pet.name : "";
}

const char *pet_species_label(int species)
{
    return (species >= 0 && species < PET_SPECIES_COUNT) ? s_species_label[species] : "";
}

const char *pet_sound(void)
{
    return s_species_sound[s_pet.species < PET_SPECIES_COUNT ? s_pet.species : 0];
}

void pet_set_pos(int x, int y)
{
    s_pos_x = (int16_t)x;
    s_pos_y = (int16_t)y;
    // arrivé sur le lieu de sa bêtise : le trou / les besoins apparaissent
    if (s_pet.naughty && (s_pet.naughty_kind == PET_NAUGHTY_DIG || s_pet.naughty_kind == PET_NAUGHTY_POOP) &&
        abs(x - s_pet.naughty_at.x) < 6 && abs(y - s_pet.naughty_at.y) < 6) {
        // if (s_pet.naughty_kind == PET_NAUGHTY_DIG) add_spot(s_pet.holes, &s_pet.n_holes, PET_MAX_HOLES, s_pet.naughty_at);
        // ⚫ MODIFIÉ (25/10/2026) : trou repris s'il existe déjà (même rebouché : il le rouvre), sinon nouveau, peu profond
        if (s_pet.naughty_kind == PET_NAUGHTY_DIG) {
            int i = hole_find(s_pet.naughty_at.x, s_pet.naughty_at.y, 8);
            if (i >= 0 && i == s_pet.bone_hole) i = -1;
            if (i < 0) i = hole_add(s_pet.naughty_at.x, s_pet.naughty_at.y, 8);
            else if (s_pet.hole_filled[i]) { s_pet.hole_filled[i] = 0; s_pet.hole_dig[i] = 20; s_mess_ver++; }
            s_dig_slot = i;
        }
        else add_spot(s_pet.poops, &s_pet.n_poops, PET_MAX_POOPS, s_pet.naughty_at);
        s_pet.naughty_at = (pet_spot_t){ -1000, -1000 };   // une seule fois
    }
}

uint32_t pet_mess_version(void)
{
    return s_mess_ver;
}

void pet_save_now(void)
{
    pet_save();
}

/* ---------- ⚫ AJOUTÉ (25/10/2026) : jeux par espèce, environnement réel ---------- */

#define B(a) (1u << PET_ACT_##a)
uint32_t pet_games_mask(int species)
{
    // ordre des espèces : PET_SPECIES (main/pet_assets_gen.h)
    static const uint32_t M[] = {
        B(PLAY) | B(FETCH) | B(FRISBEE) | B(BONE) | B(BIRDS),       // chien
        B(YARN) | B(FEATHER) | B(BIRDS),                            // chat
        B(PLAY) | B(GRAZE) | B(BRUSH),                              // mouton
        B(PLAY) | B(CLIMB) | B(GRAZE) | B(HURDLE),                  // chèvre
        B(PLAY) | B(HURDLE) | B(CARROT) | B(BRUSH),                 // cheval
        B(MUD) | B(CARROT) | B(PLAY),                               // cochon
        B(GRAZE) | B(BRUSH),                                        // vache
        B(PLAY) | B(CARROT) | B(BRUSH) | B(HURDLE),                 // âne
        B(CARROT) | B(GRAZE) | B(PLAY),                             // lapin
        B(PLAY) | B(GRAZE) | B(BRUSH) | B(HURDLE),                  // lama
    };
    return (species >= 0 && species < (int)(sizeof(M) / sizeof(M[0]))) ? M[species] : B(PLAY);
}
#undef B

void pet_set_env(bool night, bool bad_weather, bool sheltered)
{
    // au lever du jour : il se réveille s'il a assez dormi
    if (s_env_known && s_env_night && !night && s_pet.sleeping && s_pet.s.energy >= 60) {
        s_pet.sleeping = false;
        set_msg("Le jour se lève : %s se réveille.", s_pet.name);
    }
    s_env_known = true;
    s_env_night = night;
    s_env_bad = bad_weather;
    s_env_sheltered = sheltered;
}

bool pet_take_auto_potty(void)
{
    bool v = s_auto_potty;
    s_auto_potty = false;
    return v;
}

// ⚫ AJOUTÉ (25/10/2026) : bêtise décidée par l'IA (0 = creuser, 1 = renverser les croquettes, 2 = l'eau).
// Refusée s'il fait déjà une bêtise, pleure ou dort ; mêmes effets qu'une bêtise tirée au hasard.
bool pet_start_naughty(int kind)
{
    // if (!s_pet.adopted || s_pet.naughty || s_pet.crying || s_pet.sleeping) return false;
    if (!s_pet.adopted || s_pet.naughty || s_pet.crying || s_pet.sleeping || s_pet.sick) return false;   // ⚫ 25/10/2026
    // ⚫ AJOUTÉ (25/10/2026) : besoins n'importe où (seulement s'il en a besoin) ; cône renversé
    if (kind == 3) {
        if (s_pet.s.potty < 25) return false;
        s_pet.naughty = true;
        s_pet.naughty_kind = PET_NAUGHTY_POOP;
        s_pet.naughty_at = rand_spot();
        s_pet.s.potty = 0;
        s_pet.s.discipline = clampf(s_pet.s.discipline - 3);
        set_msg("%s fait ses besoins au milieu de la cour !", s_pet.name);
        health_hit(2, "bêtise : besoins dans la cour (hygiène)");
        s_naughty_acc = 0;
        s_mess_ver++;
        return true;
    }
    if (kind == 4) {
        if (s_pet.cones_down == 3) kind = 0;
        else {
            int c = (s_pet.cones_down & 1) ? 1 : (s_pet.cones_down & 2) ? 0 : (int)(esp_random() & 1);
            s_pet.naughty = true;
            s_pet.naughty_kind = PET_NAUGHTY_CONE;
            s_pet.naughty_at = c ? (pet_spot_t){ SCENE_CONE1_X, SCENE_CONE1_Y } : (pet_spot_t){ SCENE_CONE0_X, SCENE_CONE0_Y };
            s_pet.cones_down |= (uint8_t)(1u << c);
            set_msg("%s renverse un cône de la piste !", s_pet.name);
            s_naughty_acc = 0;
            s_mess_ver++;
            return true;
        }
    }
    if (kind == 1 && s_pet.food_tipped) kind = 0;
    if (kind == 2 && s_pet.water_tipped) kind = 0;
    s_pet.naughty = true;
    if (kind == 1) {
        s_pet.naughty_kind = PET_NAUGHTY_TIP_FOOD;
        s_pet.food_tipped = true;
        s_pet.naughty_at = (pet_spot_t){ SCENE_FOOD_X, SCENE_FOOD_Y };
        set_msg("%s renverse sa gamelle de croquettes !", s_pet.name);
        health_hit(1, "bêtise : gamelle de croquettes renversée");
    } else if (kind == 2) {
        s_pet.naughty_kind = PET_NAUGHTY_TIP_WATER;
        s_pet.water_tipped = true;
        s_pet.naughty_at = (pet_spot_t){ SCENE_WATER_X, SCENE_WATER_Y };
        set_msg("%s renverse sa gamelle d'eau !", s_pet.name);
        health_hit(1, "bêtise : gamelle d'eau renversée");
    } else {
        s_pet.naughty_kind = PET_NAUGHTY_DIG;
        int cont = -1;
        for (int i = 0; i < s_pet.n_holes; i++)
            if (!s_pet.hole_filled[i] && s_pet.hole_dig[i] < 100 && i != s_pet.bone_hole) cont = i;
        s_pet.naughty_at = cont >= 0 ? s_pet.holes[cont] : rand_spot();
        set_msg(cont >= 0 ? "%s retourne finir son trou !" : "%s creuse un trou dans la pelouse !", s_pet.name);
        health_hit(2, "bêtise : il a mangé de la terre");
    }
    s_naughty_acc = 0;
    s_mess_ver++;
    return true;
}

// ⚫ AJOUTÉ (25/10/2026) : le maître a répondu vite à sa demande (câlin, repas, jeu…) : il est ravi
void pet_attention_bonus(void)
{
    s_pet.s.joy = clampf(s_pet.s.joy + 6);
    s_pet.s.discipline = clampf(s_pet.s.discipline + 2);
}

// ⚫ AJOUTÉ (25/10/2026) : maladie (froid, chaleur, indigestion) : 20 à 40 min de jeu, moins s'il se repose
void pet_make_sick(int cause)
{
    if (s_pet.sick) return;
    s_pet.sick = (uint8_t)cause;
    s_pet.sick_s = 1200 + esp_random() % 1200;
    s_pet.naughty = false;
    s_pet.s.energy = clampf(s_pet.s.energy - 15);
    health_hit(cause == PET_SICK_STOMACH ? 4 : 6, pet_sick_text(cause));
    set_msg(cause == PET_SICK_COLD ? "%s a pris froid : il tousse et grelotte."
            : cause == PET_SICK_HEAT ? "%s a un coup de chaleur : il faut le mettre à l'ombre."
            : "%s a mal au ventre : il a trop mangé.", s_pet.name);
}

const char *pet_sick_text(int cause)
{
    return cause == PET_SICK_COLD ? "malade : il a pris froid" : cause == PET_SICK_HEAT ? "malade : coup de chaleur"
         : cause == PET_SICK_STOMACH ? "malade : indigestion (trop mangé)" : "";
}
