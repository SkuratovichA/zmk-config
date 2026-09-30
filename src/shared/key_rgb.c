/*
 * SPDX-License-Identifier: MIT
 *
 * Reactive per-key RGB: a pressed key lights its LED in a random colour and
 * fades out over CONFIG_SHARED_KEY_RGB_FADE_MS. Each half runs this for its own
 * keys. The strip is written only while ZMK's underglow is off; with the
 * underglow on, ZMK's effect owns every LED of the chain.
 *
 * Threads: the position listener runs on the thread that raised the event and
 * only records the press; the fade is drawn from a delayable work item on the
 * system work queue. A spinlock guards the fade table between the two.
 */

#define DT_DRV_COMPAT skuratovich_key_rgb

#include <stdbool.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/spinlock.h>
#include <zephyr/sys/util.h>

#include <drivers/ext_power.h>
#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/rgb_underglow.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define STRIP_NODE DT_CHOSEN(zmk_underglow)
#define STRIP_LEN DT_PROP(STRIP_NODE, chain_length)
#define NO_LED 255

#define FADE_MS CONFIG_SHARED_KEY_RGB_FADE_MS
#define TICK_MS CONFIG_SHARED_KEY_RGB_TICK_MS
/* Peak brightness as a 0..255 value, from the underglow's cap in percent. */
#define PEAK_VALUE ((CONFIG_ZMK_RGB_UNDERGLOW_BRT_MAX * 255) / 100)

static const struct device *const strip = DEVICE_DT_GET(STRIP_NODE);
static const uint8_t key_leds[] = DT_INST_PROP(0, key_leds);

/* One fade per LED: the colour it was lit with and when. */
struct fade {
    bool active;
    int64_t started;
    struct led_rgb peak;
};

static struct fade fades[STRIP_LEN];
static struct led_rgb pixels[STRIP_LEN];
static struct k_spinlock lock;
static bool ready;
static uint32_t rng_state;

static void tick_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(tick_work, tick_handler);

/* A small xorshift generator: the colours only have to look random. */
static uint32_t next_random(void) {
    uint32_t x = rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_state = x;
    return x;
}

/* Full saturation, so a hue in degrees and a value 0..255 give the colour. */
static struct led_rgb hue_to_rgb(uint16_t hue, uint8_t value) {
    const uint8_t sextant = (hue / 60) % 6;
    const uint32_t f = (hue % 60) * 255 / 60;
    const uint8_t q = (uint8_t)(value * (255 - f) / 255);
    const uint8_t t = (uint8_t)(value * f / 255);
    switch (sextant) {
    case 0:
        return (struct led_rgb){.r = value, .g = t, .b = 0};
    case 1:
        return (struct led_rgb){.r = q, .g = value, .b = 0};
    case 2:
        return (struct led_rgb){.r = 0, .g = value, .b = t};
    case 3:
        return (struct led_rgb){.r = 0, .g = q, .b = value};
    case 4:
        return (struct led_rgb){.r = t, .g = 0, .b = value};
    default:
        return (struct led_rgb){.r = value, .g = 0, .b = q};
    }
}

static bool underglow_is_on(void) {
    bool on = false;
    return zmk_rgb_underglow_get_state(&on) == 0 && on;
}

#if IS_ENABLED(CONFIG_ZMK_RGB_UNDERGLOW_EXT_POWER)

/*
 * On a half where the underglow manages the LED power rail, the rail is off
 * whenever the underglow is off. The rail goes on for a fade and off again
 * CONFIG_SHARED_KEY_RGB_RAIL_OFF_MS after the last one, so that dark LEDs do
 * not drain the battery.
 */
static const struct device *const ext_power =
    DEVICE_DT_GET_OR_NULL(DT_INST(0, zmk_ext_power_generic));

static void rail_off_handler(struct k_work *work) {
    ARG_UNUSED(work);
    if (ext_power == NULL || underglow_is_on()) {
        return;
    }
    ext_power_disable(ext_power);
}

static K_WORK_DELAYABLE_DEFINE(rail_off_work, rail_off_handler);

