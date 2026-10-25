// ⚫ AJOUTÉ (24/10/2026) : écran de l'animal de compagnie (voir pet_ui.h).
// ⚫ MODIFIÉ (25/10/2026) : réécrit : le personnage dessiné en objets LVGL (cercles, arcs) est remplacé par
// un chien en images (tools/assets, 26 poses x 2 sens) qui vit dans une cour : niche pour dormir, piste pour
// courir, gamelle de croquettes, gamelle d'eau. Jour / nuit (le chien dort la nuit), nuages, papillon, lucioles.
// Mise en page pour l'écran rond 480x480 :
// - bord : 6 jauges en arc (Faim, Soif, Énergie à gauche ; Bonheur, Discipline, Santé à droite) + icônes ;
// - haut : nom et âge ; bulle de dialogue pour ce que dit le chien ;
// - bas : sélecteur d'actions (roller 1 ligne piloté par l'encodeur, comme l'ancien menu des démos).
// Le chien suit des "scripts" (aller à, pose, tours de piste, saut…) : un script par action, et un
// comportement de repos selon l'humeur (a faim -> attend près de sa gamelle, bêtise -> creuse un trou…).
// ⚫ MODIFIÉ (25/10/2026) : version "plusieurs animaux" :
// - l'animal (chien, chat, mouton, chèvre, cheval, cochon) est choisi à l'adoption, sur la page web ; avant
//   l'adoption l'écran affiche un QR code pour rejoindre le réseau de la carte ;
// - niche RETIRÉE (litière de paille pour dormir, coin besoins), gamelles en objets (renversables) ;
// - menu principal + menu Jeux (Courir, Balle, Frisbee, Os à ronger, Oiseaux) ; entrée WiFi = QR code ;
// - 7e jauge "Besoins", demande de sortie, accidents ; bêtises : trous, gamelles renversées, besoins n'importe où,
//   qui restent jusqu'au nettoyage ; main qui caresse ; un seul cœur quand il est heureux ; oiseaux qui viennent
//   picorer et que l'animal poursuit ;
// - copie de la scène pour la page web (pet_ui_scene_json), qui redessine la même image.
// ⚫ MODIFIÉ (25/10/2026) : heure et météo réelles (main/weather.c) : 8 fonds (jour, couvert, pluie, neige, aube /
// crépuscule, nuit, nuit couverte, nuit enneigée) en fondu, nuages, pluie, neige, éclairs, brouillard ; abri où
// l'animal se réfugie (premier plan dessiné devant lui) ; jeux propres à chaque espèce ; main qui gronde (index
// levé) ; punition = temps mort au coin (main qui montre le coin + sablier) ; besoins pile sur le coin gravier.
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"   // ⚫ AJOUTÉ (25/10/2026) : sections critiques (portMUX)
#include "esp_log.h"
#include "esp_random.h"
#include "sdkconfig.h"
#include "lvgl.h"
#include "pet.h"
#include "pet_led.h"
#include "pet_ui.h"
#include "pet_assets.h"
#include "wifi_cfg.h"   // ⚫ AJOUTÉ (25/10/2026)
#include "weather.h"    // ⚫ AJOUTÉ (25/10/2026)
#include "ai.h"         // ⚫ AJOUTÉ (25/10/2026)
#include "nvs.h"        // ⚫ AJOUTÉ (25/10/2026) : réglage du son
#ifndef CONFIG_PET_WIFI_AP_SSID   // WiFi désactivé dans menuconfig
#define CONFIG_PET_WIFI_AP_SSID ""
#endif

#define TAG "PET_UI"

// Polices Montserrat avec accents + icônes FontAwesome (générées par lv_font_conv, voir l'en-tête des fichiers)
LV_FONT_DECLARE(font_fr_20);
LV_FONT_DECLARE(font_fr_28);
LV_FONT_DECLARE(font_pet_56);

// Icônes FontAwesome (UTF-8) présentes dans font_fr_20 / font_fr_28 (et en partie dans font_pet_56)
#define ICON_FEED     "\xEF\x8B\xA7"   // U+F2E7 couverts
#define ICON_DRINK    "\xEF\x81\x83"   // U+F043 goutte
#define ICON_SLEEP    "\xEF\x86\x86"   // U+F186 lune
#define ICON_PLAY     "\xEF\x87\xA3"   // U+F1E3 ballon
#define ICON_PET      "\xEF\x89\x96"   // U+F256 main
#define ICON_SCOLD    "\xEF\x82\xA1"   // U+F0A1 porte-voix
#define ICON_PUNISH   "\xEF\x83\xA3"   // U+F0E3 marteau
#define ICON_COMFORT  "\xEF\x92\xBE"   // U+F4BE main + cœur
#define ICON_HEART    "\xEF\x80\x84"   // U+F004
#define ICON_STAR     "\xEF\x80\x85"   // U+F005
#define ICON_ENERGY   "\xEF\x83\xA7"   // U+F0E7 éclair
#define ICON_DISC     "\xEF\x86\x9D"   // U+F19D toque de diplômé
#define ICON_HEALTH   "\xEF\x88\x9E"   // U+F21E battement de cœur
// ⚫ AJOUTÉ (25/10/2026) : nouveaux jeux
#define ICON_RUN      "\xEF\x9C\x8C"   // U+F70C coureur
#define ICON_BALL     "\xEF\x90\xB3"   // U+F433 balle
#define ICON_FRISBEE  "\xEF\x94\x9F"   // U+F51F disque
#define ICON_BONE     "\xEF\x97\x97"   // U+F5D7 os
#define ICON_WIFI     "\xEF\x87\xAB"   // U+F1EB WiFi (⚫ AJOUTÉ 25/10/2026)
// ⚫ AJOUTÉ (25/10/2026) : menus
#define ICON_CLEAN    "\xEF\x94\x9A"   // U+F51A balai
#define ICON_GAMES    "\xEF\x84\x9B"   // U+F11B manette
#define ICON_BIRD     "\xEF\x92\xBA"   // U+F4BA colombe
#define ICON_BACK     "\xEF\x81\xA0"   // U+F060 flèche retour
#define ICON_POTTY    "\xEF\x9F\x98"   // U+F7D8 toilettes
#define ICON_QR       "\xEF\x80\xA9"   // U+F029 QR code
// ⚫ AJOUTÉ (25/10/2026) : météo et jeux par espèce
#define ICON_SUN      "\xEF\x86\x85"   // U+F185
#define ICON_CLOUD    "\xEF\x83\x82"   // U+F0C2
#define ICON_CLOUDSUN "\xEF\x9B\x84"   // U+F6C4
#define ICON_RAIN     "\xEF\x9C\xBD"   // U+F73D
#define ICON_SHOWER   "\xEF\x9D\x80"   // U+F740
#define ICON_SNOW     "\xEF\x8B\x9C"   // U+F2DC
#define ICON_FOG      "\xEF\x9D\x9F"   // U+F75F
#define ICON_CLOUDMOON "\xEF\x9B\x83"  // U+F6C3
#define ICON_CARROT   "\xEF\x9E\x87"   // U+F787
#define ICON_MOUNTAIN "\xEF\x9B\xBC"   // U+F6FC
#define ICON_BRUSH    "\xEF\x95\x9D"   // U+F55D
#define ICON_SEED     "\xEF\x93\x98"   // U+F4D8

#define FRAME_MS    33       // animation du chien : ~30 images/s
#define MSG_MS      4500     // durée d'affichage d'une bulle
#define LOW_STAT    20       // jauge basse : son icône clignote
// #define WALK_SPEED  95.0f    // px/s
// #define RUN_SPEED   230.0f
// ⚫ MODIFIÉ (25/10/2026) : vitesse selon l'âge (petites pattes du bébé, adolescent fougueux)
#define WALK_SPEED  (95.0f * age_speed(false))    // px/s
#define RUN_SPEED   (230.0f * age_speed(true))
#define SNAP_MS     120      // ⚫ AJOUTÉ (25/10/2026) : copie de la scène pour la page web
#define N_BIRDS     3

// ⚫ RETIRÉ (25/10/2026) : chaîne s_act_opts (ordre = pet_action_t) : remplacée par les menus ci-dessous.

/* ---------- ⚫ AJOUTÉ (25/10/2026) : menus ---------- */

#define LEN(a) (sizeof(a) / sizeof((a)[0]))
#define LEN_GAMES_MAX 16
// typedef enum { M_ACT, M_GAMES, M_BACK, M_WIFI } menu_kind_t;
typedef enum { M_ACT, M_GAMES, M_BACK, M_WIFI, M_CONN, M_SOUND, M_VOL, M_AIINFO } menu_kind_t;   // ⚫ 25/10/2026 : + Connectivité
typedef struct {
    const char *txt;
    menu_kind_t kind;
    pet_action_t act;
} menu_item_t;

static const menu_item_t MENU_MAIN[] = {
    { ICON_FEED    "  Manger",       M_ACT,   PET_ACT_FEED },
    { ICON_DRINK   "  Boire",        M_ACT,   PET_ACT_DRINK },
    { ICON_SLEEP   "  Dormir",       M_ACT,   PET_ACT_SLEEP },
    { ICON_GAMES   "  Jeux  >",      M_GAMES, 0 },
    { ICON_POTTY   "  Besoins",      M_ACT,   PET_ACT_POTTY },
    { ICON_CLEAN   "  Nettoyer",     M_ACT,   PET_ACT_CLEAN },
    { ICON_PET     "  Caresser",     M_ACT,   PET_ACT_PET },
    { ICON_SCOLD   "  Gronder",      M_ACT,   PET_ACT_SCOLD },
    { ICON_PUNISH  "  Punir",        M_ACT,   PET_ACT_PUNISH },
    { ICON_COMFORT "  Réconforter",  M_ACT,   PET_ACT_COMFORT },
    { ICON_HEALTH  "  Soigner",      M_ACT,   PET_ACT_HEAL },      // ⚫ AJOUTÉ (25/10/2026)
    // { ICON_QR      "  WiFi",         M_WIFI,  0 },
    { ICON_WIFI    "  Connectivité  >", M_CONN, 0 },   // ⚫ MODIFIÉ (25/10/2026) : WiFi, son, IA
};
// ⚫ MODIFIÉ (25/10/2026) : tous les jeux ; le menu n'affiche que ceux de l'espèce (pet_games_mask)
static const menu_item_t GAMES_ALL[] = {
    { ICON_RUN      "  Courir",       M_ACT,  PET_ACT_PLAY },
    { ICON_BALL     "  Balle",        M_ACT,  PET_ACT_FETCH },
    { ICON_FRISBEE  "  Frisbee",      M_ACT,  PET_ACT_FRISBEE },
    { ICON_BONE     "  Os à ronger",  M_ACT,  PET_ACT_BONE },
    { ICON_BIRD     "  Oiseaux",      M_ACT,  PET_ACT_BIRDS },
    { ICON_PLAY     "  Pelote",       M_ACT,  PET_ACT_YARN },
    { ICON_BIRD     "  Plume",        M_ACT,  PET_ACT_FEATHER },
    { ICON_RUN      "  Obstacle",     M_ACT,  PET_ACT_HURDLE },
    { ICON_CARROT   "  Carotte",      M_ACT,  PET_ACT_CARROT },
    { ICON_DRINK    "  Bain de boue", M_ACT,  PET_ACT_MUD },
    { ICON_MOUNTAIN "  Grimper",      M_ACT,  PET_ACT_CLIMB },
    { ICON_BRUSH    "  Brossage",     M_ACT,  PET_ACT_BRUSH },
    { ICON_SEED     "  Brouter",      M_ACT,  PET_ACT_GRAZE },
};
static menu_item_t MENU_GAMES[LEN_GAMES_MAX];
// ⚫ AJOUTÉ (25/10/2026) : menu Connectivité (libellés recalculés : son, volume, état de l'IA)
static menu_item_t MENU_CONN[5];
static char s_conn_txt[3][48];
static bool s_sound_on = true;     // sons joués par la page web (téléphone / PC, enceinte Bluetooth appairée)
static int s_sound_vol = 2;        // 1..3
static int s_games_n;
static const menu_item_t *s_menu = MENU_MAIN;
static int s_menu_n = LEN(MENU_MAIN);
static uint32_t s_menu_t0;

/* ---------- lieux de la cour (écran) ---------- */

#define HOME_X      240
#define HOME_Y      338
// ⚫ RETIRÉ (25/10/2026) : KENNEL_DOG_X/Y, emprise de la niche (KENNEL_X0/X1/Y0/FRONT_Y/PASS_X/PASS_Y), FOOD_DOG_X/Y
// et WATER_DOG_X/Y fixes : plus de niche ; place devant les gamelles calculée selon l'espèce (gueule).
#define BED_X       (SCENE_BED_X + 8)       // couché sur la litière
#define BED_Y       (SCENE_BED_Y + 10)
// ⚫ AJOUTÉ (25/10/2026)
#define SHELTER_X   (SCENE_BED_X + 30)      // à l'abri (debout ou assis), sous le toit
#define SHELTER_Y   (SCENE_BED_Y + 2)
#define CORNER_X    430                     // temps mort : au coin de la palissade
#define CORNER_Y    266
#define PLAY_X      300                     // jeux sur place (boue, foin, herbe)
#define PLAY_Y      344

/* ---------- animations (poses de l'espèce chargée ; le sens est géré à part) ---------- */

typedef struct {
    const uint8_t *f;
    uint8_t n;
    uint16_t ms;   // durée d'une image
} anim_t;

