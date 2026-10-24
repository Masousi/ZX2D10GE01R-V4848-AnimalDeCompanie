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
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "esp_log.h"
#include "esp_random.h"
#include "sdkconfig.h"
#include "lvgl.h"
#include "pet.h"
#include "pet_led.h"
#include "pet_ui.h"
#include "pet_assets.h"

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

#define FRAME_MS    33       // animation du chien : ~30 images/s
#define MSG_MS      4500     // durée d'affichage d'une bulle
#define LOW_STAT    20       // jauge basse : son icône clignote
#define WALK_SPEED  95.0f    // px/s
#define RUN_SPEED   230.0f

// Ordre = pet_action_t
static const char *const s_act_opts =
    ICON_FEED    "  Manger\n"
    ICON_DRINK   "  Boire\n"
    ICON_SLEEP   "  Dormir\n"
    ICON_PLAY    "  Jouer\n"
    ICON_PET     "  Caresser\n"
    ICON_SCOLD   "  Gronder\n"
    ICON_PUNISH  "  Punir\n"
    ICON_COMFORT "  Réconforter";

/* ---------- lieux de la cour (écran) ---------- */

#define HOME_X      240
#define HOME_Y      338
#define KENNEL_DOG_X (SCENE_KENNEL_X + 12)     // couché devant la porte de la niche
#define KENNEL_DOG_Y (SCENE_KENNEL_Y + 16)
#define FOOD_DOG_X  (SCENE_FOOD_X - DOG_MOUTH_DX)   // tête dans la gamelle (tourné à droite)
#define FOOD_DOG_Y  (SCENE_FOOD_Y - DOG_MOUTH_DY - 2)
#define WATER_DOG_X (SCENE_WATER_X - DOG_MOUTH_DX)
#define WATER_DOG_Y (SCENE_WATER_Y - DOG_MOUTH_DY - 2)

/* ---------- animations du chien (images tournées à droite ; +1 = tournée à gauche) ---------- */

typedef struct {
    const uint16_t *f;
    uint8_t n;
    uint16_t ms;   // durée d'une image
} anim_t;

