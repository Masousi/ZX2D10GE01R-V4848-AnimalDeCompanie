// ⚫ AJOUTÉ (24/10/2026) : modèle de l'animal de compagnie (type Tamagotchi) : jauges, humeur, actions,
// évolution dans le temps et sauvegarde en flash (NVS). Aucune dépendance à l'écran ni aux LED :
// main/pet_ui.c affiche l'état, main/pet_led.c le reporte sur l'anneau de LED.
// Utilisé uniquement depuis la tâche LVGL (pas de verrou).
#pragma once

#include <stdbool.h>
#include <stdint.h>

// Actions proposées à l'encodeur (ordre du sélecteur à l'écran)
typedef enum {
    PET_ACT_FEED = 0,   // donner à manger
    PET_ACT_DRINK,      // ⚫ AJOUTÉ (25/10/2026) : donner à boire (gamelle d'eau)
    PET_ACT_SLEEP,      // faire dormir
    PET_ACT_PLAY,       // jouer (⚫ 25/10/2026 : "Courir" : tours de piste derrière la balle)
    PET_ACT_FETCH,      // ⚫ AJOUTÉ (25/10/2026) : lancer la balle, il la rapporte
    PET_ACT_FRISBEE,    // ⚫ AJOUTÉ (25/10/2026) : frisbee attrapé au vol
    PET_ACT_BONE,       // ⚫ AJOUTÉ (25/10/2026) : os à ronger (jeu calme)
    PET_ACT_PET,        // caresser
    PET_ACT_SCOLD,      // gronder
    PET_ACT_PUNISH,     // punir
    PET_ACT_COMFORT,    // réconforter
    PET_ACT_COUNT,
    PET_ACT_WAKE = PET_ACT_COUNT,   // réveiller (appui pendant le sommeil, pas dans le sélecteur)
} pet_action_t;

// Humeur affichée (visage + couleur de l'anneau), par priorité décroissante (cf. pet_mood())
typedef enum {
    PET_MOOD_SLEEPING = 0,
    PET_MOOD_CRYING,    // pleure (puni, grondé à tort) : attend d'être réconforté
    PET_MOOD_NAUGHTY,   // fait une bêtise : attend d'être grondé / puni
    PET_MOOD_SICK,      // santé basse
    PET_MOOD_HUNGRY,
    PET_MOOD_THIRSTY,   // ⚫ AJOUTÉ (25/10/2026)
    PET_MOOD_TIRED,
    PET_MOOD_SAD,
    PET_MOOD_NEUTRAL,
    PET_MOOD_HAPPY,
    PET_MOOD_COUNT,
} pet_mood_t;

// Âge -> taille du personnage
typedef enum {
    PET_STAGE_BABY = 0,
    PET_STAGE_CHILD,
    PET_STAGE_ADULT,
} pet_stage_t;

// Réaction à une action (animation à l'écran + effet sur l'anneau)
typedef enum {
    PET_REACT_NONE = 0,
    PET_REACT_JOY,      // action appréciée
    PET_REACT_LOVE,     // caresse / réconfort : cœurs
    PET_REACT_EAT,
    PET_REACT_DRINK,    // ⚫ AJOUTÉ (25/10/2026)
    PET_REACT_SLEEP,    // s'endort
    PET_REACT_WAKE,     // se réveille
    PET_REACT_REFUSE,   // refuse (pas faim, trop fatigué…)
    PET_REACT_SORRY,    // gronder / punir mérité : discipline en hausse
    PET_REACT_HURT,     // gronder / punir sans raison : pleure
} pet_react_t;

// Jauges 0..100
typedef struct {
    float food;         // satiété (0 = affamé)
    float water;        // ⚫ AJOUTÉ (25/10/2026) : hydratation (0 = assoiffé)
    float energy;
    float joy;          // bonheur
    float discipline;
    float health;       // santé : baisse si une jauge vitale est à 0, remonte quand tout va bien
} pet_stats_t;

typedef struct {
    pet_stats_t s;
    uint32_t age_s;     // âge (secondes de jeu, accélérées par PET_TIME_SCALE)
    bool sleeping;
    bool naughty;       // bêtise en cours
    bool crying;
} pet_t;

// Charge l'animal depuis la NVS (nouvel animal si absent). nvs_flash_init() doit avoir été appelé.
void pet_init(void);

// Fait évoluer l'animal de `dt_s` secondes réelles (appelé chaque seconde). Sauvegarde périodique.
// Retourne true si un événement mérite un message (bêtise, endormi d'épuisement, réveil…) : voir pet_event_text().
bool pet_tick(uint32_t dt_s);

// Applique une action, sauvegarde, et renvoie la réaction ; `msg` reçoit une phrase à afficher.
pet_react_t pet_do(pet_action_t act, const char **msg);

const pet_t *pet_get(void);
pet_mood_t pet_mood(void);
pet_stage_t pet_stage(void);
// Bien-être global 0..100 (moyenne des jauges, pénalisée par la santé) : jauge de l'anneau de LED
int pet_wellbeing(void);
// Dernier événement spontané (après pet_tick() == true)
const char *pet_event_text(void);
const char *pet_mood_text(pet_mood_t mood);
const char *pet_stage_text(pet_stage_t stage);