#define ANIM(name, ms, ...) \
    static const uint8_t name##_f[] = { __VA_ARGS__ }; \
    static const anim_t name = { name##_f, sizeof(name##_f) / sizeof(name##_f[0]), ms }

ANIM(AN_WAG,   110, P_stand_1, P_stand_2, P_stand_1, P_stand_3);
// ANIM(AN_CALM,  450, P_calm_1, P_calm_2);
// ANIM(AN_WALK,  140, P_run_1, P_run_2, P_run_3, P_run_4);
// ANIM(AN_RUN,    75, P_run_1, P_run_2, P_run_3, P_run_4);
// ⚫ MODIFIÉ (25/10/2026) : respiration au repos, vrai pas à 4 temps (8 images), galop (6 images)
ANIM(AN_CALM,  420, P_idle_1, P_idle_2, P_idle_3, P_idle_4, P_idle_3, P_idle_2, P_idle_1, P_calm_2);
ANIM(AN_WALK,   95, P_walk_1, P_walk_2, P_walk_3, P_walk_4, P_walk_5, P_walk_6, P_walk_7, P_walk_8);
ANIM(AN_RUN,    60, P_run_1, P_run_2, P_run_3, P_run_4, P_run_5, P_run_6);
ANIM(AN_PAW,   250, P_paw, P_sit_happy);   // ⚫ AJOUTÉ (25/10/2026) : tape avec la patte
ANIM(AN_MUD,   420, P_lie, P_sleep_1, P_lie_sick, P_sleep_2);   // ⚫ AJOUTÉ (25/10/2026) : se roule dans la boue
ANIM(AN_SIT,  1000, P_sit);
ANIM(AN_SIT_HAPPY, 1000, P_sit_happy);
ANIM(AN_LOVE, 1000, P_sit_love);
ANIM(AN_SAD,  1000, P_sit_sad);
ANIM(AN_CRY,   600, P_sit_cry, P_sit_sad);
ANIM(AN_LIE,  1000, P_lie);
ANIM(AN_SICK, 1000, P_lie_sick);
ANIM(AN_SLEEP, 1300, P_sleep_1, P_sleep_2);
ANIM(AN_EAT,   220, P_eat_1, P_eat_2);
ANIM(AN_DRINK, 170, P_drink_1, P_drink_2);
ANIM(AN_DIG,   130, P_dig_1, P_dig_2);
ANIM(AN_JUMP, 1000, P_jump);
ANIM(AN_GUILTY, 1000, P_guilty);
ANIM(AN_POTTY, 1000, P_potty);   // ⚫ AJOUTÉ (25/10/2026)

/* ---------- scripts ---------- */

// ⚫ 25/10/2026 : + S_CHASE (poursuit les oiseaux)
// typedef enum { S_GOTO, S_POSE, S_LAPS, S_JUMP, S_FACE, S_FUNC, S_CHASE } step_kind_t;
// ⚫ MODIFIÉ (25/10/2026) : + S_LEAP (saut en avançant : obstacle, botte de foin)
typedef enum { S_GOTO, S_POSE, S_LAPS, S_JUMP, S_FACE, S_FUNC, S_CHASE, S_LEAP } step_kind_t;

typedef struct {
    step_kind_t k;
    int16_t x, y;          // S_GOTO : destination
    bool run;              // S_GOTO : au galop
    const anim_t *anim;    // S_POSE
    uint32_t ms;           // S_POSE : durée (0 = jusqu'à nouvel ordre)
    int8_t face;           // S_FACE : 1 = droite, -1 = gauche
    void (*fn)(void);      // S_FUNC
    bool grab;             // ⚫ AJOUTÉ (25/10/2026) : S_JUMP : attrape au vol l'objet lancé (frisbee)
} step_t;

#define QMAX 16
static step_t s_q[QMAX];
static int s_qn, s_qi;           // nombre d'étapes, étape courante
static uint32_t s_step_t0;       // début de l'étape courante
static bool s_in_action;         // script d'action en cours (sinon : comportement de repos)

static struct {
    float x, y;
    int8_t face;
    const anim_t *anim;
    uint32_t anim_t0;
    float jump;                  // hauteur du saut (px)
    float lap_a, lap_done;       // tours de piste : angle courant, angle parcouru
} s_dog = { HOME_X, HOME_Y, 1, &AN_WAG, 0, 0, 0, 0 };
static int s_pose = P_stand_1;   // ⚫ 25/10/2026 : pose affichée (position de la gueule)
static int s_species;            // ⚫ AJOUTÉ (25/10/2026) : espèce chargée
static int s_stage = PET_STAGE_ADULT;   // ⚫ AJOUTÉ (25/10/2026) : âge chargé (images, vitesses)
static float age_speed(bool run)
{
    return s_stage == PET_STAGE_BABY ? (run ? 0.62f : 0.7f) : s_stage == PET_STAGE_CHILD ? (run ? 1.1f : 1.05f) : 1.0f;
}
static lv_obj_t *s_hp, *s_hp_lbl, *s_hp_why, *s_hp_bar;   // ⚫ AJOUTÉ (25/10/2026) : fenêtre "santé"

// ⚫ AJOUTÉ (25/10/2026) : objet lancé (balle, frisbee) et objet tenu dans la gueule
typedef struct {
    lv_obj_t *o;
    int id;
    float x0, y0, x1, y1, h;   // départ, arrivée, hauteur de la trajectoire
    uint32_t t0, ms;
    bool on, hover;            // hover : reste en l'air à l'arrivée (frisbee qui plane)
} proj_t;
static proj_t s_proj;
static lv_obj_t *s_carry;      // objet tenu (NULL = rien)
static int s_carry_id;
static bool s_chew;            // ronge (l'os bouge un peu)
static int16_t s_tx, s_ty;     // cible du lancer en cours
static int s_carry_lx = -9999, s_carry_ly;   // dernière position de l'objet tenu (évite les redessins)

/* ---------- objets ---------- */

typedef struct {
    lv_obj_t *arc;
    lv_obj_t *icon;
} gauge_t;

// enum { G_FOOD, G_WATER, G_ENERGY, G_JOY, G_DISC, G_HEALTH, G_COUNT };
enum { G_FOOD, G_WATER, G_ENERGY, G_POTTY, G_JOY, G_DISC, G_HEALTH, G_COUNT };   // ⚫ 25/10/2026 : + besoins
static gauge_t s_gauge[G_COUNT];

// static lv_obj_t *s_scr, *s_bg_day, *s_bg_night, *s_cloud[2], *s_kibble, *s_ball, *s_dog_img;
// ⚫ MODIFIÉ (25/10/2026) : s_bg_night remplacé par s_bg_b (fondus entre les fonds heure / météo)
static lv_obj_t *s_scr, *s_bg_day, *s_cloud[2], *s_kibble, *s_ball, *s_dog_img;
static lv_obj_t *s_frisbee, *s_bone;
// ⚫ RETIRÉ (25/10/2026) : s_hole (un seul trou), s_kennel_d, s_kennel_n (niche au premier plan)
// ⚫ AJOUTÉ (25/10/2026) : gamelles, dégâts de la cour, main, oiseaux, écran QR
static lv_obj_t *s_bowl_food, *s_bowl_water, *s_hole_o[PET_MAX_HOLES], *s_poop_o[PET_MAX_POOPS], *s_hand;
static bool s_hole_shown[PET_MAX_HOLES], s_poop_shown[PET_MAX_POOPS];
// ⚫ AJOUTÉ (25/10/2026) : trous rebouchés : terre fraîche + gazon qui repousse
static lv_obj_t *s_dirt_o[PET_MAX_HOLES], *s_sprout_o[PET_MAX_HOLES];
static lv_obj_t *s_cone_o[2];     // ⚫ AJOUTÉ (25/10/2026) : cônes de la piste
static int s_cone_pending;        // 1 / 2 : ce cône reste debout jusqu'à ce qu'il le renverse
static bool s_dirt_shown[PET_MAX_HOLES];
static lv_obj_t *s_fly, *s_firefly[6], *s_fx;
static lv_obj_t *s_info, *s_bubble, *s_bubble_lbl, *s_roller;
static lv_obj_t *s_qr_panel, *s_qr, *s_qr_title, *s_qr_text;
// ⚫ AJOUTÉ (25/10/2026) : fonds (fondu), abri au premier plan, météo, gestes, objets des jeux, besoins au gravier
static lv_obj_t *s_bg_b, *s_shelter, *s_flash, *s_fog, *s_cloud_dark[2];
#define N_DROPS  18
#define N_FLAKES 16
static struct { lv_obj_t *o; float x, y, gy; } s_drop[N_DROPS], s_flake[N_FLAKES];
static lv_obj_t *s_scold, *s_point, *s_hglass, *s_yarn, *s_wand, *s_hurdle, *s_carrot_hand, *s_hay, *s_mud, *s_brush, *s_grass;
static lv_obj_t *s_toilet_o[3];
static bool s_toilet_shown[3];
static int s_bgv = -1;           // fond affiché (BGV_xxx)
static bool s_force_night;       // sans météo : la nuit tombe quand l'animal dort (comme avant)
static uint32_t s_gesture_t0, s_muddy_until;
static int s_gesture;            // 0 = aucun, 1 = gronder, 2 = montrer le coin, 3 = sablier, 4 = plume, 5 = brosse, 6 = carotte
static int16_t s_tgt[3][2];      // cibles des jeux (pelote)
static int s_tgt_i;

static int s_mood = -1;
static bool s_night;
static uint32_t s_msg_t0;
static bool s_msg_on;
static uint32_t s_last_act_t;
static int16_t s_dig_x, s_dig_y;
// ⚫ AJOUTÉ (25/10/2026)
static uint32_t s_mess_ver = ~0u;
static int s_tip_pending;        // 1 = gamelle de croquettes, 2 = gamelle d'eau : renversée quand il y arrive
static bool s_hand_on;
static uint32_t s_hand_t0;
static bool s_qr_on;             // écran QR affiché (WiFi ou adoption)
static bool s_adopted;

typedef enum { B_OFF, B_IN, B_GROUND, B_OUT } bird_state_t;
static struct {
    lv_obj_t *o;
    bird_state_t st;
    float x, y, tx, ty, vx, vy;
    uint32_t t0;
} s_bird[N_BIRDS];

// demandes venant de la page web (autre tâche)
static portMUX_TYPE s_req_mux = portMUX_INITIALIZER_UNLOCKED;
static volatile int s_req;       // 0 = rien, 1 = adoption, 2 = réinitialisation
static char s_req_name[17];
static int s_req_species;

// copie de la scène pour la page web
static portMUX_TYPE s_snap_mux = portMUX_INITIALIZER_UNLOCKED;
// static char s_snap[3600];
static char s_snap[6144];   // ⚫ MODIFIÉ (25/10/2026) : gouttes et flocons en plus
static size_t s_snap_len;

/* ---------- utilitaires ---------- */

static int rnd(int a, int b)
{
    return a + (int)(esp_random() % (uint32_t)(b - a + 1));
}

static lv_obj_t *img_create(lv_obj_t *parent, int id)
{
    lv_obj_t *o = lv_img_create(parent);
    lv_img_set_src(o, &pet_asset(id)->img);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

// Place une image par son point d'ancrage
static void img_at(lv_obj_t *o, int id, int x, int y)
{
    lv_obj_set_pos(o, x + pet_asset(id)->ox, y + pet_asset(id)->oy);
}

// ⚫ AJOUTÉ (25/10/2026) : idem avec un descripteur (animal, image miroir)
static void img_set(lv_obj_t *o, const pet_asset_t *a, int x, int y)
{
    if (lv_img_get_src(o) != &a->img) lv_img_set_src(o, &a->img);
    lv_obj_set_pos(o, x + a->ox, y + a->oy);
}

// Arc "nu" (sans bouton), hors du groupe de l'encodeur
static lv_obj_t *bare_arc(lv_obj_t *parent)
{
    lv_obj_t *a = lv_arc_create(parent);
    lv_obj_remove_style(a, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(a, LV_OBJ_FLAG_CLICKABLE);
    lv_group_remove_obj(a);   // ne doit pas voler le focus du sélecteur d'actions
    lv_arc_set_rotation(a, 0);
    lv_obj_set_style_bg_opa(a, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(a, 0, 0);
    lv_obj_set_style_arc_rounded(a, true, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(a, true, LV_PART_INDICATOR);
    return a;
}

static void anim_opa(void *var, int32_t v)
{
    lv_obj_set_style_opa(var, (lv_opa_t)v, 0);
}

static void fade(lv_obj_t *o, lv_opa_t from, lv_opa_t to, uint32_t ms)
{
    lv_anim_del(o, anim_opa);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, o);
    lv_anim_set_exec_cb(&a, anim_opa);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_time(&a, ms);
    lv_anim_start(&a);
}

static void show(lv_obj_t *o, bool on)
{
    // ⚫ MODIFIÉ (25/10/2026) : seulement si l'état change : LVGL redessine l'objet à chaque changement de
    // drapeau, même identique (abri, nuages, papillon appelés à chaque image : 17 au lieu de 26 images/s)
    bool hidden = lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN);
    if (on && hidden) lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN);
    else if (!on && !hidden) lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
}

/* ---------- gueule et tête de l'animal ---------- */

// static int mouth_dx(void) { return PET_MOUTH[s_species][s_pose][0] * s_dog.face; }
// static int mouth_dy(void) { return PET_MOUTH[s_species][s_pose][1]; }
// ⚫ MODIFIÉ (25/10/2026) : selon l'âge
static int mouth_dx(void) { return PET_MOUTH[s_stage][s_species][s_pose][0] * s_dog.face; }
static int mouth_dy(void) { return PET_MOUTH[s_stage][s_species][s_pose][1]; }
static int head_x(void) { return (int)s_dog.x + mouth_dx() * 8 / 10; }
static int head_y(void) { return (int)(s_dog.y - s_dog.jump) + mouth_dy() - 24; }

// Place devant une gamelle pour y mettre la tête (tourné vers la droite)
// static int bowl_spot_x(int bx) { return bx - PET_MOUTH[s_species][P_eat_1][0]; }
// static int bowl_spot_y(int by) { return by - PET_MOUTH[s_species][P_eat_1][1] - 2; }
static int bowl_spot_x(int bx) { return bx - PET_MOUTH[s_stage][s_species][P_eat_1][0]; }       // ⚫ 25/10/2026
static int bowl_spot_y(int by) { return by - PET_MOUTH[s_stage][s_species][P_eat_1][1] - 2; }   // ⚫ 25/10/2026

/* ---------- effets flottants (cœurs, étoiles, Zzz, !, ?, mottes de terre) ---------- */

typedef struct {
    int16_t dx, dy, arc;   // déplacement total, hauteur de la parabole
} float_t_;

static void float_exec(void *var, int32_t v)
{
    lv_obj_t *o = var;
    float_t_ *p = lv_obj_get_user_data(o);
    lv_obj_set_style_translate_x(o, p->dx * v / 1000, 0);
    lv_obj_set_style_translate_y(o, p->dy * v / 1000 - p->arc * 4 * v * (1000 - v) / 1000000, 0);
    lv_obj_set_style_opa(o, v < 550 ? LV_OPA_COVER : (lv_opa_t)(255 * (1000 - v) / 450), 0);
}

static void float_ready(lv_anim_t *a)
{
    lv_obj_t *o = a->var;
    lv_mem_free(lv_obj_get_user_data(o));
    lv_obj_set_user_data(o, NULL);
    lv_obj_del_async(o);
}

static void float_start(lv_obj_t *o, int16_t dx, int16_t dy, int16_t arc, uint32_t time, uint32_t delay)
{
    float_t_ *p = lv_mem_alloc(sizeof(float_t_));
    if (!p) { lv_obj_del(o); return; }
    *p = (float_t_){ dx, dy, arc };
    lv_obj_set_user_data(o, p);
    lv_obj_set_style_opa(o, delay ? LV_OPA_TRANSP : LV_OPA_COVER, 0);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, o);
    lv_anim_set_exec_cb(&a, float_exec);
    lv_anim_set_values(&a, 0, 1000);
    lv_anim_set_time(&a, time);
    lv_anim_set_delay(&a, delay);
    lv_anim_set_path_cb(&a, arc ? lv_anim_path_linear : lv_anim_path_ease_out);
    lv_anim_set_ready_cb(&a, float_ready);
    lv_anim_start(&a);
}

static void float_text(const char *txt, uint32_t color, int x, int y, int16_t dx, int16_t dy, uint32_t time, uint32_t delay)
{
    lv_obj_t *l = lv_label_create(s_fx);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, &font_pet_56, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_obj_align(l, LV_ALIGN_TOP_LEFT, x - 20, y - 28);
    float_start(l, dx, dy, 0, time, delay);
}

// ⚫ MODIFIÉ (25/10/2026) : un seul cœur (demande : "juste un cœur quand il est heureux")
// static void burst(const char *txt, uint32_t color, int count) { … plusieurs icônes qui s'envolent … }
// ⚫ AJOUTÉ (25/10/2026) : bulle de pensée au-dessus de la tête (icône dans un rond blanc), qui monte et s'efface
static void thought(const char *icon, uint32_t color)
{
    lv_obj_t *l = lv_label_create(s_fx);
    lv_label_set_text(l, icon);
    lv_obj_set_style_text_font(l, &font_fr_28, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_obj_set_style_bg_color(l, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(l, LV_OPA_80, 0);
    lv_obj_set_style_radius(l, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_all(l, 7, 0);
    lv_obj_align(l, LV_ALIGN_TOP_LEFT, head_x() + s_dog.face * 18 - 20, head_y() - 66);
    float_start(l, 0, -18, 0, 2600, 0);
}
static void heal_fx(void)        { float_text(ICON_HEALTH, 0x40D070, head_x(), head_y() - 10, 0, -50, 1500, 0); }   // soigné
static void emo_confused(void)   { float_text("?", 0xFFC040, head_x(), head_y() - 14, 8, -40, 1300, 0); }   // ne comprend pas
static void emo_understood(void) { float_text("!", 0xB070FF, head_x(), head_y() - 14, 0, -36, 1100, 0); }   // a compris
static void emo_grumpy(void)     { thought(ICON_CLOUD, 0x6A6E78); }                                          // grognon

// ⚫ AJOUTÉ (25/10/2026) : son à jouer par la page web (voix de l'espèce ou bruit d'action), via la scène
static uint32_t s_snd_seq;
static char s_snd_kind[10] = "";
static void snd(const char *kind)
{
    if (!s_sound_on) return;
    s_snd_seq++;
    strlcpy(s_snd_kind, kind, sizeof(s_snd_kind));
}
static void snd_eat(void)   { snd("eat"); }
static void snd_drink(void) { snd("drink"); }
static void snd_dig(void)   { snd("dig"); }
static void snd_happy(void) { snd("happy"); }

static void heart(void)
{
    float_text(ICON_HEART, 0xFF4A8A, head_x(), head_y() - 6, 0, -70, 1500, 0);
}

static void clod_spawn(void)
{
    lv_obj_t *c = img_create(s_fx, A_clod);
    int x = (int)s_dog.x + s_dog.face * 30, y = (int)s_dog.y - 4;
    img_at(c, A_clod, x, y);
    float_start(c, -s_dog.face * rnd(30, 70), rnd(-4, 8), rnd(18, 34), 700, 0);
}

/* ---------- jour / nuit ---------- */

// ⚫ MODIFIÉ (25/10/2026) : 8 fonds selon l'heure et la météo réelles, en fondu (s_bg_day = fond affiché,
// s_bg_b = nouveau fond qui apparaît par-dessus) ; s_bg_night RETIRÉ (c'est l'un des 8 fonds).
enum { BGV_DAY, BGV_DAY_CLOUDY, BGV_DAY_RAIN, BGV_DAY_SNOW, BGV_DUSK, BGV_NIGHT, BGV_NIGHT_CLOUDY, BGV_NIGHT_SNOW };
static const uint8_t BG_ASSET[8] = { A_bg_day, A_bg_day_cloudy, A_bg_day_rain, A_bg_day_snow, A_bg_dusk, A_bg_night, A_bg_night_cloudy, A_bg_night_snow };
static const uint8_t SH_ASSET[8] = { A_shelter_day, A_shelter_day_cloudy, A_shelter_day_rain, A_shelter_day_snow, A_shelter_dusk, A_shelter_night, A_shelter_night_cloudy, A_shelter_night_snow };

static bool wx_bad(const weather_t *w) { return w->valid && (w->kind == WX_RAIN || w->kind == WX_DRIZZLE || w->kind == WX_STORM || w->kind == WX_SNOW); }

static int bg_variant(void)
{
    weather_t w;
    weather_get(&w);
    tod_t tod = weather_tod();
    if (tod == TOD_UNKNOWN) return s_force_night ? BGV_NIGHT : BGV_DAY;
    bool wet = w.kind == WX_RAIN || w.kind == WX_DRIZZLE || w.kind == WX_STORM, snow = w.kind == WX_SNOW;
    bool grey = w.kind == WX_CLOUDY || w.kind == WX_FOG;
    if (tod == TOD_NIGHT) return snow ? BGV_NIGHT_SNOW : (wet || grey) ? BGV_NIGHT_CLOUDY : BGV_NIGHT;
    if (wet) return BGV_DAY_RAIN;
    if (snow) return BGV_DAY_SNOW;
    if (grey) return BGV_DAY_CLOUDY;
    return tod == TOD_DUSK ? BGV_DUSK : BGV_DAY;
}

static void bg_ready(lv_anim_t *a)   // fin du fondu : le nouveau fond devient le fond affiché
{
    lv_img_set_src(s_bg_day, &pet_asset(BG_ASSET[s_bgv])->img);
    lv_obj_add_flag(s_bg_b, LV_OBJ_FLAG_HIDDEN);
}

static void set_night(bool night);

static void scene_update(void)
{
    int v = bg_variant();
    if (v == s_bgv) return;
    bool first = s_bgv < 0;
    s_bgv = v;
    lv_img_set_src(s_shelter, &pet_asset(SH_ASSET[v])->img);
    if (first) {
        lv_img_set_src(s_bg_day, &pet_asset(BG_ASSET[v])->img);
    } else {
        lv_img_set_src(s_bg_b, &pet_asset(BG_ASSET[v])->img);
        lv_obj_clear_flag(s_bg_b, LV_OBJ_FLAG_HIDDEN);
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, s_bg_b);
        lv_anim_set_exec_cb(&a, anim_opa);
        lv_anim_set_values(&a, 0, 255);
        lv_anim_set_time(&a, 2000);
        lv_anim_set_ready_cb(&a, bg_ready);
        lv_anim_start(&a);
    }
    set_night(v >= BGV_NIGHT);
}

static void set_night(bool night)
{
    if (night == s_night) return;
    s_night = night;
    // nuages, papillon, lucioles : voir weather_fx (météo)
    // la nuit, l'animal et les objets prennent la teinte bleutée du décor
    // (⚫ 25/10/2026 : l'animal est teinté dans frame_cb : nuit ou crotté après la boue)
    lv_obj_t *tinted[] = { s_kibble, s_ball, s_frisbee, s_bone, s_bowl_food, s_bowl_water, s_hand,
                           s_hole_o[0], s_hole_o[1], s_hole_o[2], s_hole_o[3], s_poop_o[0], s_poop_o[1], s_poop_o[2], s_poop_o[3],
                           s_dirt_o[0], s_dirt_o[1], s_dirt_o[2], s_dirt_o[3], s_sprout_o[0], s_sprout_o[1], s_sprout_o[2], s_sprout_o[3],
                           s_cone_o[0], s_cone_o[1] };   // ⚫ 25/10/2026
    for (size_t i = 0; i < LEN(tinted); i++) {
        lv_obj_set_style_img_recolor(tinted[i], lv_color_hex(0x0A1440), 0);
        lv_obj_set_style_img_recolor_opa(tinted[i], night ? LV_OPA_50 : LV_OPA_TRANSP, 0);
    }
}

// static void night_on(void)  { set_night(true); }
// static void night_off(void) { set_night(false); }
// ⚫ MODIFIÉ (25/10/2026) : avec la météo, c'est l'heure réelle qui fait la nuit ; sans Internet, comme avant
static void night_on(void)  { s_force_night = true;  scene_update(); }
static void night_off(void) { s_force_night = false; scene_update(); }

/* ---------- accessoires ---------- */

static void kibble_show(void)
{
    lv_obj_clear_flag(s_kibble, LV_OBJ_FLAG_HIDDEN);
    fade(s_kibble, 0, 255, 400);
}

static void kibble_hide(void)
{
    fade(s_kibble, 255, 0, 700);
}

static void ball_show(void)
{
    img_at(s_ball, A_ball, (int)s_dog.x + s_dog.face * 50, (int)s_dog.y - 8);   // lancée devant l'animal
    lv_obj_clear_flag(s_ball, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(s_ball, LV_OPA_COVER, 0);
}

static void ball_hide(void)
{
    fade(s_ball, 255, 0, 400);
}

static void action_done(void)
{
    s_in_action = false;
}

/* ---------- lancers et objets dans la gueule ---------- */

static void proj_start(lv_obj_t *o, int id, float x0, float y0, float x1, float y1, float h, uint32_t ms, bool hover)
{
    s_proj = (proj_t){ o, id, x0, y0, x1, y1, h, lv_tick_get(), ms, true, hover };
    lv_anim_del(o, anim_opa);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(o, LV_OPA_COVER, 0);
}

static void proj_update(uint32_t now)
{
    if (!s_proj.on) return;
    float t = lv_tick_elaps(s_proj.t0) / (float)s_proj.ms;
    if (t > 1) t = 1;
    float x = s_proj.x0 + (s_proj.x1 - s_proj.x0) * t;
    float y = s_proj.y0 + (s_proj.y1 - s_proj.y0) * t - s_proj.h * 4 * t * (1 - t);
    if (t >= 1 && s_proj.hover) y += sinf(now / 180.0f) * 3;   // plane en attendant d'être attrapé
    img_at(s_proj.o, s_proj.id, (int)x, (int)y);
}

static void carry_update(void)
{
    if (!s_carry) return;
    int x = (int)s_dog.x + mouth_dx();
    int y = (int)(s_dog.y - s_dog.jump) + mouth_dy() + 3;
    if (s_chew) x += (int)(sinf(lv_tick_get() / 90.0f) * 2);
    if (x == s_carry_lx && y == s_carry_ly) return;   // pas de redessin inutile
    s_carry_lx = x; s_carry_ly = y;
    img_at(s_carry, s_carry_id, x, y);
}

static void grab_proj(void)          // l'objet lancé passe dans la gueule
{
    if (!s_proj.on) return;
    s_proj.on = false;
    s_carry = s_proj.o;
    s_carry_id = s_proj.id;
    s_carry_lx = -9999;
}

static void drop_carry(void)         // le pose au sol devant lui, puis il disparaît
{
    if (!s_carry) return;
    img_at(s_carry, s_carry_id, (int)s_dog.x + s_dog.face * 50, (int)s_dog.y - 4);
    fade(s_carry, 255, 0, 1500);
    s_carry = NULL;
    s_chew = false;
}

static void ball_throw(void)    { proj_start(s_ball, A_ball, 240, 440, s_tx, s_ty, 150, 1000, false); }
static void frisbee_throw(void) { proj_start(s_frisbee, A_frisbee, 110, 420, s_tx, s_ty - 78, 70, 1300, true); }

static void bone_give(void)
{
    lv_img_set_src(s_bone, &pet_asset(A_bone)->img);   // ⚫ AJOUTÉ (25/10/2026) : l'objet sert aussi à la carotte
    lv_anim_del(s_bone, anim_opa);
    lv_obj_clear_flag(s_bone, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(s_bone, LV_OPA_COVER, 0);
    s_carry = s_bone;
    s_carry_id = A_bone;
    s_carry_lx = -9999;
    s_chew = true;
}

/* ---------- ⚫ AJOUTÉ (25/10/2026) : l'os : il joue avec, l'enterre, le déterre plus tard ---------- */

static void sync_mess(void);       // défini plus bas
static int s_bone_tx, s_bone_ty;   // où il lance son os pour jouer
static void bone_out(void)  { pet_bone_dig_up(); sync_mess(); bone_give(); }         // déterré : dans la gueule
static void bone_toss(void)       // il lance son os devant lui
{
    if (!s_carry) return;
    float x0 = s_dog.x + mouth_dx(), y0 = s_dog.y - s_dog.jump + mouth_dy();
    s_carry = NULL;
    s_chew = false;
    proj_start(s_bone, A_bone, x0, y0, s_bone_tx, s_bone_ty, 46, 650, false);
}
static void hole_here(void) { pet_hole_open((int)s_dog.x, (int)s_dog.y); sync_mess(); }   // il creuse pour cacher l'os
static void bone_drop_in(void)    // il laisse tomber l'os dans le trou
{
    s_carry = NULL;
    s_chew = false;
    img_at(s_bone, A_bone, (int)s_dog.x, (int)s_dog.y - 2);
    fade(s_bone, 255, 0, 700);
}
static void bone_cover(void) { pet_bone_bury((int)s_dog.x, (int)s_dog.y); sync_mess(); snd_happy(); }   // il remet la terre

/* ---------- ⚫ AJOUTÉ (25/10/2026) : la main qui caresse ---------- */

static void hand_start(void)
{
    s_hand_on = true;
    s_hand_t0 = lv_tick_get();
    lv_obj_clear_flag(s_hand, LV_OBJ_FLAG_HIDDEN);
    fade(s_hand, 0, 255, 300);
}

static void hand_stop(void)
{
    s_hand_on = false;
    fade(s_hand, 255, 0, 400);
}

// Va-et-vient sur la tête et le dos de l'animal, doigts qui se replient à chaque passage
static void hand_update(void)
{
    if (!s_hand_on) return;
    uint32_t el = lv_tick_elaps(s_hand_t0);
    float k = sinf(el / 300.0f);
    int x = head_x() - s_dog.face * (24 + (int)(k * 26));
    int y = head_y() + 4 + (int)(fabsf(k) * 4);
    int id = (el / 300) & 1 ? A_hand_2 : A_hand_1;
    img_set(s_hand, pet_asset(id), x + 22, y);
}

/* ---------- ⚫ AJOUTÉ (25/10/2026) : dégâts de la cour (trous, besoins, gamelles renversées) ---------- */

static void sync_one(lv_obj_t *o, bool *shown, bool want, int id, int x, int y, uint32_t fade_in)
{
    if (want) {
        img_at(o, id, x, y);
        if (!*shown) { show(o, true); fade(o, 0, 255, fade_in); }
    } else if (*shown) {
        fade(o, 255, 0, 900);   // disparaît en douceur (nettoyage)
    }
    *shown = want;
}

static void sync_mess(void)
{
    const pet_t *p = pet_get();
    // for (int i = 0; i < PET_MAX_HOLES; i++)
    //     sync_one(s_hole_o[i], &s_hole_shown[i], s_adopted && i < p->n_holes, A_hole, p->holes[i].x, p->holes[i].y, 2500);
    // ⚫ MODIFIÉ (25/10/2026) : trou ouvert dessiné à sa profondeur (taille 50 % -> 100 %) ; trou rebouché :
    // terre fraîche qui s'efface pendant que les pousses de gazon apparaissent puis se fondent dans la pelouse
    for (int i = 0; i < PET_MAX_HOLES; i++) {
        bool any = s_adopted && i < p->n_holes;
        bool open = any && !p->hole_filled[i];
        if (open) lv_img_set_zoom(s_hole_o[i], 128 + p->hole_dig[i] * 128 / 100);
        sync_one(s_hole_o[i], &s_hole_shown[i], open, A_hole, p->holes[i].x, p->holes[i].y, 1200);
        float k = any && p->hole_filled[i] ? (float)(p->age_s - p->hole_filled[i]) / PET_REGROW_S : 1;
        if (k < 0) k = 0;
        bool filled = any && p->hole_filled[i] && k < 1;
        if (filled) {
            img_at(s_dirt_o[i], A_dirt, p->holes[i].x, p->holes[i].y);
            img_at(s_sprout_o[i], A_sprouts, p->holes[i].x, p->holes[i].y - 2);
            if (s_dirt_shown[i]) {   // déjà là : opacités selon la repousse
                lv_obj_set_style_opa(s_dirt_o[i], (lv_opa_t)(255 * (1 - k)), 0);
                lv_obj_set_style_opa(s_sprout_o[i], (lv_opa_t)(255 * fminf(1, k * 3) * (1 - k * k)), 0);
            } else {
                show(s_dirt_o[i], true); fade(s_dirt_o[i], 0, (lv_opa_t)(255 * (1 - k)), 900);
                show(s_sprout_o[i], true); lv_obj_set_style_opa(s_sprout_o[i], (lv_opa_t)(255 * fminf(1, k * 3) * (1 - k * k)), 0);
            }
        } else if (s_dirt_shown[i]) {
            fade(s_dirt_o[i], lv_obj_get_style_opa(s_dirt_o[i], 0), 0, 900);
            fade(s_sprout_o[i], lv_obj_get_style_opa(s_sprout_o[i], 0), 0, 900);
        }
        s_dirt_shown[i] = filled;
    }
    for (int i = 0; i < PET_MAX_POOPS; i++)
        sync_one(s_poop_o[i], &s_poop_shown[i], s_adopted && i < p->n_poops, A_poop, p->poops[i].x, p->poops[i].y, 600);
    // ⚫ AJOUTÉ (25/10/2026) : cônes debout / renversés
    static const int16_t CONE[2][2] = { { SCENE_CONE0_X, SCENE_CONE0_Y }, { SCENE_CONE1_X, SCENE_CONE1_Y } };
    for (int i = 0; i < 2; i++) {
        bool down = s_adopted && (p->cones_down & (1 << i)) && s_cone_pending != i + 1;
        if (down) img_at(s_cone_o[i], A_cone_down, CONE[i][0] + 6, CONE[i][1] - 6);
        else img_at(s_cone_o[i], A_cone_up, CONE[i][0], CONE[i][1] - 12);
    }
    // une gamelle renversée par une bêtise ne bascule qu'à l'arrivée de l'animal (tip_done)
    bool ft = p->food_tipped && s_tip_pending != 1, wt = p->water_tipped && s_tip_pending != 2;
    img_set(s_bowl_food, pet_asset(ft ? A_bowl_food_tip : A_bowl_food), SCENE_FOOD_X, SCENE_FOOD_Y);
    img_set(s_bowl_water, pet_asset(wt ? A_bowl_water_tip : A_bowl_water), SCENE_WATER_X, SCENE_WATER_Y);
    if (ft) show(s_kibble, false);
    // ⚫ AJOUTÉ (25/10/2026) : besoins faits au coin gravier
    static const int8_t TOFF[3][2] = { { -8, 0 }, { 10, 3 }, { 2, -5 } };
    for (int i = 0; i < 3; i++)
        sync_one(s_toilet_o[i], &s_toilet_shown[i], s_adopted && i < p->toilet_poops, A_poop,
                 SCENE_TOILET_X + TOFF[i][0], SCENE_TOILET_Y + TOFF[i][1], 600);
    s_mess_ver = pet_mess_version();
}

static void cone_knock(void)   // ⚫ AJOUTÉ (25/10/2026)
{
    s_cone_pending = 0;
    sync_mess();
    for (int i = 0; i < 3; i++) clod_spawn();
}

static void tip_done(void)
{
    s_tip_pending = 0;
    sync_mess();
}

// Nettoyage : petites étoiles sur chaque dégât (ils s'effacent ensuite en douceur)
static void clean_fx(void)
{
    for (int i = 0; i < PET_MAX_HOLES; i++)
        if (s_hole_shown[i]) float_text(ICON_STAR, 0xFFF4A0, lv_obj_get_x(s_hole_o[i]) + 28, lv_obj_get_y(s_hole_o[i]), 0, -40, 1000, i * 120);
    for (int i = 0; i < PET_MAX_POOPS; i++)
        if (s_poop_shown[i]) float_text(ICON_STAR, 0xFFF4A0, lv_obj_get_x(s_poop_o[i]) + 15, lv_obj_get_y(s_poop_o[i]), 0, -40, 1000, i * 120);
    sync_mess();
}

// ⚫ RETIRÉ (25/10/2026) : toilet_drop() (tas qui disparaissait) : les besoins restent dans le gravier (sync_mess)

// Place pour les besoins : l'arrière-train (PET_REAR) pile au centre du gravier, tourné vers la droite
// static int potty_x(void) { return SCENE_TOILET_X - PET_REAR[s_species]; }
static int potty_x(void) { return SCENE_TOILET_X - PET_REAR[s_stage][s_species]; }   // ⚫ MODIFIÉ (25/10/2026) : selon l'âge
#define POTTY_Y (SCENE_TOILET_Y + 3)

/* ---------- ⚫ AJOUTÉ (25/10/2026) : gestes (gronder, temps mort) et objets des jeux ---------- */

static void gesture(int g)
{
    s_gesture = g;
    s_gesture_t0 = lv_tick_get();
}

static void scold_show(void)  { show(s_scold, true); fade(s_scold, 0, 255, 200); gesture(1); }
static void scold_hide(void)  { fade(s_scold, 255, 0, 300); gesture(0); }
static void point_show(void)  { show(s_point, true); fade(s_point, 0, 255, 200); gesture(2); }
static void point_hide(void)  { fade(s_point, 255, 0, 300); gesture(0); }
static void hglass_show(void) { show(s_hglass, true); fade(s_hglass, 0, 255, 300); gesture(3); }
static void hglass_hide(void) { fade(s_hglass, 255, 0, 400); gesture(0); }
static void wand_show(void)   { show(s_wand, true); fade(s_wand, 0, 255, 300); gesture(4); }
static void wand_hide(void)   { fade(s_wand, 255, 0, 400); gesture(0); }
static void brush_start(void) { show(s_brush, true); fade(s_brush, 0, 255, 300); gesture(5); }
static void brush_stop(void)  { fade(s_brush, 255, 0, 400); gesture(0); }
static void carrot_show(void) { show(s_carrot_hand, true); fade(s_carrot_hand, 0, 255, 300); gesture(6); }

static void carrot_take(void)   // la main repart, il garde la carotte dans la bouche et la croque
{
    fade(s_carrot_hand, 255, 0, 400);
    gesture(0);
    lv_obj_t *b = lv_obj_get_child(s_scr, lv_obj_get_index(s_bone));   // objet tenu : on réutilise l'image de l'os
    lv_img_set_src(b, &pet_asset(A_carrot_bit)->img);
    lv_anim_del(b, anim_opa);
    show(b, true);
    lv_obj_set_style_opa(b, LV_OPA_COVER, 0);
    s_carry = b;
    s_carry_id = A_carrot_bit;
    s_carry_lx = -9999;
    s_chew = true;
}

static void carry_eat(void)      // l'objet tenu est mangé (disparaît sans tomber)
{
    if (!s_carry) return;
    fade(s_carry, 255, 0, 600);
    s_carry = NULL;
    s_chew = false;
}

static void obj_show_at(lv_obj_t *o, int id, int x, int y) { img_at(o, id, x, y); show(o, true); fade(o, 0, 255, 400); }
static void hurdle_show(void) { obj_show_at(s_hurdle, A_hurdle, PLAY_X, PLAY_Y - 18); }
static void hurdle_hide(void) { fade(s_hurdle, 255, 0, 600); }
static void hay_show(void)    { obj_show_at(s_hay, A_hay, PLAY_X, PLAY_Y - 22); }
static void hay_hide(void)    { fade(s_hay, 255, 0, 600); }
static void mud_show(void)    { obj_show_at(s_mud, A_mud, PLAY_X, PLAY_Y - 4); }
static void mud_hide(void)    { fade(s_mud, 255, 0, 1500); s_muddy_until = lv_tick_get() + 60000; }   // reste crotté 1 min
static void grass_show(void)  { obj_show_at(s_grass, A_grass, (int)s_dog.x + s_dog.face * 46, (int)s_dog.y - 12); }
static void grass_hide(void)  { fade(s_grass, 255, 0, 800); }

// Pelote : lancée, puis l'animal la fait rouler 2 fois (cibles préparées par do_action)
static void yarn_roll(void)
{
    int i = s_tgt_i < 3 ? s_tgt_i : 2;
    float x0 = s_tgt_i ? s_tgt[s_tgt_i - 1][0] : 240, y0 = s_tgt_i ? s_tgt[s_tgt_i - 1][1] : 440;
    proj_start(s_yarn, A_yarn, x0, y0, s_tgt[i][0], s_tgt[i][1], s_tgt_i ? 14 : 90, s_tgt_i ? 600 : 900, false);
    s_tgt_i++;
}
static void yarn_hide(void) { s_proj.on = false; fade(s_yarn, 255, 0, 600); }

// Positions des gestes (appelé à chaque image)
static void gesture_update(uint32_t now)
{
    uint32_t el = lv_tick_elaps(s_gesture_t0);
    switch (s_gesture) {
    case 1: {   // index levé qui s'agite au-dessus de la tête
        int wag = (int)(sinf(el / 70.0f) * 6);
        img_set(s_scold, pet_asset(A_hand_scold), head_x() + s_dog.face * 40 + wag, head_y() - 30);
        break;
    }
    case 2: {   // main qui montre le coin (à droite de l'animal)
        bool right = CORNER_X > s_dog.x;
        img_set(s_point, right ? pet_asset_left(A_hand_point) : pet_asset(A_hand_point),
                head_x() + (right ? -50 : 50), head_y() - 26 + (int)(sinf(el / 160.0f) * 2));
        break;
    }
    // case 3: {   // sablier au-dessus de la tête, le sable s'écoule en 4 s
    //     int f = el / 1000 > 3 ? 3 : el / 1000;
    //     img_set(s_hglass, pet_asset(A_hourglass_1 + f), head_x(), head_y() - 42);
    //     break;
    // }
    // ⚫ MODIFIÉ (25/10/2026) : cœur brisé (demande) : il se fend, puis les deux moitiés s'écartent en montant
    case 3: {
        int id = el < 1500 ? A_heart_crack : A_heart_split;
        int beat = el < 1500 ? (int)(fabsf(sinf(el / 160.0f)) * 3) : 0;
        img_set(s_hglass, pet_asset(id), head_x(), head_y() - 44 - (int)(el / 220) - beat);
        break;
    }
    case 4: {   // canne à plume au-dessus de lui, la plume se balance
        img_set(s_wand, pet_asset(((el / 260) & 1) ? A_wand_2 : A_wand_1), head_x() + s_dog.face * 20 + 30, head_y() - 66);
        break;
    }
    case 5: {   // brosse qui passe le long du dos
        float k = sinf(el / 320.0f);
        img_set(s_brush, pet_asset(((el / 320) & 1) ? A_hand_brush_2 : A_hand_brush_1),
                (int)s_dog.x - s_dog.face * (int)(k * 34) + 18, head_y() + 4 + (int)(fabsf(k) * 6));
        break;
    }
    case 6: {   // main qui tend la carotte devant la bouche
        img_set(s_carrot_hand, pet_asset(A_hand_carrot), (int)s_dog.x + mouth_dx() + 46, (int)(s_dog.y - s_dog.jump) + mouth_dy() - 2);
        break;
    }
    default:
        break;
    }
    (void)now;
}

/* ---------- ⚫ AJOUTÉ (25/10/2026) : météo à l'écran ---------- */

static void weather_fx(uint32_t now, float dt)
{
    static uint32_t next_flash = 10000;
    weather_t w;
    weather_get(&w);
    bool wet = w.valid && (w.kind == WX_RAIN || w.kind == WX_DRIZZLE || w.kind == WX_STORM);
    bool snow = w.valid && w.kind == WX_SNOW;
    int nd = !wet ? 0 : w.kind == WX_DRIZZLE ? 7 : w.kind == WX_STORM ? N_DROPS : 13;
    float wind = w.valid ? fminf(w.wind, 60) : 0;
    for (int i = 0; i < N_DROPS; i++) {   // pluie : gouttes inclinées par le vent, éclaboussure au sol
        if (i >= nd) { show(s_drop[i].o, false); continue; }
        if (lv_obj_has_flag(s_drop[i].o, LV_OBJ_FLAG_HIDDEN)) {
            show(s_drop[i].o, true);
            s_drop[i].x = rnd(0, 480); s_drop[i].y = rnd(-200, 200); s_drop[i].gy = rnd(230, 470);
        }
        s_drop[i].y += 420 * dt;
        s_drop[i].x += wind * 2.5f * dt;
        if (s_drop[i].y >= s_drop[i].gy) {
            lv_obj_t *sp = img_create(s_fx, A_splash);
            img_at(sp, A_splash, (int)s_drop[i].x, (int)s_drop[i].gy);
            float_start(sp, 0, 0, 0, 260, 0);
            s_drop[i].x = rnd(-20, 480); s_drop[i].y = rnd(-40, 0); s_drop[i].gy = rnd(230, 470);
        }
        img_at(s_drop[i].o, A_drop, (int)s_drop[i].x, (int)s_drop[i].y);
    }
    for (int i = 0; i < N_FLAKES; i++) {   // neige : flocons qui dérivent
        if (!snow) { show(s_flake[i].o, false); continue; }
        if (lv_obj_has_flag(s_flake[i].o, LV_OBJ_FLAG_HIDDEN)) {
            show(s_flake[i].o, true);
            s_flake[i].x = rnd(0, 480); s_flake[i].y = rnd(-100, 470); s_flake[i].gy = rnd(240, 476);
        }
        s_flake[i].y += 38 * dt;
        s_flake[i].x += (sinf(now / 900.0f + i) * 18 + wind * 0.6f) * dt;
        if (s_flake[i].y >= s_flake[i].gy) { s_flake[i].x = rnd(0, 480); s_flake[i].y = rnd(-20, 0); }
        img_at(s_flake[i].o, A_flake, (int)s_flake[i].x, (int)s_flake[i].y);
    }
    // orage : éclair (tout l'écran s'illumine un instant)
    if (w.valid && w.kind == WX_STORM && now >= next_flash) {
        next_flash = now + rnd(6000, 18000);
        show(s_flash, true);
        fade(s_flash, 190, 0, 380);
    }
    show(s_fog, w.valid && w.kind == WX_FOG);
    // nuages : blancs par beau temps / quelques nuages, sombres par temps couvert ou pluvieux (pas la nuit)
    bool day = s_bgv >= 0 && s_bgv < BGV_NIGHT;
    bool white = day && (!w.valid || w.kind == WX_CLEAR || w.kind == WX_PARTLY);
    bool dark = day && w.valid && (w.kind == WX_CLOUDY || wet || w.kind == WX_STORM);
    show(s_cloud[0], white);
    show(s_cloud[1], white && (!w.valid || w.kind == WX_PARTLY || w.clouds > 30));
    for (int i = 0; i < 2; i++) show(s_cloud_dark[i], dark);
    // papillon : jour, beau temps, assez doux ; lucioles : nuit sans pluie ni neige
    show(s_fly, day && !wet && !snow && (!w.valid || w.temp > 10));
    bool ff = s_night && !wet && !snow && (!w.valid || w.temp > 8);
    for (int i = 0; i < 6; i++) show(s_firefly[i], ff);
}

// L'animal est-il sous l'abri ? (pieds dans la zone)
static bool sheltered(void)
{
    return s_dog.x >= SCENE_SHELTER_X0 && s_dog.x <= SCENE_SHELTER_X1 && s_dog.y >= SCENE_SHELTER_Y0 && s_dog.y <= SCENE_SHELTER_Y1;
}

/* ---------- ⚫ AJOUTÉ (25/10/2026) : oiseaux ---------- */

static void birds_spawn(void)
{
    for (int i = 0; i < N_BIRDS; i++) {
        bool from_left = esp_random() & 1;
        s_bird[i].x = from_left ? -20 : 500;
        s_bird[i].y = rnd(120, 200);
        s_bird[i].tx = rnd(200, 410);
        s_bird[i].ty = rnd(300, 360);
        s_bird[i].st = B_IN;
        s_bird[i].t0 = lv_tick_get() + i * 400;
        show(s_bird[i].o, true);
        lv_obj_set_style_opa(s_bird[i].o, LV_OPA_COVER, 0);
    }
}

static bool birds_present(float *nx, float *ny)
{
    float best = 1e9;
    bool any = false;
    for (int i = 0; i < N_BIRDS; i++) {
        if (s_bird[i].st != B_IN && s_bird[i].st != B_GROUND) continue;
        float d = fabsf(s_bird[i].x - s_dog.x) + fabsf(s_bird[i].y - s_dog.y);
        if (d < best) { best = d; *nx = s_bird[i].tx; *ny = s_bird[i].ty; any = true; }
    }
    return any;
}

static void birds_update(uint32_t now, float dt)
{
    for (int i = 0; i < N_BIRDS; i++) {
        typeof(s_bird[0]) *b = &s_bird[i];
        if (b->st == B_OFF) continue;
        if ((int32_t)(now - b->t0) < 0) continue;   // arrivée décalée
        int id;
        bool left;
        if (b->st == B_IN) {
            float dx = b->tx - b->x, dy = b->ty - b->y, d = sqrtf(dx * dx + dy * dy) + 0.01f;
            float v = 120 * dt;
            if (d <= v) { b->x = b->tx; b->y = b->ty; b->st = B_GROUND; }
            else { b->x += dx / d * v; b->y += dy / d * v; }
            left = dx < 0;
            id = (now / 90) & 1 ? A_bird_up : A_bird_down;
        } else if (b->st == B_GROUND) {
            left = ((now / 2000 + i) & 1);
            id = ((now / 400 + i) % 3 == 0) ? A_bird_peck : A_bird_stand;
            if (s_night || (fabsf(b->x - s_dog.x) < 80 && fabsf(b->y - s_dog.y) < 50)) {   // l'animal approche : envol
                b->st = B_OUT;
                b->vx = (b->x >= s_dog.x ? 1 : -1) * rnd(150, 200);
                b->vy = -rnd(110, 160);
            }
        } else {   // B_OUT
            b->x += b->vx * dt;
            b->y += b->vy * dt;
            left = b->vx < 0;
            id = (now / 80) & 1 ? A_bird_up : A_bird_down;
            if (b->x < -40 || b->x > 520 || b->y < -40) { b->st = B_OFF; show(b->o, false); continue; }
        }
        img_set(b->o, left ? pet_asset_left(id) : pet_asset(id), (int)b->x, (int)b->y);
    }
}

/* ---------- moteur de scripts ---------- */

static void q_clear(void) { s_qn = s_qi = 0; s_step_t0 = lv_tick_get(); }

static void q_push(step_t st)
{
    if (s_qn < QMAX) s_q[s_qn++] = st;
}

#define GOTO(X, Y, R)   q_push((step_t){ .k = S_GOTO, .x = (X), .y = (Y), .run = (R) })
#define POSE(A, MS)     q_push((step_t){ .k = S_POSE, .anim = &(A), .ms = (MS) })
#define FACE(F)         q_push((step_t){ .k = S_FACE, .face = (F) })
#define CALL(F)         q_push((step_t){ .k = S_FUNC, .fn = (F) })
#define LAPS()          q_push((step_t){ .k = S_LAPS })
#define JUMP()          q_push((step_t){ .k = S_JUMP })
#define JUMP_GRAB()     q_push((step_t){ .k = S_JUMP, .grab = true })   // ⚫ AJOUTÉ (25/10/2026)
#define CHASE()         q_push((step_t){ .k = S_CHASE })                // ⚫ AJOUTÉ (25/10/2026)
#define LEAP(X, Y)      q_push((step_t){ .k = S_LEAP, .x = (X), .y = (Y) })   // ⚫ AJOUTÉ (25/10/2026)

// Une nouvelle action interrompt le jeu en cours : on range les objets
static void props_reset(void)
{
    lv_obj_t *objs[] = { s_frisbee, s_bone };
    for (size_t i = 0; i < LEN(objs); i++) {
        if (!lv_obj_has_flag(objs[i], LV_OBJ_FLAG_HIDDEN)) fade(objs[i], lv_obj_get_style_opa(objs[i], 0), 0, 300);
    }
    if (s_carry == s_ball || (s_proj.on && s_proj.o == s_ball)) fade(s_ball, 255, 0, 300);
    s_proj.on = false;
    s_carry = NULL;
    s_chew = false;
    if (s_hand_on) hand_stop();
    if (s_tip_pending) tip_done();
    // ⚫ AJOUTÉ (25/10/2026) : gestes et objets des jeux
    lv_obj_t *more[] = { s_scold, s_point, s_hglass, s_wand, s_brush, s_carrot_hand, s_yarn, s_hurdle, s_hay, s_mud, s_grass };
    for (size_t i = 0; i < LEN(more); i++)
        if (!lv_obj_has_flag(more[i], LV_OBJ_FLAG_HIDDEN)) fade(more[i], lv_obj_get_style_opa(more[i], 0), 0, 300);
    s_gesture = 0;
}

static void set_anim(const anim_t *a)
{
    if (s_dog.anim != a) {
        s_dog.anim = a;
        s_dog.anim_t0 = lv_tick_get();
    }
}

// Comportement de repos selon l'humeur
static void walk_clamp(float *x, float *y);   // ⚫ AJOUTÉ (25/10/2026) : défini plus bas
// Scénario de l'os (os donné, ou envie de le ressortir) : déterrer s'il est caché, ronger, lancer / rattraper,
// puis l'enterrer (dans le même trou s'il en sort, sinon il creuse un nouveau trou dans un coin tranquille)
static void bone_script(void)
{
    int bx, by;
    bool buried = pet_bone_buried(&bx, &by);
    float base_x = s_dog.x, base_y = s_dog.y;
    if (buried) {
        GOTO(bx, by, true); FACE(1); POSE(AN_DIG, 2600); CALL(bone_out); POSE(AN_WAG, 500);
        base_x = bx; base_y = by;
    } else {
        CALL(bone_give);
    }
    POSE(AN_LIE, 3500);                                   // couché, il le ronge
    float tx = base_x + (base_x < 300 ? 1 : -1) * rnd(70, 100), ty = base_y + rnd(-14, 14);
    walk_clamp(&tx, &ty);
    s_bone_tx = (int)tx; s_bone_ty = (int)ty;
    CALL(bone_toss); POSE(AN_WAG, 350);
    GOTO((int)tx - 40, (int)ty + 2, true); FACE(1); POSE(AN_EAT, 350); CALL(grab_proj);   // il le rattrape
    POSE(AN_WAG, 600); JUMP();
    int hx = bx, hy = by;
    if (!buried) pet_lawn_spot(&hx, &hy);
    GOTO(hx, hy, false); FACE(1);
    if (!buried) { CALL(hole_here); POSE(AN_DIG, 2400); }  // nouveau trou
    CALL(bone_drop_in); POSE(AN_DIG, 2200);                // il repousse la terre du museau
    CALL(bone_cover); POSE(AN_WAG, 900); CALL(heart);
}

static void idle_plan(void)
{
    const pet_t *p = pet_get();
    int mood = pet_mood();
    if (mood != PET_MOOD_SLEEPING) CALL(night_off);   // en premier : les poses de repos durent indéfiniment
    // ⚫ AJOUTÉ (25/10/2026) : pluie, neige, orage : il va s'abriter (sauf besoin pressant : manger, boire, sortir)
    weather_t w;
    weather_get(&w);
    // if (wx_bad(&w) && (mood == PET_MOOD_CRYING || mood == PET_MOOD_TIRED || mood == PET_MOOD_SAD ||
    //                    mood == PET_MOOD_HAPPY || mood == PET_MOOD_NEUTRAL || mood == PET_MOOD_SICK)) {
    //     GOTO(SHELTER_X + rnd(-24, 24), SHELTER_Y + rnd(-6, 6), mood == PET_MOOD_HAPPY);
    //     FACE(1);
    //     const anim_t *a = mood == PET_MOOD_CRYING ? &AN_CRY : mood == PET_MOOD_TIRED || mood == PET_MOOD_SICK ? &AN_LIE
    //                     : mood == PET_MOOD_SAD ? &AN_SAD : &AN_CALM;
    //     POSE(*a, mood == PET_MOOD_HAPPY || mood == PET_MOOD_NEUTRAL ? rnd(8000, 15000) : 0);
    //     s_step_t0 = lv_tick_get();
    //     return;
    // }
    // ⚫ MODIFIÉ (25/10/2026) : sous la pluie, la neige ou l'orage, il reste à l'abri quel que soit son état (sauf la
    // nuit : il dort déjà sur la litière, sous le toit ; et les bêtises) : s'il a faim, soif ou besoin de sortir, il
    // attend sous le toit en regardant vers la gamelle / le coin gravier. Il court s'il est dehors.
    // Aussi à l'abri : grand froid (< 3 °C) et forte chaleur (> 27 °C, il cherche l'ombre et s'allonge).
    bool cold = w.valid && w.temp < 3, hot = w.valid && w.temp > 27 && weather_tod() == TOD_DAY;
    if ((wx_bad(&w) || cold || hot) && mood != PET_MOOD_SLEEPING && mood != PET_MOOD_NAUGHTY) {
        GOTO(SHELTER_X + rnd(-20, 14), SHELTER_Y + rnd(-6, 6), !sheltered() && wx_bad(&w));
        FACE(1);
        const anim_t *a = mood == PET_MOOD_CRYING ? &AN_CRY
                        : mood == PET_MOOD_TIRED || mood == PET_MOOD_SICK || hot ? &AN_LIE
                        : mood == PET_MOOD_SAD || mood == PET_MOOD_HUNGRY || mood == PET_MOOD_THIRSTY || mood == PET_MOOD_POTTY ? &AN_SAD
                        : rnd(0, 2) ? &AN_CALM : &AN_LIE;
        POSE(*a, mood == PET_MOOD_HAPPY || mood == PET_MOOD_NEUTRAL ? rnd(10000, 20000) : 0);
        s_step_t0 = lv_tick_get();
        return;
    }

    switch (mood) {
    case PET_MOOD_SLEEPING:
        // GOTO(KENNEL_DOG_X, KENNEL_DOG_Y, false); FACE(1); CALL(night_on); POSE(AN_SLEEP, 0);
        GOTO(BED_X, BED_Y, false); FACE(1); CALL(night_on); POSE(AN_SLEEP, 0);   // ⚫ 25/10/2026 : litière
        break;
    case PET_MOOD_CRYING:
        POSE(AN_CRY, 0);
        break;
    case PET_MOOD_NAUGHTY:
        // ⚫ MODIFIÉ (25/10/2026) : selon la bêtise
        switch (p->naughty_kind) {
        case PET_NAUGHTY_TIP_FOOD:
        case PET_NAUGHTY_TIP_WATER: {   // va pousser la gamelle du museau, puis prend un air fier
            bool food = p->naughty_kind == PET_NAUGHTY_TIP_FOOD;
            s_tip_pending = food ? 1 : 2;
            sync_mess();
            int bx = food ? SCENE_FOOD_X : SCENE_WATER_X, by = food ? SCENE_FOOD_Y : SCENE_WATER_Y;
            GOTO(bx - 62, by + 4, true); FACE(1); POSE(AN_DIG, 1300); CALL(tip_done); POSE(AN_WAG, 0);
            break;
        }
        case PET_NAUGHTY_CONE: {        // ⚫ AJOUTÉ (25/10/2026) : il saute sur le cône de la piste et le renverse
            int c = p->naughty_at.x == SCENE_CONE1_X ? 1 : 0;
            if (p->naughty_at.x > -500) {
                s_cone_pending = c + 1;
                sync_mess();
                GOTO(p->naughty_at.x - 34, p->naughty_at.y + 6, true); FACE(1); JUMP(); CALL(cone_knock); POSE(AN_WAG, 0);
            } else {
                POSE(AN_WAG, 0);
            }
            break;
        }
        case PET_NAUGHTY_POOP:          // besoins n'importe où
            if (p->naughty_at.x > -500) {
                s_dig_x = p->naughty_at.x;
                s_dig_y = p->naughty_at.y;
                GOTO(s_dig_x, s_dig_y, false); POSE(AN_POTTY, 2400); POSE(AN_WAG, 0);
            } else {
                POSE(AN_GUILTY, 0);
            }
            break;
        default:                        // creuse un trou
            if (p->naughty_at.x > -500) { s_dig_x = p->naughty_at.x; s_dig_y = p->naughty_at.y; }
            GOTO(s_dig_x, s_dig_y, true); POSE(AN_DIG, 0);
            break;
        }
        break;
    case PET_MOOD_POTTY:     // ⚫ AJOUTÉ (25/10/2026) : demande à sortir : attend près du coin besoins
        GOTO(SCENE_TOILET_X - 96, SCENE_TOILET_Y + 26, false); FACE(1); POSE(AN_SAD, 0);
        break;
    case PET_MOOD_SICK:
        GOTO(BED_X + 6, BED_Y + 4, false); FACE(1); POSE(AN_SICK, 0);
        break;
    case PET_MOOD_HUNGRY:    // attend devant sa gamelle
        GOTO(SCENE_FOOD_X - 72, SCENE_FOOD_Y, false); FACE(1); POSE(AN_SAD, 0);
        break;
    case PET_MOOD_THIRSTY:
        GOTO(SCENE_WATER_X - 72, SCENE_WATER_Y, false); FACE(1); POSE(AN_SAD, 0);
        break;
    // case PET_MOOD_TIRED:
    //     POSE(AN_LIE, 0);
    //     break;
    // case PET_MOOD_SAD:
    //     POSE(AN_SAD, 0);
    //     break;
    // ⚫ MODIFIÉ (25/10/2026) : plus réaliste : fatigué, il va s'allonger sur sa litière ; triste, il traîne
    // lentement vers un coin de la cour ; il renifle / broute l'herbe en se promenant (herbivores : souvent)
    case PET_MOOD_TIRED:
        GOTO(BED_X + rnd(-10, 10), BED_Y, false); FACE(1); POSE(AN_LIE, 0);
        break;
    case PET_MOOD_SAD:
        GOTO(rnd(150, 300), rnd(330, 360), false); FACE(rnd(0, 1) ? 1 : -1); POSE(AN_SAD, 0);
        break;
    case PET_MOOD_HAPPY:     // se balade, remue la queue, saute de joie de temps en temps
    {
        // ⚫ AJOUTÉ (25/10/2026) : de temps en temps, il va déterrer son os pour jouer, puis le recache
        int bx, by;
        if ((pet_games_mask(s_species) & (1u << PET_ACT_BONE)) && pet_bone_buried(&bx, &by) && rnd(0, 7) == 0) {
            s_in_action = true;   // pas interrompu en plein milieu (l'os resterait dehors)
            bone_script();
            CALL(action_done);
            break;
        }
    }
        // GOTO(rnd(170, 330), rnd(322, 356), rnd(0, 3) == 0);
        { int wx, wy; pet_lawn_spot(&wx, &wy); GOTO(wx, wy, rnd(0, 3) == 0); }   // ⚫ MODIFIÉ (25/10/2026) : partout
        if (rnd(0, 2) == 0 && s_stage != PET_STAGE_ADULT) JUMP();   // les jeunes sautent plus volontiers
        if (rnd(0, 2) == 0) POSE(AN_EAT, rnd(1500, 3500));            // renifle / broute
        POSE(*(rnd(0, 1) ? &AN_WAG : &AN_SIT_HAPPY), rnd(4000, 9000));
        break;
    default: {               // va bien : flâne tranquillement, renifle, broute (herbivores), se pose
        bool herb = s_species >= 2 && s_species != 5;   // mouton … lama (pas le chien, le chat, le cochon)
        // GOTO(rnd(170, 320), rnd(324, 354), false);
        int wx, wy;
        pet_lawn_spot(&wx, &wy);   // ⚫ MODIFIÉ (25/10/2026) : n'importe où sur la pelouse
        GOTO(wx, wy, false);
        if (rnd(0, herb ? 1 : 3) == 0) POSE(AN_EAT, rnd(2500, herb ? 7000 : 3000));
        POSE(*(rnd(0, 2) ? &AN_CALM : &AN_SIT), rnd(6000, 12000));
        break;
    }
    }
    s_step_t0 = lv_tick_get();
}

static int s_last_pose = -1, s_last_left, s_last_x, s_last_y;   // dernière image posée (remise à zéro à l'adoption)

static void dog_render(void)
{
    const anim_t *a = s_dog.anim;
    uint32_t fi = (lv_tick_elaps(s_dog.anim_t0) / a->ms) % a->n;
    int pose = a->f[fi];
    bool left = s_dog.face < 0;
    int x = (int)s_dog.x, y = (int)(s_dog.y - s_dog.jump);
    if (pose == s_last_pose && left == s_last_left && x == s_last_x && y == s_last_y) return;   // rien n'a bougé
    s_last_pose = pose; s_last_left = left; s_last_x = x; s_last_y = y;
    s_pose = pose;
    // img_set(s_dog_img, pet_animal(pose, left), x, y);
    // ⚫ MODIFIÉ (25/10/2026) : la pose est décodée dans un tampon unique (même adresse à chaque fois) :
    // on force LVGL à relire l'image
    const pet_asset_t *pa = pet_animal(pose, left);
    // lv_img_set_src(s_dog_img, NULL);
    lv_obj_invalidate(s_dog_img);   // ⚫ MODIFIÉ (25/10/2026) : NULL inondait le journal d'avertissements ; LVGL relit déjà une image donnée à nouveau
    lv_img_set_src(s_dog_img, &pa->img);
    lv_obj_set_pos(s_dog_img, x + pa->ox, y + pa->oy);
}

static void step_next(void)
{
    s_qi++;
    s_step_t0 = lv_tick_get();
}

/* ---------- ⚫ AJOUTÉ (25/10/2026) : zone de marche et abri (il ne sort plus de l'écran, ne traverse plus l'abri) ---------- */
// Abri (pieds de l'animal) : murs du fond et des côtés ; on n'y entre / n'en sort que par l'ouverture avant
#define SH_X0     30
#define SH_X1     164
#define SH_FLOOR  304      // plus haut = dans le mur du fond
#define SH_FRONT  324      // bord avant du toit
#define SH_GAP0   44       // ouverture avant (entre les poteaux)
#define SH_GAP1   150

static bool in_shelter_box(float x, float y) { return x > SH_X0 && x < SH_X1 && y < SH_FRONT; }

// Ramène un point dans la cour visible : dans le cercle de l'écran (marge pour le corps), devant la clôture,
// jamais dans les murs de l'abri
static void walk_clamp(float *x, float *y)
{
    if (*y < 244) *y = 244;
    if (*y > 404) *y = 404;
    if (*x < 56) *x = 56;
    if (*x > 430) *x = 430;
    float dx = *x - 240, dy = *y - 250, r = sqrtf(dx * dx + dy * dy);
    if (r > 200) { *x = 240 + dx * 200 / r; *y = 250 + dy * 200 / r; }
    if (in_shelter_box(*x, *y) && *y < SH_FLOOR) *y = SH_FLOOR;
}

// Le trajet a -> b traverse-t-il un mur de l'abri ? (on n'entre / ne sort que par l'avant, entre les poteaux)
static bool path_blocked(float ax, float ay, float bx, float by)
{
    float px = ax, py = ay;
    for (int i = 1; i <= 24; i++) {
        float x = ax + (bx - ax) * i / 24, y = ay + (by - ay) * i / 24;
        bool pin = in_shelter_box(px, py), cin = in_shelter_box(x, y);
        if (pin != cin) {
            float ox = pin ? x : px, oy = pin ? y : py;   // côté extérieur du passage
            if (!(oy >= SH_FRONT - 2 && ox >= SH_GAP0 && ox <= SH_GAP1)) return true;
        }
        if (cin && y < SH_FLOOR - 1) return true;          // dans le mur du fond
        px = x; py = y;
    }
    return false;
}

// Prochain point à viser pour aller vers (tx, ty) : direct, ou devant l'ouverture de l'abri
static bool route_via(float tx, float ty, float *vx, float *vy)
{
    if (!path_blocked(s_dog.x, s_dog.y, tx, ty)) return false;
    bool tin = in_shelter_box(tx, ty);
    float gx = tin ? tx : s_dog.x;
    if (gx < SH_GAP0 + 8) gx = SH_GAP0 + 8;
    if (gx > SH_GAP1 - 8) gx = SH_GAP1 - 8;
    *vx = gx;
    *vy = SH_FRONT + 12;
    if (path_blocked(s_dog.x, s_dog.y, *vx, *vy)) { *vx = s_dog.x; *vy = SH_FRONT + 12; }   // d'abord descendre
    return true;
}

static void track_pos(float a, float *x, float *y)
{
    *x = SCENE_TRACK_X + SCENE_TRACK_RX * cosf(a);
    *y = SCENE_TRACK_Y + SCENE_TRACK_RY * sinf(a) + 14;   // +14 : les pattes sur le sable
}

static void dog_update(float dt)
{
    if (s_qi >= s_qn) {           // script terminé
        if (s_in_action) s_in_action = false;
        q_clear();
        idle_plan();
    }
    step_t *st = &s_q[s_qi];
    uint32_t el = lv_tick_elaps(s_step_t0);

    switch (st->k) {
    case S_GOTO: {
        // ⚫ RETIRÉ (25/10/2026) : contournement de la niche (crosses_kennel, coin KENNEL_PASS_X/Y)
        // float dx = st->x - s_dog.x, dy = st->y - s_dog.y;
        // ⚫ MODIFIÉ (25/10/2026) : destination ramenée dans la cour visible ; passage par l'avant de l'abri
        float tx = st->x, ty = st->y;
        walk_clamp(&tx, &ty);
        st->x = tx; st->y = ty;
        float vx, vy;
        bool via = route_via(tx, ty, &vx, &vy);
        if (via) { tx = vx; ty = vy; }
        float dx = tx - s_dog.x, dy = ty - s_dog.y;
        float d = sqrtf(dx * dx + dy * dy);
        float v = (st->run ? RUN_SPEED : WALK_SPEED) * dt;
        if (d <= v || d < 1.5f) {
            s_dog.x = tx;
            s_dog.y = ty;
            if (!via) step_next();
        } else {
            s_dog.x += dx / d * v;
            s_dog.y += dy / d * v;
            if (fabsf(dx) > 2) s_dog.face = dx > 0 ? 1 : -1;
            set_anim(st->run ? &AN_RUN : &AN_WALK);
        }
        break;
    }
    case S_POSE:
        set_anim(st->anim);
        if (st->ms && el >= st->ms) step_next();
        break;
    case S_FACE:
        s_dog.face = st->face;
        step_next();
        break;
    case S_FUNC:
        if (st->fn) st->fn();
        step_next();
        break;
    case S_JUMP: {
        set_anim(&AN_JUMP);
        float t = el / 650.0f;
        s_dog.jump = t < 1 ? 48 * sinf(t * (float)M_PI) : 0;
        if (st->grab && t >= 0.45f) grab_proj();   // attrape le frisbee au sommet du saut
        if (t >= 1) step_next();
        break;
    }
    case S_LAPS: {   // 2 tours de piste dans le sens inverse des aiguilles d'une montre, la balle devant
        if (el == 0 || s_dog.lap_done == 0) {
            s_dog.lap_a = atan2f((s_dog.y - 14 - SCENE_TRACK_Y) / SCENE_TRACK_RY, (s_dog.x - SCENE_TRACK_X) / SCENE_TRACK_RX);
        }
        float w = RUN_SPEED / ((SCENE_TRACK_RX + SCENE_TRACK_RY) * 0.5f);   // vitesse angulaire approx.
        float px = s_dog.x;
        s_dog.lap_a -= w * dt;
        s_dog.lap_done += w * dt;
        track_pos(s_dog.lap_a, &s_dog.x, &s_dog.y);
        if (fabsf(s_dog.x - px) > 0.3f) s_dog.face = s_dog.x > px ? 1 : -1;
        set_anim(&AN_RUN);
        float bx, by;
        track_pos(s_dog.lap_a - 0.55f, &bx, &by);
        img_at(s_ball, A_ball, (int)bx, (int)(by - 8 - fabsf(sinf(el / 110.0f)) * 14));
        if (s_dog.lap_done >= 4 * (float)M_PI) {
            s_dog.lap_done = 0;
            step_next();
        }
        break;
    }
    case S_LEAP: {   // ⚫ AJOUTÉ (25/10/2026) : saut en avançant vers (x, y), 700 ms
        static float x0, y0;
        if (el < 40 || s_dog.jump == 0) { x0 = s_dog.x; y0 = s_dog.y; }
        float t = el / 700.0f;
        if (t > 1) t = 1;
        s_dog.x = x0 + (st->x - x0) * t;
        s_dog.y = y0 + (st->y - y0) * t;
        s_dog.jump = t < 1 ? 56 * sinf(t * (float)M_PI) + 0.01f : 0;
        if (fabsf(st->x - x0) > 2) s_dog.face = st->x > x0 ? 1 : -1;
        set_anim(&AN_JUMP);
        if (t >= 1) { s_dog.jump = 0; step_next(); }
        break;
    }
    case S_CHASE: {  // ⚫ AJOUTÉ (25/10/2026) : court vers l'oiseau le plus proche, jusqu'à ce qu'ils soient partis
        float bx, by;
        if (!birds_present(&bx, &by) || el > 10000) { step_next(); break; }
        float dx = bx - s_dog.x, dy = (by + 6) - s_dog.y, d = sqrtf(dx * dx + dy * dy);
        float v = RUN_SPEED * 0.9f * dt;
        if (d > 4) {
            // s_dog.x += dx / d * fminf(v, d);
            // s_dog.y += dy / d * fminf(v, d);
            // ⚫ MODIFIÉ (25/10/2026) : les oiseaux s'envolent hors de l'écran : il s'arrête au bord de la cour
            float nx = s_dog.x + dx / d * fminf(v, d), ny = s_dog.y + dy / d * fminf(v, d);
            walk_clamp(&nx, &ny);
            if (!path_blocked(s_dog.x, s_dog.y, nx, ny)) { s_dog.x = nx; s_dog.y = ny; }
            if (fabsf(dx) > 2) s_dog.face = dx > 0 ? 1 : -1;
        }
        set_anim(&AN_RUN);
        break;
    }
    }
}

/* ---------- jauges et textes ---------- */

static void gauge_build(int g, uint16_t a0, uint16_t a1, bool reverse, uint32_t color, const char *icon)
{
    lv_obj_t *arc = bare_arc(s_scr);
    lv_obj_set_size(arc, 464, 464);
    lv_obj_center(arc);
    lv_arc_set_bg_angles(arc, a0, a1);
    lv_arc_set_range(arc, 0, 100);
    lv_arc_set_mode(arc, reverse ? LV_ARC_MODE_REVERSE : LV_ARC_MODE_NORMAL);
    lv_obj_set_style_arc_width(arc, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 10, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(arc, LV_OPA_50, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, lv_color_hex(color), LV_PART_INDICATOR);
    lv_obj_set_style_pad_all(arc, 0, LV_PART_INDICATOR);

    // icône au milieu de l'arc, sur une pastille sombre (lisible sur le décor)
    lv_obj_t *l = lv_label_create(s_scr);
    lv_label_set_text(l, icon);
    lv_obj_set_style_text_font(l, &font_fr_20, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_obj_set_style_bg_color(l, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(l, LV_OPA_50, 0);
    lv_obj_set_style_radius(l, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_all(l, 5, 0);
    int16_t mid = (a0 + a1 + (a1 < a0 ? 360 : 0)) / 2 % 360;
    lv_obj_align(l, LV_ALIGN_CENTER, (lv_trigo_cos(mid) * 202) >> LV_TRIGO_SHIFT,
                 (lv_trigo_sin(mid) * 202) >> LV_TRIGO_SHIFT);

    s_gauge[g] = (gauge_t){ arc, l };
}

// ⚫ MODIFIÉ (25/10/2026) : alert_high : la jauge des besoins clignote quand elle est HAUTE
static void gauge_set(int g, float v, bool blink_phase, bool alert_high)
{
    lv_arc_set_value(s_gauge[g].arc, (int16_t)(v + 0.5f));
    bool low = alert_high ? v >= 70 : v < LOW_STAT;
    lv_obj_set_style_opa(s_gauge[g].icon, (low && blink_phase) ? LV_OPA_20 : LV_OPA_COVER, 0);
}

static void show_msg(const char *txt)
{
    lv_label_set_text(s_bubble_lbl, txt);
    lv_anim_del(s_bubble, anim_opa);
    lv_obj_clear_flag(s_bubble, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(s_bubble, LV_OPA_COVER, 0);
    s_msg_t0 = lv_tick_get();
    s_msg_on = true;
}

static void info_refresh(void)
{
    const pet_t *p = pet_get();
    uint32_t age = p->age_s;
    char age_txt[24];
    if (age < 3600)            snprintf(age_txt, sizeof(age_txt), "%lu min", (unsigned long)(age / 60));
    else if (age < 24 * 3600)  snprintf(age_txt, sizeof(age_txt), "%lu h %02lu", (unsigned long)(age / 3600),
                                        (unsigned long)(age % 3600 / 60));
    else                       snprintf(age_txt, sizeof(age_txt), "%lu j %lu h", (unsigned long)(age / 86400),
                                        (unsigned long)(age % 86400 / 3600));
    // icône WiFi : verte = connecté au WiFi de la maison, orange = connexion en cours, grise = pas de WiFi maison
    wifi_cfg_status_t ws;
    wifi_cfg_get_status(&ws);
    const char *wcol = ws.sta_connected ? "50E07A" : ws.sta_configured ? "FFB040" : "9AA0B8";
    // ⚫ MODIFIÉ (25/10/2026) : nom choisi à l'adoption (CONFIG_PET_NAME RETIRÉ)
    // ⚫ AJOUTÉ (25/10/2026) : météo réelle (icône + température) et heure locale
    char wx_txt[40] = "";
    weather_t w;
    weather_get(&w);
    char clk[8];
    bool has_clk = weather_clock(clk, sizeof(clk));
    if (w.valid) {
        bool night = weather_tod() == TOD_NIGHT;
        const char *ic = w.kind == WX_CLEAR ? (night ? ICON_SLEEP : ICON_SUN) : w.kind == WX_PARTLY ? (night ? ICON_CLOUDMOON : ICON_CLOUDSUN)
                       : w.kind == WX_FOG ? ICON_FOG : w.kind == WX_DRIZZLE ? ICON_RAIN : w.kind == WX_RAIN ? ICON_SHOWER
                       : w.kind == WX_SNOW ? ICON_SNOW : w.kind == WX_STORM ? ICON_ENERGY : ICON_CLOUD;
        snprintf(wx_txt, sizeof(wx_txt), "  %s %d°%s%s", ic, (int)lroundf(w.temp), has_clk ? " " : "", has_clk ? clk : "");
    } else if (has_clk) {
        snprintf(wx_txt, sizeof(wx_txt), "  %s", clk);
    }
    if (s_adopted) {
        // lv_label_set_text_fmt(s_info, "%s  ·  %s %s  #%s " ICON_WIFI "#", pet_name(), pet_stage_text(pet_stage()),
        //                       age_txt, wcol);
        // lv_label_set_text_fmt(s_info, "%s · %s%s  #%s " ICON_WIFI "#", pet_name(), age_txt, wx_txt, wcol);
        lv_label_set_text_fmt(s_info, "%s · %s%s  #%s " ICON_WIFI "#", pet_name(), pet_stage_text(pet_stage()),
                              wx_txt, wcol);   // ⚫ MODIFIÉ (25/10/2026) : âge (bébé, adolescent, adulte)
        (void)age_txt;
    } else {
        lv_label_set_text_fmt(s_info, "En attente d'adoption  #%s " ICON_WIFI "#", wcol);
    }

    // bulles WiFi (connexion établie)
    static int last_conn = -1;
    if (last_conn >= 0 && ws.sta_connected != last_conn && !s_msg_on && s_adopted) {
        char t[80];
        if (ws.sta_connected) snprintf(t, sizeof(t), "WiFi connecté : %s", ws.sta_ip);
        else snprintf(t, sizeof(t), "WiFi %s perdu, reconnexion...", ws.sta_ssid);
        show_msg(t);
    }
    last_conn = ws.sta_connected;
    // ⚫ RETIRÉ (25/10/2026) : bulle "Pour me configurer : WiFi …" : remplacée par l'écran QR code

    if (s_msg_on && lv_tick_elaps(s_msg_t0) > MSG_MS) {
        s_msg_on = false;
        fade(s_bubble, 255, 0, 500);
    }
}

/* ---------- ⚫ AJOUTÉ (25/10/2026) : écran QR code (rejoindre le réseau de la carte) ---------- */

static void qr_show(bool adoption)
{
    static const char qr_data[] = "WIFI:T:nopass;S:" CONFIG_PET_WIFI_AP_SSID ";;";
    // lv_qrcode_update(s_qr, qr_data, strlen(qr_data));
    wifi_cfg_status_t ws;
    wifi_cfg_get_status(&ws);
    // ⚫ MODIFIÉ (25/10/2026) : sur le WiFi de la maison, le QR code ouvre directement la page (http://<IP>/)
    static char url[40];
    snprintf(url, sizeof(url), "http://%s/", ws.sta_ip);
    const char *qd = (!adoption && ws.sta_connected) ? url : qr_data;
    lv_qrcode_update(s_qr, qd, strlen(qd));
    if (adoption) {
        lv_label_set_text(s_qr_title, "Adopte ton animal !");
        lv_label_set_text(s_qr_text, "Scanne le QR code avec ton téléphone :\nla page s'ouvre, choisis son nom et son espèce.");
    } else {
        lv_label_set_text(s_qr_title, ICON_WIFI "  Rejoins-moi");
        // if (ws.sta_connected) lv_label_set_text_fmt(s_qr_text, "Réseau " CONFIG_PET_WIFI_AP_SSID "\nou http://%s\nAppuie pour fermer", ws.sta_ip);
        if (ws.sta_connected) lv_label_set_text_fmt(s_qr_text, "Téléphone sur le WiFi de la maison :\nhttp://%s\nAppuie pour fermer", ws.sta_ip);   // ⚫ 25/10/2026
        else lv_label_set_text(s_qr_text, "Réseau " CONFIG_PET_WIFI_AP_SSID "\nla page s'ouvre toute seule\nAppuie pour fermer");
    }
    show(s_qr_panel, true);
    s_qr_on = true;
}

static void qr_hide(void)
{
    show(s_qr_panel, false);
    s_qr_on = false;
}

static void qr_build(void)
{
    s_qr_panel = lv_obj_create(s_scr);
    lv_obj_remove_style_all(s_qr_panel);
    lv_obj_set_size(s_qr_panel, 480, 480);
    lv_obj_set_style_bg_color(s_qr_panel, lv_color_hex(0x0A0E1E), 0);
    lv_obj_set_style_bg_opa(s_qr_panel, LV_OPA_80, 0);
    lv_obj_clear_flag(s_qr_panel, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *card = lv_obj_create(s_qr_panel);
    lv_obj_remove_style_all(card);
    lv_obj_set_size(card, 236, 236);
    lv_obj_set_style_bg_color(card, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 20, 0);
    lv_obj_align(card, LV_ALIGN_CENTER, 0, -4);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    s_qr = lv_qrcode_create(card, 200, lv_color_hex(0x10142A), lv_color_white());
    lv_obj_center(s_qr);

    s_qr_title = lv_label_create(s_qr_panel);
    lv_obj_set_style_text_font(s_qr_title, &font_fr_28, 0);
    lv_obj_set_style_text_color(s_qr_title, lv_color_white(), 0);
    lv_obj_align(s_qr_title, LV_ALIGN_TOP_MID, 0, 72);
    s_qr_text = lv_label_create(s_qr_panel);
    lv_obj_set_width(s_qr_text, 300);
    lv_label_set_long_mode(s_qr_text, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(s_qr_text, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(s_qr_text, &font_fr_20, 0);
    lv_obj_set_style_text_color(s_qr_text, lv_color_hex(0xD8DCF0), 0);
    lv_obj_align(s_qr_text, LV_ALIGN_BOTTOM_MID, 0, -62);
    show(s_qr_panel, false);
}

/* ---------- menus ---------- */

// ⚫ AJOUTÉ (25/10/2026) : réglage du son gardé en NVS ("ui" / "snd")
static void sound_save(void)
{
    nvs_handle_t h;
    if (nvs_open("ui", NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_u8(h, "snd", (uint8_t)((s_sound_on ? 0x10 : 0) | s_sound_vol));
    nvs_commit(h);
    nvs_close(h);
}
static void sound_load(void)
{
    nvs_handle_t h;
    uint8_t v;
    if (nvs_open("ui", NVS_READONLY, &h) != ESP_OK) return;
    if (nvs_get_u8(h, "snd", &v) == ESP_OK) { s_sound_on = v & 0x10; s_sound_vol = v & 0x0F; }
    if (s_sound_vol < 1 || s_sound_vol > 3) s_sound_vol = 2;
    nvs_close(h);
}

static void conn_build(void)
{
    snprintf(s_conn_txt[0], sizeof(s_conn_txt[0]), "%s  Son : %s", ICON_COMFORT, s_sound_on ? "oui" : "non");
    snprintf(s_conn_txt[1], sizeof(s_conn_txt[1]), "%s  Volume : %d / 3", ICON_ENERGY, s_sound_vol);
    ai_cfg_t c;
    ai_get_cfg(&c);
    ai_status_t st;
    ai_get_status(&st);
    snprintf(s_conn_txt[2], sizeof(s_conn_txt[2]), "%s  IA : %s", ICON_STAR,
             c.provider == AI_OFF ? "non" : st.ok ? "active" : st.calls ? "erreur" : "en attente");
    MENU_CONN[0] = (menu_item_t){ ICON_QR "  WiFi", M_WIFI, 0 };
    MENU_CONN[1] = (menu_item_t){ s_conn_txt[0], M_SOUND, 0 };
    MENU_CONN[2] = (menu_item_t){ s_conn_txt[1], M_VOL, 0 };
    MENU_CONN[3] = (menu_item_t){ s_conn_txt[2], M_AIINFO, 0 };
    MENU_CONN[4] = (menu_item_t){ ICON_BACK "  Retour", M_BACK, 0 };
}

static void menu_set(const menu_item_t *items, int n)
{
    static char opts[400];
    opts[0] = '\0';
    for (int i = 0; i < n; i++) {
        if (i) strlcat(opts, "\n", sizeof(opts));
        strlcat(opts, items[i].txt, sizeof(opts));
    }
    s_menu = items;
    s_menu_n = n;
    s_menu_t0 = lv_tick_get();
    lv_roller_set_options(s_roller, opts, LV_ROLLER_MODE_INFINITE);
    lv_roller_set_selected(s_roller, 0, LV_ANIM_OFF);
}

/* ---------- rafraîchissement global (chaque seconde) ---------- */

static void refresh(void)
{
    static uint32_t phase, last_ask;
    phase++;
    const pet_t *p = pet_get();

    if (s_adopted) {
        int mood = pet_mood();
        if (mood != s_mood) {
            int old = s_mood;
            s_mood = mood;
            // l'animal change de comportement, sauf s'il est en train d'exécuter une action
            if (!s_in_action) {
                q_clear();
                idle_plan();
            }
            if (old >= 0 && !s_msg_on && mood != PET_MOOD_NEUTRAL && mood != PET_MOOD_HAPPY) {
                char t[64];
                snprintf(t, sizeof(t), "%s %s !", pet_name(), pet_mood_text(mood));
                show_msg(t);
            }
            if (mood == PET_MOOD_NAUGHTY) float_text("!", 0xFF4060, head_x(), head_y() - 20, 0, -30, 1200, 0);
            if (mood == PET_MOOD_NAUGHTY) snd("angry");   // ⚫ AJOUTÉ (25/10/2026)
            if (old >= 0) ai_trigger(false);   // ⚫ AJOUTÉ (25/10/2026) : l'IA réagit au changement d'humeur
        }
        // ⚫ AJOUTÉ (25/10/2026) : bulle de pensée selon son état (toutes les ~14 s, pas pendant une action)
        static uint32_t last_thought;
        if (!s_in_action && !p->sleeping && lv_tick_elaps(last_thought) > 14000) {
            last_thought = lv_tick_get();
            switch (mood) {
            case PET_MOOD_HUNGRY:  thought(ICON_FEED, 0xE07A20); snd("call"); break;   // ⚫ 25/10/2026 : + voix
            case PET_MOOD_THIRSTY: thought(ICON_DRINK, 0x2A9AE8); break;
            case PET_MOOD_POTTY:   thought(ICON_POTTY, 0x9A6A30); snd("whine"); break;
            case PET_MOOD_TIRED:   thought(ICON_SLEEP, 0x6A70C8); break;
            case PET_MOOD_SAD:     thought(ICON_RAIN, 0x6A8AA8); break;
            case PET_MOOD_SICK:    thought(ICON_HEALTH, 0xD83A3A); snd("cough"); break;
            case PET_MOOD_HAPPY:   if (rnd(0, 2) == 0) thought(ICON_HEART, 0xFF4A8A); break;
            default: break;
            }
        }
        // ⚫ AJOUTÉ (25/10/2026) : sons d'ambiance : il ronfle en dormant, gémit en pleurant
        static uint32_t last_amb;
        if (lv_tick_elaps(last_amb) > 6000) {
            last_amb = lv_tick_get();
            if (p->sleeping) snd("snore");
            else if (mood == PET_MOOD_CRYING) snd("whine");
        }
        // ⚫ AJOUTÉ (25/10/2026) : demande de sortie répétée tant qu'on ne l'emmène pas
        if (mood == PET_MOOD_POTTY && lv_tick_elaps(last_ask) > 20000 && !s_msg_on) {
            last_ask = lv_tick_get();
            char t[80];
            snprintf(t, sizeof(t), "%s veut sortir faire ses besoins ! (Besoins)", pet_name());
            show_msg(t);
            float_text("!", 0xFFB020, head_x(), head_y() - 20, 0, -30, 1200, 0);
        }
        pet_led_set_state(mood, pet_wellbeing());
    } else {
        pet_led_set_state(PET_MOOD_NEUTRAL, 30);
    }
    if (pet_mess_version() != s_mess_ver) sync_mess();
    // ⚫ AJOUTÉ (25/10/2026) : le gazon repousse sur les trous rebouchés (mise à jour toutes les 20 s)
    static uint32_t last_regrow;
    if (lv_tick_elaps(last_regrow) > 20000) { last_regrow = lv_tick_get(); sync_mess(); }

    gauge_set(G_FOOD, p->s.food, phase & 1, false);
    gauge_set(G_WATER, p->s.water, phase & 1, false);
    gauge_set(G_ENERGY, p->s.energy, phase & 1, false);
    gauge_set(G_POTTY, p->s.potty, phase & 1, true);
    gauge_set(G_JOY, p->s.joy, phase & 1, false);
    gauge_set(G_DISC, p->s.discipline, phase & 1, false);
    gauge_set(G_HEALTH, p->s.health, phase & 1, false);
    info_refresh();
}

/* ---------- ⚫ AJOUTÉ (25/10/2026) : copie de la scène pour la page web ---------- */

// slot = rang de l'objet (identifiant stable : la page lisse les déplacements entre deux copies)
static size_t snap_obj(char *b, size_t o, size_t n, lv_obj_t *c, int slot)
{
    if (lv_obj_has_flag(c, LV_OBJ_FLAG_HIDDEN)) return o;
    int opa = lv_obj_get_style_opa(c, 0);
    if (opa < 8) return o;
    int x = lv_obj_get_x(c) + lv_obj_get_style_translate_x(c, 0);
    int y = lv_obj_get_y(c) + lv_obj_get_style_translate_y(c, 0);
    if (lv_obj_check_type(c, &lv_img_class)) {
        bool left;
        int idx = pet_asset_index_of(lv_img_get_src(c), &left);
        if (idx < 0) return o;
        o += snprintf(b + o, n - o, "%s[%d,%d,%d,%d,%d,%d]", o && b[o - 1] != '[' ? "," : "", slot, idx, left, x, y, opa);
    } else if (lv_obj_check_type(c, &lv_label_class)) {   // effets flottants (cœur, étoile, Z, !, ?)
        const char *t = lv_label_get_text(c);
        // const char *k = !strcmp(t, ICON_HEART) ? "heart" : !strcmp(t, ICON_STAR) ? "star" : t;
        // ⚫ MODIFIÉ (25/10/2026) : + bulles de pensée et perte de santé
        const char *k = !strcmp(t, ICON_HEART) ? "heart" : !strcmp(t, ICON_STAR) ? "star" : !strcmp(t, ICON_FEED) ? "feed"
                      : !strcmp(t, ICON_DRINK) ? "drink" : !strcmp(t, ICON_POTTY) ? "potty" : !strcmp(t, ICON_SLEEP) ? "tired"
                      : !strcmp(t, ICON_RAIN) ? "sad" : !strcmp(t, ICON_HEALTH) ? "sick" : !strcmp(t, ICON_CLOUD) ? "grumpy"
                      : !strncmp(t, ICON_HEALTH " ", 4) ? "hurt" : t;
        if (strlen(k) > 6) return o;
        lv_color32_t col;
        col.full = lv_color_to32(lv_obj_get_style_text_color(c, 0));
        o += snprintf(b + o, n - o, "%s[%d,\"%s\",%d,%d,%d,\"#%02x%02x%02x\"]", o && b[o - 1] != '[' ? "," : "",
                      slot, k, x + 20, y + 28, opa, col.ch.red, col.ch.green, col.ch.blue);
    }
    return o < n ? o : n - 1;
}

static void snap_build(void)
{
    static char b[sizeof(s_snap)];
    size_t n = sizeof(b) - 200, o = 0;
    // o += snprintf(b + o, n - o, "{\"night\":%d,\"adopted\":%d,\"o\":[", s_night, s_adopted);
    // ⚫ MODIFIÉ (25/10/2026) : + brouillard, éclair, animal crotté (calques non image)
    // o += snprintf(b + o, n - o, "{\"night\":%d,\"adopted\":%d,\"fog\":%d,\"flash\":%d,\"mud\":%d,\"o\":[", s_night, s_adopted,
    // ⚫ MODIFIÉ (25/10/2026) : + son à jouer (numéro, sorte), espèce, volume
    o += snprintf(b + o, n - o, "{\"night\":%d,\"adopted\":%d,\"fog\":%d,\"flash\":%d,\"mud\":%d,"
                  "\"snd\":[%lu,\"%s\"],\"sp\":%d,\"vol\":%d,\"son\":%d,\"o\":[", s_night, s_adopted,
                  !lv_obj_has_flag(s_fog, LV_OBJ_FLAG_HIDDEN),
                  lv_obj_has_flag(s_flash, LV_OBJ_FLAG_HIDDEN) ? 0 : lv_obj_get_style_opa(s_flash, 0),
                  lv_tick_elaps(s_muddy_until) > 0x80000000u, (unsigned long)s_snd_seq, s_snd_kind, s_species,
                  s_sound_vol, s_sound_on);
    uint32_t cnt = lv_obj_get_child_cnt(s_scr);
    for (uint32_t i = 0; i < cnt && o < n; i++) {
        lv_obj_t *c = lv_obj_get_child(s_scr, i);
        if (c == s_fx) {
            for (uint32_t j = 0; j < lv_obj_get_child_cnt(s_fx) && o < n; j++) {
                lv_obj_t *e = lv_obj_get_child(s_fx, j);
                o = snap_obj(b, o, n, e, 1000 + ((uintptr_t)e >> 3) % 100000);   // effet : identifiant d'objet
            }
        } else if (lv_obj_check_type(c, &lv_img_class)) {
            o = snap_obj(b, o, n, c, (int)i);
        }
    }
    o += snprintf(b + o, sizeof(b) - o, "],\"msg\":\"");
    if (s_msg_on) {   // texte de la bulle, échappé
        for (const char *t = lv_label_get_text(s_bubble_lbl); *t && o < sizeof(b) - 8; t++) {
            if (*t == '"' || *t == '\\') b[o++] = '\\';
            if ((unsigned char)*t >= 0x20) b[o++] = *t;
        }
    }
    o += snprintf(b + o, sizeof(b) - o, "\"}");
    portENTER_CRITICAL(&s_snap_mux);
    memcpy(s_snap, b, o + 1);
    s_snap_len = o;
    portEXIT_CRITICAL(&s_snap_mux);
}

size_t pet_ui_scene_json(char *out, size_t n)
{
    portENTER_CRITICAL(&s_snap_mux);
    size_t l = s_snap_len < n - 1 ? s_snap_len : n - 1;
    memcpy(out, s_snap, l);
    out[l] = '\0';
    portEXIT_CRITICAL(&s_snap_mux);
    return l;
}

void pet_ui_request_adopt(const char *name, int species)
{
    portENTER_CRITICAL(&s_req_mux);
    strlcpy(s_req_name, name, sizeof(s_req_name));
    s_req_species = species;
    s_req = 1;
    portEXIT_CRITICAL(&s_req_mux);
}

void pet_ui_request_reset(void)
{
    s_req = 2;
}

// ⚫ AJOUTÉ (25/10/2026) : action demandée depuis la page web (exécutée par la tâche LVGL)
static volatile int s_req_act = -1;
bool pet_ui_request_action(int act)
{
    if (!s_adopted || act < 0 || act >= PET_ACT_COUNT) return false;
    bool game = false;
    for (size_t i = 0; i < LEN(GAMES_ALL); i++) if ((int)GAMES_ALL[i].act == act) game = true;
    if (game && !(pet_games_mask(s_species) & (1u << act))) return false;   // pas un jeu de cette espèce
    s_req_act = act;
    return true;
}

// Liste des actions possibles (menu principal + jeux de l'espèce) : [{"a":0,"t":"Manger","g":0}, …]
size_t pet_ui_actions_json(char *out, size_t n)
{
    size_t o = snprintf(out, n, "[");
    const int sp = s_species;
    for (int g = 0; g < 2; g++) {
        const menu_item_t *m = g ? GAMES_ALL : MENU_MAIN;
        size_t cnt = g ? LEN(GAMES_ALL) : LEN(MENU_MAIN);
        for (size_t i = 0; i < cnt && o + 60 < n; i++) {
            if (m[i].kind != M_ACT) continue;
            if (g && !(pet_games_mask(sp) & (1u << m[i].act))) continue;
            const char *t = strstr(m[i].txt, "  ");   // après l'icône
            o += snprintf(out + o, n - o, "%s{\"a\":%d,\"t\":\"%s\",\"g\":%d}", o > 1 ? "," : "", (int)m[i].act,
                          t ? t + 2 : m[i].txt, g);
        }
    }
    o += snprintf(out + o, n - o, "]");
    return o;
}

/* ---------- ⚫ AJOUTÉ (25/10/2026) : adoption / réinitialisation ---------- */

static void animal_enter(bool hello)
{
    s_adopted = true;
    s_species = pet_get()->species;
    // pet_assets_load_species(s_species);
    s_stage = pet_stage();                      // ⚫ MODIFIÉ (25/10/2026) : images de son âge
    pet_assets_load(s_species, s_stage);
    s_last_pose = -1;   // nouvelle espèce : l'image doit être reposée
    // ⚫ AJOUTÉ (25/10/2026) : menu Jeux = jeux de cette espèce
    uint32_t mask = pet_games_mask(s_species);
    s_games_n = 0;
    for (size_t i = 0; i < LEN(GAMES_ALL) && s_games_n < LEN_GAMES_MAX - 1; i++)
        if (mask & (1u << GAMES_ALL[i].act)) MENU_GAMES[s_games_n++] = GAMES_ALL[i];
    MENU_GAMES[s_games_n++] = (menu_item_t){ ICON_BACK "  Retour", M_BACK, 0 };
    s_dog.x = HOME_X; s_dog.y = HOME_Y; s_dog.face = 1;
    // if (pet_get()->sleeping) { s_dog.x = BED_X; s_dog.y = BED_Y; set_night(true); }
    if (pet_get()->sleeping) { s_dog.x = BED_X; s_dog.y = BED_Y; s_force_night = true; scene_update(); }
    show(s_dog_img, true);
    show(s_roller, true);
    qr_hide();
    s_mood = -1;
    q_clear();
    sync_mess();
    if (hello) {
        char t[64];
        if (pet_get()->age_s == 0) snprintf(t, sizeof(t), "%s ! Je suis %s.", pet_sound(), pet_name());
        else snprintf(t, sizeof(t), "%s est content de te revoir !", pet_name());
        show_msg(t);
        pet_led_react(PET_REACT_WAKE);
    }
}

static void animal_leave(void)
{
    s_adopted = false;
    q_clear();
    props_reset();
    show(s_dog_img, false);
    show(s_roller, false);
    // set_night(false);
    s_force_night = false;   // ⚫ MODIFIÉ (25/10/2026)
    scene_update();
    sync_mess();
    qr_show(true);
}

static void do_action(pet_action_t act);
static void requests_handle(void)
{
    // ⚫ AJOUTÉ (25/10/2026) : action lancée depuis la page web
    int act = s_req_act;
    if (act >= 0) {
        s_req_act = -1;
        if (s_adopted && !s_qr_on) do_action((pet_action_t)act);
    }
    int req = s_req;
    if (!req) return;
    char name[17];
    int species;
    portENTER_CRITICAL(&s_req_mux);
    strlcpy(name, s_req_name, sizeof(name));
    species = s_req_species;
    s_req = 0;
    portEXIT_CRITICAL(&s_req_mux);
    if (req == 1 && !s_adopted && pet_adopt(name, species)) {
        animal_enter(true);
    } else if (req == 2) {
        pet_reset_all();
        animal_leave();
    }
}

/* ---------- minuteries ---------- */

// Chaque seconde : le temps passe pour l'animal
// ⚫ AJOUTÉ (25/10/2026) : fenêtre "santé" (4,5 s) : jauge, points perdus et raison, + croix rouge qui s'envole
// ⚫ MODIFIÉ (25/10/2026) : fenêtre retirée (voir health_popup), hp_hide_cb n'est plus utilisé
__attribute__((unused)) static void hp_hide_cb(lv_timer_t *t)
{
    fade(s_hp, 255, 0, 500);
    lv_timer_del(t);
}

// ⚫ AJOUTÉ (25/10/2026) : décision de l'IA : phrase dans la bulle + comportement, seulement quand les règles
// le permettent (pas pendant une action, ni quand il a faim, soif, besoin de sortir, est malade, fait une bêtise
// ou dort ; sous la pluie il reste à l'abri ; fatigué, il ne fait que se reposer ou observer)
// ⚫ AJOUTÉ (25/10/2026) : demande en cours (câlin, repas, eau, jeu, sortie) : si le maître y répond dans les
// 90 s, l'animal est ravi (bonus de bonheur)
static int s_ai_want = -1;        // 0 câlin, 1 manger, 2 boire, 3 jouer, 4 sortir
static uint32_t s_ai_want_t0;
static void ai_answered(pet_action_t act)
{
    if (s_ai_want < 0 || lv_tick_elaps(s_ai_want_t0) > 90000) { s_ai_want = -1; return; }
    bool game = act == PET_ACT_PLAY || (act >= PET_ACT_FETCH && act <= PET_ACT_BONE) || (act >= PET_ACT_BIRDS && act <= PET_ACT_GRAZE);
    bool ok = (s_ai_want == 0 && (act == PET_ACT_PET || act == PET_ACT_COMFORT)) || (s_ai_want == 1 && act == PET_ACT_FEED)
           || (s_ai_want == 2 && act == PET_ACT_DRINK) || (s_ai_want == 3 && game) || (s_ai_want == 4 && act == PET_ACT_POTTY);
    if (ok) pet_attention_bonus();
    s_ai_want = -1;
}

// static void want_feed(void)  { thought(ICON_FEED, 0xE07A20); } … (⚫ MODIFIÉ 25/10/2026 : + il appelle)
static void want_feed(void)  { thought(ICON_FEED, 0xE07A20); snd("call"); }
static void want_drink(void) { thought(ICON_DRINK, 0x2A9AE8); snd("call"); }
static void want_potty(void) { thought(ICON_POTTY, 0x9A6A30); snd("whine"); }
static void want_play(void)  { thought(ICON_BALL, 0x30A050); snd("happy"); }
static void want_hug(void)   { thought(ICON_PET, 0xFF4A8A); snd("soft"); }

static void ai_behave(const char *b, const char *phrase)
{
    int m = pet_mood();
    if (m == PET_MOOD_SLEEPING) return;
    if (phrase[0] && !s_msg_on) show_msg(phrase);
    // ⚫ AJOUTÉ (25/10/2026) : bêtises proposées par l'IA (refusées si l'état ne s'y prête pas)
    if (!strncmp(b, "betise_", 7)) {
        if (s_in_action || (m != PET_MOOD_NEUTRAL && m != PET_MOOD_HAPPY && m != PET_MOOD_SAD)) return;
        static uint32_t last_ai_naughty;
        if (last_ai_naughty && lv_tick_elaps(last_ai_naughty) < 10 * 60000) return;   // une au plus toutes les 10 min
        last_ai_naughty = lv_tick_get();
        // pet_start_naughty(!strcmp(b, "betise_gamelle") ? 1 : !strcmp(b, "betise_eau") ? 2 : 0);
        pet_start_naughty(!strcmp(b, "betise_gamelle") ? 1 : !strcmp(b, "betise_eau") ? 2 : !strcmp(b, "betise_besoins") ? 3
                          : !strcmp(b, "betise_cone") ? 4 : 0);   // ⚫ MODIFIÉ (25/10/2026)
        return;   // le changement d'humeur lance le script de la bêtise (refresh -> idle_plan)
    }
    // ⚫ AJOUTÉ (25/10/2026) : demandes au maître
    if (!strncmp(b, "reclamer_", 9) || !strcmp(b, "demander_sortir")) {
        const pet_t *p = pet_get();
        if (s_in_action || m == PET_MOOD_NAUGHTY || m == PET_MOOD_CRYING) return;
        weather_t w0;
        weather_get(&w0);
        bool rain = wx_bad(&w0);
        q_clear();
        if (!strcmp(b, "reclamer_manger") && p->s.food < 85) {
            if (rain) { GOTO(SHELTER_X, SHELTER_Y, false); } else { GOTO(SCENE_FOOD_X - 72, SCENE_FOOD_Y, false); }
            FACE(1); CALL(want_feed); POSE(AN_SAD, 9000); s_ai_want = 1;
        } else if (!strcmp(b, "reclamer_eau") && p->s.water < 85) {
            if (rain) { GOTO(SHELTER_X, SHELTER_Y, false); } else { GOTO(SCENE_WATER_X - 72, SCENE_WATER_Y, false); }
            FACE(1); CALL(want_drink); POSE(AN_SAD, 9000); s_ai_want = 2;
        } else if (!strcmp(b, "demander_sortir") && p->s.potty > 40) {
            GOTO(SCENE_TOILET_X - 96, SCENE_TOILET_Y + 26, false); FACE(1); CALL(want_potty); POSE(AN_SAD, 9000); s_ai_want = 4;
        } else if (!strcmp(b, "reclamer_jeu") && p->s.energy > 30 && !rain) {
            GOTO(HOME_X + rnd(-30, 30), 392, true); CALL(want_play); JUMP(); POSE(AN_WAG, 2500); JUMP(); POSE(AN_WAG, 4000); s_ai_want = 3;
        } else if (!strcmp(b, "reclamer_calin")) {   // vient tout devant, vers le maître, et attend une caresse
            if (rain) { GOTO(SHELTER_X, SHELTER_Y, false); } else { GOTO(HOME_X + rnd(-30, 30), 392, false); }
            CALL(want_hug); POSE(AN_SIT_HAPPY, 3000); CALL(want_hug); POSE(AN_SIT_HAPPY, 5000); s_ai_want = 0;
        } else {
            POSE(AN_CALM, 4000);
        }
        if (s_ai_want >= 0) s_ai_want_t0 = lv_tick_get();
        s_step_t0 = lv_tick_get();
        return;
    }
    bool free_mood = m == PET_MOOD_NEUTRAL || m == PET_MOOD_HAPPY || m == PET_MOOD_SAD || m == PET_MOOD_TIRED;
    if (!free_mood || s_in_action) return;
    weather_t w;
    weather_get(&w);
    bool calm_only = m == PET_MOOD_TIRED;
    if (calm_only && strcmp(b, "se_reposer") && strcmp(b, "observer") && strcmp(b, "s_abriter")) return;
    q_clear();
    if (!strcmp(b, "s_abriter") || wx_bad(&w)) {
        GOTO(SHELTER_X + rnd(-16, 12), SHELTER_Y, !sheltered() && wx_bad(&w)); FACE(1); POSE(AN_CALM, 15000);
    } else if (!strcmp(b, "renifler")) {
        GOTO(rnd(170, 330), rnd(326, 362), false); POSE(AN_EAT, rnd(3000, 5000)); POSE(AN_CALM, 5000);
    } else if (!strcmp(b, "jouer_seul")) {
        GOTO(rnd(300, 400), rnd(318, 350), true); JUMP(); GOTO(rnd(160, 260), rnd(330, 370), true); POSE(AN_WAG, 3000);
    } else if (!strcmp(b, "se_reposer")) {
        GOTO(BED_X + rnd(-8, 8), BED_Y, false); FACE(1); POSE(AN_LIE, 20000);
    } else if (!strcmp(b, "bouder")) {
        GOTO(rnd(330, 400), rnd(300, 330), false); FACE(1); POSE(AN_SAD, 12000);
    } else if (!strcmp(b, "reclamer")) {   // vient devant, vers le maître, et réclame de l'attention
        GOTO(HOME_X + rnd(-30, 30), 392, false); POSE(AN_SIT_HAPPY, 2500); JUMP(); POSE(AN_SIT_HAPPY, 4000);
    } else if (!strcmp(b, "observer")) {
        GOTO(rnd(200, 320), rnd(300, 332), false); POSE(AN_SIT, 10000);
    } else {                                // flâner
        // GOTO(rnd(170, 320), rnd(324, 370), false); POSE(AN_CALM, 8000);
        int wx, wy;
        pet_lawn_spot(&wx, &wy);   // ⚫ MODIFIÉ (25/10/2026) : partout sur la pelouse
        GOTO(wx, wy, false); POSE(AN_CALM, 8000);
    }
    s_step_t0 = lv_tick_get();
}

static void health_popup(int delta, const char *why)
{
    int h = (int)pet_get()->s.health;
    uint32_t col = h >= 60 ? 0x50E07A : h >= 30 ? 0xFFB040 : 0xFF4A4A;
    lv_label_set_text_fmt(s_hp_lbl, "#FF5A5A " ICON_HEALTH "#  Santé %d %%  #FF7070 (%d)#", h, delta);
    lv_label_set_text(s_hp_why, why);
    lv_bar_set_value(s_hp_bar, h, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_hp_bar, lv_color_hex(col), LV_PART_INDICATOR);
    lv_anim_del(s_hp, anim_opa);
    // lv_obj_set_style_opa(s_hp, LV_OPA_COVER, 0);
    // show(s_hp, true);
    // lv_obj_move_foreground(s_hp);
    // lv_timer_create(hp_hide_cb, 4500, NULL);
    // float_text(ICON_HEALTH, 0xFF4040, head_x(), head_y() - 6, 0, -60, 1600, 0);
    // ⚫ MODIFIÉ (25/10/2026) : plus de fenêtre encadrée de rouge (demande) : "♥ -6" qui s'élève au-dessus de
    // l'animal ; la raison est dans la bulle de dialogue, la jauge Santé montre le niveau
    static char t[24];
    snprintf(t, sizeof(t), ICON_HEALTH " %d", delta);
    lv_obj_t *l = lv_label_create(s_fx);
    lv_label_set_text(l, t);
    lv_obj_set_style_text_font(l, &font_fr_28, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(0xFF4A4A), 0);
    lv_obj_align(l, LV_ALIGN_TOP_LEFT, head_x() - 26, head_y() - 30);
    float_start(l, 0, -55, 0, 2200, 0);
    (void)col;
    if (!s_in_action) pet_led_react(PET_REACT_HURT);
    ESP_LOGW(TAG, "Santé %d (%d) : %s", h, delta, why);
}

static void tick_cb(lv_timer_t *t)
{
    requests_handle();
    // ⚫ AJOUTÉ (25/10/2026) : heure / météo réelles : fond, nuit, environnement de l'animal
    scene_update();
    weather_t w;
    weather_get(&w);
    if (w.valid && s_adopted) pet_set_env(weather_tod() == TOD_NIGHT, wx_bad(&w), sheltered());
    // ⚫ AJOUTÉ (25/10/2026) : la pluie commence : il file à l'abri tout de suite (même s'il se reposait dehors) ;
    // elle s'arrête : il sort et s'ébroue (gouttelettes)
    static int was_bad = -1;
    bool bad = wx_bad(&w);
    static uint32_t last_shelter_chk;
    if (s_adopted && !s_in_action && was_bad >= 0 && bad != (bool)was_bad) {
        q_clear();
        if (!bad && sheltered() && !pet_get()->sleeping) {
            POSE(AN_WAG, 600);
            for (int i = 0; i < 4; i++) float_text(ICON_DRINK, 0x8AD0FF, head_x() - 30 + i * 18, head_y() + 10, (i - 2) * 14, -30, 700, i * 60);
            POSE(AN_WAG, 700);
        } else {
            idle_plan();
        }
        s_step_t0 = lv_tick_get();
    } else if (s_adopted && !s_in_action && bad && !sheltered() && lv_tick_elaps(last_shelter_chk) > 5000) {
        last_shelter_chk = lv_tick_get();
        int m = pet_mood();
        if (m != PET_MOOD_SLEEPING && m != PET_MOOD_NAUGHTY && s_q[s_qi].k == S_POSE) { q_clear(); idle_plan(); }
    }
    was_bad = bad;
    if (pet_take_auto_potty() && s_adopted) {   // il est allé seul au coin besoins
        q_clear();
        props_reset();
        s_in_action = true;
        GOTO(potty_x(), POTTY_Y, false); FACE(1); POSE(AN_POTTY, 2400); CALL(sync_mess); POSE(AN_WAG, 800); CALL(action_done);
        s_step_t0 = lv_tick_get();
    }
    if (pet_tick(1)) {
        show_msg(pet_event_text());
        ESP_LOGI(TAG, "%s", pet_event_text());
    }
    // ⚫ AJOUTÉ (25/10/2026) : il grandit : nouvelles images (proportions de l'âge), petite fête
    if (s_adopted && (int)pet_stage() != s_stage) {
        s_stage = pet_stage();
        pet_assets_load(s_species, s_stage);
        s_last_pose = -1;
        static char grown[96];
        snprintf(grown, sizeof(grown), "%s a grandi : %s !", pet_name(),
                 s_stage == PET_STAGE_CHILD ? "c'est un adolescent" : "c'est maintenant un adulte");
        show_msg(grown);
        for (int i = 0; i < 5; i++) float_text(ICON_STAR, 0xFFE070, head_x() - 40 + i * 20, head_y(), 0, -60, 1400, i * 150);
        ESP_LOGI(TAG, "%s", grown);
    }
    // ⚫ AJOUTÉ (25/10/2026) : décision de l'IA
    char ai_b[24], ai_p[100];
    if (s_adopted && ai_take(ai_b, sizeof(ai_b), ai_p, sizeof(ai_p))) ai_behave(ai_b, ai_p);
    // ⚫ AJOUTÉ (25/10/2026) : perte de santé (puni / grondé sans raison, bêtise, accident) : on la montre
    static uint32_t hseq;
    int hd;
    const char *why;
    uint32_t sq = pet_health_event(&hd, &why);
    if (sq != hseq) {
        hseq = sq;
        if (s_adopted) health_popup(hd, why);
    }
    // retour automatique au menu principal après 30 s dans le menu Jeux
    // if (s_menu == MENU_GAMES && lv_tick_elaps(s_menu_t0) > 30000) menu_set(MENU_MAIN, LEN(MENU_MAIN));
    if ((s_menu == MENU_GAMES || s_menu == MENU_CONN) && lv_tick_elaps(s_menu_t0) > 30000) menu_set(MENU_MAIN, LEN(MENU_MAIN));   // ⚫ 25/10/2026
    refresh();
}

// ~30 fois par seconde : animal, oiseaux, papillon, lucioles, Zzz, trou, main
static void frame_cb(lv_timer_t *t)
{
    if (s_req_act >= 0) requests_handle();   // ⚫ AJOUTÉ (25/10/2026) : bouton de la page web : tout de suite
    static uint32_t last, next_z, next_clod, fly_t0, next_heart, next_birds = 45000, last_snap;
    static float fly_x = 300, fly_y = 300, fly_tx = 300, fly_ty = 300;
    static float ff_x[6], ff_y[6], ff_vx[6], ff_vy[6];
    static bool ff_init;
    uint32_t now = lv_tick_get();
    float dt = last ? lv_tick_elaps(last) / 1000.0f : 0;
    if (dt > 0.1f) dt = 0.1f;
    last = now;

    if (s_adopted) {
        dog_update(dt);
        dog_render();
        pet_set_pos((int)s_dog.x, (int)s_dog.y);   // ⚫ AJOUTÉ (25/10/2026) : lieu des accidents / bêtises
        proj_update(now);
        carry_update();
        hand_update();
        gesture_update(now);   // ⚫ AJOUTÉ (25/10/2026)
        // ⚫ AJOUTÉ (25/10/2026) : sous l'abri : toit et poteaux avant dessinés devant lui
        show(s_shelter, sheltered());
        // ⚫ AJOUTÉ (25/10/2026) : teinte : crotté après la boue, bleutée la nuit
        static int tint = -1;
        int want = lv_tick_elaps(s_muddy_until) > 0x80000000u ? 2 : s_night ? 1 : 0;
        if (want != tint) {
            tint = want;
            lv_obj_set_style_img_recolor(s_dog_img, lv_color_hex(want == 2 ? 0x5A3A1E : 0x0A1440), 0);
            lv_obj_set_style_img_recolor_opa(s_dog_img, want == 2 ? LV_OPA_40 : want ? LV_OPA_50 : LV_OPA_TRANSP, 0);
        }

        // dort : Zzz qui s'envolent de la litière
        if (s_mood == PET_MOOD_SLEEPING && s_dog.anim == &AN_SLEEP && now >= next_z) {
            next_z = now + 1400;
            float_text((esp_random() & 1) ? "Z" : "z", 0xB8C4FF, head_x(), head_y() + 40, 40, -80, 2600, 0);
        }
        // creuse : mottes de terre (le trou apparaît en fondu, cf. sync_mess)
        if (s_dog.anim == &AN_DIG && now >= next_clod) { next_clod = now + rnd(120, 260); clod_spawn(); }
        static uint32_t next_dig_snd;   // ⚫ AJOUTÉ (25/10/2026) : bruit de grattage (une fois par seconde au plus)
        if (s_dog.anim == &AN_DIG && now >= next_dig_snd) { next_dig_snd = now + 1100; snd_dig(); }
        // ⚫ AJOUTÉ (25/10/2026) : heureux : juste un cœur de temps en temps
        if (s_mood == PET_MOOD_HAPPY && !s_in_action && now >= next_heart) { next_heart = now + 4000; heart(); }
        // ⚫ AJOUTÉ (25/10/2026) : des oiseaux viennent picorer de temps en temps ; l'animal joueur leur court après
        // if (!s_night && now >= next_birds) {
        weather_t ww;
        weather_get(&ww);
        if (!s_night && !wx_bad(&ww) && now >= next_birds) {   // ⚫ 25/10/2026 : pas d'oiseaux sous la pluie
            next_birds = now + rnd(45000, 90000);
            float bx, by;
            if (!birds_present(&bx, &by)) birds_spawn();
        }
        float bx, by;
        if (!s_in_action && (s_mood == PET_MOOD_HAPPY || s_mood == PET_MOOD_NEUTRAL) && birds_present(&bx, &by) &&
            s_q[s_qi].k == S_POSE && rnd(0, 60) == 0) {
            q_clear();
            s_in_action = true;
            CHASE(); POSE(AN_SIT_HAPPY, 1200); CALL(action_done);
        }
    }
    birds_update(now, dt);
    weather_fx(now, dt);   // ⚫ AJOUTÉ (25/10/2026)

    if (!lv_obj_has_flag(s_fly, LV_OBJ_FLAG_HIDDEN)) {   // papillon : vole d'un point à l'autre en battant des ailes
        float dx = fly_tx - fly_x, dy = fly_ty - fly_y;
        if (dx * dx + dy * dy < 16 || !fly_t0) {
            fly_t0 = now;
            fly_tx = rnd(60, 420);
            fly_ty = rnd(200, 330);
        }
        float d = sqrtf(dx * dx + dy * dy) + 0.01f;
        fly_x += dx / d * 40 * dt + sinf(now / 300.0f) * 0.6f;
        fly_y += dy / d * 40 * dt + cosf(now / 170.0f) * 0.9f;
        img_set(s_fly, pet_asset(((now / 90) & 1) ? A_fly_1 : A_fly_2), (int)fly_x, (int)fly_y);
    }
    if (s_night) {    // lucioles qui dérivent et scintillent
        for (int i = 0; i < 6; i++) {
            if (!ff_init) { ff_x[i] = rnd(40, 440); ff_y[i] = rnd(220, 380); }
            ff_vx[i] += (rnd(-100, 100) / 100.0f) * 30 * dt;
            ff_vy[i] += (rnd(-100, 100) / 100.0f) * 30 * dt;
            ff_vx[i] *= 0.97f; ff_vy[i] *= 0.97f;
            ff_x[i] += ff_vx[i] * dt * 10; ff_y[i] += ff_vy[i] * dt * 10;
            if (ff_x[i] < 40 || ff_x[i] > 440) ff_vx[i] = -ff_vx[i];
            if (ff_y[i] < 210 || ff_y[i] > 390) ff_vy[i] = -ff_vy[i];
            img_at(s_firefly[i], A_firefly, (int)ff_x[i], (int)ff_y[i]);
            lv_obj_set_style_opa(s_firefly[i], (lv_opa_t)(150 + 105 * sinf(now / 400.0f + i * 1.7f)), 0);
        }
        ff_init = true;
    }

    if (lv_tick_elaps(last_snap) >= SNAP_MS) { last_snap = now; snap_build(); }
}

/* ---------- nuages ---------- */

static void cloud_x(void *var, int32_t v)
{
    lv_obj_set_x(var, v);
}

static void cloud_start(lv_obj_t *c, int y, uint32_t period, uint32_t delay)
{
    lv_obj_set_y(c, y);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, c);
    lv_anim_set_exec_cb(&a, cloud_x);
    lv_anim_set_values(&a, -140, 480);
    lv_anim_set_time(&a, period);
    lv_anim_set_delay(&a, delay);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}

/* ---------- actions ---------- */

static void do_action(pet_action_t act)
{
    // anti-rebond : un appui toutes les 500 ms au plus
    if (lv_tick_elaps(s_last_act_t) < 500) return;
    s_last_act_t = lv_tick_get();

    if (pet_get()->sleeping) act = PET_ACT_WAKE;   // pendant le sommeil, tout appui réveille

    const char *msg = NULL;
    pet_react_t r = pet_do(act, &msg);
    if (msg) show_msg(msg);
    if (msg) { ai_note_action(msg); ai_trigger(false); }   // ⚫ AJOUTÉ (25/10/2026) : l'IA réagit à l'action
    ai_answered(act);   // ⚫ AJOUTÉ (25/10/2026) : réponse à sa demande ?
    pet_led_react(r);

    // script de l'animal
    q_clear();
    props_reset();
    s_in_action = true;
    s_dog.jump = 0;
    s_dog.lap_done = 0;
    switch (r) {
    case PET_REACT_EAT:
        CALL(sync_mess);   // gamelle remise droite si elle était renversée
        // CALL(kibble_show); GOTO(bowl_spot_x(SCENE_FOOD_X), bowl_spot_y(SCENE_FOOD_Y), true); FACE(1); POSE(AN_EAT, 3200); CALL(kibble_hide);
        CALL(kibble_show); GOTO(bowl_spot_x(SCENE_FOOD_X), bowl_spot_y(SCENE_FOOD_Y), true); FACE(1); CALL(snd_eat); POSE(AN_EAT, 3200); CALL(kibble_hide);   // ⚫ 25/10/2026 : + son
        POSE(AN_WAG, 900);
        break;
    case PET_REACT_DRINK:
        CALL(sync_mess);
        // GOTO(bowl_spot_x(SCENE_WATER_X), bowl_spot_y(SCENE_WATER_Y), true); FACE(1); POSE(AN_DRINK, 2800); POSE(AN_WAG, 900);
        GOTO(bowl_spot_x(SCENE_WATER_X), bowl_spot_y(SCENE_WATER_Y), true); FACE(1); CALL(snd_drink); POSE(AN_DRINK, 2800); POSE(AN_WAG, 900);   // ⚫ 25/10/2026
        break;
    case PET_REACT_SLEEP:
        GOTO(BED_X, BED_Y, false); FACE(1); CALL(night_on); POSE(AN_SLEEP, 1200);
        break;
    case PET_REACT_WAKE:
        // CALL(night_off); POSE(AN_LIE, 900); POSE(AN_WAG, 1200);
        // ⚫ MODIFIÉ (25/10/2026) : réveillé avant d'être reposé : grognon (petit nuage), sinon content
        CALL(night_off); POSE(AN_LIE, 900);
        if (pet_get()->s.energy < 60) { CALL(emo_grumpy); POSE(AN_SAD, 1800); } else POSE(AN_WAG, 1200);
        break;
    case PET_REACT_JOY:
        if (act == PET_ACT_FETCH) {          // balle lancée loin : il court la chercher et la rapporte
            s_tx = rnd(350, 410);
            s_ty = rnd(300, 336);
            CALL(ball_throw); POSE(AN_WAG, 500);
            GOTO(s_tx - 46, s_ty + 2, true); FACE(1); POSE(AN_EAT, 350); CALL(grab_proj);
            GOTO(HOME_X, HOME_Y + 8, true); FACE(-1); POSE(AN_SIT_HAPPY, 500); CALL(drop_carry);
            POSE(AN_WAG, 1400); CALL(heart);
        } else if (act == PET_ACT_FRISBEE) { // frisbee qui plane : il court dessous et saute pour l'attraper
            s_tx = rnd(320, 400);
            s_ty = rnd(300, 330);
            CALL(frisbee_throw); POSE(AN_WAG, 400);
            GOTO(s_tx - 40, s_ty, true); FACE(1); JUMP_GRAB();
            GOTO(HOME_X, HOME_Y + 8, true); FACE(-1); CALL(drop_carry);
            JUMP(); POSE(AN_WAG, 1200); CALL(heart);
        } else if (act == PET_ACT_BONE) {    // couché, il ronge son os
            // CALL(bone_give); POSE(AN_LIE, 4500); CALL(drop_carry); POSE(AN_WAG, 800);
            bone_script();   // ⚫ MODIFIÉ (25/10/2026) : il joue avec, puis l'enterre (ou le déterre d'abord)
        } else if (act == PET_ACT_BIRDS) {   // ⚫ AJOUTÉ (25/10/2026) : les oiseaux arrivent, il les poursuit
            CALL(birds_spawn); POSE(AN_WAG, 1400); CHASE(); JUMP(); POSE(AN_SIT_HAPPY, 1200); CALL(heart);
        // ⚫ AJOUTÉ (25/10/2026) : jeux propres à certaines espèces
        } else if (act == PET_ACT_YARN) {     // pelote : il la poursuit et la fait rouler d'un coup de patte
            for (int i = 0; i < 3; i++) { s_tgt[i][0] = rnd(170, 390); s_tgt[i][1] = rnd(316, 356); }
            s_tgt_i = 0;
            CALL(yarn_roll); POSE(AN_WAG, 500);
            for (int i = 0; i < 3; i++) {
                GOTO(s_tgt[i][0] - 34, s_tgt[i][1] + 2, true); FACE(1); POSE(AN_PAW, 520);
                if (i < 2) CALL(yarn_roll);
            }
            CALL(yarn_hide); POSE(AN_SIT_HAPPY, 1000); CALL(heart);
        } else if (act == PET_ACT_FEATHER) {  // plume : il saute pour l'attraper
            CALL(wand_show); POSE(AN_PAW, 700); JUMP(); POSE(AN_PAW, 450); JUMP(); POSE(AN_PAW, 450); JUMP();
            CALL(wand_hide); POSE(AN_SIT_HAPPY, 900); CALL(heart);
        } else if (act == PET_ACT_HURDLE) {   // obstacle : élan, saut, réception
            CALL(hurdle_show); GOTO(PLAY_X - 130, PLAY_Y, true); FACE(1); POSE(AN_WAG, 400);
            GOTO(PLAY_X - 70, PLAY_Y, true); LEAP(PLAY_X + 70, PLAY_Y); GOTO(PLAY_X + 110, PLAY_Y, true);
            POSE(AN_WAG, 900); CALL(hurdle_hide); CALL(heart);
        } else if (act == PET_ACT_CARROT) {   // une main lui tend une carotte, il la croque
            FACE(1); CALL(carrot_show); POSE(AN_CALM, 1100); CALL(carrot_take); POSE(AN_CALM, 2600); CALL(carry_eat); CALL(heart);
        } else if (act == PET_ACT_MUD) {      // bain de boue : il se roule dedans (et reste crotté un moment)
            CALL(mud_show); GOTO(PLAY_X, PLAY_Y - 2, false); POSE(AN_MUD, 3600); CALL(mud_hide); POSE(AN_WAG, 900); CALL(heart);
        } else if (act == PET_ACT_CLIMB) {    // grimpe sur la botte de foin, fier, puis redescend
            CALL(hay_show); GOTO(PLAY_X - 84, PLAY_Y, false); FACE(1); LEAP(PLAY_X, PLAY_Y - 40);
            POSE(AN_WAG, 2400); LEAP(PLAY_X + 84, PLAY_Y + 4); CALL(hay_hide); CALL(heart);
        } else if (act == PET_ACT_BRUSH) {    // une main le brosse le long du dos
            CALL(brush_start); POSE(AN_LOVE, 3400); CALL(brush_stop); CALL(heart);
        } else if (act == PET_ACT_GRAZE) {    // une touffe d'herbe fraîche devant lui, il broute
            CALL(grass_show); POSE(AN_EAT, 3400); CALL(grass_hide); CALL(heart);
        } else if (act == PET_ACT_PLAY) {
            float sx, sy;
            track_pos((float)M_PI * 0.5f, &sx, &sy);   // départ : devant la piste
            CALL(ball_show); GOTO((int)sx, (int)sy, true); LAPS(); CALL(ball_hide);
            JUMP(); POSE(AN_WAG, 1200); CALL(heart);
        } else {
            JUMP(); POSE(AN_WAG, 1200); CALL(heart);
        }
        break;
    case PET_REACT_LOVE:   // ⚫ MODIFIÉ (25/10/2026) : une main vient le caresser, un seul cœur
        CALL(hand_start); POSE(AN_LOVE, 2800); CALL(hand_stop); CALL(heart); POSE(AN_WAG, 800);
        break;
    case PET_REACT_REFUSE:
        FACE(-s_dog.face); POSE(AN_CALM, 1400);
        float_text("?", 0xFFC040, head_x(), head_y() - 10, 10, -40, 1200, 0);
        break;
    case PET_REACT_SORRY:
    case PET_REACT_HURT:
        // ⚫ MODIFIÉ (25/10/2026) : gestes : gronder = main qui agite l'index au-dessus de lui ; punir = la main
        // montre le coin, il y va et attend, tête basse, que le sablier s'écoule (temps mort, sans violence)
        if (act == PET_ACT_PUNISH) {
            CALL(point_show); POSE(AN_GUILTY, 1200); CALL(point_hide);
            GOTO(CORNER_X, CORNER_Y, false); FACE(1); CALL(hglass_show); POSE(*(r == PET_REACT_HURT ? &AN_CRY : &AN_GUILTY), 4200);
            CALL(hglass_hide);
        } else {
            CALL(scold_show); POSE(*(r == PET_REACT_HURT ? &AN_CRY : &AN_GUILTY), 2200); CALL(scold_hide);
            CALL(*(r == PET_REACT_HURT ? &emo_confused : &emo_understood));   // ⚫ AJOUTÉ (25/10/2026)
        }
        if (r == PET_REACT_HURT) POSE(AN_CRY, 1500);
        break;
    case PET_REACT_POTTY:  // ⚫ MODIFIÉ (25/10/2026) : arrière-train pile au-dessus du gravier, les besoins y restent
        GOTO(potty_x(), POTTY_Y, false); FACE(1); POSE(AN_POTTY, 2600); CALL(sync_mess);
        POSE(AN_WAG, 900); CALL(heart);
        break;
    case PET_REACT_HEAL:   // ⚫ AJOUTÉ (25/10/2026) : médicament, une caresse, puis repos sur la litière
        CALL(hand_start); POSE(AN_SICK, 1800); CALL(hand_stop); CALL(heal_fx);
        GOTO(BED_X, BED_Y, false); FACE(1); POSE(AN_SICK, 2000);
        break;
    case PET_REACT_CLEAN:  // ⚫ AJOUTÉ (25/10/2026)
        CALL(clean_fx); POSE(AN_SIT_HAPPY, 1600); CALL(heart);
        break;
    default:
        break;
    }
    CALL(action_done);
    s_step_t0 = lv_tick_get();
    // ⚫ AJOUTÉ (25/10/2026) : voix / bruit selon la réaction
    switch (r) {
    case PET_REACT_JOY:    snd("happy"); break;
    case PET_REACT_LOVE:
    case PET_REACT_HEAL:   snd("soft"); break;
    case PET_REACT_REFUSE: snd("short"); break;
    case PET_REACT_SORRY:
    case PET_REACT_HURT:   snd("whine"); break;
    case PET_REACT_SLEEP:
    case PET_REACT_WAKE:   snd("yawn"); break;
    default: break;
    }
    refresh();
}

// Appui sur l'encodeur : le roller envoie VALUE_CHANGED (lv_roller.c, release_handler)
static void roller_cb(lv_event_t *e)
{
    lv_obj_t *roller = lv_event_get_target(e);
    uint16_t id = lv_roller_get_selected(roller);
    s_menu_t0 = lv_tick_get();
    // ⚫ MODIFIÉ (25/10/2026) : menus (principal / Jeux), écran QR
    if (s_qr_on) {                               // un appui ferme l'écran QR (sauf en attente d'adoption)
        if (s_adopted) qr_hide();
    } else if (id < s_menu_n) {
        const menu_item_t *m = &s_menu[id];
        switch (m->kind) {
        // case M_GAMES: menu_set(MENU_GAMES, LEN(MENU_GAMES)); break;
        case M_GAMES: menu_set(MENU_GAMES, s_games_n); break;   // ⚫ MODIFIÉ (25/10/2026) : jeux de l'espèce
        case M_BACK:  menu_set(MENU_MAIN, LEN(MENU_MAIN)); break;
        case M_WIFI:  qr_show(false); break;
        // ⚫ AJOUTÉ (25/10/2026) : Connectivité
        case M_CONN:  conn_build(); menu_set(MENU_CONN, LEN(MENU_CONN)); break;
        case M_SOUND:
        case M_VOL:
            if (m->kind == M_SOUND) s_sound_on = !s_sound_on;
            else s_sound_vol = s_sound_vol % 3 + 1;
            sound_save();
            if (s_sound_on) snd("happy");   // essai
            conn_build(); menu_set(MENU_CONN, LEN(MENU_CONN));
            lv_roller_set_selected(roller, id, LV_ANIM_OFF);
            break;
        case M_AIINFO: {
            ai_cfg_t c;
            ai_get_cfg(&c);
            ai_status_t st;
            ai_get_status(&st);
            char t[140];
            if (c.provider == AI_OFF) snprintf(t, sizeof(t), "IA désactivée : à régler sur la page web (Intelligence artificielle).");
            else if (st.ok) snprintf(t, sizeof(t), "%s : \"%s\" (il y a %lu min)", ai_provider_name(c.provider), st.phrase, (unsigned long)(st.age_s / 60));
            else snprintf(t, sizeof(t), "%s : %s", ai_provider_name(c.provider), st.error[0] ? st.error : "pas encore d'appel");
            show_msg(t);
            break;
        }
        default:      if (s_adopted) do_action(m->act); break;
        }
    }
    // Après validation, LVGL sort le roller du mode édition : on y revient tout de suite
    // pour que la rotation continue de parcourir les actions (sinon il faudrait un appui de plus).
    lv_group_set_editing(lv_obj_get_group(roller), true);
}

/* ---------- construction de l'écran ---------- */

void pet_ui_start(void)
{
    pet_init();
    sound_load();   // ⚫ AJOUTÉ (25/10/2026)
    bool assets_ok = pet_assets_init();

    s_scr = lv_scr_act();
    lv_obj_clear_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_scr, LV_OPA_COVER, 0);
    if (!assets_ok) {   // ⚫ AJOUTÉ (25/10/2026) : pack d'images absent : message clair plutôt qu'un plantage
        lv_obj_t *l = lv_label_create(s_scr);
        lv_label_set_text(l, "Images absentes\n\nFlasher avec idf.py flash\n(partition \"assets\")");
        lv_obj_set_style_text_font(l, &font_fr_28, 0);
        lv_obj_set_style_text_color(l, lv_color_white(), 0);
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(l);
        return;
    }

    // décor (du fond vers l'avant)
    s_bg_day = img_create(s_scr, A_bg_day);
    // s_bg_night = img_create(s_scr, A_bg_night);
    // ⚫ MODIFIÉ (25/10/2026) : second fond pour les fondus entre les 8 fonds (heure / météo)
    s_bg_b = img_create(s_scr, A_bg_day);
    show(s_bg_b, false);
    s_cloud[0] = img_create(s_scr, A_cloud_1);
    s_cloud[1] = img_create(s_scr, A_cloud_2);
    cloud_start(s_cloud[0], 50, 95000, 0);
    cloud_start(s_cloud[1], 98, 70000, 30000);
    // ⚫ AJOUTÉ (25/10/2026) : nuages sombres (temps couvert / pluie)
    for (int i = 0; i < 2; i++) { s_cloud_dark[i] = img_create(s_scr, A_cloud_dark); show(s_cloud_dark[i], false); }
    cloud_start(s_cloud_dark[0], 40, 120000, 0);
    cloud_start(s_cloud_dark[1], 84, 90000, 45000);
    // ⚫ AJOUTÉ (25/10/2026) : dégâts de la cour (sous l'animal), gamelles
    for (int i = 0; i < PET_MAX_HOLES; i++) { s_hole_o[i] = img_create(s_scr, A_hole); show(s_hole_o[i], false); }
    for (int i = 0; i < 2; i++) s_cone_o[i] = img_create(s_scr, A_cone_up);   // ⚫ AJOUTÉ (25/10/2026)
    for (int i = 0; i < PET_MAX_HOLES; i++) {   // ⚫ AJOUTÉ (25/10/2026)
        s_dirt_o[i] = img_create(s_scr, A_dirt); show(s_dirt_o[i], false);
        s_sprout_o[i] = img_create(s_scr, A_sprouts); show(s_sprout_o[i], false);
    }
    for (int i = 0; i < PET_MAX_POOPS; i++) { s_poop_o[i] = img_create(s_scr, A_poop); show(s_poop_o[i], false); }
    for (int i = 0; i < 3; i++) { s_toilet_o[i] = img_create(s_scr, A_poop); show(s_toilet_o[i], false); }   // ⚫ 25/10/2026
    // ⚫ AJOUTÉ (25/10/2026) : objets des jeux posés au sol (sous l'animal)
    s_mud = img_create(s_scr, A_mud); show(s_mud, false);
    s_hurdle = img_create(s_scr, A_hurdle); show(s_hurdle, false);
    s_hay = img_create(s_scr, A_hay); show(s_hay, false);
    s_grass = img_create(s_scr, A_grass); show(s_grass, false);
    s_yarn = img_create(s_scr, A_yarn); show(s_yarn, false);
    s_bowl_food = img_create(s_scr, A_bowl_food);
    s_bowl_water = img_create(s_scr, A_bowl_water);
    s_kibble = img_create(s_scr, A_kibble);
    img_at(s_kibble, A_kibble, SCENE_FOOD_X, SCENE_FOOD_Y - 7);
    lv_obj_set_style_opa(s_kibble, LV_OPA_TRANSP, 0);
    s_dog_img = lv_img_create(s_scr);
    lv_obj_clear_flag(s_dog_img, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    show(s_dog_img, false);
    // objets de jeu dessinés devant l'animal (il les tient dans la gueule)
    s_ball = img_create(s_scr, A_ball);
    show(s_ball, false);
    s_frisbee = img_create(s_scr, A_frisbee);
    show(s_frisbee, false);
    s_bone = img_create(s_scr, A_bone);
    show(s_bone, false);
    for (int i = 0; i < N_BIRDS; i++) { s_bird[i].o = img_create(s_scr, A_bird_stand); show(s_bird[i].o, false); }
    s_hand = img_create(s_scr, A_hand_1);   // ⚫ AJOUTÉ (25/10/2026)
    show(s_hand, false);
    // ⚫ AJOUTÉ (25/10/2026) : premier plan de l'abri (devant l'animal quand il est dessous), gestes, météo
    s_shelter = img_create(s_scr, A_shelter_day);
    img_at(s_shelter, A_shelter_day, 0, 0);
    show(s_shelter, false);
    s_scold = img_create(s_scr, A_hand_scold); show(s_scold, false);
    s_point = img_create(s_scr, A_hand_point); show(s_point, false);
    // s_hglass = img_create(s_scr, A_hourglass_1); show(s_hglass, false);
    s_hglass = img_create(s_scr, A_heart_crack); show(s_hglass, false);   // ⚫ MODIFIÉ (25/10/2026) : cœur brisé
    s_wand = img_create(s_scr, A_wand_1); show(s_wand, false);
    s_brush = img_create(s_scr, A_hand_brush_1); show(s_brush, false);
    s_carrot_hand = img_create(s_scr, A_hand_carrot); show(s_carrot_hand, false);
    for (int i = 0; i < N_DROPS; i++) { s_drop[i].o = img_create(s_scr, A_drop); show(s_drop[i].o, false); }
    for (int i = 0; i < N_FLAKES; i++) { s_flake[i].o = img_create(s_scr, A_flake); show(s_flake[i].o, false); }
    s_fog = lv_obj_create(s_scr);
    lv_obj_remove_style_all(s_fog);
    lv_obj_set_size(s_fog, 480, 480);
    lv_obj_set_style_bg_color(s_fog, lv_color_hex(0xD8DEE4), 0);
    lv_obj_set_style_bg_opa(s_fog, LV_OPA_30, 0);
    lv_obj_clear_flag(s_fog, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    show(s_fog, false);
    s_fly = img_create(s_scr, A_fly_1);
    for (int i = 0; i < 6; i++) {
        s_firefly[i] = img_create(s_scr, A_firefly);
        lv_obj_add_flag(s_firefly[i], LV_OBJ_FLAG_HIDDEN);
    }
    // ⚫ AJOUTÉ (25/10/2026) : éclair d'orage (tout l'écran blanc un instant)
    s_flash = lv_obj_create(s_scr);
    lv_obj_remove_style_all(s_flash);
    lv_obj_set_size(s_flash, 480, 480);
    lv_obj_set_style_bg_color(s_flash, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(s_flash, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_flash, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    show(s_flash, false);
    // calque des effets (cœurs, Zzz, mottes), sous l'interface
    s_fx = lv_obj_create(s_scr);
    lv_obj_remove_style_all(s_fx);
    lv_obj_set_size(s_fx, 480, 480);
    lv_obj_clear_flag(s_fx, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    // jauges sur le bord (angles LVGL : 0° = 3 h, sens horaire) ; toutes se remplissent vers le haut
    // ⚫ MODIFIÉ (25/10/2026) : 4 à gauche (+ Besoins), 3 à droite
    gauge_build(G_FOOD,   108, 138, false, 0xFF8A2A, ICON_FEED);     // gauche, bas
    gauge_build(G_WATER,  144, 174, false, 0x38B8FF, ICON_DRINK);
    gauge_build(G_ENERGY, 180, 210, false, 0xFFD43A, ICON_ENERGY);
    gauge_build(G_POTTY,  216, 246, false, 0xC08A40, ICON_POTTY);    // gauche, haut
    gauge_build(G_JOY,    292, 330, true,  0xFF4A8A, ICON_HEART);    // droite, haut
    gauge_build(G_DISC,   340,  20, true,  0xA070FF, ICON_DISC);     // droite
    gauge_build(G_HEALTH,  30,  68, true,  0x50E07A, ICON_HEALTH);   // droite, bas

    // nom + âge sur une pastille
    s_info = lv_label_create(s_scr);
    lv_obj_set_style_text_font(s_info, &font_fr_20, 0);
    lv_obj_set_style_text_color(s_info, lv_color_white(), 0);
    lv_obj_set_style_bg_color(s_info, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_info, LV_OPA_40, 0);
    lv_obj_set_style_radius(s_info, 14, 0);
    lv_obj_set_style_pad_hor(s_info, 14, 0);
    lv_obj_set_style_pad_ver(s_info, 5, 0);
    lv_label_set_recolor(s_info, true);   // couleur de l'icône WiFi
    lv_obj_align(s_info, LV_ALIGN_TOP_MID, 0, 34);

    // ⚫ AJOUTÉ (25/10/2026) : fenêtre "santé" (sous la bulle)
    s_hp = lv_obj_create(s_scr);
    lv_obj_remove_style_all(s_hp);
    lv_obj_set_size(s_hp, 320, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(s_hp, lv_color_hex(0x1A0E10), 0);
    lv_obj_set_style_bg_opa(s_hp, LV_OPA_80, 0);
    lv_obj_set_style_radius(s_hp, 16, 0);
    lv_obj_set_style_border_color(s_hp, lv_color_hex(0xFF4A4A), 0);
    lv_obj_set_style_border_width(s_hp, 2, 0);
    lv_obj_set_style_pad_all(s_hp, 8, 0);
    lv_obj_set_style_pad_row(s_hp, 4, 0);
    lv_obj_set_flex_flow(s_hp, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_hp, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(s_hp, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(s_hp, LV_ALIGN_TOP_MID, 0, 158);
    s_hp_lbl = lv_label_create(s_hp);
    lv_label_set_recolor(s_hp_lbl, true);
    lv_obj_set_style_text_font(s_hp_lbl, &font_fr_20, 0);
    lv_obj_set_style_text_color(s_hp_lbl, lv_color_white(), 0);
    s_hp_bar = lv_bar_create(s_hp);
    lv_obj_set_size(s_hp_bar, 280, 10);
    lv_bar_set_range(s_hp_bar, 0, 100);
    lv_obj_set_style_bg_color(s_hp_bar, lv_color_hex(0x3A2A2C), LV_PART_MAIN);
    s_hp_why = lv_label_create(s_hp);
    lv_label_set_long_mode(s_hp_why, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_hp_why, 290);
    lv_obj_set_style_text_align(s_hp_why, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(s_hp_why, &font_fr_20, 0);
    lv_obj_set_style_text_color(s_hp_why, lv_color_hex(0xFFC8C8), 0);
    show(s_hp, false);

    // bulle de dialogue
    s_bubble = lv_obj_create(s_scr);
    lv_obj_remove_style_all(s_bubble);
    lv_obj_set_size(s_bubble, 300, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(s_bubble, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(s_bubble, LV_OPA_90, 0);
    lv_obj_set_style_radius(s_bubble, 16, 0);
    lv_obj_set_style_pad_all(s_bubble, 8, 0);
    lv_obj_clear_flag(s_bubble, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(s_bubble, LV_ALIGN_TOP_MID, 0, 74);
    s_bubble_lbl = lv_label_create(s_bubble);
    lv_label_set_long_mode(s_bubble_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_bubble_lbl, lv_pct(100));
    lv_obj_set_style_text_align(s_bubble_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(s_bubble_lbl, &font_fr_20, 0);
    lv_obj_set_style_text_color(s_bubble_lbl, lv_color_hex(0x30260E), 0);
    lv_obj_add_flag(s_bubble, LV_OBJ_FLAG_HIDDEN);

    // sélecteur d'actions : une ligne, sur une pastille sombre en bas
    s_roller = lv_roller_create(s_scr);
    lv_roller_set_visible_row_count(s_roller, 1);
    lv_obj_set_width(s_roller, 236);
    lv_obj_align(s_roller, LV_ALIGN_BOTTOM_MID, 0, -40);
    lv_obj_set_style_text_font(s_roller, &font_fr_28, LV_PART_MAIN);
    lv_obj_set_style_text_font(s_roller, &font_fr_28, LV_PART_SELECTED);
    lv_obj_set_style_text_color(s_roller, lv_color_white(), LV_PART_SELECTED);
    lv_obj_set_style_text_align(s_roller, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_roller, lv_color_hex(0x10142A), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_roller, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_roller, LV_OPA_TRANSP, LV_PART_SELECTED);
    lv_obj_set_style_radius(s_roller, 24, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_roller, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_roller, lv_color_hex(0xFFD27A), LV_PART_MAIN);
    lv_obj_set_style_border_opa(s_roller, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_outline_width(s_roller, 0, LV_PART_MAIN | LV_STATE_FOCUS_KEY);
    lv_obj_set_style_outline_width(s_roller, 0, LV_PART_MAIN | LV_STATE_EDITED);
    lv_obj_add_event_cb(s_roller, roller_cb, LV_EVENT_VALUE_CHANGED, NULL);
    menu_set(MENU_MAIN, LEN(MENU_MAIN));

    qr_build();   // ⚫ AJOUTÉ (25/10/2026) : au-dessus de tout

    // Le roller est dans le groupe par défaut (main.c) : seul objet du groupe (les arcs en sont retirés),
    // on le met directement en mode édition -> la rotation parcourt les actions dès le départ.
    lv_group_t *g = lv_obj_get_group(s_roller);
    if (g) {
        lv_group_focus_obj(s_roller);
        lv_group_set_editing(g, true);
    } else {
        ESP_LOGW(TAG, "Pas de groupe LVGL : l'encodeur ne pilotera pas les actions");
    }

    scene_update();   // ⚫ AJOUTÉ (25/10/2026) : fond selon l'heure / la météo (ou jour par défaut)
    // ⚫ MODIFIÉ (25/10/2026) : animal adopté -> il entre dans la cour ; sinon écran d'adoption (QR code)
    sync_mess();
    if (pet_get()->adopted) animal_enter(true);
    else animal_leave();
    refresh();

    lv_timer_create(tick_cb, 1000, NULL);
    lv_timer_create(frame_cb, FRAME_MS, NULL);
    ESP_LOGI(TAG, "Prêt : %s (%s), %d images", s_adopted ? pet_name() : "aucun animal",
             s_adopted ? pet_species_label(s_species) : "-", pet_pack_count());
}
