// ⚫ AJOUTÉ (25/10/2026) : images du chien et de la cour, générées par tools/assets (npm run gen)
// et intégrées au firmware (main/assets/*.bin, EMBED_FILES). Chaque image a un point d'ancrage :
// pour le chien, le sol sous son corps ; pour les accessoires, leur centre.
#pragma once

#include <stdint.h>
#include "lvgl.h"
#include "pet_assets_gen.h"

typedef enum {
#define X(name, w, h, ox, oy, alpha) A_##name,
    PET_ASSETS(X)
#undef X
    A_COUNT,
} pet_asset_id_t;

typedef struct {
    lv_img_dsc_t img;
    int16_t ox, oy;   // coin haut-gauche de l'image par rapport à l'ancrage
} pet_asset_t;

extern pet_asset_t g_pet_assets[A_COUNT];

// Remplit les descripteurs LVGL (à appeler une fois avant d'utiliser les images)
void pet_assets_init(void);
