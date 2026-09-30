# SPDX-License-Identifier: MIT
#
# Code shared by the status screens of this config. A shield selects the parts
# its screen needs through the SHARED_* symbols of the Kconfig file next to this
# one. The include directory is set for every build, because the shield sources
# use the headers (a keymap reaches include/dt-bindings through dts_root).

zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

if(CONFIG_SHARED_DRAW OR CONFIG_SHARED_KEYS_CORE OR CONFIG_SHARED_STATUS_STATE OR CONFIG_SHARED_CLOCK
   OR CONFIG_SHARED_KEY_RGB)
  zephyr_library_named(zmk_config_shared)
  zephyr_library_include_directories(${CMAKE_SOURCE_DIR}/include)
  zephyr_library_sources_ifdef(CONFIG_SHARED_DRAW src/shared/draw.c src/shared/glyphs.c)
  zephyr_library_sources_ifdef(CONFIG_SHARED_KEYS_CORE src/shared/keys_core.c)
  zephyr_library_sources_ifdef(CONFIG_SHARED_STATUS_STATE src/shared/status_state.c)
  zephyr_library_sources_ifdef(CONFIG_SHARED_CLOCK src/shared/clock.c)
  zephyr_library_sources_ifdef(CONFIG_SHARED_BEHAVIOR_CLOCK src/shared/behavior_clock.c)
  zephyr_library_sources_ifdef(CONFIG_SHARED_KEY_RGB src/shared/key_rgb.c)
endif()
