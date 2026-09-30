/*
 * SPDX-License-Identifier: MIT
 *
 * Host render harness driver: creates an LVGL display shaped like the firmware's (I1 colour,
 * partial rendering, invalidated areas rounded for a tiled 1-bit panel), builds the status
 * screen, plays a scripted scenario on a virtual clock and dumps every frame as raw pixels.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lvgl.h>

#include <zephyr/kernel.h>
#include <zmk/endpoints.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/usb.h>

#ifdef HARNESS_OLED
#include <besim/clock.h>
#include <zmk/activity.h>
#include <zmk/events/activity_state_changed.h>
#endif

/* Set per variant with -D: the nice!view rounds x only, the OLED rounds y too. */
#ifndef HARNESS_ROUND_Y
#define HARNESS_ROUND_Y 0
#endif

/* The panel size: the nice!view is 160x68, the OLED variant passes 128x64 with -D. */
#ifndef SCREEN_W
#define SCREEN_W 160
#endif
#ifndef SCREEN_H
#define SCREEN_H 68
#endif
#define STRIDE_BYTES ((SCREEN_W + 7) / 8)
#define PALETTE_BYTES 8
#define DRAW_BUF_BYTES (((PALETTE_BYTES + STRIDE_BYTES * SCREEN_H) + 3) & ~3)

#define TICK_MS 10
#define SETTLE_MS 50

/* Defined by the shield's custom_status_screen.c. */
lv_obj_t *zmk_display_status_screen(void);

static lv_display_t *display;
static uint8_t draw_buf[DRAW_BUF_BYTES] LV_ATTRIBUTE_MEM_ALIGN;

/* Kept across flushes: an area the widgets forget to invalidate stays stale, as on the panel. */
static uint8_t framebuffer[SCREEN_H][SCREEN_W];

static const char *out_dir;

static uint32_t harness_ms(void) { return (uint32_t)harness_clock_ms; }

/* Display ---------------------------------------------------------------------------------- */

/* The Zephyr glue rounds the invalidated area to the panel's tiles before LVGL draws it. */
static void invalidate_area_cb(lv_event_t *e) {
    lv_area_t *area = lv_event_get_param(e);

    area->x1 &= ~7;
    area->x2 |= 7;
    if (area->x2 > SCREEN_W - 1) {
        area->x2 = SCREEN_W - 1;
    }
#if HARNESS_ROUND_Y
    area->y1 &= ~7;
    area->y2 |= 7;
    if (area->y2 > SCREEN_H - 1) {
        area->y2 = SCREEN_H - 1;
    }
#endif
}

/*
 * px_map starts with the 8 palette bytes; the pixels follow at 1 bit each, most significant bit
 * first, row by row over the flushed width, each row padded to whole bytes.
 */
static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    const int width = area->x2 - area->x1 + 1;
    const int stride = (width + 7) / 8;
    const uint8_t *pixels = px_map + PALETTE_BYTES;

    for (int y = area->y1; y <= area->y2; y++) {
        const uint8_t *row = pixels + (y - area->y1) * stride;
        for (int x = 0; x < width; x++) {
            const int fx = area->x1 + x;
            if (y < 0 || y >= SCREEN_H || fx < 0 || fx >= SCREEN_W) {
                continue;
            }
            framebuffer[y][fx] = (row[x >> 3] >> (7 - (x & 7))) & 1;
        }
    }
    lv_display_flush_ready(disp);
}

static void create_display(void) {
    display = lv_display_create(SCREEN_W, SCREEN_H);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_I1);
    lv_display_set_buffers(display, draw_buf, NULL, sizeof(draw_buf),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display, flush_cb);
    lv_display_add_event_cb(display, invalidate_area_cb, LV_EVENT_INVALIDATE_AREA, NULL);
}

/* Time ------------------------------------------------------------------------------------- */

/* The firmware ticks LVGL every 10 ms. */
static void tick(void) {
    harness_clock_ms += TICK_MS;
    harness_run_due_work();
    lv_timer_handler();
}

static void advance(int ms) {
    for (int elapsed = 0; elapsed < ms; elapsed += TICK_MS) {
        tick();
    }
}

static void settle(void) { advance(SETTLE_MS); }

/* Frames ----------------------------------------------------------------------------------- */

static void dump_frame(const char *name) {
    /* The refresh timer is 33 ms and the steps are shorter: draw what is pending so the frame
     * shows the state after the step. Invalidation is not touched, so stale areas stay stale. */
    lv_refr_now(display);

    char path[512];
    snprintf(path, sizeof(path), "%s/%s.raw", out_dir, name);
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        fprintf(stderr, "cannot open %s\n", path);
        exit(1);
    }
    if (fwrite(framebuffer, 1, sizeof(framebuffer), file) != sizeof(framebuffer)) {
        fprintf(stderr, "cannot write %s\n", path);
        exit(1);
    }
    fclose(file);

    /* On stdout, next to the contrast lines of the display stub, in the order they happen. */
    printf("frame %s\n", name);
}

/* Scenario helpers ------------------------------------------------------------------------- */

static void raise_position(uint32_t position, bool pressed) {
    HARNESS_RAISE(zmk_position_state_changed, {.source = ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL,
                                               .position = position,
                                               .state = pressed,
                                               .timestamp = k_uptime_get()});
}

static void raise_layer(uint8_t layer, bool state) {
    harness_scenario.layer_active[layer] = state;
    HARNESS_RAISE(zmk_layer_state_changed,
                  {.layer = layer, .state = state, .locked = false, .timestamp = k_uptime_get()});
}

