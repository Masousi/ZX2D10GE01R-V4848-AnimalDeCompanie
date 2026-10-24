// ⚫ AJOUTÉ (25/10/2026) : FICHIER GÉNÉRÉ par tools/assets/gen.mjs : ne pas modifier à la main.
// Images intégrées depuis main/assets/*.bin (format LVGL 8.3, couleurs 16 bits, tramées).
#pragma once

// X(nom, largeur, hauteur, décalage x, décalage y, alpha)
#define PET_ASSETS(X) \
    X(bg_day, 480, 480, 0, 0, 0) \
    X(kennel_d, 168, 128, 30, 170, 1) \
    X(bg_night, 480, 480, 0, 0, 0) \
    X(kennel_n, 168, 128, 30, 170, 1) \
    X(dog_stand_1_r, 165, 115, -97, -107, 1) \
    X(dog_stand_1_l, 165, 115, -68, -107, 1) \
    X(dog_stand_2_r, 159, 115, -91, -107, 1) \
    X(dog_stand_2_l, 159, 115, -68, -107, 1) \
    X(dog_stand_3_r, 167, 115, -99, -107, 1) \
    X(dog_stand_3_l, 167, 115, -68, -107, 1) \
    X(dog_calm_1_r, 167, 115, -99, -107, 1) \
    X(dog_calm_1_l, 167, 115, -68, -107, 1) \
    X(dog_calm_2_r, 167, 115, -99, -107, 1) \
    X(dog_calm_2_l, 167, 115, -68, -107, 1) \
    X(dog_run_1_r, 174, 115, -100, -107, 1) \
    X(dog_run_1_l, 174, 115, -74, -107, 1) \
    X(dog_run_2_r, 174, 115, -100, -107, 1) \
    X(dog_run_2_l, 174, 115, -74, -107, 1) \
    X(dog_run_3_r, 173, 108, -100, -100, 1) \
    X(dog_run_3_l, 173, 108, -73, -100, 1) \
    X(dog_run_4_r, 174, 109, -100, -101, 1) \
    X(dog_run_4_l, 174, 109, -74, -101, 1) \
    X(dog_sit_r, 133, 118, -77, -110, 1) \
    X(dog_sit_l, 133, 118, -56, -110, 1) \
    X(dog_sit_happy_r, 137, 119, -81, -111, 1) \
    X(dog_sit_happy_l, 137, 119, -56, -111, 1) \
    X(dog_sit_love_r, 136, 119, -80, -111, 1) \
    X(dog_sit_love_l, 136, 119, -56, -111, 1) \
    X(dog_sit_sad_r, 134, 107, -71, -99, 1) \
    X(dog_sit_sad_l, 134, 107, -63, -99, 1) \
    X(dog_sit_cry_r, 127, 116, -71, -108, 1) \
    X(dog_sit_cry_l, 127, 116, -56, -108, 1) \
    X(dog_lie_r, 155, 86, -96, -78, 1) \
    X(dog_lie_l, 155, 86, -59, -78, 1) \
    X(dog_lie_sick_r, 167, 68, -95, -60, 1) \
    X(dog_lie_sick_l, 167, 68, -72, -60, 1) \
    X(dog_sleep_1_r, 165, 65, -95, -57, 1) \
    X(dog_sleep_1_l, 165, 65, -70, -57, 1) \
    X(dog_sleep_2_r, 165, 68, -95, -60, 1) \
    X(dog_sleep_2_l, 165, 68, -70, -60, 1) \
    X(dog_eat_1_r, 154, 92, -95, -84, 1) \
    X(dog_eat_1_l, 154, 92, -59, -84, 1) \
    X(dog_eat_2_r, 156, 83, -98, -75, 1) \
    X(dog_eat_2_l, 156, 83, -58, -75, 1) \
    X(dog_drink_1_r, 157, 85, -98, -77, 1) \
    X(dog_drink_1_l, 157, 85, -59, -77, 1) \
    X(dog_drink_2_r, 156, 85, -98, -77, 1) \
    X(dog_drink_2_l, 156, 85, -58, -77, 1) \
    X(dog_dig_1_r, 162, 113, -92, -105, 1) \
    X(dog_dig_1_l, 162, 113, -70, -105, 1) \
    X(dog_dig_2_r, 170, 105, -100, -97, 1) \
    X(dog_dig_2_l, 170, 105, -70, -97, 1) \
    X(dog_jump_r, 149, 146, -100, -138, 1) \
    X(dog_jump_l, 149, 146, -49, -138, 1) \
    X(dog_guilty_r, 155, 87, -81, -79, 1) \
    X(dog_guilty_l, 155, 87, -74, -79, 1) \
    X(kibble, 40, 16, -19, -7, 1) \
    X(ball, 16, 16, -8, -8, 1) \
    X(cloud_1, 119, 42, -61, -20, 1) \
    X(cloud_2, 86, 32, -45, -16, 1) \
    X(fly_1, 20, 16, -10, -7, 1) \
    X(fly_2, 8, 16, -4, -7, 1) \
    X(firefly, 16, 16, -8, -8, 1) \
    X(hole, 52, 20, -28, -10, 1) \
    X(clod, 8, 7, -4, -4, 1) \
    X(frisbee, 32, 13, -16, -6, 1) \
    X(bone, 34, 14, -17, -7, 1) \

