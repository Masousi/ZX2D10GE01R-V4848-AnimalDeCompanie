// ⚫ AJOUTÉ (24/10/2026) : écran de l'animal de compagnie (LVGL, écran rond 480x480).
// - tourner l'encodeur : choisir une action (Manger, Dormir, Jouer, Caresser, Gronder, Punir, Réconforter) ;
// - appuyer : valider l'action ; pendant le sommeil, un appui réveille l'animal.
#pragma once

// Charge l'animal (NVS), construit l'écran et démarre le jeu. À appeler depuis la tâche LVGL,
// après screen_init() et la création du groupe LVGL de l'encodeur.
void pet_ui_start(void);
