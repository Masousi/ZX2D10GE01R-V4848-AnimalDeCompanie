// ⚫ AJOUTÉ (25/10/2026) : serveur WiFi de configuration (voir wifi_cfg.h).
// Réseau de la carte (AP) + connexion au WiFi de la maison (STA), serveur web (page main/wifi_page.html),
// API JSON (/api/status, /api/scan, /api/wifi, /api/forget, /api/pet) et DNS "portail captif" (toute requête
// DNS -> 192.168.4.1, donc le téléphone ouvre la page tout seul).
// Le pilote WiFi n'écrit rien en flash (WIFI_STORAGE_RAM, CONFIG_ESP32_WIFI_NVS_ENABLED=n) : seuls les
// identifiants validés dans la page sont sauvegardés (namespace NVS "wifi"), chaque écriture flash perturbant l'écran.
// ⚫ MODIFIÉ (25/10/2026) : réseau de la carte OUVERT (on le rejoint en scannant le QR code de l'écran) ;
// + tableau de bord : /api/scene (scène en direct, redessinée par la page avec les mêmes images), /api/pack et
// /api/img (images du pack, en cache dans le navigateur), /api/species, /api/adopt (nom + espèce, une fois),
// /api/reset (efface toutes les données de l'animal).
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "esp_http_server.h"
#include "nvs.h"
#include "lwip/sockets.h"
#include "sdkconfig.h"
#include "pet.h"
#include "pet_ui.h"        // ⚫ AJOUTÉ (25/10/2026)
#include "pet_assets.h"    // ⚫ AJOUTÉ (25/10/2026)
#include "weather.h"       // ⚫ AJOUTÉ (25/10/2026)
#include "ai.h"            // ⚫ AJOUTÉ (25/10/2026)
#include "wifi_cfg.h"

#define TAG "WIFI"

#if CONFIG_PET_WIFI

#define CORE         1          // WiFi, serveur web et DNS sur le cœur 1 (le cœur 0 sert l'écran)
#define NVS_NS       "wifi"
#define MAX_SCAN     20

extern const char s_page_start[] asm("_binary_wifi_page_html_start");
extern const char s_page_end[]   asm("_binary_wifi_page_html_end");

static wifi_cfg_status_t s_st;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static char s_ssid[33], s_pass[65];
static esp_timer_handle_t s_retry_timer;
static int s_retry;
static bool s_scanning;

/* ---------- identifiants (NVS) ---------- */

static void creds_load(void)
{
    nvs_handle_t h;
    s_ssid[0] = s_pass[0] = '\0';
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return;
    size_t l1 = sizeof(s_ssid), l2 = sizeof(s_pass);
    if (nvs_get_str(h, "ssid", s_ssid, &l1) != ESP_OK) s_ssid[0] = '\0';
    if (nvs_get_str(h, "pass", s_pass, &l2) != ESP_OK) s_pass[0] = '\0';
    nvs_close(h);
}

