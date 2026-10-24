// ⚫ AJOUTÉ (25/10/2026) : serveur WiFi de configuration (voir wifi_cfg.h).
// Réseau de la carte (AP) + connexion au WiFi de la maison (STA), serveur web (page main/wifi_page.html),
// API JSON (/api/status, /api/scan, /api/wifi, /api/forget, /api/pet) et DNS "portail captif" (toute requête
// DNS -> 192.168.4.1, donc le téléphone ouvre la page tout seul).
// Le pilote WiFi n'écrit rien en flash (WIFI_STORAGE_RAM, CONFIG_ESP32_WIFI_NVS_ENABLED=n) : seuls les
// identifiants validés dans la page sont sauvegardés (namespace NVS "wifi"), chaque écriture flash perturbant l'écran.
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
            portENTER_CRITICAL(&s_mux);
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
    snprintf(buf, sizeof(buf), "{\"ap\":%s,\"clients\":%d,\"configured\":%s,\"connected\":%s,\"ssid\":%s,\"ip\":\"%s\",\"rssi\":%d}",
             ap, st.ap_clients, st.sta_configured ? "true" : "false", st.sta_connected ? "true" : "false",
             ssid, st.sta_ip, st.sta_rssi);
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
    char buf[400], name[64];
    json_str(name, sizeof(name), CONFIG_PET_NAME);
    snprintf(buf, sizeof(buf),
             "{\"name\":%s,\"stage\":\"%s\",\"age\":%lu,\"mood\":\"%s\",\"sleeping\":%s,"
             "\"food\":%d,\"water\":%d,\"energy\":%d,\"joy\":%d,\"discipline\":%d,\"health\":%d}",
             name, pet_stage_text(pet_stage()), (unsigned long)p->age_s, pet_mood_text(pet_mood()),
             p->sleeping ? "true" : "false", (int)p->s.food, (int)p->s.water, (int)p->s.energy,
             (int)p->s.joy, (int)p->s.discipline, (int)p->s.health);
    return send_json(req, buf);
}

// Portail captif : toute autre adresse (tests de connectivité Android / iOS…) renvoie vers la page
static esp_err_t h_any(httpd_req_t *req)
{
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    return httpd_resp_send(req, NULL, 0);
}

static void http_start(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.core_id = CORE;
    cfg.stack_size = 6144;
    cfg.max_uri_handlers = 10;
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
    strlcpy((char *)ap.ap.password, CONFIG_PET_WIFI_AP_PASS, sizeof(ap.ap.password));
    ap.ap.channel = CONFIG_PET_WIFI_AP_CHANNEL;
    ap.ap.max_connection = 4;
    ap.ap.authmode = strlen(CONFIG_PET_WIFI_AP_PASS) >= 8 ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;

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
