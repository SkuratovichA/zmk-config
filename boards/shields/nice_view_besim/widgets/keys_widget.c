/*
 * SPDX-License-Identifier: MIT
 *
 * Pressed-keys view: a 2x3 grid of keycaps in the middle band of the screen.
 *
 *  - a key that is held is a solid cap with its label knocked out
 *  - for the first ~120 ms after the press the cap "pops" (extra outline)
 *  - a released key stays as a hollow cap for ~600 ms, shrinking half-way, then goes
 *  - with nothing held the band shows a small keyboard glyph
 *
 * Labels come from the keymap: the binding at that position on the highest
 * active layer (skipping &trans), so LOWER + J shows an arrow, not "J". On the
 * right half only the base layer is known, so it shows base-layer labels.
 * On the left half keys from both halves are shown (the central sees them all).
 */

#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/spinlock.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <dt-bindings/zmk/bt.h>
#include <dt-bindings/zmk/hid_usage_pages.h>
#include <dt-bindings/zmk/outputs.h>
#include <zmk/behavior.h>
#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/keymap.h>

#include "keys_widget.h"
#include "util.h"

#if !NICEVIEW_IS_CENTRAL
/*
 * ZMK only compiles keymap.c on the central, so the peripheral extracts the
 * keymap from the devicetree itself (same macros ZMK uses) and shows base-layer
 * labels.
 */
#include <zmk/matrix.h>
#define BESIM_KEYMAP_LAYER(node)                                                                   \
    {LISTIFY(DT_PROP_LEN(node, bindings), ZMK_KEYMAP_EXTRACT_BINDING, (, ), node)},
static const struct zmk_behavior_binding keymap_layers[][ZMK_KEYMAP_LEN] = {
    ZMK_KEYMAP_LAYERS_FOREACH(BESIM_KEYMAP_LAYER)};
#endif

#define KEYS_SLOTS 6
#define KEYS_TICK_MS 100
#define KEYS_POP_MS 120
#define KEYS_GHOST_MS 600
#define KEYS_LABEL_MAX 6

#define CAP_W 33
#define CAP_H 20
#define CAP_GAP_X 2
#define CAP_GAP_Y 2
#define CAP_TOP 2

struct key_slot {
    bool used;
    bool held;
    uint32_t position;
    int64_t pressed_at;
    int64_t released_at;
    char label[KEYS_LABEL_MAX];
};

/* Written from the event thread, read from the display thread. */
static struct key_slot slots[KEYS_SLOTS];
static struct k_spinlock slots_lock;

/* Display-thread only. */
static lv_obj_t *keys_canvas;
static uint8_t keys_cbuf[CANVAS_BUF_SIZE];
static lv_timer_t *keys_timer;
static bool keys_idle_drawn;

/* Labels ----------------------------------------------------------------- */

#define KEYCODE_MODS(kc) (((kc) >> 24) & 0xFF)

static const char *keyboard_usage_label(uint16_t id, char *buf, size_t len) {
    if (id >= 0x04 && id <= 0x1D) {
        buf[0] = 'A' + (id - 0x04);
        buf[1] = '\0';
        return buf;
    }
    if (id >= 0x1E && id <= 0x26) {
        buf[0] = '1' + (id - 0x1E);
        buf[1] = '\0';
        return buf;
    }
    if (id >= 0x3A && id <= 0x45) {
        snprintf(buf, len, "F%d", id - 0x3A + 1);
        return buf;
    }
    switch (id) {
    case 0x27:
        return "0";
    case 0x28:
        return "ENT";
    case 0x29:
        return "ESC";
    case 0x2A:
        return "BSP";
    case 0x2B:
        return "TAB";
    case 0x2C:
        return "SPC";
    case 0x2D:
        return "-";
    case 0x2E:
        return "=";
    case 0x2F:
        return "[";
    case 0x30:
        return "]";
    case 0x31:
        return "\\";
    case 0x33:
        return ";";
    case 0x34:
        return "'";
    case 0x35:
        return "`";
    case 0x36:
        return ",";
    case 0x37:
        return ".";
    case 0x38:
        return "/";
    case 0x39:
        return "CAPS";
    case 0x4A:
        return "HOME";
    case 0x4B:
        return "PGUP";
    case 0x4C:
        return "DEL";
    case 0x4D:
        return "END";
    case 0x4E:
        return "PGDN";
    case 0x4F:
        return LV_SYMBOL_RIGHT;
    case 0x50:
        return LV_SYMBOL_LEFT;
    case 0x51:
        return LV_SYMBOL_DOWN;
    case 0x52:
        return LV_SYMBOL_UP;
    case 0xE0:
    case 0xE4:
        return "CTL";
    case 0xE1:
    case 0xE5:
        return "SFT";
    case 0xE2:
    case 0xE6:
        return "ALT";
    case 0xE3:
    case 0xE7:
        return "CMD";
    default:
        snprintf(buf, len, "%02X", id);
        return buf;
    }
}