static esp_err_t creds_save(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    if (s_ssid[0]) {
        err = nvs_set_str(h, "ssid", s_ssid);
        if (err == ESP_OK) err = nvs_set_str(h, "pass", s_pass);
    } else {
        err = nvs_erase_all(h);
    }
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

/* ---------- WiFi ---------- */

static void sta_apply(void)
{
    wifi_config_t c = { 0 };
    strlcpy((char *)c.sta.ssid, s_ssid, sizeof(c.sta.ssid));
    strlcpy((char *)c.sta.password, s_pass, sizeof(c.sta.password));
    c.sta.pmf_cfg.capable = true;
    esp_wifi_set_config(WIFI_IF_STA, &c);
    portENTER_CRITICAL(&s_mux);
    s_st.sta_configured = s_ssid[0] != '\0';
    strlcpy(s_st.sta_ssid, s_ssid, sizeof(s_st.sta_ssid));
    portEXIT_CRITICAL(&s_mux);
}

static void retry_cb(void *arg)
{
    if (s_ssid[0] && !s_scanning) esp_wifi_connect();
}

static void on_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT) {
        switch (id) {
        case WIFI_EVENT_STA_START:
            if (s_ssid[0]) esp_wifi_connect();
            break;
        case WIFI_EVENT_STA_DISCONNECTED: {
            // ⚫ AJOUTÉ (25/10/2026) : raison affichée (15 / 204 / 205 = mot de passe refusé, 201 = réseau introuvable)
            uint8_t why = ((wifi_event_sta_disconnected_t *)data)->reason;
            ESP_LOGW(TAG, "WiFi maison perdu (raison %d)", why);
            portENTER_CRITICAL(&s_mux);
            s_st.sta_reason = why;
            s_st.sta_connected = false;
            s_st.sta_ip[0] = '\0';
            portEXIT_CRITICAL(&s_mux);
            if (s_ssid[0] && !s_scanning) {
                // nouvel essai : 2 s, 4 s, 8 s… jusqu'à 30 s
                uint64_t ms = 2000ULL << (s_retry < 4 ? s_retry : 4);
                if (ms > 30000) ms = 30000;
                s_retry++;
                esp_timer_stop(s_retry_timer);
                esp_timer_start_once(s_retry_timer, ms * 1000);
            }
            break;
        }
        case WIFI_EVENT_AP_STACONNECTED:
            portENTER_CRITICAL(&s_mux); s_st.ap_clients++; portEXIT_CRITICAL(&s_mux);
            break;
        case WIFI_EVENT_AP_STADISCONNECTED:
            portENTER_CRITICAL(&s_mux); if (s_st.ap_clients > 0) s_st.ap_clients--; portEXIT_CRITICAL(&s_mux);
            break;
        default:
            break;
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *e = data;
        wifi_ap_record_t ap;
        int8_t rssi = (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) ? ap.rssi : 0;
        portENTER_CRITICAL(&s_mux);
        s_st.sta_connected = true;
        s_st.sta_reason = 0;   // ⚫ AJOUTÉ (25/10/2026)
        snprintf(s_st.sta_ip, sizeof(s_st.sta_ip), IPSTR, IP2STR(&e->ip_info.ip));
        s_st.sta_rssi = rssi;
        portEXIT_CRITICAL(&s_mux);
        s_retry = 0;
        ESP_LOGI(TAG, "Connecté à %s, IP %s", s_ssid, s_st.sta_ip);
    }
}

void wifi_cfg_get_status(wifi_cfg_status_t *out)
{
    portENTER_CRITICAL(&s_mux);
    *out = s_st;
    portEXIT_CRITICAL(&s_mux);
}

/* ---------- utilitaires HTTP / JSON ---------- */

// Ajoute `s` à `out` en chaîne JSON échappée
static int json_str(char *out, size_t n, const char *s)
{
    size_t o = 0;
    if (o < n) out[o++] = '"';
    for (; *s && o + 7 < n; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') { out[o++] = '\\'; out[o++] = c; }
        else if (c < 0x20) o += snprintf(out + o, n - o, "\\u%04x", c);
        else out[o++] = c;
    }
    if (o < n) out[o++] = '"';
    if (o < n) out[o] = '\0';
    return (int)o;
}

static void url_decode(char *s)
{
    char *o = s;
    for (; *s; s++) {
        if (*s == '+') *o++ = ' ';
        else if (*s == '%' && s[1] && s[2]) {
            char h[3] = { s[1], s[2], 0 };
            *o++ = (char)strtol(h, NULL, 16);
            s += 2;
        } else *o++ = *s;
    }
    *o = '\0';
}

static esp_err_t send_json(httpd_req_t *req, const char *json)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_sendstr(req, json);
}

/* ---------- pages ---------- */

static esp_err_t h_root(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, s_page_start, s_page_end - s_page_start - 1);   // EMBED_TXTFILES ajoute un \0
}

static esp_err_t h_status(httpd_req_t *req)
{
    wifi_cfg_status_t st;
    wifi_cfg_get_status(&st);
    char buf[256], ssid[80], ap[80];
    json_str(ssid, sizeof(ssid), st.sta_ssid);
    json_str(ap, sizeof(ap), CONFIG_PET_WIFI_AP_SSID);
    // snprintf(buf, sizeof(buf), "{\"ap\":%s,\"clients\":%d,\"configured\":%s,\"connected\":%s,\"ssid\":%s,\"ip\":\"%s\",\"rssi\":%d}",
    //          ap, st.ap_clients, st.sta_configured ? "true" : "false", st.sta_connected ? "true" : "false",
    //          ssid, st.sta_ip, st.sta_rssi);
    // ⚫ MODIFIÉ (25/10/2026) : + raison de la déconnexion
    snprintf(buf, sizeof(buf), "{\"ap\":%s,\"clients\":%d,\"configured\":%s,\"connected\":%s,\"ssid\":%s,\"ip\":\"%s\",\"rssi\":%d,\"reason\":%d}",
             ap, st.ap_clients, st.sta_configured ? "true" : "false", st.sta_connected ? "true" : "false",
             ssid, st.sta_ip, st.sta_rssi, st.sta_reason);
    return send_json(req, buf);
}