#define ANIM(name, ms, ...) \
    static const uint16_t name##_f[] = { __VA_ARGS__ }; \
    static const anim_t name = { name##_f, sizeof(name##_f) / sizeof(name##_f[0]), ms }

ANIM(AN_WAG,   110, A_dog_stand_1_r, A_dog_stand_2_r, A_dog_stand_1_r, A_dog_stand_3_r);
ANIM(AN_CALM,  450, A_dog_calm_1_r, A_dog_calm_2_r);
ANIM(AN_WALK,  140, A_dog_run_1_r, A_dog_run_2_r, A_dog_run_3_r, A_dog_run_4_r);
ANIM(AN_RUN,    75, A_dog_run_1_r, A_dog_run_2_r, A_dog_run_3_r, A_dog_run_4_r);
ANIM(AN_SIT,  1000, A_dog_sit_r);
ANIM(AN_SIT_HAPPY, 1000, A_dog_sit_happy_r);
ANIM(AN_LOVE, 1000, A_dog_sit_love_r);
ANIM(AN_SAD,  1000, A_dog_sit_sad_r);
ANIM(AN_CRY,   600, A_dog_sit_cry_r, A_dog_sit_sad_r);
ANIM(AN_LIE,  1000, A_dog_lie_r);
ANIM(AN_SICK, 1000, A_dog_lie_sick_r);
ANIM(AN_SLEEP, 1300, A_dog_sleep_1_r, A_dog_sleep_2_r);
ANIM(AN_EAT,   220, A_dog_eat_1_r, A_dog_eat_2_r);
ANIM(AN_DRINK, 170, A_dog_drink_1_r, A_dog_drink_2_r);
ANIM(AN_DIG,   130, A_dog_dig_1_r, A_dog_dig_2_r);
ANIM(AN_JUMP, 1000, A_dog_jump_r);
ANIM(AN_GUILTY, 1000, A_dog_guilty_r);

/* ---------- scripts ---------- */

typedef enum { S_GOTO, S_POSE, S_LAPS, S_JUMP, S_FACE, S_FUNC } step_kind_t;

typedef struct {
    step_kind_t k;
    int16_t x, y;          // S_GOTO : destination
    bool run;              // S_GOTO : au galop
    const anim_t *anim;    // S_POSE
    uint32_t ms;           // S_POSE : durée (0 = jusqu'à nouvel ordre)
    int8_t face;           // S_FACE : 1 = droite, -1 = gauche
    void (*fn)(void);      // S_FUNC
} step_t;

#define QMAX 12
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

/* ---------- objets ---------- */

typedef struct {
    lv_obj_t *arc;
    lv_obj_t *icon;
} gauge_t;

enum { G_FOOD, G_WATER, G_ENERGY, G_JOY, G_DISC, G_HEALTH, G_COUNT };
static gauge_t s_gauge[G_COUNT];

static lv_obj_t *s_scr, *s_bg_day, *s_bg_night, *s_cloud[2], *s_hole, *s_kibble, *s_ball, *s_dog_img;
static lv_obj_t *s_fly, *s_firefly[6], *s_fx;
static lv_obj_t *s_info, *s_bubble, *s_bubble_lbl, *s_roller;

static int s_mood = -1;
static bool s_night;
static uint32_t s_msg_t0;
static bool s_msg_on;
static uint32_t s_last_act_t;
static bool s_dig_spot_set;
static int16_t s_dig_x, s_dig_y;
static float s_hole_opa;

/* ---------- utilitaires ---------- */

static int rnd(int a, int b)
{
    return a + (int)(esp_random() % (uint32_t)(b - a + 1));
}

static lv_obj_t *img_create(lv_obj_t *parent, int id)
{
    lv_obj_t *o = lv_img_create(parent);
    lv_img_set_src(o, &g_pet_assets[id].img);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

// Place une image par son point d'ancrage
static void img_at(lv_obj_t *o, int id, int x, int y)
{
    lv_obj_set_pos(o, x + g_pet_assets[id].ox, y + g_pet_assets[id].oy);
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
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, o);
    lv_anim_set_exec_cb(&a, anim_opa);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_time(&a, ms);
    lv_anim_start(&a);
}

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

// Position de la tête du chien (pour les effets)
static int head_x(void) { return (int)s_dog.x + s_dog.face * 48; }
static int head_y(void) { return (int)s_dog.y - 96; }

static void burst(const char *txt, uint32_t color, int count)
{
    for (int i = 0; i < count; i++) {
        float_text(txt, color, head_x() + rnd(-30, 30), head_y() + rnd(-10, 10), rnd(-40, 40), -rnd(60, 110), 1400, i * 200);
    }
}

static void clod_spawn(void)
{
    lv_obj_t *c = img_create(s_fx, A_clod);
    int x = (int)s_dog.x + s_dog.face * 30, y = (int)s_dog.y - 4;
    img_at(c, A_clod, x, y);
    float_start(c, -s_dog.face * rnd(30, 70), rnd(-4, 8), rnd(18, 34), 700, 0);
}

/* ---------- jour / nuit ---------- */

static void night_ready(lv_anim_t *a)
{
    if (s_night) lv_obj_add_flag(s_bg_day, LV_OBJ_FLAG_HIDDEN);   // inutile de dessiner le jour sous la nuit
    else lv_obj_add_flag(s_bg_night, LV_OBJ_FLAG_HIDDEN);
}

static void set_night(bool night)
{
    if (night == s_night) return;
    s_night = night;
    lv_obj_clear_flag(s_bg_day, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_bg_night, LV_OBJ_FLAG_HIDDEN);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_bg_night);
    lv_anim_set_exec_cb(&a, anim_opa);
    lv_anim_set_values(&a, night ? 0 : 255, night ? 255 : 0);
    lv_anim_set_time(&a, 1800);
    lv_anim_set_ready_cb(&a, night_ready);
    lv_anim_start(&a);
    for (int i = 0; i < 2; i++) {
        if (night) lv_obj_add_flag(s_cloud[i], LV_OBJ_FLAG_HIDDEN); else lv_obj_clear_flag(s_cloud[i], LV_OBJ_FLAG_HIDDEN);
    }
    if (night) lv_obj_add_flag(s_fly, LV_OBJ_FLAG_HIDDEN); else lv_obj_clear_flag(s_fly, LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < 6; i++) {
        if (night) lv_obj_clear_flag(s_firefly[i], LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(s_firefly[i], LV_OBJ_FLAG_HIDDEN);
    }
    // ⚫ AJOUTÉ (25/10/2026) : la nuit, le chien et les accessoires prennent la teinte bleutée du décor
    lv_obj_t *tinted[] = { s_dog_img, s_kibble, s_ball, s_hole };
    for (size_t i = 0; i < sizeof(tinted) / sizeof(tinted[0]); i++) {
        lv_obj_set_style_img_recolor(tinted[i], lv_color_hex(0x0A1440), 0);
        lv_obj_set_style_img_recolor_opa(tinted[i], night ? LV_OPA_50 : LV_OPA_TRANSP, 0);
    }
}

static void night_on(void)  { set_night(true); }
static void night_off(void) { set_night(false); }

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
    img_at(s_ball, A_ball, (int)s_dog.x + s_dog.face * 50, (int)s_dog.y - 8);   // lancée devant le chien
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

static void set_anim(const anim_t *a)
{
    if (s_dog.anim != a) {
        s_dog.anim = a;
        s_dog.anim_t0 = lv_tick_get();
    }
}

// Comportement de repos selon l'humeur
static void idle_plan(void)
{
    int mood = pet_mood();
    if (mood != PET_MOOD_NAUGHTY) s_dig_spot_set = false;
    if (mood != PET_MOOD_SLEEPING) CALL(night_off);   // en premier : les poses de repos durent indéfiniment

    switch (mood) {
    case PET_MOOD_SLEEPING:
        GOTO(KENNEL_DOG_X, KENNEL_DOG_Y, false); FACE(1); CALL(night_on); POSE(AN_SLEEP, 0);
        break;
    case PET_MOOD_CRYING:
        POSE(AN_CRY, 0);
        break;
    case PET_MOOD_NAUGHTY:   // creuse un trou dans la pelouse
        if (!s_dig_spot_set) {
            s_dig_spot_set = true;
            s_dig_x = rnd(215, 300);
            s_dig_y = rnd(326, 346);
        }
        GOTO(s_dig_x, s_dig_y, true); POSE(AN_DIG, 0);
        break;
    case PET_MOOD_SICK:
        GOTO(KENNEL_DOG_X + 6, KENNEL_DOG_Y + 4, false); FACE(1); POSE(AN_SICK, 0);
        break;
    case PET_MOOD_HUNGRY:    // attend devant sa gamelle
        GOTO(SCENE_FOOD_X - 72, SCENE_FOOD_Y, false); FACE(1); POSE(AN_SAD, 0);
        break;
    case PET_MOOD_THIRSTY:
        GOTO(SCENE_WATER_X - 72, SCENE_WATER_Y, false); FACE(1); POSE(AN_SAD, 0);
        break;
    case PET_MOOD_TIRED:
        POSE(AN_LIE, 0);
        break;
    case PET_MOOD_SAD:
        POSE(AN_SAD, 0);
        break;
    case PET_MOOD_HAPPY:     // se balade, remue la queue, saute de joie de temps en temps
        GOTO(rnd(170, 330), rnd(322, 356), rnd(0, 3) == 0);
        if (rnd(0, 2) == 0) JUMP();
        POSE(*(rnd(0, 1) ? &AN_WAG : &AN_SIT_HAPPY), rnd(4000, 9000));
        break;
    default:                 // va bien : flâne tranquillement
        GOTO(rnd(170, 320), rnd(324, 354), false);
        POSE(*(rnd(0, 2) ? &AN_CALM : &AN_SIT), rnd(6000, 12000));
        break;
    }
    s_step_t0 = lv_tick_get();
}

static void dog_render(void)
{
    const anim_t *a = s_dog.anim;
    uint32_t fi = (lv_tick_elaps(s_dog.anim_t0) / a->ms) % a->n;
    int id = a->f[fi] + (s_dog.face < 0 ? 1 : 0);
    int x = (int)s_dog.x, y = (int)(s_dog.y - s_dog.jump);
    static int last_id = -1, last_x, last_y;
    if (id == last_id && x == last_x && y == last_y) return;   // rien n'a bougé : pas de redessin
    last_id = id; last_x = x; last_y = y;
    lv_img_set_src(s_dog_img, &g_pet_assets[id].img);
    img_at(s_dog_img, id, x, y);
}

static void step_next(void)
{
    s_qi++;
    s_step_t0 = lv_tick_get();
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
        float dx = st->x - s_dog.x, dy = st->y - s_dog.y;
        float d = sqrtf(dx * dx + dy * dy);
        float v = (st->run ? RUN_SPEED : WALK_SPEED) * dt;
        if (d <= v || d < 1.5f) {
            s_dog.x = st->x;
            s_dog.y = st->y;
            step_next();
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

static void gauge_set(int g, float v, bool blink_phase)
{
    lv_arc_set_value(s_gauge[g].arc, (int16_t)(v + 0.5f));
    bool low = v < LOW_STAT;
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
    lv_label_set_text_fmt(s_info, "%s  ·  %s %s", CONFIG_PET_NAME, pet_stage_text(pet_stage()), age_txt);

    if (s_msg_on && lv_tick_elaps(s_msg_t0) > MSG_MS) {
        s_msg_on = false;
        fade(s_bubble, 255, 0, 500);
    }
}

/* ---------- rafraîchissement global (chaque seconde) ---------- */

static void refresh(void)
{
    static uint32_t phase;
    phase++;
    const pet_t *p = pet_get();

    int mood = pet_mood();
    if (mood != s_mood) {
        int old = s_mood;
        s_mood = mood;
        // le chien change de comportement, sauf s'il est en train d'exécuter une action
        if (!s_in_action) {
            q_clear();
            idle_plan();
        }
        if (old >= 0 && !s_msg_on && mood != PET_MOOD_NEUTRAL && mood != PET_MOOD_HAPPY) {
            char t[64];
            snprintf(t, sizeof(t), "%s %s !", CONFIG_PET_NAME, pet_mood_text(mood));
            show_msg(t);
        }
        if (mood == PET_MOOD_NAUGHTY) float_text("!", 0xFF4060, head_x(), head_y() - 20, 0, -30, 1200, 0);
    }

    gauge_set(G_FOOD, p->s.food, phase & 1);
    gauge_set(G_WATER, p->s.water, phase & 1);
    gauge_set(G_ENERGY, p->s.energy, phase & 1);
    gauge_set(G_JOY, p->s.joy, phase & 1);
    gauge_set(G_DISC, p->s.discipline, phase & 1);
    gauge_set(G_HEALTH, p->s.health, phase & 1);
    info_refresh();

    pet_led_set_state(mood, pet_wellbeing());
}

/* ---------- minuteries ---------- */

// Chaque seconde : le temps passe pour le chien
static void tick_cb(lv_timer_t *t)
{
    if (pet_tick(1)) {
        show_msg(pet_event_text());
        ESP_LOGI(TAG, "%s", pet_event_text());
    }
    refresh();
}

// ~30 fois par seconde : chien, papillon, lucioles, Zzz, trou
static void frame_cb(lv_timer_t *t)
{
    static uint32_t last, next_z, next_clod, fly_t0;
    static float fly_x = 300, fly_y = 300, fly_tx = 300, fly_ty = 300;
    static float ff_x[6], ff_y[6], ff_vx[6], ff_vy[6];
    static bool ff_init;
    uint32_t now = lv_tick_get();
    float dt = last ? lv_tick_elaps(last) / 1000.0f : 0;
    if (dt > 0.1f) dt = 0.1f;
    last = now;

    dog_update(dt);
    dog_render();

    // dort : Zzz qui s'envolent de la niche
    if (s_mood == PET_MOOD_SLEEPING && s_dog.anim == &AN_SLEEP && now >= next_z) {
        next_z = now + 1400;
        float_text((esp_random() & 1) ? "Z" : "z", 0xB8C4FF, head_x(), head_y() + 40, 40, -80, 2600, 0);
    }
    // creuse : mottes de terre + le trou apparaît
    if (s_dog.anim == &AN_DIG) {
        if (now >= next_clod) { next_clod = now + rnd(120, 260); clod_spawn(); }
        if (s_hole_opa < 255) {
            if (s_hole_opa == 0) {
                img_at(s_hole, A_hole, (int)s_dog.x + s_dog.face * 36, (int)s_dog.y - 2);
                lv_obj_clear_flag(s_hole, LV_OBJ_FLAG_HIDDEN);
            }
            s_hole_opa += 255 * dt / 3.0f;
            if (s_hole_opa > 255) s_hole_opa = 255;
            lv_obj_set_style_opa(s_hole, (lv_opa_t)s_hole_opa, 0);
        }
    } else if (s_hole_opa > 0 && s_mood != PET_MOOD_NAUGHTY) {   // la pelouse repousse
        s_hole_opa -= 255 * dt / 6.0f;
        if (s_hole_opa <= 0) { s_hole_opa = 0; lv_obj_add_flag(s_hole, LV_OBJ_FLAG_HIDDEN); }
        lv_obj_set_style_opa(s_hole, (lv_opa_t)s_hole_opa, 0);
    }

    if (!s_night) {   // papillon : vole d'un point à l'autre en battant des ailes
        float dx = fly_tx - fly_x, dy = fly_ty - fly_y;
        if (dx * dx + dy * dy < 16 || !fly_t0) {
            fly_t0 = now;
            fly_tx = rnd(60, 420);
            fly_ty = rnd(200, 330);
        }
        float d = sqrtf(dx * dx + dy * dy) + 0.01f;
        fly_x += dx / d * 40 * dt + sinf(now / 300.0f) * 0.6f;
        fly_y += dy / d * 40 * dt + cosf(now / 170.0f) * 0.9f;
        int fid = ((now / 90) & 1) ? A_fly_1 : A_fly_2;
        if (lv_img_get_src(s_fly) != &g_pet_assets[fid].img) lv_img_set_src(s_fly, &g_pet_assets[fid].img);
        img_at(s_fly, fid, (int)fly_x, (int)fly_y);
    } else {          // lucioles qui dérivent et scintillent
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
    pet_led_react(r);

    // script du chien
    q_clear();
    s_in_action = true;
    s_dog.jump = 0;
    s_dog.lap_done = 0;
    switch (r) {
    case PET_REACT_EAT:
        CALL(kibble_show); GOTO(FOOD_DOG_X, FOOD_DOG_Y, true); FACE(1); POSE(AN_EAT, 3200); CALL(kibble_hide);
        POSE(AN_WAG, 900);
        break;
    case PET_REACT_DRINK:
        GOTO(WATER_DOG_X, WATER_DOG_Y, true); FACE(1); POSE(AN_DRINK, 2800); POSE(AN_WAG, 900);
        break;
    case PET_REACT_SLEEP:
        GOTO(KENNEL_DOG_X, KENNEL_DOG_Y, false); FACE(1); CALL(night_on); POSE(AN_SLEEP, 1200);
        break;
    case PET_REACT_WAKE:
        CALL(night_off); POSE(AN_LIE, 900); POSE(AN_WAG, 1200);
        break;
    case PET_REACT_JOY:
        if (act == PET_ACT_PLAY) {
            float sx, sy;
            track_pos((float)M_PI * 0.5f, &sx, &sy);   // départ : devant la piste
            CALL(ball_show); GOTO((int)sx, (int)sy, true); LAPS(); CALL(ball_hide);
            JUMP(); POSE(AN_WAG, 1200);
            burst(ICON_STAR, 0xFFD84A, 3);
        } else {
            JUMP(); POSE(AN_WAG, 1200);
            burst(ICON_STAR, 0xFFD84A, 3);
        }
        break;
    case PET_REACT_LOVE:
        POSE(AN_LOVE, 2400); POSE(AN_WAG, 800);
        burst(ICON_HEART, 0xFF4A8A, 5);
        break;
    case PET_REACT_REFUSE:
        FACE(-s_dog.face); POSE(AN_CALM, 1400);
        float_text("?", 0xFFC040, head_x(), head_y() - 10, 10, -40, 1200, 0);
        break;
    case PET_REACT_SORRY:
        if (act == PET_ACT_PUNISH) { GOTO(KENNEL_DOG_X + 20, KENNEL_DOG_Y, false); FACE(-1); POSE(AN_GUILTY, 2600); }
        else                       { POSE(AN_GUILTY, 2400); }
        float_text("!", 0xB070FF, head_x(), head_y() - 10, 10, -40, 1200, 0);
        break;
    case PET_REACT_HURT:
        POSE(AN_CRY, 2000);
        float_text("!", 0xFF4040, head_x(), head_y() - 10, 10, -40, 1200, 0);
        break;
    default:
        break;
    }
    CALL(action_done);
    s_step_t0 = lv_tick_get();
    refresh();
}

// Appui sur l'encodeur : le roller envoie VALUE_CHANGED (lv_roller.c, release_handler)
static void roller_cb(lv_event_t *e)
{
    lv_obj_t *roller = lv_event_get_target(e);
    uint16_t id = lv_roller_get_selected(roller);
    if (id < PET_ACT_COUNT) do_action((pet_action_t)id);
    // Après validation, LVGL sort le roller du mode édition : on y revient tout de suite
    // pour que la rotation continue de parcourir les actions (sinon il faudrait un appui de plus).
    lv_group_set_editing(lv_obj_get_group(roller), true);
}

/* ---------- construction de l'écran ---------- */

void pet_ui_start(void)
{
    pet_init();
    pet_assets_init();

    s_scr = lv_scr_act();
    lv_obj_clear_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_scr, LV_OPA_COVER, 0);

    // décor (du fond vers l'avant)
    s_bg_day = img_create(s_scr, A_bg_day);
    s_bg_night = img_create(s_scr, A_bg_night);
    lv_obj_set_style_opa(s_bg_night, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(s_bg_night, LV_OBJ_FLAG_HIDDEN);
    s_cloud[0] = img_create(s_scr, A_cloud_1);
    s_cloud[1] = img_create(s_scr, A_cloud_2);
    cloud_start(s_cloud[0], 50, 95000, 0);
    cloud_start(s_cloud[1], 98, 70000, 30000);
    s_hole = img_create(s_scr, A_hole);
    lv_obj_add_flag(s_hole, LV_OBJ_FLAG_HIDDEN);
    s_kibble = img_create(s_scr, A_kibble);
    img_at(s_kibble, A_kibble, SCENE_FOOD_X, SCENE_FOOD_Y - 7);
    lv_obj_set_style_opa(s_kibble, LV_OPA_TRANSP, 0);
    s_ball = img_create(s_scr, A_ball);
    lv_obj_add_flag(s_ball, LV_OBJ_FLAG_HIDDEN);
    s_dog_img = img_create(s_scr, A_dog_stand_1_r);
    s_fly = img_create(s_scr, A_fly_1);
    for (int i = 0; i < 6; i++) {
        s_firefly[i] = img_create(s_scr, A_firefly);
        lv_obj_add_flag(s_firefly[i], LV_OBJ_FLAG_HIDDEN);
    }
    // calque des effets (cœurs, Zzz, mottes), sous l'interface
    s_fx = lv_obj_create(s_scr);
    lv_obj_remove_style_all(s_fx);
    lv_obj_set_size(s_fx, 480, 480);
    lv_obj_clear_flag(s_fx, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    // jauges sur le bord (angles LVGL : 0° = 3 h, sens horaire) ; toutes se remplissent vers le haut
    gauge_build(G_FOOD,   112, 150, false, 0xFF8A2A, ICON_FEED);     // gauche, bas
    gauge_build(G_WATER,  160, 200, false, 0x38B8FF, ICON_DRINK);    // gauche
    gauge_build(G_ENERGY, 210, 248, false, 0xFFD43A, ICON_ENERGY);   // gauche, haut
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
    lv_obj_align(s_info, LV_ALIGN_TOP_MID, 0, 34);

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
    lv_roller_set_options(s_roller, s_act_opts, LV_ROLLER_MODE_INFINITE);
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

    // Le roller est dans le groupe par défaut (main.c) : seul objet du groupe (les arcs en sont retirés),
    // on le met directement en mode édition -> la rotation parcourt les actions dès le départ.
    lv_group_t *g = lv_obj_get_group(s_roller);
    if (g) {
        lv_group_focus_obj(s_roller);
        lv_group_set_editing(g, true);
    } else {
        ESP_LOGW(TAG, "Pas de groupe LVGL : l'encodeur ne pilotera pas les actions");
    }

    // départ : au lit s'il dormait, sinon au milieu de la cour
    if (pet_get()->sleeping) { s_dog.x = KENNEL_DOG_X; s_dog.y = KENNEL_DOG_Y; set_night(true); }
    q_clear();
    refresh();
    char hello[64];
    if (pet_get()->age_s == 0) snprintf(hello, sizeof(hello), "Ouaf ! Je suis %s.", CONFIG_PET_NAME);
    else                       snprintf(hello, sizeof(hello), "%s est content de te revoir !", CONFIG_PET_NAME);
    show_msg(hello);
    pet_led_react(PET_REACT_WAKE);

    lv_timer_create(tick_cb, 1000, NULL);
    lv_timer_create(frame_cb, FRAME_MS, NULL);
    ESP_LOGI(TAG, "Chien prêt (%s), %d images", CONFIG_PET_NAME, A_COUNT);
}