static const char *consumer_usage_label(uint16_t id) {
    switch (id) {
    case 0xE9:
        return "VOL+";
    case 0xEA:
        return "VOL-";
    case 0xE2:
        return "MUTE";
    case 0xCD:
        return "PLAY";
    case 0xB5:
        return "NEXT";
    case 0xB6:
        return "PREV";
    case 0x6F:
        return "BRI+";
    case 0x70:
        return "BRI-";
    default:
        return "MEDIA";
    }
}

/* Returns false when nothing should be shown for this binding. */
static bool binding_label(const struct zmk_behavior_binding *binding, char *out, size_t len) {
    const char *dev = binding->behavior_dev;
    char tmp[KEYS_LABEL_MAX];

    if (strcmp(dev, "key_press") == 0) {
        const uint32_t keycode = binding->param1;
        const uint8_t page = ZMK_HID_USAGE_PAGE(keycode);
        const uint16_t id = ZMK_HID_USAGE_ID(keycode);
        const char *label = (page == HID_USAGE_CONSUMER)
                                ? consumer_usage_label(id)
                                : keyboard_usage_label(id, tmp, sizeof(tmp));
        if (KEYCODE_MODS(keycode) != 0 && page != HID_USAGE_CONSUMER) {
            snprintf(out, len, "+%s", label);
        } else {
            strncpy(out, label, len - 1);
            out[len - 1] = '\0';
        }
        return true;
    }
    if (strcmp(dev, "momentary_layer") == 0) {
        snprintf(out, len, "L%u", (unsigned)binding->param1);
        return true;
    }
    if (strcmp(dev, "bluetooth") == 0) {
        switch (binding->param1) {
        case BT_SEL_CMD:
            snprintf(out, len, "BT%u", (unsigned)binding->param2 + 1);
            break;
        case BT_DISC_CMD:
            snprintf(out, len, "DC%u", (unsigned)binding->param2 + 1);
            break;
        case BT_CLR_ALL_CMD:
            strncpy(out, "CLR*", len);
            break;
        case BT_CLR_CMD:
            strncpy(out, "CLR", len);
            break;
        default:
            strncpy(out, "BT", len);
            break;
        }
        out[len - 1] = '\0';
        return true;
    }
    if (strcmp(dev, "outputs") == 0) {
        const char *label = binding->param1 == OUT_USB   ? "USB"
                            : binding->param1 == OUT_BLE ? "BLE"
                                                         : "OUT";
        strncpy(out, label, len - 1);
        out[len - 1] = '\0';
        return true;
    }
    if (strcmp(dev, "bootload") == 0) {
        strncpy(out, "BOOT", len - 1);
        out[len - 1] = '\0';
        return true;
    }
    if (strcmp(dev, "studio_unlock") == 0) {
        strncpy(out, "OPEN", len - 1);
        out[len - 1] = '\0';
        return true;
    }
    if (strcmp(dev, "none") == 0) {
        return false;
    }
    strncpy(out, "?", len - 1);
    out[len - 1] = '\0';
    return true;
}

