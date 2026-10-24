// ⚫ AJOUTÉ (24/10/2026) : modèle de l'animal de compagnie (voir pet.h).
// Règles du jeu (temps "de jeu" = temps réel x PET_TIME_SCALE) :
// - ⚫ 25/10/2026 : + soif : l'hydratation se vide en ~45 min (2x moins vite en dormant) ;
// - éveillé : la satiété se vide en ~1 h, l'énergie en ~1 h 30, le bonheur en ~45 min (2x plus vite s'il a faim
//   ou est épuisé), la discipline en ~3 h ;
// - endormi : l'énergie remonte en ~10 min, la satiété baisse 2x moins vite ; réveil spontané à 100 % ;
// - énergie à 0 : il s'endort d'épuisement ; satiété ou bonheur à 0 : la santé baisse (~30 min jusqu'à 0) ;
// - bêtises au hasard, d'autant plus souvent que la discipline est basse : gronder / punir les corrige.
//   Gronder ou punir sans raison le fait pleurer : il faut alors le réconforter.
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_random.h"
#include "nvs.h"
#include "sdkconfig.h"
#include "pet.h"

#define TAG "PET"

#define NVS_NS        "pet"
#define NVS_KEY       "state"
// #define SAVE_MAGIC    0x50455431u   // "PET1" : change si la structure sauvegardée change
#define SAVE_MAGIC    0x50455432u   // ⚫ MODIFIÉ (25/10/2026) : "PET2" (ajout de la soif)

// Vitesses en points par seconde de jeu
#define FOOD_DECAY      (100.0f / 3600)
#define WATER_DECAY     (100.0f / 2700)   // ⚫ AJOUTÉ (25/10/2026)
#define ENERGY_DECAY    (100.0f / 5400)
#define ENERGY_RECOVER  (100.0f / 600)
#define JOY_DECAY       (100.0f / 2700)
#define DISC_DECAY      (100.0f / 10800)
#define HEALTH_DECAY    (100.0f / 1800)
#define HEALTH_RECOVER  (100.0f / 3000)

#define NAUGHTY_CHECK_S 60      // tirage d'une bêtise toutes les 60 s de jeu
#define CRY_MAX_S       120     // pleure au plus 2 min s'il n'est pas consolé (puis boude)

#define STAGE_CHILD_S   (3600)          // bébé pendant 1 h
#define STAGE_ADULT_S   (6 * 3600)      // enfant jusqu'à 6 h

typedef struct {
    uint32_t magic;
    pet_t pet;
} pet_save_t;

static pet_t s_pet;
static uint32_t s_naughty_acc;   // secondes de jeu depuis le dernier tirage de bêtise
static uint32_t s_cry_s;         // durée des pleurs en cours
static uint32_t s_save_acc;      // secondes réelles depuis la dernière sauvegarde
static char s_msg[96];

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
}

static void pet_new(void)
{
    memset(&s_pet, 0, sizeof(s_pet));
    s_pet.s = (pet_stats_t){ .food = 70, .water = 70, .energy = 80, .joy = 70, .discipline = 30, .health = 100 };
}

static void pet_save(void)
{
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
        if (nvs_get_blob(h, NVS_KEY, &sv, &len) == ESP_OK && len == sizeof(sv) && sv.magic == SAVE_MAGIC) {
            s_pet = sv.pet;
            clamp_all();
            ESP_LOGI(TAG, "Animal rechargé (âge %lu s)", (unsigned long)s_pet.age_s);
        }
        nvs_close(h);
    }
    if (s_pet.age_s == 0) ESP_LOGI(TAG, "Nouvel animal : %s", CONFIG_PET_NAME);
}

static void set_msg(const char *fmt, const char *name)
{
    snprintf(s_msg, sizeof(s_msg), fmt, name);
}

