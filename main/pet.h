// ⚫ AJOUTÉ (24/10/2026) : modèle de l'animal de compagnie (type Tamagotchi) : jauges, humeur, actions,
// évolution dans le temps et sauvegarde en flash (NVS). Aucune dépendance à l'écran ni aux LED :
// main/pet_ui.c affiche l'état, main/pet_led.c le reporte sur l'anneau de LED.
// Utilisé uniquement depuis la tâche LVGL (pas de verrou).
// ⚫ MODIFIÉ (25/10/2026) : plusieurs espèces, adoption (nom + espèce choisis une fois sur la page web,
// verrouillés jusqu'à réinitialisation), besoins (jauge, demande de sortie, accidents), bêtises variées
// (trous, gamelles renversées, besoins n'importe où) qui restent dans la cour jusqu'au nettoyage.
// La page web lit l'état depuis une autre tâche (lecture seule, valeurs indicatives).
#pragma once

#include <stdbool.h>
#include <stdint.h>

// Actions proposées à l'encodeur (ordre du sélecteur à l'écran)
// ⚫ 25/10/2026 : l'ordre du sélecteur est maintenant défini par main/pet_ui.c (menu principal + menu Jeux)
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
    PET_ACT_POTTY,      // ⚫ AJOUTÉ (25/10/2026) : l'emmener au coin besoins
    PET_ACT_CLEAN,      // ⚫ AJOUTÉ (25/10/2026) : nettoyer la cour (trous, besoins, gamelles renversées)
    PET_ACT_BIRDS,      // ⚫ AJOUTÉ (25/10/2026) : jeu : courir après les oiseaux
    // ⚫ AJOUTÉ (25/10/2026) : jeux propres à certaines espèces (voir pet_games_mask)
    PET_ACT_YARN,       // pelote de laine (chat)
    PET_ACT_FEATHER,    // canne à plume (chat)
    PET_ACT_HURDLE,     // saut d'obstacle (cheval, âne, chèvre, lama)
    PET_ACT_CARROT,     // carotte (cheval, âne, lapin, cochon)
    PET_ACT_MUD,        // bain de boue (cochon)
    PET_ACT_CLIMB,      // grimper sur la botte de foin (chèvre)
    PET_ACT_BRUSH,      // brossage (cheval, âne, vache, mouton, lama)
    PET_ACT_GRAZE,      // brouter de l'herbe fraîche (mouton, chèvre, vache, lapin, lama)
    PET_ACT_HEAL,       // ⚫ AJOUTÉ (25/10/2026) : soigner (médicament) quand il est malade
    PET_ACT_COUNT,
    PET_ACT_WAKE = PET_ACT_COUNT,   // réveiller (appui pendant le sommeil, pas dans le sélecteur)
} pet_action_t;

// Humeur affichée (visage + couleur de l'anneau), par priorité décroissante (cf. pet_mood())
typedef enum {
    PET_MOOD_SLEEPING = 0,
    PET_MOOD_CRYING,    // pleure (puni, grondé à tort) : attend d'être réconforté
    PET_MOOD_NAUGHTY,   // fait une bêtise : attend d'être grondé / puni
    PET_MOOD_POTTY,     // ⚫ AJOUTÉ (25/10/2026) : demande à sortir faire ses besoins
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
    PET_STAGE_CHILD,        // ⚫ 25/10/2026 : adolescent
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
    PET_REACT_POTTY,    // ⚫ AJOUTÉ (25/10/2026) : besoins faits au bon endroit
    PET_REACT_CLEAN,    // ⚫ AJOUTÉ (25/10/2026) : cour nettoyée
    PET_REACT_HEAL,     // ⚫ AJOUTÉ (25/10/2026) : soigné
} pet_react_t;

// ⚫ AJOUTÉ (25/10/2026) : sorte de bêtise en cours
typedef enum {
    PET_NAUGHTY_NONE = 0,
    PET_NAUGHTY_DIG,         // creuse un trou dans la pelouse
    PET_NAUGHTY_TIP_FOOD,    // renverse la gamelle de croquettes
    PET_NAUGHTY_TIP_WATER,   // renverse la gamelle d'eau
    PET_NAUGHTY_POOP,        // fait ses besoins n'importe où (⚫ 25/10/2026 : seulement un accident, plus une bêtise au hasard)
    PET_NAUGHTY_CONE,        // ⚫ AJOUTÉ (25/10/2026) : renverse un cône de la piste
} pet_naughty_t;

// Jauges 0..100
typedef struct {
    float food;         // satiété (0 = affamé)
    float water;        // ⚫ AJOUTÉ (25/10/2026) : hydratation (0 = assoiffé)
    float energy;
    float joy;          // bonheur
    float discipline;
    float health;       // santé : baisse si une jauge vitale est à 0, remonte quand tout va bien
    float potty;        // ⚫ AJOUTÉ (25/10/2026) : besoins (0 = rien, 100 = ne peut plus se retenir)
} pet_stats_t;

// ⚫ AJOUTÉ (25/10/2026) : dégâts laissés dans la cour (coordonnées écran)
#define PET_MAX_HOLES  4
#define PET_MAX_POOPS  4
typedef struct {
    int16_t x, y;
} pet_spot_t;

