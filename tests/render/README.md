# Host render harness for the status screens

This harness compiles LVGL 9.3 and the status-screen widget sources with the host C compiler, drives them with a scripted scenario on a virtual clock and dumps every frame as raw pixels and as a PNG. It exists so that the screens can be looked at, and compared byte for byte between two versions of the drawing code, without a keyboard at hand.

The display model follows the firmware's Zephyr glue: an LVGL display in I1 colour with partial rendering, invalidated areas rounded to the panel's 8-pixel tiles, and a framebuffer that survives between flushes. An area the widgets forget to invalidate therefore stays stale in the dump, as it does on the panel. The nice!view of the Lily is 160x68, the OLED of the Kyria is 128x64.

## Running

```
bash tests/render/run.sh lily-old
bash tests/render/run.sh lily-new
bash tests/render/run.sh compare
bash tests/render/run.sh oled
```

The script needs Apple clang as `cc`, python3 with the standard library, and a west workspace that holds ZMK at the revision of `config/west.yml` together with its LVGL module: point `ZMK_WS` at it (default `~/zmk-ws`; the workspace the Docker build uses works, as it is a plain directory), or set `LVGL_DIR` and `ZMK_INCLUDE` one by one. The old Lily sources for `lily-old` are exported with `git archive` from the commit `LILY_OLD_REV` (default `f02177d`) into `build/lily-old-src/`. It compiles LVGL once into `build/lvgl/` and reuses the objects on later runs, compiles the variant's sources, the stubs and `main.c` with `-Wall`, links `build/<variant>`, runs it and converts the frames. It prints the run's own log (one `frame <name>` line per frame, and a `contrast=<value>` line for every contrast change of the OLED) and then one line per frame with the raw file's size, and exits non-zero on any failure. The paths of LVGL, of ZMK's `dt-bindings` headers and of the Lily sources are defaults at the top of `run.sh` and can be overridden with the environment variables `LVGL_DIR`, `ZMK_INCLUDE`, `LILY_OLD_DIR`, `LILY_NEW_DIR` and `OLED_DIR`.

Output goes to `out/<variant>/<name>.raw` (one byte per pixel, rows of the panel's width, 0 is LVGL black and 1 is LVGL white) and `out/<variant>/<name>.png` (scaled 4x, 8-bit greyscale). `build/` and `out/` are ignored by git.

## Variants

- `lily-old`: a frozen copy of the Lily sources at commit f02177d, the reference the new code has to match. 160x68, 10 frames.
- `lily-new`: the Lily shield in `boards/shields/nice_view_besim/` as it is now, drawing through the shared sources in `src/shared/` (`draw.c`, `glyphs.c`, `keys_core.c`, `status_state.c`), with the shared headers from `include/`. 160x68, the same 10 frames, and the same `harness_config.h` as `lily-old`.
- `oled`: the Kyria's screen in `boards/shields/kyria_oled/` with the same shared sources plus `clock.c`. It is built with `-DHARNESS_OLED=1`, which adds the clock scenario to `main.c`, rounds invalidated areas in y as well as in x, and uses `harness_config_oled.h` instead of `harness_config.h`. 128x64, 18 frames. `behavior_clock.c` is not built, because it needs ZMK's driver model; the scenario drives the clock through the shared core in `shared/clock.h`.
- `compare`: not a build. It compares every `*.raw` of `out/lily-old` with the file of the same name in `out/lily-new` with `cmp`, prints `identical` or `differs at byte N` per frame and `identical: K of N` at the end. It exits 1 when any frame differs or is missing, and says so when a variant has not been run yet. A difference is a finding about the shared split, not something to be made to pass.

## Polarity

The two panels are lit the opposite way. On the Lily, LVGL white (pixel value 1) is the light pixel, the nice!view is a reflective display that shows black on white. On the OLED, LVGL black (pixel value 0) is the lit pixel. `run.sh` passes `--lit-bit 1` for the Lily variants and `--lit-bit 0` for the OLED to `png.py`, so that in every PNG the lit pixel is drawn bright. The `.raw` files keep LVGL's values as they are.

## Layout

- `run.sh` builds and runs a variant and compares the two Lily variants, `main.c` is the driver and the scenario, `png.py` writes the PNGs.
- `stubs/` holds host stand-ins for the Zephyr and ZMK headers the sources include, and `stubs/stubs.c` their function bodies over one scenario state that `main.c` edits. The stubs directory comes before ZMK's include directory, so every `zmk/` include resolves to a stub and only `dt-bindings/` reaches the real headers.
- Delayable work (`k_work_delayable`) is armed with a deadline on the virtual clock and run by `harness_run_due_work()`, which `main.c` calls on every 10 ms tick before LVGL's timer handler. Plain `k_work_submit_to_queue` stays synchronous.
- `autoconf_lv.h` and `lv_conf_host.h` carry the LVGL settings, `harness_config.h` the Kconfig symbols of the Lily left build and `harness_config_oled.h` those of the Kyria's OLED build.

## Frames

Frames 1 to 10 are the same on all variants.

1. `01-boot`: own battery 87 percent and USB powered, right half 64 percent, USB output, layer BASE, profiles 1 and 2 bonded and none connected.
2. `02-ble`: the output changes to Bluetooth profile 2, which is connected.
3. `03-fn`: layer 2, named FN, becomes active.
4. `04-press-pop`: key position 0 is pressed and its cap pops with the extra outline.
5. `05-held`: 200 ms later the key is still held, a solid cap.
6. `06-ghost-fresh`: the key is released and stays as a hollow cap.
7. `07-ghost-old`: 350 ms later the hollow cap has shrunk.
8. `08-idle`: 400 ms later the cap is gone and the keyboard glyph shows.
9. `09-three`: positions 1, 2 and 3 pressed 50 ms apart and released together.
10. `10-low-battery`: battery 12 percent, USB unplugged, layer 2 off.

The `oled` variant continues with the clock. The scenario raises the activity events itself, since the harness does not model ZMK's activity tracking, and each one prints a `contrast=` line when the view manager changes the panel contrast.

11. `11-idle-clock-unset`: the activity state becomes idle and 270 s pass, which with ZMK's own 30 s idle timeout is the 300 s of the clock. The face shows `--:--` because the time was never set, at contrast 16.
12. `12-clock-set-peek`: the time is set to 09:41 by hand. The clock shows 09:41 at contrast 128, in peek mode.
13. `13-after-peek`: 10 s later the peek has ended and the status view is back.
14. `14-idle-clock`: idle again for 270 s, the clock shows 09:45 dimmed, and the face has moved.
15. `15-minute-tick`: 60 s later it shows 09:46 and the face has moved again.
16. `16-back-to-status`: the activity state becomes active and position 1 is pressed: the status view with the cap `ENT`, contrast 128.
17. `17-peek`: position 1 is released and the clock key is requested: the clock peeks.
18. `18-peek-cancelled`: position 0 is pressed, which cancels the peek: the status view with the cap `A`.
