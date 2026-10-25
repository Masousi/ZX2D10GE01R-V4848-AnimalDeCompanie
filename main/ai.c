// ⚫ AJOUTÉ (25/10/2026) : IA du comportement (voir ai.h).
// Tâche sur le cœur 1 (comme le WiFi et la météo). Une seule écriture flash : quand on enregistre les réglages.
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"
#include "nvs.h"
#include "wifi_cfg.h"
#include "weather.h"
#include "pet.h"
#include "ai.h"

#define TAG "IA"
#define RESP_SIZE   8192
#define MIN_GAP_S   60           // au plus un appel par minute (quotas gratuits)

typedef struct {
    uint8_t provider;
    uint8_t period_min;
    char key[200];
    char model[64];
    char url[96];
} ai_store_t;

// static ai_store_t s_cfg = { .provider = AI_OFF, .period_min = 10 };
static ai_store_t s_cfg = { .provider = AI_OFF, .period_min = 5 };   // ⚫ MODIFIÉ (25/10/2026) : plus vivant
static ai_status_t s_st;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static volatile bool s_want, s_force;
static volatile bool s_new;            // décision pas encore prise par l'interface
static uint32_t s_last_call_s;
static char s_last_action[80] = "aucune";
static uint32_t s_last_action_s;

static const char *const NAMES[AI_PROVIDER_COUNT] = { "Désactivée", "Google Gemini", "Groq", "OpenRouter", "Ollama (PC)" };
static const char *const MODELS[AI_PROVIDER_COUNT] = {
    // ⚫ MODIFIÉ (25/10/2026) : Gemini : alias toujours valide (le modèle 2.5 a été retiré : HTTP 404)
    "", "gemini-flash-latest", "llama-3.1-8b-instant", "meta-llama/llama-3.3-70b-instruct:free", "llama3.2",
};
static const char *const URLS[AI_PROVIDER_COUNT] = {
    "", "https://generativelanguage.googleapis.com/v1beta/openai", "https://api.groq.com/openai/v1",
    "https://openrouter.ai/api/v1", "http://192.168.0.10:11434/v1",
};

// Comportements que l'IA peut choisir (appliqués par main/pet_ui.c)
// static const char *const BEHAVIOURS[] = { "flaner", "renifler", "jouer_seul", "se_reposer", "s_abriter", "bouder",
//                                           "reclamer", "observer" };
// ⚫ MODIFIÉ (25/10/2026) : l'IA fait vivre l'animal : demandes au maître et bêtises (rares)
static const char *const BEHAVIOURS[] = { "flaner", "renifler", "jouer_seul", "se_reposer", "s_abriter", "bouder",
                                          "observer", "reclamer_calin", "reclamer_manger", "reclamer_eau",
                                          "reclamer_jeu", "demander_sortir", "betise_creuser", "betise_gamelle",
                                          "betise_eau", "betise_besoins", "betise_cone" };   // ⚫ 25/10/2026 : + besoins, cône

static uint32_t now_s(void) { return (uint32_t)(esp_timer_get_time() / 1000000); }

const char *ai_provider_name(ai_provider_t p) { return p < AI_PROVIDER_COUNT ? NAMES[p] : "?"; }
const char *ai_default_model(ai_provider_t p) { return p < AI_PROVIDER_COUNT ? MODELS[p] : ""; }

/* ---------- réglages ---------- */

static void cfg_load(void)
{
    nvs_handle_t h;
    if (nvs_open("ai", NVS_READONLY, &h) != ESP_OK) return;
    size_t len = sizeof(s_cfg);
    ai_store_t c;
    if (nvs_get_blob(h, "cfg", &c, &len) == ESP_OK && len == sizeof(c) && c.provider < AI_PROVIDER_COUNT) s_cfg = c;
    nvs_close(h);
}

