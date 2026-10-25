// ⚫ AJOUTÉ (24/10/2026) : anneau de LED = état de l'animal (voir pet_led.h).
// Reprend les briques de l'ancien main/encoder_led.c (hsv, gamma, 50 images/s, tâche sur le cœur 0).
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_random.h"
#include "sdkconfig.h"
#include "board.h"
#include "ws2812.h"
#include "pet_led.h"

#define TAG "PET_LED"

#define LED_COUNT     CONFIG_PET_LED_COUNT
#define LED_MAX       CONFIG_PET_LED_BRIGHTNESS
#define FRAME_MS      20                         // 50 images/s
#define REACT_FRAMES  (1600 / FRAME_MS)          // durée d'un effet de réaction
#define MOOD_XFADE    (800 / FRAME_MS)           // fondu entre deux humeurs

typedef struct { float r, g, b; } rgbf_t;        // couleur 0..1 (avant luminosité / gamma)

static volatile int s_mood = PET_MOOD_NEUTRAL;
static volatile int s_level = 50;                // bien-être 0..100
static volatile int s_react = PET_REACT_NONE;
static volatile bool s_react_new;

static float s_drop[LED_COUNT];                  // gouttes de l'humeur "pleure"

static rgbf_t rgb(uint32_t hex)
{
    return (rgbf_t){ ((hex >> 16) & 0xFF) / 255.0f, ((hex >> 8) & 0xFF) / 255.0f, (hex & 0xFF) / 255.0f };
}

static rgbf_t scale(rgbf_t c, float v)
{
    return (rgbf_t){ c.r * v, c.g * v, c.b * v };
}

static rgbf_t mix(rgbf_t a, rgbf_t b, float t)
{
    return (rgbf_t){ a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t };
}

// teinte (0..1, cyclique) -> RGB (repris de encoder_led.c)
static rgbf_t hsv(float h, float s, float v)
{
    h = h - floorf(h);
    float i = floorf(h * 6.0f), f = h * 6.0f - i;
    float p = v * (1 - s), q = v * (1 - f * s), t = v * (1 - (1 - f) * s);
    switch ((int)i % 6) {
        case 0:  return (rgbf_t){v, t, p};
        case 1:  return (rgbf_t){q, v, p};
        case 2:  return (rgbf_t){p, v, t};
        case 3:  return (rgbf_t){p, q, v};
        case 4:  return (rgbf_t){t, p, v};
        default: return (rgbf_t){v, p, q};
    }
}

static uint8_t to_led(float c)   // 0..1 -> 0..LED_MAX avec correction gamma
{
    if (c <= 0) return 0;
    if (c > 1) c = 1;
    return (uint8_t)(c * c * LED_MAX + 0.5f);
}

static float breath(float t, float period, float lo, float hi)
{
    return lo + (hi - lo) * (0.5f - 0.5f * cosf(t * 2 * (float)M_PI / period));
}

