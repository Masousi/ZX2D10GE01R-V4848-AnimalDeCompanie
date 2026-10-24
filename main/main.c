// ⚫ MODIFIÉ (24/10/2026) : firmware "Animal de compagnie" (type Tamagotchi) : main.c ne fait plus
// qu'initialiser les périphériques puis lance le jeu (main/pet_ui.c, pet.c, pet_led.c).
// ⚫ RETIRÉ (24/10/2026) : tout ce qui concernait les démos LVGL : lv_demos.h, menu.h (menu principal,
// app_menu_requested / app_menu_show / app_menu_poll), app_demo_run() (Widgets, Keypad & encoder, Benchmark,
// Stress, Music, LED animations), défilement automatique des onglets Widgets (widgets_autoplay_*),
// ancien aiguillage #if/#elif conservé en #if 0, et encoder_led_start() (remplacé par pet_led_start()).
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "lvgl.h"
#include "board.h"
#include "pet_led.h"
#include "pet_ui.h"
#include "wifi_cfg.h"   // ⚫ AJOUTÉ (25/10/2026)

#define TAG "MAIN"

static void increase_lvgl_tick(void* arg) {
    lv_tick_inc(portTICK_PERIOD_MS);
}

// écran RGB 480x480 + LVGL + encodeur + bouton (main/screen.c)
extern void screen_init(void);

// ⚫ AJOUTÉ (24/10/2026) : groupe LVGL par défaut relié à l'encodeur.
// Sans groupe, LVGL ignore l'encodeur. Les widgets créés ensuite y sont ajoutés automatiquement :
// tourner = déplacer le focus, appuyer = activer / entrer en édition (rotation = changer la valeur),
// appuyer à nouveau = valider.
static void encoder_group_init(void) {
    lv_group_t * g = lv_group_create();
    lv_group_set_default(g);
    for (lv_indev_t * indev = lv_indev_get_next(NULL); indev; indev = lv_indev_get_next(indev)) {
        if (lv_indev_get_type(indev) == LV_INDEV_TYPE_ENCODER) {
            lv_indev_set_group(indev, g);
        }
    }
}

// ⚫ AJOUTÉ (24/10/2026) : NVS (sauvegarde de l'animal) ; effacée si la partition est pleine ou d'un autre format
static void nvs_init(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS effacée (%s)", esp_err_to_name(err));
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

void lvgl_task(void* arg) {
    // ⚫ AJOUTÉ (24/10/2026) : screen_init() alloue le frame buffer RGB 480x480 en PSRAM.
    // Si CONFIG_SPIRAM est désactivé (sdkconfig régénéré), l'allocation échoue -> panic -> écran noir.
    // Les valeurs requises sont figées dans sdkconfig.defaults.
    // screen_init();
    // ⚫ MODIFIÉ (25/10/2026) : screen_init() est appelé par app_main (cœur 0), voir plus bas

    // Tick interface for LVGL
    const esp_timer_create_args_t periodic_timer_args = {
        .callback = increase_lvgl_tick,
        .name = "periodic_gui"
    };
    esp_timer_handle_t periodic_timer;
    esp_timer_create(&periodic_timer_args, &periodic_timer);
    esp_timer_start_periodic(periodic_timer, portTICK_PERIOD_MS * 1000);

    encoder_group_init();

    // ⚫ AJOUTÉ (24/10/2026) : l'application : l'animal de compagnie
    pet_ui_start();

    for (;;) {
        lv_task_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ⚫ AJOUTÉ (24/10/2026) : app_main rend la main tout de suite ; LVGL tourne dans sa propre tâche sur le cœur 1
void app_main(void) {
    nvs_init();
    // ⚫ AJOUTÉ (25/10/2026) : écran initialisé ici, sur le cœur 0 : l'interruption de l'écran (recopie des
    // bounce buffers depuis la PSRAM) est attachée au cœur qui crée le panneau ; LVGL garde le cœur 1 pour lui.
    screen_init();
    pet_led_start();   // ⚫ AJOUTÉ (24/10/2026) : anneau de LED WS2812 (tâche sur le cœur 0)
    wifi_cfg_start();  // ⚫ AJOUTÉ (25/10/2026) : réseau "AnimalDeCompagnie_WiFi" + page de configuration (cœur 1)
    xTaskCreatePinnedToCore(lvgl_task, NULL, 8 * 1024, NULL, 5, NULL, 1);
}