static esp_err_t h_scan(httpd_req_t *req)
{
    // un scan est impossible pendant une tentative de connexion : on la suspend le temps du scan
    s_scanning = true;
    esp_timer_stop(s_retry_timer);
    wifi_cfg_status_t st;
    wifi_cfg_get_status(&st);
    if (!st.sta_connected) esp_wifi_disconnect();

    wifi_scan_config_t sc = { .show_hidden = false };
    esp_err_t err = esp_wifi_scan_start(&sc, true);
    uint16_t n = MAX_SCAN;
    static wifi_ap_record_t rec[MAX_SCAN];
    if (err == ESP_OK) err = esp_wifi_scan_get_ap_records(&n, rec);
    s_scanning = false;
    if (!st.sta_connected && s_ssid[0]) esp_wifi_connect();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Scan impossible (%s)", esp_err_to_name(err));
        return send_json(req, "[]");
    }

    char *buf = malloc(2600);
    if (!buf) return httpd_resp_send_500(req);
    size_t o = 0;
    buf[o++] = '[';
    for (int i = 0; i < n; i++) {
        if (!rec[i].ssid[0]) continue;
        bool dup = false;   // même réseau vu sur plusieurs bornes : on garde le premier (le plus fort)
        for (int j = 0; j < i; j++) if (!strcmp((char *)rec[j].ssid, (char *)rec[i].ssid)) { dup = true; break; }
        if (dup) continue;
        if (o > 1) buf[o++] = ',';
        o += snprintf(buf + o, 2600 - o, "{\"ssid\":");
        o += json_str(buf + o, 2600 - o, (char *)rec[i].ssid);
        o += snprintf(buf + o, 2600 - o, ",\"rssi\":%d,\"lock\":%s}", rec[i].rssi,
                      rec[i].authmode == WIFI_AUTH_OPEN ? "false" : "true");
        if (o > 2500) break;
    }
    buf[o++] = ']';
    buf[o] = '\0';
    esp_err_t r = send_json(req, buf);
    free(buf);
    return r;
}

static esp_err_t h_wifi(httpd_req_t *req)
{
    char body[200] = { 0 };
    int len = httpd_req_recv(req, body, sizeof(body) - 1);
    if (len <= 0) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "corps vide");
    body[len] = '\0';
    char ssid[100] = { 0 }, pass[200] = { 0 };
    httpd_query_key_value(body, "ssid", ssid, sizeof(ssid));
    httpd_query_key_value(body, "pass", pass, sizeof(pass));
    url_decode(ssid);
    url_decode(pass);
    if (!ssid[0] || strlen(ssid) > 32 || strlen(pass) > 64) {
        return send_json(req, "{\"ok\":false,\"msg\":\"Nom de réseau ou mot de passe invalide\"}");
    }
    strlcpy(s_ssid, ssid, sizeof(s_ssid));
    strlcpy(s_pass, pass, sizeof(s_pass));
    esp_err_t err = creds_save();
    ESP_LOGI(TAG, "Nouveau WiFi : %s (sauvegarde %s)", s_ssid, esp_err_to_name(err));
    esp_wifi_disconnect();
    sta_apply();
    s_retry = 0;
    esp_wifi_connect();
    return send_json(req, err == ESP_OK ? "{\"ok\":true}" : "{\"ok\":false,\"msg\":\"Sauvegarde impossible\"}");
}

static esp_err_t h_forget(httpd_req_t *req)
{
    s_ssid[0] = s_pass[0] = '\0';
    esp_timer_stop(s_retry_timer);
    esp_wifi_disconnect();
    creds_save();
    sta_apply();
    ESP_LOGI(TAG, "WiFi de la maison oublié");
    return send_json(req, "{\"ok\":true}");
}