static const struct zmk_behavior_binding *resolve_binding(uint32_t position) {
#if NICEVIEW_IS_CENTRAL
    for (int index = zmk_keymap_highest_layer_active(); index >= 0; index--) {
        const zmk_keymap_layer_id_t id = zmk_keymap_layer_index_to_id(index);
        if (!zmk_keymap_layer_active(id)) {
            continue;
        }
        const struct zmk_behavior_binding *binding =
            zmk_keymap_get_layer_binding_at_idx(id, position);
        if (binding == NULL || strcmp(binding->behavior_dev, "transparent") == 0) {
            continue;
        }
        return binding;
    }
    return NULL;
#else
    if (position >= ZMK_KEYMAP_LEN) {
        return NULL;
    }
    const struct zmk_behavior_binding *binding = &keymap_layers[0][position];
    if (strcmp(binding->behavior_dev, "transparent") == 0) {
        return NULL;
    }
    return binding;
#endif
}

/* Slots (event thread) ------------------------------------------------ */

static int find_slot_for_position(uint32_t position) {
    for (int i = 0; i < KEYS_SLOTS; i++) {
        if (slots[i].used && slots[i].position == position) {
            return i;
        }
    }
    return -1;
}

static int find_free_slot(void) {
    for (int i = 0; i < KEYS_SLOTS; i++) {
        if (!slots[i].used) {
            return i;
        }
    }
    /* All taken: evict the oldest released key, else the oldest pressed one. */
    int victim = -1;
    int64_t oldest = INT64_MAX;
    for (int i = 0; i < KEYS_SLOTS; i++) {
        if (!slots[i].held && slots[i].released_at < oldest) {
            oldest = slots[i].released_at;
            victim = i;
        }
    }
    if (victim >= 0) {
        return victim;
    }
    for (int i = 0; i < KEYS_SLOTS; i++) {
        if (slots[i].pressed_at < oldest) {
            oldest = slots[i].pressed_at;
            victim = i;
        }
    }
    return victim;
}

static void keys_redraw_work_cb(struct k_work *work);
K_WORK_DEFINE(keys_redraw_work, keys_redraw_work_cb);

static int keys_position_listener(const zmk_event_t *eh) {
    const struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);
    if (ev == NULL || !zmk_display_is_initialized()) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    const int64_t now = k_uptime_get();

    if (ev->state) {
        char label[KEYS_LABEL_MAX];
        const struct zmk_behavior_binding *binding = resolve_binding(ev->position);
        if (binding == NULL) {
            strncpy(label, "?", sizeof(label));
        } else if (!binding_label(binding, label, sizeof(label))) {
            return ZMK_EV_EVENT_BUBBLE;
        }

        k_spinlock_key_t key = k_spin_lock(&slots_lock);
        int i = find_slot_for_position(ev->position);
        if (i < 0) {
            i = find_free_slot();
        }
        slots[i].used = true;
        slots[i].held = true;
        slots[i].position = ev->position;
        slots[i].pressed_at = now;
        slots[i].released_at = 0;
        memcpy(slots[i].label, label, sizeof(slots[i].label));
        k_spin_unlock(&slots_lock, key);
    } else {
        k_spinlock_key_t key = k_spin_lock(&slots_lock);
        const int i = find_slot_for_position(ev->position);
        if (i >= 0 && slots[i].held) {
            slots[i].held = false;
            slots[i].released_at = now;
        }
        k_spin_unlock(&slots_lock, key);
    }

    k_work_submit_to_queue(zmk_display_work_q(), &keys_redraw_work);
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(widget_keys, keys_position_listener);
ZMK_SUBSCRIPTION(widget_keys, zmk_position_state_changed);

/* Drawing (display thread) -------------------------------------------- */

