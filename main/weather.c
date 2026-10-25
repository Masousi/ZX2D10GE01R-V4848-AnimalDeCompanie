// ⚫ AJOUTÉ (25/10/2026) : heure et météo réelles (voir weather.h).
// Tâche sur le cœur 1 (comme le WiFi) : attend la connexion au WiFi de la maison, synchronise l'heure (NTP),
// trouve la position (IP ou choix sur la page web), puis lit la météo Open-Meteo toutes les 15 min.
// Une seule écriture flash : la position, quand elle change.
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_sntp.h"
#include "cJSON.h"
#include "nvs.h"
#include "wifi_cfg.h"
#include "weather.h"

#define TAG "METEO"
#define REFRESH_S   900          // météo toutes les 15 min
#define RETRY_S     60
#define BUF_SIZE    6144

typedef struct {
    float lat, lon;
    char place[40];
    uint8_t manual;              // 1 = choisie sur la page web
} loc_t;

static weather_t s_w;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static loc_t s_loc;
static bool s_have_loc;
static volatile bool s_force;    // nouvelle position : relire la météo tout de suite
static volatile bool s_want_auto;
static uint32_t s_last_ok;       // instant (s depuis le démarrage) de la dernière météo reçue
static char *s_buf;

/* ---------- HTTP ---------- */

// GET -> corps dans buf (terminé par 0). Renvoie la longueur, ou -1.
static int http_get(const char *url, char *buf, size_t n)
{
    esp_http_client_config_t c = {
        .url = url,
        .timeout_ms = 10000,
        .crt_bundle_attach = esp_crt_bundle_attach,   // HTTPS : certificats racine intégrés à l'IDF
        .user_agent = "AnimalDeCompagnie/1.0 (ESP32-S3)",
        .buffer_size = 2048,
    };
    esp_http_client_handle_t h = esp_http_client_init(&c);
    if (!h) return -1;
    int len = -1;
    if (esp_http_client_open(h, 0) == ESP_OK) {
        esp_http_client_fetch_headers(h);
        int status = esp_http_client_get_status_code(h);
        int r;
        len = 0;
        while (len < (int)n - 1 && (r = esp_http_client_read(h, buf + len, n - 1 - len)) > 0) len += r;
        buf[len] = '\0';
        if (status != 200) {
            ESP_LOGW(TAG, "%s : HTTP %d", url, status);
            len = -1;
        }
        esp_http_client_close(h);
    } else {
        ESP_LOGW(TAG, "Connexion impossible : %s", url);
    }
    esp_http_client_cleanup(h);
    return len;
}

static void url_encode(const char *in, char *out, size_t n)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t o = 0;
    for (; *in && o + 4 < n; in++) {
        unsigned char c = (unsigned char)*in;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_') out[o++] = c;
        else { out[o++] = '%'; out[o++] = hex[c >> 4]; out[o++] = hex[c & 15]; }
    }
    out[o] = '\0';
}

/* ---------- position ---------- */

static void loc_load(void)
{
    nvs_handle_t h;
    if (nvs_open("wx", NVS_READONLY, &h) != ESP_OK) return;
    size_t len = sizeof(s_loc);
    if (nvs_get_blob(h, "loc", &s_loc, &len) == ESP_OK && len == sizeof(s_loc)) s_have_loc = true;
    nvs_close(h);
}