static esp_err_t h_pet(httpd_req_t *req)
{
    const pet_t *p = pet_get();   // lecture seule depuis une autre tâche : valeurs indicatives
    // char buf[600], name[64];
    char buf[800], name[64];   // ⚫ MODIFIÉ (25/10/2026)
    // json_str(name, sizeof(name), CONFIG_PET_NAME);
    json_str(name, sizeof(name), pet_name());   // ⚫ MODIFIÉ (25/10/2026) : nom choisi à l'adoption
    int hd;                                    // ⚫ AJOUTÉ (25/10/2026) : dernière perte de santé
    const char *hwhy;
    uint32_t hseq = pet_health_event(&hd, &hwhy);
    // ⚫ MODIFIÉ (25/10/2026) : + adoption, espèce, besoins, dégâts de la cour
    snprintf(buf, sizeof(buf),
             "{\"adopted\":%s,\"name\":%s,\"species\":%d,\"speciesLabel\":\"%s\",\"sound\":\"%s\","
             "\"stage\":\"%s\",\"age\":%lu,\"mood\":\"%s\",\"sleeping\":%s,"
             "\"food\":%d,\"water\":%d,\"energy\":%d,\"potty\":%d,\"joy\":%d,\"discipline\":%d,\"health\":%d,"
             // "\"holes\":%d,\"poops\":%d,\"foodTipped\":%s,\"waterTipped\":%s,\"wellbeing\":%d}",
             // ⚫ MODIFIÉ (25/10/2026) : + âge (0..2) et dernière perte de santé (numéro, points, raison)
             "\"holes\":%d,\"poops\":%d,\"foodTipped\":%s,\"waterTipped\":%s,\"wellbeing\":%d,"
             "\"stageId\":%d,\"hseq\":%lu,\"hdelta\":%d,\"hwhy\":\"%s\",\"sick\":%d,\"cones\":%d}",   // ⚫ 25/10/2026
             p->adopted ? "true" : "false", name, p->species, pet_species_label(p->species), pet_sound(),
             pet_stage_text(pet_stage()), (unsigned long)p->age_s, pet_mood_text(pet_mood()),
             p->sleeping ? "true" : "false", (int)p->s.food, (int)p->s.water, (int)p->s.energy, (int)p->s.potty,
             (int)p->s.joy, (int)p->s.discipline, (int)p->s.health, p->n_holes, p->n_poops,
             p->food_tipped ? "true" : "false", p->water_tipped ? "true" : "false", pet_wellbeing(),
             (int)pet_stage(), (unsigned long)hseq, hd, hwhy, p->sick, p->cones_down);
    return send_json(req, buf);
}

/* ---------- ⚫ AJOUTÉ (25/10/2026) : tableau de bord, adoption, réinitialisation ---------- */

// Scène affichée à l'écran (objets, positions, opacités), redessinée par la page
static esp_err_t h_scene(httpd_req_t *req)
{
    // static char buf[3600];
    static char buf[6144];   // ⚫ MODIFIÉ (25/10/2026) : scène plus riche (pluie, neige)
    pet_ui_scene_json(buf, sizeof(buf));
    return send_json(req, buf);
}

// Description du pack d'images : la page télécharge chaque image une fois (/api/img)
static esp_err_t h_pack(httpd_req_t *req)
{
    int n = pet_pack_count();
    char *buf = malloc(64 + n * 40 + PET_ANIMAL_BASE * 24);
    if (!buf) return httpd_resp_send_500(req);
    size_t o = snprintf(buf, 64, "{\"v\":%u,\"base\":%d,\"poses\":%d,\"e\":[", (unsigned)PET_PACK_SIZE, PET_ANIMAL_BASE, PET_POSE_COUNT);
    for (int i = 0; i < n; i++) {
        uint16_t w, h; int16_t ox, oy; uint8_t cf;
        pet_pack_entry(i, NULL, &w, &h, &ox, &oy, &cf);
        o += sprintf(buf + o, "%s[%u,%u,%d,%d,%u]", i ? "," : "", w, h, ox, oy, cf);
    }
    // noms des images de décor / accessoires (la page ne teinte pas la nuit le ciel, les lucioles, les nuages)
    static const char *const names[] = {
#define X(name, idx) #name,
        PET_ASSETS(X)
#undef X
    };
    o += sprintf(buf + o, "],\"names\":[");
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) o += sprintf(buf + o, "%s\"%s\"", i ? "," : "", names[i]);
    o += sprintf(buf + o, "]}");
    esp_err_t r = send_json(req, buf);
    free(buf);
    return r;
}