typedef struct {
    pet_stats_t s;
    uint32_t age_s;     // âge (secondes de jeu, accélérées par PET_TIME_SCALE)
    bool sleeping;
    bool naughty;       // bêtise en cours
    bool crying;
    // ⚫ AJOUTÉ (25/10/2026)
    bool adopted;       // nom et espèce choisis (page web) : le temps passe
    uint8_t species;    // index dans PET_SPECIES (main/pet_assets_gen.h)
    char name[17];
    uint8_t naughty_kind;   // pet_naughty_t
    pet_spot_t naughty_at;  // lieu de la bêtise en cours
    uint8_t n_holes, n_poops;
    pet_spot_t holes[PET_MAX_HOLES], poops[PET_MAX_POOPS];
    bool food_tipped, water_tipped;
    uint8_t toilet_poops;   // ⚫ AJOUTÉ (25/10/2026) : besoins faits au coin gravier (restent jusqu'au nettoyage)
    // ⚫ AJOUTÉ (25/10/2026) : trous réalistes et os enterré (en fin de structure : une ancienne sauvegarde,
    // plus courte, est relue telle quelle). Un trou reste tant qu'on ne le rebouche pas ; rebouché, la terre
    // fraîche se recouvre de gazon en PET_REGROW_S secondes de jeu, puis il disparaît.
    uint8_t hole_dig[PET_MAX_HOLES];      // profondeur 0..100 % (il reprend un trou commencé)
    uint32_t hole_filled[PET_MAX_HOLES];  // âge (s de jeu) du rebouchage ; 0 = trou ouvert
    bool bone_buried;                     // son os est enterré dans le trou bone_hole
    int8_t bone_hole;                     // -1 = aucun
    // ⚫ AJOUTÉ (25/10/2026) : cônes renversés (bits 0 et 1), maladie (cause, durée restante en s de jeu)
    uint8_t cones_down;
    uint8_t sick;                         // pet_sick_t
    uint32_t sick_s;
    // ⚫ AJOUTÉ (25/10/2026) : heure réelle (UTC, s depuis 1970) de la dernière sauvegarde : au redémarrage, une
    // fois l'heure synchronisée, le temps passé carte éteinte est rattrapé (âge, repousse, guérison, jauges)
    uint32_t saved_epoch;
} pet_t;

// ⚫ AJOUTÉ (25/10/2026) : causes de maladie
typedef enum { PET_SICK_NONE = 0, PET_SICK_COLD, PET_SICK_HEAT, PET_SICK_STOMACH } pet_sick_t;
const char *pet_sick_text(int cause);

#define PET_REGROW_S   (8 * 3600)         // ⚫ AJOUTÉ (25/10/2026) : le gazon recouvre un trou rebouché en 8 h

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

// ⚫ AJOUTÉ (25/10/2026)
// Adoption : nom (1 à 16 caractères) et espèce ; refusée si un animal est déjà adopté. Sauvegarde immédiate.
bool pet_adopt(const char *name, int species);
// Efface toutes les données de l'animal (NVS) : il faudra en adopter un nouveau
void pet_reset_all(void);
const char *pet_name(void);
const char *pet_species_label(int species);
const char *pet_sound(void);
// Position actuelle de l'animal à l'écran (accidents à l'endroit où il se trouve)
void pet_set_pos(int x, int y);
// Change à chaque modification des dégâts de la cour (l'écran se met à jour)
uint32_t pet_mess_version(void);
// Sauvegarde immédiate (avant redémarrage)
void pet_save_now(void);

// ⚫ AJOUTÉ (25/10/2026)
// Jeux possibles pour une espèce : bits (1 << PET_ACT_xxx)
uint32_t pet_games_mask(int species);
// Environnement réel (météo / heure, main/weather.c) : nuit, mauvais temps, animal à l'abri ou non
void pet_set_env(bool night, bool bad_weather, bool sheltered);
// L'animal est allé faire ses besoins tout seul (à animer une fois) : renvoie true et efface le drapeau
bool pet_take_auto_potty(void);
// ⚫ AJOUTÉ (25/10/2026) : dernière perte de santé : numéro (change à chaque perte), points perdus (négatif), raison
uint32_t pet_health_event(int *delta, const char **why);

// ⚫ AJOUTÉ (25/10/2026) : os enterré, trous
// Os enterré ? (position du trou)
bool pet_bone_buried(int *x, int *y);
// Il déterre son os : le trou se rouvre (il le remettra dans le même trou)
void pet_bone_dig_up(void);
// Il creuse un trou en (x, y) pour y cacher son os (ou reprend le trou de l'os) : renvoie l'index du trou
int pet_hole_open(int x, int y);
// Il remet la terre sur l'os en (x, y) : trou rebouché, l'os y reste caché
void pet_bone_bury(int x, int y);
// Un coin de pelouse libre (loin des gamelles, du gravier, de l'abri, des autres trous), dans l'écran
void pet_lawn_spot(int *x, int *y);
// ⚫ AJOUTÉ (25/10/2026) : vie décidée par l'IA
// bool pet_start_naughty(int kind);   // 0 creuser, 1 croquettes, 2 eau
bool pet_start_naughty(int kind);   // ⚫ MODIFIÉ (25/10/2026) : 0 creuser, 1 croquettes, 2 eau, 3 besoins n'importe où, 4 cône
void pet_attention_bonus(void);     // demande satisfaite à temps
void pet_make_sick(int cause);      // ⚫ AJOUTÉ (25/10/2026) : pet_sick_t