bool pet_tick(uint32_t dt_s)
{
    bool event = false;
    for (uint32_t n = 0; n < dt_s * CONFIG_PET_TIME_SCALE; n++) {   // pas de 1 s de jeu
        pet_stats_t *s = &s_pet.s;
        s_pet.age_s++;

        if (s_pet.sleeping) {
            s->food   -= FOOD_DECAY / 2;
            s->water  -= WATER_DECAY / 2;
            s->energy += ENERGY_RECOVER;
            if (s->energy >= 100) {
                s_pet.sleeping = false;
                set_msg("%s se réveille en pleine forme !", CONFIG_PET_NAME);
                event = true;
            }
        } else {
            s->food       -= FOOD_DECAY;
            s->water      -= WATER_DECAY;
            s->energy     -= ENERGY_DECAY;
            // s->joy        -= JOY_DECAY * ((s->food < 20 || s->energy < 15) ? 2 : 1);
            s->joy        -= JOY_DECAY * ((s->food < 20 || s->water < 20 || s->energy < 15) ? 2 : 1);
            s->discipline -= DISC_DECAY * (s_pet.naughty ? 3 : 1);   // une bêtise ignorée s'installe

            if (s->energy <= 0) {
                s_pet.sleeping = true;
                s_pet.crying = false;
                s->health -= 5;
                set_msg("%s s'endort d'épuisement...", CONFIG_PET_NAME);
                event = true;
            }

            if (s_pet.crying && ++s_cry_s >= CRY_MAX_S) {   // personne ne l'a consolé : il boude
                s_pet.crying = false;
                s->joy -= 10;
                set_msg("%s boude dans son coin.", CONFIG_PET_NAME);
                event = true;
            }

            if (++s_naughty_acc >= NAUGHTY_CHECK_S) {
                s_naughty_acc = 0;
                // 5 %/min si discipline à 100, 45 %/min si discipline à 0
                uint32_t p = 50 + (uint32_t)((100 - s->discipline) * 4);   // pour mille
                if (!s_pet.naughty && !s_pet.crying && (esp_random() % 1000) < p) {
                    s_pet.naughty = true;
                    set_msg("%s fait une bêtise !", CONFIG_PET_NAME);
                    event = true;
                }
            }
        }

        // if (s->food <= 0 || s->joy <= 0) {
        if (s->food <= 0 || s->water <= 0 || s->joy <= 0) {
            s->health -= HEALTH_DECAY;
        // } else if (s->food > 40 && s->joy > 40 && s->energy > 20) {
        } else if (s->food > 40 && s->water > 40 && s->joy > 40 && s->energy > 20) {
            s->health += HEALTH_RECOVER;
        }
        clamp_all();
    }

    s_save_acc += dt_s;
    if (event || s_save_acc >= CONFIG_PET_SAVE_PERIOD_S) pet_save();
    return event;
}

pet_react_t pet_do(pet_action_t act, const char **msg)
{
    pet_stats_t *s = &s_pet.s;
    const char *name = CONFIG_PET_NAME;
    pet_react_t r = PET_REACT_NONE;

    switch (act) {
    case PET_ACT_FEED:
        if (s->food >= 90) {
            s->joy -= 3;
            set_msg("%s n'a plus faim !", name);
            r = PET_REACT_REFUSE;
        } else {
            s->food += 30;
            s->joy += 3;
            s->health += 1;
            set_msg("Miam ! %s se régale.", name);
            r = PET_REACT_EAT;
        }
        break;

    // ⚫ AJOUTÉ (25/10/2026) : boire à la gamelle d'eau
    case PET_ACT_DRINK:
        if (s->water >= 90) {
            set_msg("%s n'a pas soif.", name);
            r = PET_REACT_REFUSE;
        } else {
            s->water += 35;
            s->health += 1;
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

    case PET_ACT_PET:
        if (s_pet.crying) {
            s->joy += 4;
            set_msg("%s renifle un peu...", name);
        } else if (s_pet.naughty) {
            // une caresse pendant une bêtise l'encourage à recommencer
            s->joy += 8;
            s->discipline -= 6;
            // set_msg("%s ronronne... sans regretter sa bêtise.", name);
            set_msg("%s remue la queue... sans regretter sa bêtise.", name);   // ⚫ 25/10/2026 : c'est un chien
        } else {
            s->joy += 10;
            // set_msg("%s ronronne de bonheur.", name);
            set_msg("%s se roule de bonheur.", name);   // ⚫ 25/10/2026 : c'est un chien
        }
        r = PET_REACT_LOVE;
        break;

    case PET_ACT_SCOLD:
        if (s_pet.naughty) {
            s_pet.naughty = false;
            s->discipline += 15;
            s->joy -= 4;
            set_msg("%s baisse les oreilles. Compris !", name);
            r = PET_REACT_SORRY;
        } else {
            s->joy -= 12;
            s_pet.crying = true;
            s_cry_s = 0;
            set_msg("%s ne comprend pas pourquoi...", name);
            r = PET_REACT_HURT;
        }
        break;

    case PET_ACT_PUNISH:
        if (s_pet.naughty) {
            s_pet.naughty = false;
            s->discipline += 30;
            s->joy -= 12;
            s_pet.crying = true;          // sanction méritée, mais il a quand même du chagrin
            s_cry_s = CRY_MAX_S - 40;     // chagrin court
            // set_msg("%s file au coin. Il ne recommencera plus.", name);
            set_msg("%s file à la niche. Il ne recommencera plus.", name);   // ⚫ 25/10/2026
            r = PET_REACT_SORRY;
        } else {
            s->joy -= 25;
            s->health -= 5;
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

    clamp_all();
    pet_save();
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
    if (s_pet.naughty)    return PET_MOOD_NAUGHTY;
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
    case PET_STAGE_CHILD: return "Enfant";
    default:              return "Adulte";
    }
}