static void loc_save(void)
{
    nvs_handle_t h;
    if (nvs_open("wx", NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_blob(h, "loc", &s_loc, sizeof(s_loc));
    nvs_commit(h);
    nvs_close(h);
}

static double jnum(const cJSON *o, const char *k, double def)
{
    const cJSON *v = cJSON_GetObjectItem(o, k);
    return cJSON_IsNumber(v) ? v->valuedouble : def;
}

static const char *jstr(const cJSON *o, const char *k)
{
    const cJSON *v = cJSON_GetObjectItem(o, k);
    return cJSON_IsString(v) ? v->valuestring : "";
}

// Position d'après l'adresse IP publique
static bool geolocate(void)
{
    const char *urls[] = { "https://ipapi.co/json/", "http://ip-api.com/json/?fields=status,lat,lon,city" };
    for (int i = 0; i < 2; i++) {
        if (http_get(urls[i], s_buf, BUF_SIZE) <= 0) continue;
        cJSON *j = cJSON_Parse(s_buf);
        if (!j) continue;
        double lat = jnum(j, i ? "lat" : "latitude", 999), lon = jnum(j, i ? "lon" : "longitude", 999);
        if (lat < 90 && lon < 180) {
            s_loc = (loc_t){ (float)lat, (float)lon, "", 0 };
            strlcpy(s_loc.place, jstr(j, "city"), sizeof(s_loc.place));
            cJSON_Delete(j);
            s_have_loc = true;
            loc_save();
            ESP_LOGI(TAG, "Position d'après l'IP : %s (%.3f, %.3f)", s_loc.place, s_loc.lat, s_loc.lon);
            return true;
        }
        cJSON_Delete(j);
    }
    return false;
}

/* ---------- météo ---------- */

static wx_kind_t wmo_kind(int c)
{
    if (c == 0) return WX_CLEAR;
    if (c <= 2) return WX_PARTLY;
    if (c == 3) return WX_CLOUDY;
    if (c == 45 || c == 48) return WX_FOG;
    if (c >= 51 && c <= 57) return WX_DRIZZLE;
    if ((c >= 61 && c <= 67) || (c >= 80 && c <= 82)) return WX_RAIN;
    if ((c >= 71 && c <= 77) || c == 85 || c == 86) return WX_SNOW;
    if (c >= 95) return WX_STORM;
    return WX_CLOUDY;
}

// "2026-10-25T08:21" -> minutes depuis minuit
static int hm_min(const char *iso)
{
    const char *t = strchr(iso, 'T');
    return t ? atoi(t + 1) * 60 + atoi(t + 4) : -1;
}

static bool fetch_weather(void)
{
    char url[300];
    snprintf(url, sizeof(url), "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
             "&current=temperature_2m,weather_code,is_day,cloud_cover,wind_speed_10m&daily=sunrise,sunset"
             "&timezone=auto&forecast_days=1", s_loc.lat, s_loc.lon);
    if (http_get(url, s_buf, BUF_SIZE) <= 0) return false;
    cJSON *j = cJSON_Parse(s_buf);
    if (!j) return false;
    const cJSON *cur = cJSON_GetObjectItem(j, "current"), *daily = cJSON_GetObjectItem(j, "daily");
    if (!cur) { cJSON_Delete(j); return false; }
    weather_t w = { 0 };
    w.valid = true;
    w.code = (int)jnum(cur, "weather_code", 0);
    w.kind = wmo_kind(w.code);
    w.temp = (float)jnum(cur, "temperature_2m", 0);
    w.clouds = (int)jnum(cur, "cloud_cover", 0);
    w.wind = (float)jnum(cur, "wind_speed_10m", 0);
    w.is_day = jnum(cur, "is_day", 1) != 0;
    w.utc_offset = (int32_t)jnum(j, "utc_offset_seconds", 0);
    w.sunrise_min = w.sunset_min = -1;
    if (daily) {
        const cJSON *sr = cJSON_GetArrayItem(cJSON_GetObjectItem(daily, "sunrise"), 0);
        const cJSON *ss = cJSON_GetArrayItem(cJSON_GetObjectItem(daily, "sunset"), 0);
        if (cJSON_IsString(sr)) w.sunrise_min = hm_min(sr->valuestring);
        if (cJSON_IsString(ss)) w.sunset_min = hm_min(ss->valuestring);
    }
    cJSON_Delete(j);
    w.lat = s_loc.lat;
    w.lon = s_loc.lon;
    w.auto_loc = !s_loc.manual;
    strlcpy(w.place, s_loc.place, sizeof(w.place));
    portENTER_CRITICAL(&s_mux);
    bool t_ok = s_w.time_ok;
    s_w = w;
    s_w.time_ok = t_ok;
    portEXIT_CRITICAL(&s_mux);
    s_last_ok = (uint32_t)(esp_log_timestamp() / 1000);
    ESP_LOGI(TAG, "%s : %s, %.1f °C, nuages %d %%, %s (lever %02d:%02d, coucher %02d:%02d)", w.place,
             weather_text(w.kind), w.temp, w.clouds, w.is_day ? "jour" : "nuit",
             w.sunrise_min / 60, w.sunrise_min % 60, w.sunset_min / 60, w.sunset_min % 60);
    return true;
}

static void weather_task(void *arg)
{
    bool sntp_on = false;
    uint32_t next = 0;
    for (;;) {
        wifi_cfg_status_t ws;
        wifi_cfg_get_status(&ws);
        uint32_t now = (uint32_t)(esp_log_timestamp() / 1000);
        if (ws.sta_connected) {
            if (!sntp_on) {   // heure réelle
                sntp_setoperatingmode(SNTP_OPMODE_POLL);
                sntp_setservername(0, "pool.ntp.org");
                sntp_init();
                sntp_on = true;
            }
            if (time(NULL) > 1700000000) { portENTER_CRITICAL(&s_mux); s_w.time_ok = true; portEXIT_CRITICAL(&s_mux); }
            if (s_want_auto) { s_want_auto = false; s_have_loc = false; s_force = true; }
            if (!s_have_loc && !geolocate()) next = now + RETRY_S;
            if (s_have_loc && (s_force || now >= next)) {
                s_force = false;
                next = now + (fetch_weather() ? REFRESH_S : RETRY_S);
            }
        }
        portENTER_CRITICAL(&s_mux);
        s_w.updated_s = s_w.valid ? now - s_last_ok : 0;
        portEXIT_CRITICAL(&s_mux);
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

/* ---------- API ---------- */

void weather_start(void)
{
    s_buf = malloc(BUF_SIZE);
    loc_load();
    if (s_have_loc) {
        strlcpy(s_w.place, s_loc.place, sizeof(s_w.place));
        s_w.lat = s_loc.lat;
        s_w.lon = s_loc.lon;
        s_w.auto_loc = !s_loc.manual;
    }
    xTaskCreatePinnedToCore(weather_task, "meteo", 6144, NULL, 3, NULL, 1);
}

void weather_get(weather_t *out)
{
    portENTER_CRITICAL(&s_mux);
    *out = s_w;
    portEXIT_CRITICAL(&s_mux);
}

static int local_minutes(const weather_t *w)
{
    if (!w->time_ok) return -1;
    time_t t = time(NULL) + w->utc_offset;
    return (int)((t % 86400) / 60);
}

tod_t weather_tod(void)
{
    weather_t w;
    weather_get(&w);
    if (!w.valid) return TOD_UNKNOWN;
    int m = local_minutes(&w);
    if (m < 0 || w.sunrise_min < 0 || w.sunset_min < 0) return w.is_day ? TOD_DAY : TOD_NIGHT;
    if (abs(m - w.sunrise_min) <= 35 || abs(m - w.sunset_min) <= 35) return TOD_DUSK;
    return (m > w.sunrise_min && m < w.sunset_min) ? TOD_DAY : TOD_NIGHT;
}

bool weather_clock(char *out, size_t n)
{
    weather_t w;
    weather_get(&w);
    if (!w.time_ok) return false;
    int32_t off = w.valid ? w.utc_offset : 0;
    time_t t = time(NULL) + off;
    snprintf(out, n, "%02d:%02d", (int)((t % 86400) / 3600), (int)((t % 3600) / 60));
    return true;
}

const char *weather_text(wx_kind_t k)
{
    switch (k) {
    case WX_CLEAR:   return "ciel dégagé";
    case WX_PARTLY:  return "quelques nuages";
    case WX_CLOUDY:  return "couvert";
    case WX_FOG:     return "brouillard";
    case WX_DRIZZLE: return "bruine";
    case WX_RAIN:    return "pluie";
    case WX_SNOW:    return "neige";
    case WX_STORM:   return "orage";
    default:         return "inconnu";
    }
}

void weather_set_location(float lat, float lon, const char *place)
{
    s_loc = (loc_t){ lat, lon, "", 1 };
    strlcpy(s_loc.place, place ? place : "", sizeof(s_loc.place));
    s_have_loc = true;
    loc_save();
    s_force = true;
    portENTER_CRITICAL(&s_mux);
    strlcpy(s_w.place, s_loc.place, sizeof(s_w.place));
    s_w.lat = lat;
    s_w.lon = lon;
    s_w.auto_loc = false;
    portEXIT_CRITICAL(&s_mux);
    ESP_LOGI(TAG, "Nouvelle position : %s (%.3f, %.3f)", s_loc.place, lat, lon);
}

void weather_set_auto(void)
{
    s_want_auto = true;
}

bool weather_geocode(const char *query, char *out, size_t n)
{
    char q[120], url[260];
    url_encode(query, q, sizeof(q));
    snprintf(url, sizeof(url), "https://geocoding-api.open-meteo.com/v1/search?name=%s&count=6&language=fr&format=json", q);
    char *buf = malloc(BUF_SIZE);
    if (!buf) return false;
    bool ok = false;
    if (http_get(url, buf, BUF_SIZE) > 0) {
        cJSON *j = cJSON_Parse(buf);
        const cJSON *res = j ? cJSON_GetObjectItem(j, "results") : NULL;
        size_t o = snprintf(out, n, "[");
        const cJSON *it;
        int i = 0;
        cJSON_ArrayForEach(it, res) {
            if (o + 160 > n) break;
            o += snprintf(out + o, n - o, "%s{\"name\":\"%s\",\"admin\":\"%s\",\"country\":\"%s\",\"lat\":%.4f,\"lon\":%.4f}",
                          i++ ? "," : "", jstr(it, "name"), jstr(it, "admin1"), jstr(it, "country"),
                          jnum(it, "latitude", 0), jnum(it, "longitude", 0));
        }
        snprintf(out + o, n - o, "]");
        ok = true;
        cJSON_Delete(j);
    }
    free(buf);
    return ok;
}
