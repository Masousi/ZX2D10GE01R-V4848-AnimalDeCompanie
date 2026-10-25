// ⚫ AJOUTÉ (25/10/2026) : IA (facultative) qui choisit le comportement de l'animal et le fait "parler".
// - fournisseurs gratuits, tous appelés en format "chat completions" compatible OpenAI :
//   Google Gemini (AI Studio), Groq, OpenRouter (modèles ":free"), Ollama (PC de la maison) ;
// - réglages (fournisseur, clé, modèle, adresse, période) sur la page web, gardés en NVS ("ai") ;
// - toutes les N minutes, et après une action (au plus une fois par minute), la carte envoie l'état de
//   l'animal (jauges, humeur, âge, météo, heure, dernière action) ; l'IA répond un comportement parmi une
//   liste fermée et une phrase courte en français. Les règles du jeu (jauges, besoins, pluie) restent
//   prioritaires : l'IA ne fait que choisir parmi des comportements autorisés.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum { AI_OFF = 0, AI_GEMINI, AI_GROQ, AI_OPENROUTER, AI_OLLAMA, AI_PROVIDER_COUNT } ai_provider_t;

typedef struct {
    ai_provider_t provider;
    char model[64];
    char url[96];          // Ollama : http://<PC>:11434 (vide = adresse par défaut du fournisseur)
    int period_min;        // minutes entre deux appels (en plus des appels après une action)
    bool has_key;
    char key_tail[8];      // 4 derniers caractères de la clé (jamais la clé entière)
} ai_cfg_t;

typedef struct {
    bool ok;
    uint32_t age_s;        // secondes depuis la dernière réponse (ou tentative)
    int calls;
    char behaviour[24];
    char phrase[100];
    char error[96];
} ai_status_t;

void ai_start(void);
void ai_get_cfg(ai_cfg_t *out);
// key NULL ou "" : garder la clé actuelle
void ai_set_cfg(ai_provider_t p, const char *key, const char *model, const char *url, int period_min);
void ai_get_status(ai_status_t *out);
// Demande un appel dès que possible (bouton "Tester" de la page, ou après une action : limité à 1 / min)
void ai_trigger(bool force);
// Dernière décision non encore appliquée (appelé par l'interface) : true si nouvelle
bool ai_take(char *behaviour, size_t nb, char *phrase, size_t np);
// Dernière action faite (texte), pour le contexte envoyé à l'IA
void ai_note_action(const char *what);
const char *ai_provider_name(ai_provider_t p);
const char *ai_default_model(ai_provider_t p);