// Couleur de l'humeur pour la LED i (lit = LED comprise dans la jauge de bien-être)
static rgbf_t mood_color(int mood, int i, bool lit, float t)
{
    if (!lit) return (rgbf_t){0, 0, 0};
    switch (mood) {
    case PET_MOOD_HAPPY:      // vert, vague douce qui parcourt la jauge
        return scale(rgb(0x30FF60), 0.55f + 0.45f * sinf(t * 3.0f - i * 0.8f));
    case PET_MOOD_HUNGRY:     // orange, clignotement lent
        return scale(rgb(0xFF6A00), (fmodf(t, 1.6f) < 0.8f) ? 1.0f : 0.15f);
    // ⚫ AJOUTÉ (25/10/2026) : doit faire ses besoins : ambre, deux clignotements rapides puis pause
    case PET_MOOD_POTTY: {
        float ph = fmodf(t, 1.4f);
        return scale(rgb(0xFF9A10), (ph < 0.15f || (ph > 0.3f && ph < 0.45f)) ? 1.0f : 0.1f);
    }
    // ⚫ AJOUTÉ (25/10/2026) : a soif : cyan, clignotement lent
    case PET_MOOD_THIRSTY:
        return scale(rgb(0x00C8FF), (fmodf(t, 1.6f) < 0.8f) ? 1.0f : 0.15f);
    case PET_MOOD_TIRED:      // jaune pâle, respiration lente et faible
        return scale(rgb(0xFFC020), breath(t, 5.0f, 0.15f, 0.55f));
    case PET_MOOD_SAD:        // bleu, respiration lente
        return scale(rgb(0x2050FF), breath(t, 4.0f, 0.25f, 0.8f));
    case PET_MOOD_CRYING:     // bleu clair, gouttes qui tombent au hasard
        return mix(scale(rgb(0x1030A0), 0.3f), rgb(0x60B0FF), s_drop[i]);
    case PET_MOOD_NAUGHTY:    // rouge / magenta en alternance rapide
        return ((i + (int)(t * 4)) % 2) ? rgb(0xFF0010) : scale(rgb(0xFF00C0), 0.7f);
    case PET_MOOD_SICK: {     // vert acide, double battement de cœur irrégulier
        float ph = fmodf(t, 1.4f);
        float v = (ph < 0.12f || (ph > 0.25f && ph < 0.37f)) ? 1.0f : 0.12f;
        return scale(rgb(0x90FF00), v);
    }
    case PET_MOOD_SLEEPING:   // indigo, respiration très lente
        return scale(rgb(0x3010A0), breath(t, 8.0f, 0.05f, 0.4f));
    default:                  // va bien : blanc bleuté, respiration
        return scale(rgb(0x60C0FF), breath(t, 4.0f, 0.45f, 0.9f));
    }
}

// Effet de réaction pour la LED i, k = 0..1 avancement de l'effet
static rgbf_t react_color(int react, int i, float k, float t)
{
    const float n = LED_COUNT;
    switch (react) {
    case PET_REACT_JOY:       // arc-en-ciel qui tourne vite
        return hsv(i / n + t * 1.2f, 1, 1);
    case PET_REACT_LOVE: {    // rose, battements de cœur sur tout l'anneau
        float ph = fmodf(k * 3, 1.0f);
        return scale(rgb(0xFF2070), (ph < 0.15f || (ph > 0.3f && ph < 0.45f)) ? 1.0f : 0.3f);
    }
    case PET_REACT_EAT: {     // comète orange / jaune qui tourne
        float head = fmodf(k * n * 3, n);
        float d = fmodf(head - i + n, n);
        return scale(mix(rgb(0xFFD000), rgb(0xFF4000), d / n), expf(-d * 0.5f));
    }
    // ⚫ AJOUTÉ (25/10/2026) : boit : vaguelettes bleues qui partent du centre de l'anneau
    case PET_REACT_DRINK: {
        float d = fabsf(i - (n - 1) / 2.0f);
        return scale(mix(rgb(0x0050FF), rgb(0x80E0FF), 0.5f + 0.5f * sinf(d * 1.2f - t * 10)), 0.35f + 0.65f * (1 - k));
    }
    // ⚫ AJOUTÉ (25/10/2026) : besoins faits au bon endroit : vert qui se remplit ; cour nettoyée : balayage blanc
    case PET_REACT_POTTY:
        return scale(rgb(0x40E060), (i / n < k * 1.4f) ? 1.0f : 0.1f);
    case PET_REACT_CLEAN: {
        float d = fabsf(fmodf(k * n * 2, n) - i);
        return scale(rgb(0xFFFFFF), expf(-d * 0.6f) * 0.9f + 0.05f);
    }
    case PET_REACT_SLEEP:     // l'anneau s'éteint en indigo d'un bout à l'autre
        return scale(rgb(0x3010A0), (i / n > k) ? 0.8f : 0.05f);
    case PET_REACT_WAKE:      // lever de soleil : blanc chaud qui monte
        return scale(rgb(0xFFB060), sinf(k * (float)M_PI));
    case PET_REACT_REFUSE:    // double clignotement ambre
        return scale(rgb(0xFF9000), (fmodf(k * 4, 1.0f) < 0.5f && k < 0.5f) ? 1.0f : 0.05f);
    case PET_REACT_SORRY: {   // balayage violet (discipline)
        float head = k * n * 2;
        float d = fabsf(fmodf(head, n) - i);
        return scale(rgb(0x9040FF), expf(-d * 0.7f) + 0.1f);
    }
    case PET_REACT_HURT:      // flash rouge puis bleu
        return k < 0.3f ? rgb(0xFF0000) : scale(rgb(0x2040FF), 1 - k);
    default:
        return (rgbf_t){0, 0, 0};
    }
}

