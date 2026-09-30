#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
#
# Host render harness: compiles LVGL 9.3 and a variant's widget sources with the host C
# compiler, plays the scenario in main.c and writes every frame to out/<variant>/ as a raw
# file and a PNG.
#
#   bash tests/render/run.sh <variant>      variant: lily-old, lily-new or oled
#   bash tests/render/run.sh compare        lily-old against lily-new, frame by frame

set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$HERE/../.." && pwd)"
BUILD_DIR="$HERE/build"
OUT_DIR="$HERE/out"

# A west workspace with ZMK checked out at the revision of config/west.yml (LVGL and Zephyr
# come with it): ZMK_WS, or the pieces one by one. The old Lily sources are exported from
# git at LILY_OLD_REV, the commit before the display code was shared.
ZMK_WS="${ZMK_WS:-$HOME/zmk-ws}"
LVGL_DIR="${LVGL_DIR:-$ZMK_WS/modules/lib/gui/lvgl}"
ZMK_INCLUDE="${ZMK_INCLUDE:-$ZMK_WS/zmk/app/include}"
LILY_OLD_REV="${LILY_OLD_REV:-f02177d}"
LILY_OLD_DIR="${LILY_OLD_DIR:-$BUILD_DIR/lily-old-src/boards/shields/nice_view_besim}"
LILY_NEW_DIR="${LILY_NEW_DIR:-$REPO_ROOT/boards/shields/nice_view_besim}"
OLED_DIR="${OLED_DIR:-$REPO_ROOT/boards/shields/kyria_oled}"
SHARED_SRC_DIR="$REPO_ROOT/src/besim"
SHARED_INCLUDE_DIR="$REPO_ROOT/include"
CC="${CC:-cc}"
JOBS="${JOBS:-$(sysctl -n hw.ncpu 2>/dev/null || echo 4)}"

usage() {
    echo "usage: bash run.sh <lily-old|lily-new|oled|compare>" >&2
    exit 2
}

# Compares the frames of lily-old and lily-new byte for byte: one line per frame, then a count.
# Exits 1 when a frame differs or is missing, or when one of the variants has not been run.
compare_variants() {
    local variant dir
    for variant in lily-old lily-new; do
        dir="$OUT_DIR/$variant"
        if ! compgen -G "$dir/*.raw" >/dev/null; then
            echo "$variant has not been run: no frames in $dir (run: bash tests/render/run.sh $variant)" >&2
            exit 1
        fi
    done

    local old new name output rc pos
    local total=0 same=0
    for old in "$OUT_DIR/lily-old"/*.raw; do
        name="$(basename "$old")"
        new="$OUT_DIR/lily-new/$name"
        total=$((total + 1))
        if [ ! -f "$new" ]; then
            echo "${name%.raw}: missing in lily-new"
            continue
        fi
        rc=0
        output="$(cmp "$old" "$new" 2>&1)" || rc=$?
        case "$rc" in
        0)
            echo "${name%.raw}: identical"
            same=$((same + 1))
            ;;
        1)
            pos="$(printf '%s' "$output" | sed -n 's/.*differ: [a-z]* \([0-9][0-9]*\).*/\1/p')"
            if [ -n "$pos" ]; then
                echo "${name%.raw}: differs at byte $pos"
            else
                echo "${name%.raw}: differs ($output)"
            fi
            ;;
        *)
            echo "${name%.raw}: cmp failed ($output)"
            ;;
        esac
    done
    echo "identical: $same of $total"
    if [ "$same" -ne "$total" ]; then
        exit 1
    fi
    exit 0
}

[ $# -eq 1 ] || usage
VARIANT="$1"

if [ "$VARIANT" = compare ]; then
    compare_variants
fi

# Defaults of every variant; the OLED overrides the panel size, the polarity and the config.
# VARIANT_DEFS holds -D and -I flags, and is never empty, which bash 3.2 requires under set -u.
SCREEN_W=160
SCREEN_H=68
FRAME_COUNT=10
CONFIG_HEADER="$HERE/harness_config.h"

case "$VARIANT" in
lily-old)
    if [ ! -f "$LILY_OLD_DIR/custom_status_screen.c" ]; then
        mkdir -p "$BUILD_DIR/lily-old-src"
        git -C "$REPO_ROOT" archive "$LILY_OLD_REV" boards/shields/nice_view_besim |
            tar -x -C "$BUILD_DIR/lily-old-src"
    fi
    SRC_ROOT="$LILY_OLD_DIR"
    VARIANT_SRCS=(
        "$SRC_ROOT/custom_status_screen.c"
        "$SRC_ROOT/widgets/util.c"
        "$SRC_ROOT/widgets/status.c"
        "$SRC_ROOT/widgets/keys_widget.c"
        "$SRC_ROOT/widgets/glyphs.c"
        "$SRC_ROOT/widgets/bolt.c"
    )
    VARIANT_DEFS=(-DHARNESS_ROUND_Y=0)
    LIT_BIT=1
    ;;