static esp_err_t h_img(httpd_req_t *req)
{
    char q[32], v[8];
    int idx = -1;
    if (httpd_req_get_url_query_str(req, q, sizeof(q)) == ESP_OK && httpd_query_key_value(q, "i", v, sizeof(v)) == ESP_OK) idx = atoi(v);
    uint32_t size;
    const uint8_t *d = pet_pack_entry(idx, &size, NULL, NULL, NULL, NULL, NULL);
    if (!d) return httpd_resp_send_404(req);
    httpd_resp_set_type(req, "application/octet-stream");
    httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=31536000, immutable");   // URL versionnée (?v=)
    // for (uint32_t off = 0; off < size; off += 4096) {
    //     uint32_t len = size - off < 4096 ? size - off : 4096;
    //     if (httpd_resp_send_chunk(req, (const char *)d + off, len) != ESP_OK) return ESP_FAIL;
    // }
    // ⚫ MODIFIÉ (25/10/2026) : envoi par petits morceaux avec une pause : une lecture rapide de la flash occupe le
    // bus mémoire partagé avec l'écran (PSRAM) et peut décaler l'image quand la page charge une nouvelle image
    for (uint32_t off = 0; off < size; off += 1024) {
        uint32_t len = size - off < 1024 ? size - off : 1024;
        if (httpd_resp_send_chunk(req, (const char *)d + off, len) != ESP_OK) return ESP_FAIL;
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    return httpd_resp_send_chunk(req, NULL, 0);
}

static esp_err_t h_species(httpd_req_t *req)
{
    char buf[600];
    size_t o = snprintf(buf, sizeof(buf), "[");
    for (int i = 0; i < PET_SPECIES_COUNT; i++) {
        o += snprintf(buf + o, sizeof(buf) - o, "%s{\"id\":%d,\"label\":\"%s\",\"img\":%d}", i ? "," : "", i,
                      pet_species_label(i), PET_ANIMAL_BASE + i * PET_POSE_COUNT + P_stand_1);
    }
    snprintf(buf + o, sizeof(buf) - o, "]");
    return send_json(req, buf);
}

static esp_err_t h_adopt(httpd_req_t *req)
{
    char body[200] = { 0 };
    int len = httpd_req_recv(req, body, sizeof(body) - 1);
    if (len <= 0) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "corps vide");
    body[len] = '\0';
    char name[100] = { 0 }, sp[8] = { 0 };
    httpd_query_key_value(body, "name", name, sizeof(name));
    httpd_query_key_value(body, "species", sp, sizeof(sp));
    url_decode(name);
    int species = atoi(sp);
    if (pet_get()->adopted) return send_json(req, "{\"ok\":false,\"msg\":\"Un animal est déjà adopté : réinitialiser d'abord.\"}");
    if (!name[0] || strlen(name) > 16) return send_json(req, "{\"ok\":false,\"msg\":\"Le nom doit faire de 1 à 16 caractères.\"}");
    if (species < 0 || species >= PET_SPECIES_COUNT) return send_json(req, "{\"ok\":false,\"msg\":\"Espèce inconnue.\"}");
    pet_ui_request_adopt(name, species);
    ESP_LOGI(TAG, "Adoption demandée : %s (%s)", name, pet_species_label(species));
    return send_json(req, "{\"ok\":true}");
}

static esp_err_t h_reset(httpd_req_t *req)
{
    pet_ui_request_reset();
    ESP_LOGW(TAG, "Réinitialisation demandée depuis la page web");
    return send_json(req, "{\"ok\":true}");
}

/* ---------- ⚫ AJOUTÉ (25/10/2026) : météo et position ---------- */

static esp_err_t h_weather(httpd_req_t *req)
{
    weather_t w;
    weather_get(&w);
    char clk[8] = "";
    bool has_clk = weather_clock(clk, sizeof(clk));
    static const char *TOD[] = { "jour", "aube/crépuscule", "nuit" };
    tod_t t = weather_tod();
    char place[96];
    json_str(place, sizeof(place), w.place);
    char buf[512];
    snprintf(buf, sizeof(buf),
             "{\"valid\":%s,\"time_ok\":%s,\"clock\":\"%s\",\"tod\":\"%s\",\"kind\":%d,\"text\":\"%s\",\"temp\":%.1f,"
             "\"clouds\":%d,\"wind\":%.0f,\"sunrise\":\"%02d:%02d\",\"sunset\":\"%02d:%02d\",\"lat\":%.4f,\"lon\":%.4f,"
             "\"auto\":%s,\"place\":%s,\"age\":%lu}",
             w.valid ? "true" : "false", w.time_ok ? "true" : "false", has_clk ? clk : "", t == TOD_UNKNOWN ? "" : TOD[t],
             (int)w.kind, weather_text(w.kind), w.temp, w.clouds, w.wind, w.sunrise_min / 60, w.sunrise_min % 60,
             w.sunset_min / 60, w.sunset_min % 60, w.lat, w.lon, w.auto_loc ? "true" : "false", place, (unsigned long)w.updated_s);
    return send_json(req, buf);
}

// Recherche de ville : /api/geo?q=Lyon
static esp_err_t h_geo(httpd_req_t *req)
{
    char q[160] = { 0 }, name[100] = { 0 };
    if (httpd_req_get_url_query_str(req, q, sizeof(q)) != ESP_OK || httpd_query_key_value(q, "q", name, sizeof(name)) != ESP_OK)
        return send_json(req, "[]");
    url_decode(name);
    char *out = malloc(3072);
    if (!out) return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "mémoire");
    // esp_err_t r = send_json(req, weather_geocode(name, out, 3072) ? out : "[]");
    // ⚫ MODIFIÉ (25/10/2026) : erreur distincte de "aucune ville" (la page affiche le bon message)
    wifi_cfg_status_t st;
    wifi_cfg_get_status(&st);
    esp_err_t r = send_json(req, weather_geocode(name, out, 3072) ? out
                                 : st.sta_connected ? "{\"error\":\"service\"}" : "{\"error\":\"offline\"}");
    free(out);
    return r;
}