static void cfg_save(void)
{
    nvs_handle_t h;
    if (nvs_open("ai", NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_blob(h, "cfg", &s_cfg, sizeof(s_cfg));
    nvs_commit(h);
    nvs_close(h);
}

void ai_get_cfg(ai_cfg_t *out)
{
    memset(out, 0, sizeof(*out));
    portENTER_CRITICAL(&s_mux);
    out->provider = s_cfg.provider;
    out->period_min = s_cfg.period_min;
    strlcpy(out->model, s_cfg.model, sizeof(out->model));
    strlcpy(out->url, s_cfg.url, sizeof(out->url));
    size_t kl = strlen(s_cfg.key);
    out->has_key = kl > 0;
    if (kl >= 4) strlcpy(out->key_tail, s_cfg.key + kl - 4, sizeof(out->key_tail));
    portEXIT_CRITICAL(&s_mux);
}

void ai_set_cfg(ai_provider_t p, const char *key, const char *model, const char *url, int period_min)
{
    if (p >= AI_PROVIDER_COUNT) p = AI_OFF;
    if (period_min < 3) period_min = 3;
    if (period_min > 120) period_min = 120;
    portENTER_CRITICAL(&s_mux);
    if (p != s_cfg.provider && !(key && key[0])) s_cfg.key[0] = '\0';   // autre fournisseur : autre clé
    s_cfg.provider = p;
    if (key && key[0]) strlcpy(s_cfg.key, key, sizeof(s_cfg.key));
    strlcpy(s_cfg.model, model ? model : "", sizeof(s_cfg.model));
    strlcpy(s_cfg.url, url ? url : "", sizeof(s_cfg.url));
    s_cfg.period_min = (uint8_t)period_min;
    portEXIT_CRITICAL(&s_mux);
    cfg_save();
    ESP_LOGI(TAG, "Réglages : %s, modèle %s, toutes les %d min", NAMES[p], model && model[0] ? model : MODELS[p], period_min);
    s_force = true;
}

void ai_get_status(ai_status_t *out)
{
    portENTER_CRITICAL(&s_mux);
    *out = s_st;
    portEXIT_CRITICAL(&s_mux);
    out->age_s = s_last_call_s ? now_s() - s_last_call_s : 0;
}

void ai_trigger(bool force)
{
    if (force) s_force = true;
    else s_want = true;
}

void ai_note_action(const char *what)
{
    portENTER_CRITICAL(&s_mux);
    strlcpy(s_last_action, what, sizeof(s_last_action));
    s_last_action_s = now_s();
    portEXIT_CRITICAL(&s_mux);
}

bool ai_take(char *behaviour, size_t nb, char *phrase, size_t np)
{
    if (!s_new) return false;
    portENTER_CRITICAL(&s_mux);
    strlcpy(behaviour, s_st.behaviour, nb);
    strlcpy(phrase, s_st.phrase, np);
    s_new = false;
    portEXIT_CRITICAL(&s_mux);
    return true;
}

/* ---------- appel ---------- */

static void set_error(const char *fmt, const char *a)
{
    portENTER_CRITICAL(&s_mux);
    s_st.ok = false;
    snprintf(s_st.error, sizeof(s_st.error), fmt, a ? a : "");
    portEXIT_CRITICAL(&s_mux);
    ESP_LOGW(TAG, "%s", s_st.error);
}

// Garde une phrase affichable par la police de l'écran (ASCII + accents français), 90 octets au plus
static void sanitize(const char *in, char *out, size_t n)
{
    size_t o = 0;
    const unsigned char *s = (const unsigned char *)in;
    while (*s && o + 3 < n && o < 90) {
        if (*s == 0xE2 && s[1] == 0x80 && (s[2] == 0x99 || s[2] == 0x98)) { out[o++] = '\''; s += 3; continue; }   // ’ ‘
        if (*s == 0xE2 && s[1] == 0x80 && (s[2] == 0x9C || s[2] == 0x9D)) { out[o++] = '"'; s += 3; continue; }    // “ ”
        if (*s < 0x80) { if (*s >= 0x20) out[o++] = *s; s++; continue; }
        if ((*s == 0xC2 || *s == 0xC3) && s[1] >= 0x80 && s[1] < 0xC0) { out[o++] = s[0]; out[o++] = s[1]; s += 2; continue; }
        // autre caractère (emoji…) : ignoré
        s++;
        while (*s >= 0x80 && *s < 0xC0) s++;
    }
    out[o] = '\0';
}

static char *build_request(const char *model)
{
    const pet_t *p = pet_get();
    weather_t w;
    weather_get(&w);
    char clk[8] = "?";
    weather_clock(clk, sizeof(clk));
    char beh[160] = "";
    for (size_t i = 0; i < sizeof(BEHAVIOURS) / sizeof(BEHAVIOURS[0]); i++) {
        strlcat(beh, i ? ", " : "", sizeof(beh));
        strlcat(beh, BEHAVIOURS[i], sizeof(beh));
    }
    // char sys[900];
    char sys[1900];   // ⚫ MODIFIÉ (25/10/2026) : consignes plus longues (vie de l'animal)
    snprintf(sys, sizeof(sys),
             "Tu es le cerveau d'un animal de compagnie virtuel (un %s nommé %s, %s) qui vit dans une cour avec un abri, "
             "une litière, deux gamelles, un coin pour ses besoins et une piste. Tu décides de son comportement de façon "
             "réaliste pour son espèce et son âge, selon ses besoins, son humeur, la météo et l'heure. "
             "Réponds UNIQUEMENT par un objet JSON, sans texte autour : "
             "{\"comportement\": un parmi [%s], \"phrase\": une phrase courte (60 caractères max, en français, sans emoji) "
             "qui décrit ce que fait ou ressent l'animal, à la 3e personne}. Varie les phrases. "
             // ⚫ AJOUTÉ (25/10/2026) : faire vivre l'animal
             "Fais-le vivre comme un vrai animal : il réclame un câlin quand il se sent seul (bonheur bas, pas de caresse "
             "depuis longtemps), à manger si faim < 60, à boire si soif < 60, à jouer s'il a de l'énergie et s'ennuie, "
             "à sortir si besoins > 50 ; il s'abrite s'il pleut ; il se repose si énergie < 30 ou s'il est malade. Les bêtises "
             "(betise_creuser, betise_gamelle, betise_eau, betise_cone = renverser un cône de la piste, betise_besoins = "
             "faire ses besoins n'importe où, seulement si besoins > 40) restent rares : plus probables s'il s'ennuie, est jeune ou peu "
             "discipliné, presque jamais si discipline > 70 ou s'il vient d'être grondé. Ne répète pas la même demande "
             "si le maître vient d'y répondre.",
             pet_species_label(p->species), pet_name(), pet_stage_text(pet_stage()), beh);
    char usr[700];
    uint32_t since = s_last_action_s ? now_s() - s_last_action_s : 0;
    snprintf(usr, sizeof(usr),
             "État : humeur \"%s\", faim %d/100 (100 = rassasié), soif %d/100 (100 = désaltéré), énergie %d/100, "
             "besoins %d/100 (100 = doit sortir), bonheur %d/100, discipline %d/100, santé %d/100. "
             "%s%s%sMétéo : %s, %.0f °C, vent %.0f km/h ; heure %s (%s). "
             "Dernière action du maître : %s (il y a %lu s). Ton dernier comportement : %s.",
             pet_mood_text(pet_mood()), (int)p->s.food, (int)p->s.water, (int)p->s.energy, (int)p->s.potty,
             (int)p->s.joy, (int)p->s.discipline, (int)p->s.health,
             p->n_holes ? "Il y a des trous dans la pelouse. " : "", p->n_poops ? "Il y a des besoins à ramasser. " : "",
             p->sick ? "Il est MALADE. " : "",
             w.valid ? weather_text(w.kind) : "inconnue", w.temp, w.wind, clk,
             weather_tod() == TOD_NIGHT ? "nuit" : weather_tod() == TOD_DUSK ? "aube/crépuscule" : "jour",
             s_last_action, (unsigned long)since, s_st.behaviour[0] ? s_st.behaviour : "aucun");

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "model", model);
    cJSON *msgs = cJSON_AddArrayToObject(root, "messages");
    cJSON *m1 = cJSON_CreateObject();
    cJSON_AddStringToObject(m1, "role", "system");
    cJSON_AddStringToObject(m1, "content", sys);
    cJSON_AddItemToArray(msgs, m1);
    cJSON *m2 = cJSON_CreateObject();
    cJSON_AddStringToObject(m2, "role", "user");
    cJSON_AddStringToObject(m2, "content", usr);
    cJSON_AddItemToArray(msgs, m2);
    cJSON_AddNumberToObject(root, "temperature", 0.9);
    // cJSON_AddNumberToObject(root, "max_tokens", 300);
    // if (s_cfg.provider == AI_GEMINI) cJSON_AddStringToObject(root, "reasoning_effort", "none");   // réponse rapide
    // ⚫ MODIFIÉ (25/10/2026) : "reasoning_effort: none" refusé par les modèles Gemini récents (HTTP 400) ; les
    // modèles qui réfléchissent consomment des jetons avant de répondre : marge plus grande
    cJSON_AddNumberToObject(root, "max_tokens", 1500);
    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return out;
}

static void call_ai(void)
{
    ai_store_t c;
    portENTER_CRITICAL(&s_mux);
    c = s_cfg;
    portEXIT_CRITICAL(&s_mux);
    const char *model = c.model[0] ? c.model : MODELS[c.provider];
    const char *base = c.url[0] ? c.url : URLS[c.provider];
    if (c.provider != AI_OLLAMA && !c.key[0]) { set_error("Clé API manquante%s", ""); return; }

    char url[160];
    size_t bl = strlen(base);
    // l'adresse peut être donnée avec ou sans /v1 (Ollama : http://PC:11434)
    snprintf(url, sizeof(url), "%.*s%s/chat/completions", (int)(bl && base[bl - 1] == '/' ? bl - 1 : bl), base,
             c.provider == AI_OLLAMA && !strstr(base, "/v1") ? "/v1" : "");
    char *resp = malloc(RESP_SIZE);
    if (!resp) { set_error("Mémoire insuffisante%s", ""); return; }
    int len = -1, status = 0;
    // ⚫ AJOUTÉ (25/10/2026) : Gemini surchargé (503) ou quota (429) : nouvel essai tout de suite avec le modèle "lite"
    for (int attempt = 0; attempt < 2; attempt++) {
    char *body = build_request(model);
    if (!body) { free(resp); set_error("Mémoire insuffisante%s", ""); return; }

    esp_http_client_config_t hc = {
        .url = url,
        .method = HTTP_METHOD_POST,
        // .timeout_ms = 20000,
        .timeout_ms = 45000,   // ⚫ MODIFIÉ (25/10/2026) : les modèles qui réfléchissent sont plus lents
        .crt_bundle_attach = esp_crt_bundle_attach,
        .user_agent = "AnimalDeCompagnie/1.0 (ESP32-S3)",
        .buffer_size = 2048,
        .buffer_size_tx = 1024,
    };
    esp_http_client_handle_t h = esp_http_client_init(&hc);
    if (!h) { free(body); free(resp); set_error("Client HTTP impossible%s", ""); return; }
    esp_http_client_set_header(h, "Content-Type", "application/json");
    char auth[220];
    if (c.key[0]) {
        snprintf(auth, sizeof(auth), "Bearer %s", c.key);
        esp_http_client_set_header(h, "Authorization", auth);
    }
    if (c.provider == AI_OPENROUTER) esp_http_client_set_header(h, "X-Title", "AnimalDeCompagnie");

    len = -1; status = 0;
    size_t blen = strlen(body);
    if (esp_http_client_open(h, blen) == ESP_OK) {
        esp_http_client_write(h, body, blen);
        esp_http_client_fetch_headers(h);
        status = esp_http_client_get_status_code(h);
        int r;
        len = 0;
        while (len < RESP_SIZE - 1 && (r = esp_http_client_read(h, resp + len, RESP_SIZE - 1 - len)) > 0) len += r;
        resp[len] = '\0';
        esp_http_client_close(h);
    }
    esp_http_client_cleanup(h);
    free(body);
    if ((status == 503 || status == 429) && c.provider == AI_GEMINI && !c.model[0] && attempt == 0) {
        ESP_LOGW(TAG, "Gemini HTTP %d : essai avec gemini-flash-lite-latest", status);
        model = "gemini-flash-lite-latest";
        continue;
    }
    break;
    }
    s_last_call_s = now_s();
    portENTER_CRITICAL(&s_mux);
    s_st.calls++;
    portEXIT_CRITICAL(&s_mux);

    if (len < 0) { free(resp); set_error("Connexion impossible à %s", base); return; }
    if (status != 200) {
        // char e[64];
        // snprintf(e, sizeof(e), "%d", status);
        // ESP_LOGW(TAG, "Réponse : %.300s", resp);
        // ⚫ MODIFIÉ (25/10/2026) : message d'erreur du service (error.message, Google renvoie [ {error} ]) affiché sur la page
        char e[72];
        cJSON *ej = cJSON_Parse(resp);
        const cJSON *eo = cJSON_IsArray(ej) ? cJSON_GetArrayItem(ej, 0) : ej;
        const cJSON *em = eo ? cJSON_GetObjectItem(cJSON_GetObjectItem(eo, "error"), "message") : NULL;
        snprintf(e, sizeof(e), "%d%s%.56s", status, cJSON_IsString(em) ? " : " : "", cJSON_IsString(em) ? em->valuestring : "");
        cJSON_Delete(ej);
        for (char *q = resp; *q; q++) if (*q == '\n' || *q == '\r') *q = ' ';
        ESP_LOGW(TAG, "Réponse : %.400s", resp);
        free(resp);
        set_error(status == 401 || status == 403 ? "Clé refusée (HTTP %s)" : status == 429 ? "Quota dépassé (HTTP %s), réessai plus tard"
                  : status == 404 ? "Modèle ou adresse introuvable (HTTP %s)" : "Erreur du service (HTTP %s)", e);
        return;
    }
    // choices[0].message.content -> objet JSON {comportement, phrase}
    cJSON *j = cJSON_Parse(resp);
    free(resp);
    const cJSON *ch = j ? cJSON_GetArrayItem(cJSON_GetObjectItem(j, "choices"), 0) : NULL;
    const cJSON *ct = ch ? cJSON_GetObjectItem(cJSON_GetObjectItem(ch, "message"), "content") : NULL;
    const char *txt = cJSON_IsString(ct) ? ct->valuestring : NULL;
    const char *a = txt ? strchr(txt, '{') : NULL, *b = txt ? strrchr(txt, '}') : NULL;
    cJSON *d = (a && b && b > a) ? cJSON_ParseWithLength(a, b - a + 1) : NULL;
    const cJSON *jb = d ? cJSON_GetObjectItem(d, "comportement") : NULL, *jp = d ? cJSON_GetObjectItem(d, "phrase") : NULL;
    if (!cJSON_IsString(jb) || !cJSON_IsString(jp)) {
        set_error("Réponse illisible%s", "");
        if (txt) ESP_LOGW(TAG, "Texte reçu : %.200s", txt);
    } else {
        bool known = false;
        for (size_t i = 0; i < sizeof(BEHAVIOURS) / sizeof(BEHAVIOURS[0]); i++) if (!strcmp(jb->valuestring, BEHAVIOURS[i])) known = true;
        portENTER_CRITICAL(&s_mux);
        s_st.ok = true;
        s_st.error[0] = '\0';
        strlcpy(s_st.behaviour, known ? jb->valuestring : "flaner", sizeof(s_st.behaviour));
        sanitize(jp->valuestring, s_st.phrase, sizeof(s_st.phrase));
        s_new = true;
        portEXIT_CRITICAL(&s_mux);
        ESP_LOGI(TAG, "%s : %s / \"%s\"", NAMES[c.provider], s_st.behaviour, s_st.phrase);
    }
    cJSON_Delete(d);
    cJSON_Delete(j);
}

static void ai_task(void *arg)
{
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        if (s_cfg.provider == AI_OFF) { s_want = s_force = false; continue; }
        wifi_cfg_status_t ws;
        wifi_cfg_get_status(&ws);
        const pet_t *p = pet_get();
        if (!ws.sta_connected || !p->adopted) continue;
        uint32_t t = now_s(), since = s_last_call_s ? t - s_last_call_s : 100000;
        bool due = since >= (uint32_t)s_cfg.period_min * 60 && !p->sleeping;   // pas d'appel pendant son sommeil
        bool asked = (s_want && since >= MIN_GAP_S) || (s_force && since >= 5);
        if (!due && !asked) continue;
        s_want = s_force = false;
        call_ai();
    }
}

void ai_start(void)
{
    cfg_load();
    xTaskCreatePinnedToCore(ai_task, "ia", 8192, NULL, 2, NULL, 1);
    ESP_LOGI(TAG, "IA : %s", NAMES[s_cfg.provider]);
}
