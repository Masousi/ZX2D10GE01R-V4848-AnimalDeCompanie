// ⚫ AJOUTÉ (25/10/2026) : descripteurs LVGL des images intégrées (voir pet_assets.h).
// Les .bin contiennent les pixels bruts (pas d'en-tête) : RGB565 (fond) ou RGB565 + alpha (sprites).
#include "pet_assets.h"

// symboles créés par EMBED_FILES : _binary_<fichier>_bin_start / _end
#define X(name, w, h, ox, oy, alpha) \
    extern const uint8_t _binary_##name##_bin_start[] asm("_binary_" #name "_bin_start");
PET_ASSETS(X)
#undef X

pet_asset_t g_pet_assets[A_COUNT];
int16_t g_dog_mouth_dx[A_COUNT], g_dog_mouth_dy[A_COUNT];   // ⚫ AJOUTÉ (25/10/2026)

void pet_assets_init(void)
{
#define X(name, w_, h_, ox_, oy_, alpha) \
    g_pet_assets[A_##name] = (pet_asset_t){ \
        .img = { \
            .header = { .cf = (alpha) ? LV_IMG_CF_TRUE_COLOR_ALPHA : LV_IMG_CF_TRUE_COLOR, .w = (w_), .h = (h_) }, \
            .data_size = (uint32_t)(w_) * (h_) * ((alpha) ? 3 : 2), \
            .data = _binary_##name##_bin_start, \
        }, \
        .ox = (ox_), .oy = (oy_), \
    };
    PET_ASSETS(X)
#undef X
    // ⚫ AJOUTÉ (25/10/2026)
#define X(name, dx, dy) g_dog_mouth_dx[A_##name] = (dx); g_dog_mouth_dy[A_##name] = (dy);
    PET_DOG_MOUTHS(X)
#undef X
}