// Position choisie : lat=..&lon=..&name=..  ou  auto=1
static esp_err_t h_location(httpd_req_t *req)
{
    char body[200] = { 0 };
    int len = httpd_req_recv(req, body, sizeof(body) - 1);
    if (len <= 0) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "corps vide");
    body[len] = '\0';
    char v[16] = { 0 }, la[20] = { 0 }, lo[20] = { 0 }, name[100] = { 0 };
    if (httpd_query_key_value(body, "auto", v, sizeof(v)) == ESP_OK && v[0] == '1') {
        weather_set_auto();
        ESP_LOGI(TAG, "Position : automatique (adresse IP)");
        return send_json(req, "{\"ok\":true}");
    }
    httpd_query_key_value(body, "lat", la, sizeof(la));
    httpd_query_key_value(body, "lon", lo, sizeof(lo));
    httpd_query_key_value(body, "name", name, sizeof(name));
    url_decode(name);
    float lat = strtof(la, NULL), lon = strtof(lo, NULL);
    if (!la[0] || !lo[0] || lat < -90 || lat > 90 || lon < -180 || lon > 180)
        return send_json(req, "{\"ok\":false,\"msg\":\"Position invalide.\"}");
    weather_set_location(lat, lon, name);
    ESP_LOGI(TAG, "Position choisie : %s (%.4f, %.4f)", name, lat, lon);
    return send_json(req, "{\"ok\":true}");
}

/* ---------- ⚫ AJOUTÉ (25/10/2026) : IA ---------- */

static esp_err_t h_ai_get(httpd_req_t *req)
{
    ai_cfg_t c;
    ai_get_cfg(&c);
    ai_status_t s;
    ai_get_status(&s);
    char model[140], url[200], beh[60], ph[220], err[200], defs[400];
    json_str(model, sizeof(model), c.model);
    json_str(url, sizeof(url), c.url);
    json_str(beh, sizeof(beh), s.behaviour);
    json_str(ph, sizeof(ph), s.phrase);
    json_str(err, sizeof(err), s.error);
    size_t o = snprintf(defs, sizeof(defs), "[");
    for (int i = 0; i < AI_PROVIDER_COUNT; i++) {
        char m[80];
        json_str(m, sizeof(m), ai_default_model(i));
        o += snprintf(defs + o, sizeof(defs) - o, "%s%s", i ? "," : "", m);
    }
    snprintf(defs + o, sizeof(defs) - o, "]");
    char buf[1400];
    snprintf(buf, sizeof(buf),
             "{\"provider\":%d,\"model\":%s,\"url\":%s,\"period\":%d,\"hasKey\":%s,\"keyTail\":\"%s\",\"defaults\":%s,"
             "\"ok\":%s,\"calls\":%d,\"age\":%lu,\"behaviour\":%s,\"phrase\":%s,\"error\":%s}",
             c.provider, model, url, c.period_min, c.has_key ? "true" : "false", c.key_tail, defs,
             s.ok ? "true" : "false", s.calls, (unsigned long)s.age_s, beh, ph, err);
    return send_json(req, buf);
}

static esp_err_t h_ai_set(httpd_req_t *req)
{
    char body[600] = { 0 };
    int len = httpd_req_recv(req, body, sizeof(body) - 1);
    if (len <= 0) return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "corps vide");
    body[len] = '\0';
    char pv[8] = { 0 }, key[220] = { 0 }, model[80] = { 0 }, url[120] = { 0 }, per[8] = { 0 };
    httpd_query_key_value(body, "provider", pv, sizeof(pv));
    httpd_query_key_value(body, "key", key, sizeof(key));
    httpd_query_key_value(body, "model", model, sizeof(model));
    httpd_query_key_value(body, "url", url, sizeof(url));
    httpd_query_key_value(body, "period", per, sizeof(per));
    url_decode(key); url_decode(model); url_decode(url);
    ai_set_cfg((ai_provider_t)atoi(pv), key, model, url, per[0] ? atoi(per) : 10);
    return send_json(req, "{\"ok\":true}");
}

static esp_err_t h_ai_test(httpd_req_t *req)
{
    ai_trigger(true);
    return send_json(req, "{\"ok\":true}");
}

