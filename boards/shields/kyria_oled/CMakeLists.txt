# SPDX-License-Identifier: MIT

if(CONFIG_ZMK_DISPLAY AND CONFIG_KYRIA_OLED_WIDGET_STATUS)
  zephyr_library_include_directories(${CMAKE_SOURCE_DIR}/include)
  zephyr_library_sources(custom_status_screen.c)
  zephyr_library_sources(status_view.c)
  zephyr_library_sources(keys_view.c)
  zephyr_library_sources_ifdef(CONFIG_KYRIA_OLED_CLOCK clock_view.c)
  zephyr_library_sources_ifdef(CONFIG_KYRIA_OLED_CLOCK view_manager.c)
endif()