static void pet_led_task(void *arg)
{
    if (ws2812_init(LED_PIN, LED_COUNT) != ESP_OK) {
        ESP_LOGE(TAG, "ws2812_init a échoué, anneau désactivé");
        vTaskDelete(NULL);
    }

    uint32_t frame = 0;
    int react = PET_REACT_NONE;
    uint32_t react_t = 0;
    int mood = s_mood, prev_mood = mood;
    uint32_t mood_t = MOOD_XFADE;      // images depuis le dernier changement d'humeur
    float shown_level = s_level;       // jauge lissée (monte / descend en douceur)

    for (;;) {
        float t = frame * FRAME_MS / 1000.0f;

        if (s_react_new) {
            s_react_new = false;
            react = s_react;
            react_t = 0;
        }
        if (s_mood != mood) {
            prev_mood = mood;
            mood = s_mood;
            mood_t = 0;
        }
        shown_level += (s_level - shown_level) * 0.05f;
        float lit_f = shown_level / 100.0f * LED_COUNT;
        if (lit_f < 1) lit_f = 1;      // toujours au moins une LED : l'animal est là

        // gouttes de pluie (pleure)
        for (int i = 0; i < LED_COUNT; i++) {
            s_drop[i] *= 0.92f;
            if ((esp_random() % 1000) < 25) s_drop[i] = 1.0f;
        }

        float k_mood = mood_t < MOOD_XFADE ? (float)mood_t / MOOD_XFADE : 1.0f;
        float k_react = react != PET_REACT_NONE ? (float)react_t / REACT_FRAMES : 0;
        // apparition / disparition douce de l'effet de réaction
        float react_mix = 0;
        if (react != PET_REACT_NONE) {
            react_mix = 1;
            if (k_react < 0.1f) react_mix = k_react / 0.1f;
            if (k_react > 0.85f) react_mix = (1 - k_react) / 0.15f;
        }

        for (int i = 0; i < LED_COUNT; i++) {
#if CONFIG_PET_LED_REVERSE
            int li = (LED_COUNT - i) % LED_COUNT;
#else
            int li = i;
#endif
            bool lit = li < (int)lit_f;
            rgbf_t c = mood_color(mood, li, lit, t);
            // dernière LED de la jauge : partiellement allumée (jauge continue)
            if (li == (int)lit_f) c = scale(mood_color(mood, li, true, t), lit_f - (int)lit_f);
            if (k_mood < 1) c = mix(mood_color(prev_mood, li, lit, t), c, k_mood);
            if (react_mix > 0) c = mix(c, react_color(react, li, k_react, t), react_mix);
            ws2812_set(i, to_led(c.r), to_led(c.g), to_led(c.b));
        }
        ws2812_show();

        frame++;
        if (mood_t < MOOD_XFADE) mood_t++;
        if (react != PET_REACT_NONE && ++react_t >= REACT_FRAMES) react = PET_REACT_NONE;
        vTaskDelay(pdMS_TO_TICKS(FRAME_MS));
    }
}

void pet_led_start(void)
{
    memset(s_drop, 0, sizeof(s_drop));
    // pile 4 Ko (calculs flottants), cœur 0 (LVGL tourne sur le cœur 1)
    xTaskCreatePinnedToCore(pet_led_task, "pet_led", 4 * 1024, NULL, 3, NULL, 0);
}

void pet_led_set_state(pet_mood_t mood, int wellbeing)
{
    s_mood = mood;
    s_level = wellbeing;
}

void pet_led_react(pet_react_t react)
{
    s_react = react;
    s_react_new = true;
}