static void draw_cap(lv_obj_t *canvas, lv_coord_t x, lv_coord_t y, const char *label, bool solid,
                     bool pop, lv_coord_t shrink) {
    lv_draw_rect_dsc_t fg_dsc;
    init_rect_dsc(&fg_dsc, LVGL_FOREGROUND);
    fg_dsc.radius = 4;
    lv_draw_rect_dsc_t bg_dsc;
    init_rect_dsc(&bg_dsc, LVGL_BACKGROUND);
    bg_dsc.radius = 3;

    x += shrink;
    y += shrink;
    const lv_coord_t w = CAP_W - 2 * shrink;
    const lv_coord_t h = CAP_H - 2 * shrink;

    if (pop) {
        canvas_draw_rect(canvas, x - 2, y - 2, w + 4, h + 4, &fg_dsc);
        canvas_draw_rect(canvas, x - 1, y - 1, w + 2, h + 2, &bg_dsc);
    }
    canvas_draw_rect(canvas, x, y, w, h, &fg_dsc);
    if (!solid) {
        canvas_draw_rect(canvas, x + 1, y + 1, w - 2, h - 2, &bg_dsc);
    }

    const bool wide = strlen(label) > 3;
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, solid ? LVGL_BACKGROUND : LVGL_FOREGROUND,
                   wide ? &lv_font_unscii_8 : &lv_font_montserrat_12, LV_TEXT_ALIGN_CENTER);
    const lv_coord_t text_y = y + (wide ? (h - 8) / 2 : (h - 14) / 2);
    canvas_draw_text(canvas, x, text_y, w, &label_dsc, label);
}

static void draw_idle(lv_obj_t *canvas) {
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &lv_font_montserrat_16, LV_TEXT_ALIGN_CENTER);
    lv_canvas_fill_bg(canvas, LVGL_BACKGROUND, LV_OPA_COVER);
    canvas_draw_text(canvas, 0, 26, CANVAS_SIZE, &label_dsc, LV_SYMBOL_KEYBOARD);
    rotate_canvas(canvas);
}

static void keys_redraw(void) {
    if (keys_canvas == NULL) {
        return;
    }

    const int64_t now = k_uptime_get();
    struct key_slot local[KEYS_SLOTS];

    k_spinlock_key_t key = k_spin_lock(&slots_lock);
    for (int i = 0; i < KEYS_SLOTS; i++) {
        if (slots[i].used && !slots[i].held && now - slots[i].released_at >= KEYS_GHOST_MS) {
            slots[i].used = false;
        }
    }
    memcpy(local, slots, sizeof(local));
    k_spin_unlock(&slots_lock, key);

    bool any = false;
    for (int i = 0; i < KEYS_SLOTS; i++) {
        any = any || local[i].used;
    }

    if (!any) {
        if (!keys_idle_drawn) {
            draw_idle(keys_canvas);
            keys_idle_drawn = true;
        }
        return;
    }
    keys_idle_drawn = false;

    lv_canvas_fill_bg(keys_canvas, LVGL_BACKGROUND, LV_OPA_COVER);
    for (int i = 0; i < KEYS_SLOTS; i++) {
        if (!local[i].used) {
            continue;
        }
        const lv_coord_t x = (i % 2) * (CAP_W + CAP_GAP_X);
        const lv_coord_t y = CAP_TOP + (i / 2) * (CAP_H + CAP_GAP_Y);
        if (local[i].held) {
            const bool pop = now - local[i].pressed_at < KEYS_POP_MS;
            draw_cap(keys_canvas, x, y, local[i].label, true, pop, 0);
        } else {
            const lv_coord_t shrink = (now - local[i].released_at < KEYS_GHOST_MS / 2) ? 0 : 2;
            draw_cap(keys_canvas, x, y, local[i].label, false, false, shrink);
        }
    }
    rotate_canvas(keys_canvas);
}

static void keys_redraw_work_cb(struct k_work *work) { keys_redraw(); }

static void keys_tick_cb(lv_timer_t *timer) { keys_redraw(); }

/* ---------------------------------------------------------------------- */

int zmk_widget_keys_init(lv_obj_t *parent, lv_coord_t x) {
    keys_canvas = lv_canvas_create(parent);
    lv_obj_align(keys_canvas, LV_ALIGN_TOP_LEFT, x, 0);
    lv_canvas_set_buffer(keys_canvas, keys_cbuf, CANVAS_SIZE, CANVAS_SIZE, CANVAS_COLOR_FORMAT);

    keys_redraw();
    keys_timer = lv_timer_create(keys_tick_cb, KEYS_TICK_MS, NULL);
    return 0;
}
