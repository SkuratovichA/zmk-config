/*
 * SPDX-License-Identifier: MIT
 *
 * Where everything sits on the 128x64 OLED. All values are pixels.
 *
 *   y  0..23   status strip: batteries, output, profiles, layer
 *   y 24..63   six key caps in a 3x2 grid, or the idle glyph
 *   clock      a 102x48 face that moves inside the screen, shown instead
 */

#pragma once

#define OLED_WIDTH 128
#define OLED_HEIGHT 64

/* Status strip, one canvas at 0, 0. All positions below are inside it. */
#define OLED_STATUS_W 128
#define OLED_STATUS_H 24

/*
 * Battery row: "L 87%" and "R 64%" as text (UNSCII 8, five characters, 40 px),
 * the charge bolt after the left one when USB power is present.
 */
#define OLED_BATTERY_Y 0
#define OLED_BATTERY_LEFT_X 0
#define OLED_BATTERY_RIGHT_X 66
#define OLED_BATTERY_TEXT_MAX_W 47
#define OLED_BATTERY_BOLT_X 44

/* Output in Montserrat 12, whose line of 15 ends at the last row of the strip. */
#define OLED_OUTPUT_X 0
#define OLED_OUTPUT_Y 9
#define OLED_OUTPUT_MAX_W 43

/* Five Bluetooth profile boxes: x = OLED_PROFILE_X + i * OLED_PROFILE_PITCH. */
#define OLED_PROFILE_X 44
#define OLED_PROFILE_Y 13
#define OLED_PROFILE_SIZE 7
#define OLED_PROFILE_PITCH 9

/* Layer name in UNSCII 8, right-aligned: five characters fit. */
#define OLED_LAYER_X 88
#define OLED_LAYER_Y 12
#define OLED_LAYER_MAX_W 39

/*
 * Keys area, in screen coordinates. Each cap has a canvas of its own, so a
 * change redraws and sends one cap and not the whole area. The canvas of cap
 * i sits at x = (i % OLED_KEYS_COLS) * OLED_CAP_PITCH_X and
 * y = OLED_KEYS_Y + (i / OLED_KEYS_COLS) * OLED_CAP_PITCH_Y. The cap is drawn
 * at OLED_CAP_INSET inside it, which leaves room for the pop outline.
 */
#define OLED_KEYS_Y 24
#define OLED_KEYS_COLS 3
#define OLED_CAP_CANVAS_W 42
#define OLED_CAP_CANVAS_H 20
#define OLED_CAP_PITCH_X 43
#define OLED_CAP_PITCH_Y 20
#define OLED_CAP_W 38
#define OLED_CAP_H 16
#define OLED_CAP_INSET 2

/* The keyboard glyph shown while no key is on the screen, screen coordinates. */
#define OLED_IDLE_GLYPH_W 40
#define OLED_IDLE_GLYPH_H 20
#define OLED_IDLE_GLYPH_X 44
#define OLED_IDLE_GLYPH_Y 34

/*
 * Clock face, one canvas. It is moved once a minute so that no pixel stays
 * lit for hours: x = (n * 7) % OLED_CLOCK_SHIFT_X_RANGE and
 * y = (n * 5) % OLED_CLOCK_SHIFT_Y_RANGE for minute counter n.
 */
#define OLED_CLOCK_FACE_W 102
#define OLED_CLOCK_FACE_H 48
#define OLED_CLOCK_SHIFT_X_RANGE 27
#define OLED_CLOCK_SHIFT_Y_RANGE 17

/*
 * Seven-segment digits of 20x36 at y 0 of the face, bars 4 thick that overlap
 * at the corners: horizontal bars span the digit's width at y 0, 16 and 32;
 * vertical bars sit at x 0 and x 16, the upper pair from y 0 to 19, the lower
 * pair from y 16 to 35.
 */
#define OLED_CLOCK_DIGIT_W 20
#define OLED_CLOCK_DIGIT_H 36
#define OLED_CLOCK_SEGMENT 4
#define OLED_CLOCK_DIGIT_X0 0
#define OLED_CLOCK_DIGIT_X1 24
#define OLED_CLOCK_DIGIT_X2 58
#define OLED_CLOCK_DIGIT_X3 82
#define OLED_CLOCK_COLON_X 49
#define OLED_CLOCK_COLON_Y_TOP 10
#define OLED_CLOCK_COLON_Y_BOTTOM 22
#define OLED_CLOCK_COLON_SIZE 4

/* Battery line in UNSCII 8, eleven characters: "L 87% R 64%". */
#define OLED_CLOCK_BATTERY_X 7
#define OLED_CLOCK_BATTERY_Y 39