lily-new)
    SRC_ROOT="$LILY_NEW_DIR"
    VARIANT_SRCS=(
        "$SRC_ROOT/custom_status_screen.c"
        "$SRC_ROOT/widgets/util.c"
        "$SRC_ROOT/widgets/status.c"
        "$SRC_ROOT/widgets/keys_widget.c"
        "$SRC_ROOT/widgets/bolt.c"
        "$SHARED_SRC_DIR/draw.c"
        "$SHARED_SRC_DIR/glyphs.c"
        "$SHARED_SRC_DIR/keys_core.c"
        "$SHARED_SRC_DIR/status_state.c"
    )
    VARIANT_DEFS=(-DHARNESS_ROUND_Y=0 -I"$SHARED_INCLUDE_DIR")
    LIT_BIT=1
    ;;
oled)
    SRC_ROOT="$OLED_DIR"
    # behavior_clock.c is left out: it needs ZMK's driver model.
    VARIANT_SRCS=(
        "$SRC_ROOT/custom_status_screen.c"
        "$SRC_ROOT/status_view.c"
        "$SRC_ROOT/keys_view.c"
        "$SRC_ROOT/clock_view.c"
        "$SRC_ROOT/view_manager.c"
        "$SHARED_SRC_DIR/draw.c"
        "$SHARED_SRC_DIR/glyphs.c"
        "$SHARED_SRC_DIR/keys_core.c"
        "$SHARED_SRC_DIR/status_state.c"
        "$SHARED_SRC_DIR/clock.c"
    )
    VARIANT_DEFS=(-DHARNESS_ROUND_Y=1 -DHARNESS_OLED=1 -DSCREEN_W=128 -DSCREEN_H=64
        -I"$SHARED_INCLUDE_DIR")
    SCREEN_W=128
    SCREEN_H=64
    FRAME_COUNT=18
    CONFIG_HEADER="$HERE/harness_config_oled.h"
    # On the OLED, LVGL black is the lit pixel.
    LIT_BIT=0
    ;;
*)
    usage
    ;;
esac

FRAME_BYTES=$((SCREEN_W * SCREEN_H))

# LVGL is configured the way the firmware does it: CONFIG_LV_* macros from autoconf_lv.h and
# a small lv_conf.h found through LV_CONF_PATH.
LV_CONF_FLAGS=(
    -imacros "$HERE/autoconf_lv.h"
    -DLV_CONF_INCLUDE_SIMPLE=1
    "-DLV_CONF_PATH=\"$HERE/lv_conf_host.h\""
)

LVGL_CFLAGS=(
    -std=gnu11 -O1 -w
    -I"$LVGL_DIR" -I"$LVGL_DIR/src"
    "${LV_CONF_FLAGS[@]}"
)

# The stubs directory comes before ZMK's include directory, so every zmk/ include resolves to
# a stub and only dt-bindings/ falls through to the real headers.
APP_CFLAGS=(
    -std=gnu11 -O1 -Wall
    -include "$CONFIG_HEADER"
    -I"$HERE/stubs" -I"$ZMK_INCLUDE"
    -I"$LVGL_DIR" -I"$LVGL_DIR/src"
    -I"$SRC_ROOT"
    "${LV_CONF_FLAGS[@]}"
    "${VARIANT_DEFS[@]}"
)

for path in "$LVGL_DIR/lvgl.h" "$ZMK_INCLUDE/dt-bindings/zmk/keys.h" "$CONFIG_HEADER" \
    "${VARIANT_SRCS[@]}"; do
    if [ ! -f "$path" ]; then
        echo "missing input: $path" >&2
        exit 1
    fi