static void set_boot_state(void) {
    harness_scenario.battery = 87;
    harness_scenario.usb_powered = true;
    harness_scenario.peripheral_battery = 64;
    harness_scenario.peripheral_connected = true;
    harness_scenario.transport = ZMK_TRANSPORT_USB;
    harness_scenario.endpoint_ble_profile = 0;
    harness_scenario.ble_active_profile = 0;
    for (int i = 0; i < HARNESS_PROFILES; i++) {
        harness_scenario.profile_connected[i] = false;
        harness_scenario.profile_open[i] = i > 1; /* profiles 0 and 1 are bonded */
    }
    memset(harness_scenario.layer_active, 0, sizeof(harness_scenario.layer_active));
    harness_scenario.layer_active[0] = true;
    harness_scenario.layer_names[0] = "BASE";
    harness_scenario.layer_names[2] = "FN";
}

/* Scenario --------------------------------------------------------------------------------- */

#ifdef HARNESS_OLED
/*
 * The clock of the OLED screen, on top of the ten common steps. The clock is driven through the
 * shared core; the activity events are raised here, because the harness does not model ZMK's
 * activity tracking. The idle clock starts 270 s after the idle event: the 300 s idle time of
 * the clock less the 30 s that ZMK itself waits before it raises the event.
 */
static void run_oled_scenario(void) {
    /* 11. idle: the clock face shows --:-- at contrast 16 */
    HARNESS_RAISE(zmk_activity_state_changed, {.state = ZMK_ACTIVITY_IDLE});
    advance(270000);
    dump_frame("11-idle-clock-unset");

    /* 12. the time is set by hand: 09:41 at full contrast, in peek mode */
    besim_clock_adjust_hours(9);
    besim_clock_adjust_minutes(41);
    tick();
    dump_frame("12-clock-set-peek");

    /* 13. the peek ends and the status view is back */
    advance(10000);
    tick();
    dump_frame("13-after-peek");

    /* 14. idle again: 09:45, dimmed, the face has moved */
    HARNESS_RAISE(zmk_activity_state_changed, {.state = ZMK_ACTIVITY_IDLE});
    advance(270000);
    dump_frame("14-idle-clock");

    /* 15. a minute later: 09:46, the face has moved again */
    advance(60000);
    dump_frame("15-minute-tick");

    /* 16. activity: the status view is back, a key shows */
    HARNESS_RAISE(zmk_activity_state_changed, {.state = ZMK_ACTIVITY_ACTIVE});
    raise_position(1, true);
    tick();
    dump_frame("16-back-to-status");

    /* 17. a clock key: the peek */
    raise_position(1, false);
    besim_clock_request_show();
    tick();
    dump_frame("17-peek");

    /* 18. any other key cancels the peek */
    raise_position(0, true);
    tick();
    dump_frame("18-peek-cancelled");
    raise_position(0, false);
}
#endif

static void run_scenario(void) {
    /* 1. boot */
    settle();
    dump_frame("01-boot");

    /* 2. endpoint changes to BLE, profile 1 connected */
    harness_scenario.transport = ZMK_TRANSPORT_BLE;
    harness_scenario.endpoint_ble_profile = 1;
    harness_scenario.ble_active_profile = 1;
    harness_scenario.profile_connected[1] = true;
    HARNESS_RAISE(zmk_endpoint_changed,
                  {.endpoint = {.transport = ZMK_TRANSPORT_BLE, .ble = {.profile_index = 1}}});
    HARNESS_RAISE(zmk_ble_active_profile_changed, {.index = 1, .profile = NULL});
    settle();
    dump_frame("02-ble");

    /* 3. layer 2 (FN) becomes active */
    raise_layer(2, true);
    settle();
    dump_frame("03-fn");

    /* 4. position 0 pressed */
    raise_position(0, true);
    tick();
    dump_frame("04-press-pop");

    /* 5. held */
    advance(200);
    dump_frame("05-held");

    /* 6. released */
    raise_position(0, false);
    tick();
    dump_frame("06-ghost-fresh");

    /* 7. the ghost shrinks */
    advance(350);
    dump_frame("07-ghost-old");

    /* 8. the ghost is gone */
    advance(400);
    dump_frame("08-idle");

    /* 9. positions 1, 2 and 3 pressed 50 ms apart, then all released */
    raise_position(1, true);
    advance(50);
    raise_position(2, true);
    advance(50);
    raise_position(3, true);
    advance(50);
    raise_position(1, false);
    raise_position(2, false);
    raise_position(3, false);
    tick();
    dump_frame("09-three");

    /* 10. low battery, USB unplugged, layer 2 off */
    harness_scenario.battery = 12;
    harness_scenario.usb_powered = false;
    HARNESS_RAISE(zmk_battery_state_changed, {.state_of_charge = 12});
    HARNESS_RAISE(zmk_usb_conn_state_changed, {.conn_state = ZMK_USB_CONN_NONE});
    raise_layer(2, false);
    settle();
    dump_frame("10-low-battery");

#ifdef HARNESS_OLED
    run_oled_scenario();
#endif
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <output directory>\n", argv[0]);
        return 2;
    }
    out_dir = argv[1];

    set_boot_state();

    lv_init();
    lv_tick_set_cb(harness_ms);
    create_display();

    lv_obj_t *screen = zmk_display_status_screen();
    lv_screen_load(screen);
    /* Set once the screen exists, as the firmware does, so that listeners start reacting. */
    harness_scenario.display_initialized = true;

    run_scenario();
    return 0;
}
