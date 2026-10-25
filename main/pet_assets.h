// ⚫ AJOUTÉ (25/10/2026) : images du chien et de la cour, générées par tools/assets (npm run gen)
// et intégrées au firmware (main/assets/*.bin, EMBED_FILES). Chaque image a un point d'ancrage :
// pour le chien, le sol sous son corps ; pour les accessoires, leur centre.
// ⚫ MODIFIÉ (25/10/2026) : plusieurs animaux : les images sont lues dans le pack de la partition "assets"
// (assets/pet_assets.bin, cf. tools/assets/gen.mjs pour le format), directement en flash (sans copie).
// Les images de l'animal choisi sont décodées (256 couleurs -> RGB565 + alpha) en PSRAM, dans les deux sens.
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"
#include "pet_assets_gen.h"

// Décor et accessoires : A_<nom> = index dans le pack
typedef enum {
#define X(name, idx) A_##name = (idx),
    PET_ASSETS(X)
#undef X
} pet_asset_id_t;

// Poses : P_<nom>
typedef enum {
#define X(name) P_##name,
    PET_POSES(X)
#undef X
} pet_pose_t;

typedef struct {
    lv_img_dsc_t img;
    int16_t ox, oy;   // coin haut-gauche de l'image par rapport à l'ancrage
} pet_asset_t;

// Ouvre le pack (à appeler une fois). false si la partition "assets" est absente ou invalide.
bool pet_assets_init(void);

// Décode les images de l'espèce (index dans PET_SPECIES) : pet_animal() les renvoie ensuite.
bool pet_assets_load_species(int species);
// ⚫ AJOUTÉ (25/10/2026) : espèce + âge (0 bébé, 1 adolescent, 2 adulte) : chaque âge a ses propres images
bool pet_assets_load(int species, int age);

// Image de décor / accessoire ; left = version tournée vers la gauche (oiseaux), créée à la demande
const pet_asset_t *pet_asset(int id);
const pet_asset_t *pet_asset_left(int id);

// Image de l'animal chargé, pour une pose et un sens
const pet_asset_t *pet_animal(int pose, bool left);

// Pour la page web : index du pack d'une image affichée (-1 si inconnue) et sens
int pet_asset_index_of(const void *src, bool *left);

// Accès brut à une entrée du pack (page web)
const uint8_t *pet_pack_entry(int idx, uint32_t *size, uint16_t *w, uint16_t *h, int16_t *ox, int16_t *oy, uint8_t *cf);
int pet_pack_count(void);