done

# --- LVGL, compiled once ---------------------------------------------------------------------

LVGL_OBJ_DIR="$BUILD_DIR/lvgl"
mkdir -p "$LVGL_OBJ_DIR"

compile_lvgl() {
    local src="$1" obj="$2"
    if "$CC" "${LVGL_CFLAGS[@]}" -c "$src" -o "$obj.tmp" 2>"$obj.log"; then
        mv "$obj.tmp" "$obj"
        rm -f "$obj.log"
    fi
}

lvgl_objs=()
compiled=0
skipped=0
while IFS= read -r src; do
    rel="${src#"$LVGL_DIR"/src/}"
    obj="$LVGL_OBJ_DIR/${rel//\//__}.o"
    lvgl_objs+=("$obj")
    if [ -f "$obj" ]; then
        skipped=$((skipped + 1))
        continue
    fi
    compiled=$((compiled + 1))
    while [ "$(jobs -r | wc -l | tr -d ' ')" -ge "$JOBS" ]; do
        sleep 0.05
    done
    compile_lvgl "$src" "$obj" &
done < <(find "$LVGL_DIR/src" -name '*.c' | sort)
wait

missing=0
for obj in "${lvgl_objs[@]}"; do
    if [ ! -f "$obj" ]; then
        missing=$((missing + 1))
        if [ "$missing" -le 3 ]; then
            echo "LVGL compile failed: $obj" >&2
            head -30 "$obj.log" >&2 || true
        fi
    fi
done
if [ "$missing" -gt 0 ]; then
    echo "LVGL: $missing object(s) failed to compile" >&2
    exit 1
fi
echo "LVGL: ${#lvgl_objs[@]} objects ($compiled compiled, $skipped reused)"

# --- variant sources, stubs and main ---------------------------------------------------------

APP_OBJ_DIR="$BUILD_DIR/obj-$VARIANT"
rm -rf "$APP_OBJ_DIR"
mkdir -p "$APP_OBJ_DIR"

app_objs=()
failed=0
for src in "${VARIANT_SRCS[@]}" "$HERE/stubs/stubs.c" "$HERE/main.c"; do
    name="${src#"$HERE"/}"
    name="${name#"$SRC_ROOT"/}"
    name="${name#"$REPO_ROOT"/}"
    obj="$APP_OBJ_DIR/${name//\//__}.o"
    app_objs+=("$obj")
    if ! "$CC" "${APP_CFLAGS[@]}" -c "$src" -o "$obj" 2>"$obj.log"; then
        echo "compile failed: $src" >&2
        failed=1
    fi
    if [ -s "$obj.log" ]; then
        head -60 "$obj.log" >&2
    fi
done
if [ "$failed" -ne 0 ]; then
    exit 1
fi

BIN="$BUILD_DIR/$VARIANT"
"$CC" -o "$BIN" "${app_objs[@]}" "${lvgl_objs[@]}" -lm
echo "linked $BIN"

# --- run and convert -------------------------------------------------------------------------

VARIANT_OUT="$OUT_DIR/$VARIANT"
rm -rf "$VARIANT_OUT"
mkdir -p "$VARIANT_OUT"
"$BIN" "$VARIANT_OUT"

shopt -s nullglob
frames=("$VARIANT_OUT"/*.raw)
if [ "${#frames[@]}" -eq 0 ]; then
    echo "no frames were written" >&2
    exit 1
fi
for raw in "${frames[@]}"; do
    bytes="$(wc -c <"$raw" | tr -d ' ')"
    if [ "$bytes" -ne "$FRAME_BYTES" ]; then
        echo "$(basename "$raw"): $bytes bytes, expected $FRAME_BYTES" >&2
        exit 1
    fi
    python3 "$HERE/png.py" --width "$SCREEN_W" --height "$SCREEN_H" --scale 4 \
        --lit-bit "$LIT_BIT" "$raw" "${raw%.raw}.png"
    echo "$(basename "${raw%.raw}"): $bytes bytes"
done

if [ "${#frames[@]}" -ne "$FRAME_COUNT" ]; then
    echo "expected $FRAME_COUNT frames, got ${#frames[@]}" >&2
    exit 1
fi
echo "done: ${#frames[@]} frames in $VARIANT_OUT"