// ⚫ AJOUTÉ (25/10/2026) : boutons d'actions du tableau de bord
static esp_err_t h_actions(httpd_req_t *req)
{
    char buf[900];
    pet_ui_actions_json(buf, sizeof(buf));
    return send_json(req, buf);
}

static esp_err_t h_action(httpd_req_t *req)
{
    char body[40] = { 0 }, v[8] = { 0 };
    int len = httpd_req_recv(req, body, sizeof(body) - 1);
    if (len > 0) body[len] = '\0';
    if (len <= 0 || httpd_query_key_value(body, "a", v, sizeof(v)) != ESP_OK)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "action ?");
    bool ok = pet_ui_request_action(atoi(v));
    return send_json(req, ok ? "{\"ok\":true}" : "{\"ok\":false,\"msg\":\"Action impossible pour le moment.\"}");
}

// Portail captif : toute autre adresse (tests de connectivité Android / iOS…) renvoie vers la page
static esp_err_t h_any(httpd_req_t *req)
{
    httpd_resp_set_status(req, "302 Found");
    // httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    // ⚫ MODIFIÉ (25/10/2026) : adresse relative : marche aussi sur l'adresse de la carte dans le WiFi maison
    httpd_resp_set_hdr(req, "Location", "/");
    return httpd_resp_send(req, NULL, 0);
}

static void http_start(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.core_id = CORE;
    // cfg.stack_size = 6144;
    cfg.stack_size = 8192;       // ⚫ MODIFIÉ (25/10/2026) : la recherche de ville passe par HTTPS
    // cfg.max_uri_handlers = 10;
    // cfg.max_uri_handlers = 16;   // ⚫ MODIFIÉ (25/10/2026)
    // cfg.max_uri_handlers = 20;   // ⚫ MODIFIÉ (25/10/2026) : météo / position
    // cfg.max_uri_handlers = 24;   // ⚫ MODIFIÉ (25/10/2026) : + actions
    cfg.max_uri_handlers = 28;   // ⚫ MODIFIÉ (25/10/2026) : + IA
    cfg.max_open_sockets = 6;    // ⚫ AJOUTÉ (25/10/2026) : la page charge plusieurs images en parallèle
    cfg.lru_purge_enable = true;
    cfg.uri_match_fn = httpd_uri_match_wildcard;
    httpd_handle_t srv = NULL;
    if (httpd_start(&srv, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "Serveur web impossible à démarrer");
        return;
    }
    const httpd_uri_t uris[] = {
        { .uri = "/",            .method = HTTP_GET,  .handler = h_root },
        { .uri = "/api/status",  .method = HTTP_GET,  .handler = h_status },
        { .uri = "/api/scan",    .method = HTTP_GET,  .handler = h_scan },
        { .uri = "/api/wifi",    .method = HTTP_POST, .handler = h_wifi },
        { .uri = "/api/forget",  .method = HTTP_POST, .handler = h_forget },
        { .uri = "/api/pet",     .method = HTTP_GET,  .handler = h_pet },
        // ⚫ AJOUTÉ (25/10/2026)
        { .uri = "/api/scene",   .method = HTTP_GET,  .handler = h_scene },
        { .uri = "/api/pack",    .method = HTTP_GET,  .handler = h_pack },
        { .uri = "/api/img",     .method = HTTP_GET,  .handler = h_img },
        { .uri = "/api/species", .method = HTTP_GET,  .handler = h_species },
        { .uri = "/api/adopt",   .method = HTTP_POST, .handler = h_adopt },
        { .uri = "/api/reset",   .method = HTTP_POST, .handler = h_reset },
        { .uri = "/api/weather", .method = HTTP_GET,  .handler = h_weather },    // ⚫ AJOUTÉ (25/10/2026)
        { .uri = "/api/geo",     .method = HTTP_GET,  .handler = h_geo },        // ⚫ AJOUTÉ (25/10/2026)
        { .uri = "/api/location",.method = HTTP_POST, .handler = h_location },   // ⚫ AJOUTÉ (25/10/2026)
        { .uri = "/api/actions", .method = HTTP_GET,  .handler = h_actions },    // ⚫ AJOUTÉ (25/10/2026)
        { .uri = "/api/action",  .method = HTTP_POST, .handler = h_action },     // ⚫ AJOUTÉ (25/10/2026)
        { .uri = "/api/ai",      .method = HTTP_GET,  .handler = h_ai_get },     // ⚫ AJOUTÉ (25/10/2026)
        { .uri = "/api/ai",      .method = HTTP_POST, .handler = h_ai_set },     // ⚫ AJOUTÉ (25/10/2026)
        { .uri = "/api/ai/test", .method = HTTP_POST, .handler = h_ai_test },    // ⚫ AJOUTÉ (25/10/2026)
        { .uri = "/*",           .method = HTTP_GET,  .handler = h_any },   // en dernier
    };
    for (size_t i = 0; i < sizeof(uris) / sizeof(uris[0]); i++) httpd_register_uri_handler(srv, &uris[i]);
}

