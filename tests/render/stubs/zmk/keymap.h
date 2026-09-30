/*
 * SPDX-License-Identifier: MIT
 *
 * Host stub of ZMK's keymap header: the layer types and the functions the widgets call.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <zmk/behavior.h>

typedef uint8_t zmk_keymap_layer_id_t;
#define ZMK_KEYMAP_LAYER_ID_INVAL UINT8_MAX
typedef uint8_t zmk_keymap_layer_index_t;
typedef uint32_t zmk_keymap_layers_state_t;

zmk_keymap_layer_id_t zmk_keymap_layer_index_to_id(zmk_keymap_layer_index_t layer_index);
bool zmk_keymap_layer_active(zmk_keymap_layer_id_t layer);
zmk_keymap_layer_index_t zmk_keymap_highest_layer_active(void);
const char *zmk_keymap_layer_name(zmk_keymap_layer_id_t layer);
const struct zmk_behavior_binding *zmk_keymap_get_layer_binding_at_idx(zmk_keymap_layer_id_t layer,
                                                                       uint8_t binding_idx);
