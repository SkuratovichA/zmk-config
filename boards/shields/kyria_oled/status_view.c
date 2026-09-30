/*
 * SPDX-License-Identifier: MIT
 *
 * The status strip of the Kyria OLED screen: both batteries, the output with the Bluetooth
 * profiles, and the layer name, all drawn on one canvas that is redrawn as a whole.
 */

#include <stdio.h>

#include <zephyr/kernel.h>
#include <lvgl.h>

#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/endpoints.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/split_peripheral_status_changed.h>

#include <besim/draw.h>
#include <besim/status_state.h>

#include "layout.h"
#include "status_view.h"

static lv_obj_t *strip_canvas;
static uint8_t strip_buf[BESIM_CANVAS_BUF_SIZE(OLED_STATUS_W, OLED_STATUS_H)] __aligned(4);

/* The last state of each listener; the strip is redrawn from these four. */
static struct besim_battery_status_state battery_state;
static struct besim_peripheral_battery_status_state peripheral_state;
static struct besim_output_status_state output_state;
static struct besim_layer_status_state layer_state;

static void init_hollow_dsc(lv_draw_rect_dsc_t *dsc) {
    init_rect_dsc(dsc, LVGL_FOREGROUND);
    dsc->bg_opa = LV_OPA_TRANSP;
    dsc->border_color = LVGL_FOREGROUND;
    dsc->border_width = 1;
    dsc->border_opa = LV_OPA_COVER;
}

static void draw_output(lv_obj_t *canvas) {
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &lv_font_montserrat_12, LV_TEXT_ALIGN_LEFT);

    char text[16];
    switch (output_state.selected_endpoint.transport) {
    case ZMK_TRANSPORT_USB:
        snprintf(text, sizeof(text), "USB");
        break;
    case ZMK_TRANSPORT_BLE:
        snprintf(text, sizeof(text), "BT%d", output_state.active_profile_index + 1);
        break;
    default:
        snprintf(text, sizeof(text), "---");
        break;
    }
    canvas_draw_text(canvas, OLED_OUTPUT_X, OLED_OUTPUT_Y, OLED_OUTPUT_MAX_W, &label_dsc, text);
}

static void draw_profiles(lv_obj_t *canvas) {
    lv_draw_rect_dsc_t filled_dsc;
    init_rect_dsc(&filled_dsc, LVGL_FOREGROUND);
    lv_draw_rect_dsc_t hollow_dsc;
    init_hollow_dsc(&hollow_dsc);

    for (int i = 0; i < BESIM_PROFILE_COUNT; i++) {
        lv_coord_t x = OLED_PROFILE_X + i * OLED_PROFILE_PITCH;
        lv_coord_t y = OLED_PROFILE_Y;

        if (!output_state.profiles_bonded[i]) {
            canvas_draw_rect(canvas, x + 3, y + 3, 1, 1, &filled_dsc);
        } else if (i != output_state.active_profile_index) {
            canvas_draw_rect(canvas, x, y, OLED_PROFILE_SIZE, OLED_PROFILE_SIZE, &hollow_dsc);
        } else if (output_state.active_profile_connected) {
            canvas_draw_rect(canvas, x, y, OLED_PROFILE_SIZE, OLED_PROFILE_SIZE, &filled_dsc);
        } else {
            canvas_draw_rect(canvas, x, y, OLED_PROFILE_SIZE, OLED_PROFILE_SIZE, &hollow_dsc);
            canvas_draw_rect(canvas, x + 2, y + 2, 3, 3, &filled_dsc);
        }
    }
}

static void draw_layer(lv_obj_t *canvas) {
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &lv_font_unscii_8, LV_TEXT_ALIGN_RIGHT);
    /* Five characters fill the area; a longer name (renamed in Studio) overflows to the left
     * instead of wrapping onto a second line. */
    label_dsc.flag |= LV_TEXT_FLAG_EXPAND;

    char text[8];
    if (layer_state.label != NULL) {
        canvas_draw_text(canvas, OLED_LAYER_X, OLED_LAYER_Y, OLED_LAYER_MAX_W, &label_dsc,
                         layer_state.label);
    } else {
        snprintf(text, sizeof(text), "L%u", (unsigned int)layer_state.index);
        canvas_draw_text(canvas, OLED_LAYER_X, OLED_LAYER_Y, OLED_LAYER_MAX_W, &label_dsc, text);
    }
}

