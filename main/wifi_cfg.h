// ⚫ AJOUTÉ (25/10/2026) : serveur WiFi de configuration.
// - la carte crée son réseau (CONFIG_PET_WIFI_AP_SSID, "AnimalDeCompagnie_WiFi" par défaut) ;
// - un téléphone qui s'y connecte est redirigé vers la page de configuration (portail captif, http://192.168.4.1) :
//   choix du WiFi de la maison (liste des réseaux détectés), mot de passe, état du chien ;
// - les identifiants sont sauvegardés en NVS et la carte s'y reconnecte à chaque démarrage (mode AP + STA).
// Tout tourne sur le cœur 1 : le cœur 0 garde l'interruption de l'écran.
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool ap_on;              // réseau de la carte actif
    int ap_clients;          // téléphones connectés au réseau de la carte
    bool sta_configured;     // un WiFi de la maison est enregistré
    bool sta_connected;      // connecté au WiFi de la maison
    char sta_ssid[33];
    char sta_ip[16];
    int8_t sta_rssi;
} wifi_cfg_status_t;

// Démarre le WiFi et le serveur (nvs_flash_init() doit avoir été appelé). Sans effet si CONFIG_PET_WIFI=n.
void wifi_cfg_start(void);

// État courant (lisible depuis n'importe quelle tâche)
void wifi_cfg_get_status(wifi_cfg_status_t *out);
