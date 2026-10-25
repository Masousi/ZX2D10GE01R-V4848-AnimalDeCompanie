// ⚫ AJOUTÉ (25/10/2026) : heure et météo réelles, par Internet (quand la carte est sur le WiFi de la maison).
// - heure : NTP (pool.ntp.org) ; fuseau et lever / coucher du soleil : Open-Meteo (timezone=auto) ;
// - position : trouvée d'après l'adresse IP (ipapi.co, sinon ip-api.com), ou choisie sur la page web
//   (recherche de ville : géocodage Open-Meteo), gardée en NVS ;
// - météo : Open-Meteo (gratuit, sans clé), rafraîchie toutes les 15 min.
// Sans Internet : valid = false, l'écran garde son comportement d'avant (jour, nuit quand l'animal dort).
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    WX_UNKNOWN = 0,
    WX_CLEAR,      // ciel dégagé
    WX_PARTLY,     // quelques nuages
    WX_CLOUDY,     // couvert
    WX_FOG,        // brouillard
    WX_DRIZZLE,    // bruine
    WX_RAIN,       // pluie
    WX_SNOW,       // neige
    WX_STORM,      // orage
} wx_kind_t;

typedef enum { TOD_UNKNOWN = -1, TOD_DAY = 0, TOD_DUSK, TOD_NIGHT } tod_t;

typedef struct {
    bool valid;            // météo reçue
    bool time_ok;          // heure NTP synchronisée
    wx_kind_t kind;
    int code;              // code météo WMO
    float temp;            // °C
    int clouds;            // couverture nuageuse %
    float wind;            // km/h
    bool is_day;
    int sunrise_min, sunset_min;   // minutes locales depuis minuit
    int32_t utc_offset;    // secondes
    float lat, lon;
    bool auto_loc;         // position trouvée d'après l'adresse IP
    char place[40];
    uint32_t updated_s;    // âge de la dernière mise à jour (s)
} weather_t;

void weather_start(void);
void weather_get(weather_t *out);
// Moment de la journée selon le soleil (aube / crépuscule = 40 min autour du lever / coucher)
tod_t weather_tod(void);
// Heure locale "HH:MM" (false si l'heure n'est pas encore connue)
bool weather_clock(char *out, size_t n);
const char *weather_text(wx_kind_t k);
// Position choisie sur la page web (sauvegardée) ; weather_set_auto() revient à la position d'après l'IP
void weather_set_location(float lat, float lon, const char *place);
void weather_set_auto(void);
// Recherche de ville (géocodage Open-Meteo) : JSON [{"name","admin","country","lat","lon"}, …] dans out
bool weather_geocode(const char *query, char *out, size_t n);