static void redraw_strip(void) {
    if (strip_canvas == NULL) {
        return;
    }
    lv_canvas_fill_bg(strip_canvas, LVGL_BACKGROUND, LV_OPA_COVER);

#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
    bool charging = battery_state.usb_present;
#else
    bool charging = false;
#endif
    draw_battery_row(strip_canvas, OLED_BATTERY_LEFT_X, OLED_BATTERY_Y, OLED_BATTERY_BODY_W, "L",
                     battery_state.level, charging, true);
    draw_battery_row(strip_canvas, OLED_BATTERY_RIGHT_X, OLED_BATTERY_Y, OLED_BATTERY_BODY_W, "R",
                     peripheral_state.level, false, peripheral_state.known);

    draw_output(strip_canvas);
    draw_profiles(strip_canvas);
    draw_layer(strip_canvas);
}

static void set_battery_status(struct besim_battery_status_state state) {
    battery_state = state;
    redraw_strip();
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_battery_status, struct besim_battery_status_state,
                            set_battery_status, besim_battery_status_get_state)
ZMK_SUBSCRIPTION(widget_battery_status, zmk_battery_state_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_battery_status, zmk_usb_conn_state_changed);
#endif

#if IS_ENABLED(CONFIG_ZMK_SPLIT) && IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)

static void set_peripheral_battery_status(struct besim_peripheral_battery_status_state state) {
    peripheral_state = state;
    redraw_strip();
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_peripheral_battery_status,
                            struct besim_peripheral_battery_status_state,
                            set_peripheral_battery_status,
                            besim_peripheral_battery_status_get_state)
ZMK_SUBSCRIPTION(widget_peripheral_battery_status, zmk_split_peripheral_status_changed);
#if IS_ENABLED(CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING)
ZMK_SUBSCRIPTION(widget_peripheral_battery_status, zmk_peripheral_battery_state_changed);
#endif

#endif /* CONFIG_ZMK_SPLIT && CONFIG_ZMK_SPLIT_ROLE_CENTRAL */

static void set_output_status(struct besim_output_status_state state) {
    output_state = state;
    redraw_strip();
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_output_status, struct besim_output_status_state,
                            set_output_status, besim_output_status_get_state)
ZMK_SUBSCRIPTION(widget_output_status, zmk_endpoint_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_output_status, zmk_usb_conn_state_changed);
#endif
#if IS_ENABLED(CONFIG_ZMK_BLE)
ZMK_SUBSCRIPTION(widget_output_status, zmk_ble_active_profile_changed);
#endif

static void set_layer_status(struct besim_layer_status_state state) {
    layer_state = state;
    redraw_strip();
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_layer_status, struct besim_layer_status_state,
                            set_layer_status, besim_layer_status_get_state)
ZMK_SUBSCRIPTION(widget_layer_status, zmk_layer_state_changed);

int oled_status_view_init(lv_obj_t *parent) {
    strip_canvas = lv_canvas_create(parent);
    if (strip_canvas == NULL) {
        return -ENOMEM;
    }
    lv_canvas_set_buffer(strip_canvas, strip_buf, OLED_STATUS_W, OLED_STATUS_H,
                         BESIM_CANVAS_COLOR_FORMAT);
    lv_obj_set_pos(strip_canvas, 0, 0);
    lv_canvas_fill_bg(strip_canvas, LVGL_BACKGROUND, LV_OPA_COVER);

    widget_battery_status_init();
#if IS_ENABLED(CONFIG_ZMK_SPLIT) && IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    widget_peripheral_battery_status_init();
#endif
    widget_output_status_init();
    widget_layer_status_init();

    return 0;
}