// X(image, dx, dy) : gueule par rapport à l'ancrage du chien
#define PET_DOG_MOUTHS(X) \
    X(dog_stand_1_r, 59, -75) \
    X(dog_stand_1_l, -59, -75) \
    X(dog_stand_2_r, 59, -75) \
    X(dog_stand_2_l, -59, -75) \
    X(dog_stand_3_r, 59, -75) \
    X(dog_stand_3_l, -59, -75) \
    X(dog_calm_1_r, 59, -75) \
    X(dog_calm_1_l, -59, -75) \
    X(dog_calm_2_r, 59, -75) \
    X(dog_calm_2_l, -59, -75) \
    X(dog_run_1_r, 63, -73) \
    X(dog_run_1_l, -63, -73) \
    X(dog_run_2_r, 64, -75) \
    X(dog_run_2_l, -64, -75) \
    X(dog_run_3_r, 63, -65) \
    X(dog_run_3_l, -63, -65) \
    X(dog_run_4_r, 64, -67) \
    X(dog_run_4_l, -64, -67) \
    X(dog_sit_r, 44, -82) \
    X(dog_sit_l, -44, -82) \
    X(dog_sit_happy_r, 44, -86) \
    X(dog_sit_happy_l, -44, -86) \
    X(dog_sit_love_r, 43, -90) \
    X(dog_sit_love_l, -43, -90) \
    X(dog_sit_sad_r, 52, -59) \
    X(dog_sit_sad_l, -52, -59) \
    X(dog_sit_cry_r, 47, -89) \
    X(dog_sit_cry_l, -47, -89) \
    X(dog_lie_r, 51, -48) \
    X(dog_lie_l, -51, -48) \
    X(dog_lie_sick_r, 62, -24) \
    X(dog_lie_sick_l, -62, -24) \
    X(dog_sleep_1_r, 60, -19) \
    X(dog_sleep_1_l, -60, -19) \
    X(dog_sleep_2_r, 60, -23) \
    X(dog_sleep_2_l, -60, -23) \
    X(dog_eat_1_r, 43, 2) \
    X(dog_eat_1_l, -43, 2) \
    X(dog_eat_2_r, 38, 6) \
    X(dog_eat_2_l, -38, 6) \
    X(dog_drink_1_r, 44, 2) \
    X(dog_drink_1_l, -44, 2) \
    X(dog_drink_2_r, 40, 5) \
    X(dog_drink_2_l, -40, 5) \
    X(dog_dig_1_r, 58, -34) \
    X(dog_dig_1_l, -58, -34) \
    X(dog_dig_2_r, 58, -34) \
    X(dog_dig_2_l, -58, -34) \
    X(dog_jump_r, 40, -113) \
    X(dog_jump_l, -40, -113) \
    X(dog_guilty_r, 63, -38) \
    X(dog_guilty_l, -63, -38) \

// Positions du décor (pixels écran) et bouche du chien qui mange (par rapport à son ancrage, tourné à droite)
#define SCENE_KENNEL_X   100
#define SCENE_KENNEL_Y   296
#define SCENE_FOOD_X     186
#define SCENE_FOOD_Y     378
#define SCENE_WATER_X    352
#define SCENE_WATER_Y    372
#define SCENE_TRACK_X    318
#define SCENE_TRACK_Y    252
#define SCENE_TRACK_RX   112
#define SCENE_TRACK_RY   26
#define DOG_MOUTH_DX     43
#define DOG_MOUTH_DY     2
