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