static void rail_on(void) {
    if (ext_power == NULL) {
        return;
    }
    k_work_cancel_delayable(&rail_off_work);
    if (ext_power_get(ext_power) <= 0) {
        ext_power_enable(ext_power);
    }
}

static void rail_release(void) {
    if (ext_power == NULL) {
        return;
    }
    k_work_schedule(&rail_off_work, K_MSEC(CONFIG_SHARED_KEY_RGB_RAIL_OFF_MS));
}

#else

static void rail_on(void) {}
static void rail_release(void) {}

#endif /* CONFIG_ZMK_RGB_UNDERGLOW_EXT_POWER */

/* Scales the peak by (1 - t)^2, so the light dies away gently. */
static struct led_rgb faded(const struct led_rgb *peak, int64_t elapsed) {
    if (elapsed >= FADE_MS) {
        return (struct led_rgb){0};
    }
    const uint32_t k = (uint32_t)((FADE_MS - elapsed) * 255 / FADE_MS); /* 255 at the start */
    const uint32_t kk = k * k;
    return (struct led_rgb){
        .r = (uint8_t)(peak->r * kk / (255 * 255)),
        .g = (uint8_t)(peak->g * kk / (255 * 255)),
        .b = (uint8_t)(peak->b * kk / (255 * 255)),
    };
}

static void tick_handler(struct k_work *work) {
    ARG_UNUSED(work);
    const int64_t now = k_uptime_get();
    bool any = false;

    k_spinlock_key_t key = k_spin_lock(&lock);
    if (underglow_is_on()) {
        /* ZMK's effect owns the strip now; forget the fades. */
        for (int i = 0; i < STRIP_LEN; i++) {
            fades[i].active = false;
        }
        k_spin_unlock(&lock, key);
        return;
    }
    for (int i = 0; i < STRIP_LEN; i++) {
        if (!fades[i].active) {
            pixels[i] = (struct led_rgb){0};
            continue;
        }
        const int64_t elapsed = now - fades[i].started;
        pixels[i] = faded(&fades[i].peak, elapsed);
        if (elapsed >= FADE_MS) {
            fades[i].active = false;
        } else {
            any = true;
        }
    }
    k_spin_unlock(&lock, key);

    const int rc = led_strip_update_rgb(strip, pixels, STRIP_LEN);
    if (rc != 0) {
        LOG_WRN("led strip update failed: %d", rc);
    }
    if (any) {
        k_work_schedule(&tick_work, K_MSEC(TICK_MS));
    } else {
        rail_release();
    }
}

static int position_listener(const zmk_event_t *eh) {
    const struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);
    if (ev == NULL || !ev->state || !ready) {
        return ZMK_EV_EVENT_BUBBLE;
    }
    /* On the central, keys of the other half arrive too; that half lights its own. */
    if (ev->source != ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL) {
        return ZMK_EV_EVENT_BUBBLE;
    }
    if (ev->position >= ARRAY_SIZE(key_leds)) {
        return ZMK_EV_EVENT_BUBBLE;
    }
    const uint8_t led = key_leds[ev->position];
    if (led == NO_LED || led >= STRIP_LEN || underglow_is_on()) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    const uint16_t hue = next_random() % 360;
    k_spinlock_key_t key = k_spin_lock(&lock);
    fades[led].active = true;
    fades[led].started = k_uptime_get();
    fades[led].peak = hue_to_rgb(hue, PEAK_VALUE);
    k_spin_unlock(&lock, key);

    rail_on();
    k_work_schedule(&tick_work, K_NO_WAIT);
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(skuratovich_key_rgb, position_listener);
ZMK_SUBSCRIPTION(skuratovich_key_rgb, zmk_position_state_changed);

static int key_rgb_init(void) {
    if (!device_is_ready(strip)) {
        LOG_ERR("led strip not ready, per-key rgb off");
        return -ENODEV;
    }
    rng_state = k_cycle_get_32() ^ 0x9e3779b9u;
    if (rng_state == 0) {
        rng_state = 0x9e3779b9u;
    }
    ready = true;
    return 0;
}

SYS_INIT(key_rgb_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
