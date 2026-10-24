# ⚫ MODIFIÉ (24/10/2026) : ajout de toutes les démos LVGL (le composant lvgl 8.3.3 ne compile que src/).
# Chaque fichier de démo est protégé par son #if LV_USE_DEMO_xxx : une démo désactivée compile vide.
# ⚫ RETIRÉ (24/10/2026) : firmware "Animal de compagnie" : plus aucune démo LVGL compilée
# (ancien idf_component_get_property(LVGL_DIR ...) + file(GLOB LVGL_DEMOS ${LVGL_DIR}/demos/benchmark|widgets|
# keypad_encoder|stress|music ...) supprimés).

# ⚫ AJOUTÉ (25/10/2026) : images du chien et de la cour (générées par tools/assets, cf. main/pet_assets.h)
file(GLOB PET_ASSET_FILES "${CMAKE_CURRENT_SOURCE_DIR}/assets/*.bin")

idf_component_register(
    # SRC_DIRS ${DIRS} ${LVGL_DEMOS} "."
    # INCLUDE_DIRS "." ${LVGL_DEMOS} ${DIRS}
    # ⚫ MODIFIÉ (24/10/2026) : sources de main/ uniquement
    SRC_DIRS "."
    INCLUDE_DIRS "."
    # REQUIRES esp_lcd driver lvgl esp_timer button mt8901
    # ⚫ MODIFIÉ (24/10/2026) : + ws2812 (LED de l'encodeur)
    # ⚫ MODIFIÉ (24/10/2026) : + nvs_flash (sauvegarde de l'animal)
    REQUIRES esp_lcd driver lvgl esp_timer button mt8901 ws2812 nvs_flash
    # ⚫ AJOUTÉ (25/10/2026)
    EMBED_FILES ${PET_ASSET_FILES}
)

target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")