/* ---------- DNS du portail captif ---------- */

static void dns_task(void *arg)
{
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct sockaddr_in addr = { .sin_family = AF_INET, .sin_port = htons(53), .sin_addr.s_addr = htonl(INADDR_ANY) };
    if (sock < 0 || bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "DNS impossible à démarrer");
        if (sock >= 0) close(sock);
        vTaskDelete(NULL);
    }
    uint8_t buf[512];
    for (;;) {
        struct sockaddr_in from;
        socklen_t fl = sizeof(from);
        int n = recvfrom(sock, buf, sizeof(buf) - 16, 0, (struct sockaddr *)&from, &fl);
        if (n < 12 || (buf[2] & 0x80)) continue;                  // trop court, ou déjà une réponse
        // fin de la (première) question : noms en labels puis type (2) + classe (2)
        int q = 12;
        while (q < n && buf[q]) q += buf[q] + 1;
        if (q + 5 > n) continue;
        uint16_t qtype = (buf[q + 1] << 8) | buf[q + 2];
        int end = q + 5;
        buf[2] = 0x81; buf[3] = 0x80;                             // réponse, récursion disponible
        buf[6] = 0; buf[7] = (qtype == 1) ? 1 : 0;                // 1 réponse pour une requête A
        buf[8] = buf[9] = buf[10] = buf[11] = 0;
        int o = end;
        if (qtype == 1) {
            const uint8_t ans[] = { 0xC0, 0x0C, 0, 1, 0, 1, 0, 0, 0, 60, 0, 4, 192, 168, 4, 1 };
            memcpy(buf + o, ans, sizeof(ans));
            o += sizeof(ans);
        }
        sendto(sock, buf, o, 0, (struct sockaddr *)&from, fl);
    }
}

/* ---------- démarrage ---------- */

static void wifi_init_task(void *arg)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();
    esp_netif_t *sta = esp_netif_create_default_wifi_sta();
    esp_netif_set_hostname(sta, "animal-de-compagnie");

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));   // aucune écriture flash par le pilote
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_event, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_event, NULL);
    const esp_timer_create_args_t ta = { .callback = retry_cb, .name = "wifi_retry" };
    esp_timer_create(&ta, &s_retry_timer);

    wifi_config_t ap = { 0 };
    strlcpy((char *)ap.ap.ssid, CONFIG_PET_WIFI_AP_SSID, sizeof(ap.ap.ssid));
    ap.ap.ssid_len = strlen(CONFIG_PET_WIFI_AP_SSID);
    // strlcpy((char *)ap.ap.password, CONFIG_PET_WIFI_AP_PASS, sizeof(ap.ap.password));
    ap.ap.channel = CONFIG_PET_WIFI_AP_CHANNEL;
    ap.ap.max_connection = 4;
    // ap.ap.authmode = strlen(CONFIG_PET_WIFI_AP_PASS) >= 8 ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    ap.ap.authmode = WIFI_AUTH_OPEN;   // ⚫ MODIFIÉ (25/10/2026) : réseau ouvert, rejoint par QR code

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap));
    creds_load();
    sta_apply();
    ESP_ERROR_CHECK(esp_wifi_start());
    portENTER_CRITICAL(&s_mux);
    s_st.ap_on = true;
    portEXIT_CRITICAL(&s_mux);

    http_start();
    xTaskCreatePinnedToCore(dns_task, "dns", 3072, NULL, 4, NULL, CORE);
    ESP_LOGI(TAG, "Réseau de configuration : %s (%s), page http://192.168.4.1 ; WiFi maison : %s",
             CONFIG_PET_WIFI_AP_SSID, ap.ap.authmode == WIFI_AUTH_OPEN ? "ouvert" : "WPA2",
             s_ssid[0] ? s_ssid : "aucun");
    vTaskDelete(NULL);
}

void wifi_cfg_start(void)
{
    // initialisation sur le cœur 1 : les interruptions du WiFi y sont attachées
    xTaskCreatePinnedToCore(wifi_init_task, "wifi_init", 4096, NULL, 5, NULL, CORE);
}

#else

void wifi_cfg_start(void) {}
void wifi_cfg_get_status(wifi_cfg_status_t *out) { memset(out, 0, sizeof(*out)); }

#endif
