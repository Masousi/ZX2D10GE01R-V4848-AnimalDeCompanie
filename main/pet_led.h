// ⚫ AJOUTÉ (24/10/2026) : anneau de LED (WS2812, GPIO LED_PIN) = état de l'animal.
// - nombre de LED allumées = bien-être global (pet_wellbeing) ;
// - couleur + rythme = humeur (vert = heureux, orange clignotant = faim, cyan clignotant = soif (⚫ 25/10/2026), bleu = triste / pleure,
//   rouge-magenta = bêtise, jaune pâle = fatigué, vert acide = malade, indigo très lent = dort…) ;
// - chaque action déclenche un effet court par-dessus (arc-en-ciel = joie, rose = câlin, etc.).
// Remplace l'ancien main/encoder_led.c (comète + chenillard des démos).
#pragma once

#include "pet.h"

// Lance la tâche d'animation (appelé une fois par main.c).
void pet_led_start(void);

// Met à jour l'état affiché (appelé par l'interface, depuis la tâche LVGL).
void pet_led_set_state(pet_mood_t mood, int wellbeing);

// Joue l'effet de réaction à une action (~1,5 s).
void pet_led_react(pet_react_t react);
