# LEARNING.md

Errors hit in this project and how they were fixed. Check here before starting a task.

- **Keymap used `&bt bt_disc N` and `&kp none`** — neither `bt_disc` nor `none` is a
  defined macro, so devicetree compilation failed and CI on `master` was red.
  Fix: `&bt BT_DISC N` and a bare `&none`.
- **`CONFIG_ZMK_KEYBOARD_NAME='name'` with single quotes** — Kconfig strings need
  double quotes: `CONFIG_ZMK_KEYBOARD_NAME="besimboard"`.
- **`CONFIG_BT_BUF_ACL_TX_COUNT=10` alone fails to compile** with
  `static assertion failed: "Increase Event RX buffer count to be greater than ACL TX buffer count"`.
  Zephyr requires `CONFIG_BT_BUF_EVT_RX_COUNT` (default 10) to be strictly greater, so raise it
  (we use 16) whenever the ACL TX count is raised.
- **`nice_nano_v2` is not a board any more** on ZMK main (Zephyr 4.1, Dec 2025): use
  `nice_nano//zmk` in build.yaml / `west build -b`, otherwise "No board named 'nice_nano_v2' found".
- **Local docker builds need `west zephyr-export` inside the same `docker run`** (the CMake package
  registry lives in the container's home, which `--rm` throws away); ZMK's `find_package(Zephyr)`
  does not use `ZEPHYR_BASE`.
- **`CONFIG_EC11_TRIGGER_OWN_THREAD=y` froze the keyboard when the encoder was used.** The EC11
  "own thread" is cooperative with a 1 KB stack and ZMK runs the entire sensor → behavior → HID →
  Bluetooth chain synchronously on it (the system work queue has 3 KB for the same work); the
  firmware has no stack guard or reset-on-fatal, so an overflow just hangs. Keep the documented
  `CONFIG_EC11_TRIGGER_GLOBAL_THREAD=y`. More generally: do not ship untested threading/BLE-buffer
  tuning to a keyboard that has no serial log; change one thing at a time.
- **There are three ZMK repos**: `zmk-config` (canonical, this one), `zmk-config-new`, and
  `zmk-config-lily58-pro` (the layout the user considers "normal": Shift on the home row, Ctrl
  below, layer 1 leaves ESC/Enter transparent, layer 2 has F-keys on the home row).
- **`docker pull`/`docker run` hung forever on this Mac** because every docker client call spawned `docker-credential-desktop get`, which never returns. Fix without touching `~/.docker/config.json`: run docker with `DOCKER_CONFIG=<dir holding a config.json of "{}">` and `DOCKER_HOST=unix:///Users/suka-/.docker/run/docker.sock`; leftover `docker-credential-desktop` processes (parent pid 1) ignore SIGTERM and need `kill -9`.
- **`CONFIG_WS2812_STRIP=y` aborts the build** on Zephyr 4.1: the symbol does not exist. `WS2812_STRIP_SPI` defaults to y as soon as the devicetree has a `worldsemi,ws2812-spi` node, and `ZMK_RGB_UNDERGLOW` selects `LED_STRIP`, so no WS2812 conf line is needed at all.
- **The external power rail state (`ext_power/state`) reaches flash only 60 s after a change** (`CONFIG_ZMK_SETTINGS_SAVE_DEBOUNCE`, default 60000). A reset within that minute restores the old state. With an OLED on the rail (Kyria left half), the recovery is: `&ext_power EP_ON`, wait a minute, then reset; the SSD1306 driver has no re-init path, so a rail that was cut leaves the panel dark until the next reset. `CONFIG_ZMK_RGB_UNDERGLOW_EXT_POWER=n` on the half with the display keeps the rail on when the LEDs are switched off. The settings_reset image erases the whole settings partition, including this key.
- **A conf assignment to a Kconfig symbol whose dependencies are unmet is only a warning** (the Studio lock lines of `config/*.conf` on the right halves, where `ZMK_STUDIO` is off); an assignment to an undefined symbol is a hard error.
- **Shell gotchas of the macOS host**: `cat -A` does not exist (use `cat -et`); in zsh a word starting with `=` (like `echo ======`) is expanded and fails, so quote it or start separators with a dash; an unquoted `$FILES` is not word-split, so lists go into arrays.
- **Workflow stages have only Read, Write and Edit**: no Glob, no Grep, no delete. A brief that asks a stage to grep the tree or to remove a file gets a "not done" back; do the grep in the brief and the deletion in a haiku command stage.
