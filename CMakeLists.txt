# SPDX-License-Identifier: MIT
#
# Code shared by the status screens of this config. A shield selects the parts
# its screen needs through the BESIM_* symbols of the Kconfig file next to this
# one. The include directory is set for every build, because the shield sources
# use the headers (a keymap reaches include/dt-bindings through dts_root).

zephyr_include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

if(CONFIG_BESIM_DRAW OR CONFIG_BESIM_KEYS_CORE OR CONFIG_BESIM_STATUS_STATE OR CONFIG_BESIM_CLOCK)
  zephyr_library_named(besim)
  zephyr_library_include_directories(${CMAKE_SOURCE_DIR}/include)
  zephyr_library_sources_ifdef(CONFIG_BESIM_DRAW src/besim/draw.c src/besim/glyphs.c)
  zephyr_library_sources_ifdef(CONFIG_BESIM_KEYS_CORE src/besim/keys_core.c)
  zephyr_library_sources_ifdef(CONFIG_BESIM_STATUS_STATE src/besim/status_state.c)
  zephyr_library_sources_ifdef(CONFIG_BESIM_CLOCK src/besim/clock.c)
  zephyr_library_sources_ifdef(CONFIG_BESIM_BEHAVIOR_CLOCK src/besim/behavior_clock.c)
endif()
