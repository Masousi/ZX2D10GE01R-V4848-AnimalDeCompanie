// ⚫ AJOUTÉ (24/10/2026) : écran de l'animal de compagnie (LVGL, écran rond 480x480).
// - tourner l'encodeur : choisir une action (Manger, Dormir, Jouer, Caresser, Gronder, Punir, Réconforter) ;
// - appuyer : valider l'action ; pendant le sommeil, un appui réveille l'animal.
// ⚫ MODIFIÉ (25/10/2026) : menu principal + menu Jeux, QR code WiFi, écran d'adoption ; la page web reçoit une
// copie de la scène (pet_ui_scene_json) et peut demander une adoption ou une réinitialisation.
#pragma once

#include <stddef.h>

// Charge l'animal (NVS), construit l'écran et démarre le jeu. À appeler depuis la tâche LVGL,
// après screen_init() et la création du groupe LVGL de l'encodeur.
void pet_ui_start(void);

// ⚫ AJOUTÉ (25/10/2026) : appelables depuis n'importe quelle tâche (serveur web)
// Dernière image de la scène en JSON (objets affichés : image du pack, sens, position, opacité ; textes flottants)
size_t pet_ui_scene_json(char *out, size_t n);
// Adoption / réinitialisation : exécutées par la tâche LVGL à la seconde suivante
void pet_ui_request_adopt(const char *name, int species);
void pet_ui_request_reset(void);
// ⚫ AJOUTÉ (25/10/2026) : boutons de la page web : lancer une action (false si impossible : pas d'animal,
// pas un jeu de l'espèce) ; liste des actions possibles en JSON
#include <stdbool.h>
bool pet_ui_request_action(int act);
size_t pet_ui_actions_json(char *out, size_t n);
