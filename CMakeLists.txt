# Native SDL2 Galaxian for Linux handhelds and desktops (aarch64, x86_64),
# Windows (MinGW), macOS and the web (Emscripten). See GALAXIAN_PORT_PLAN.md.
cmake_minimum_required(VERSION 3.13)
project(galaxian VERSION 0.2.1 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
# The sim must stay bit-exact across architectures: no -ffast-math.
add_compile_options(-Wall -Wextra -Wno-unused-parameter)

option(GALAXIAN_SANITIZE "Build with ASan/UBSan" OFF)
if(GALAXIAN_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

add_library(sdl2 INTERFACE)
if(EMSCRIPTEN)
  # Emscripten's own SDL2 port
  target_compile_options(sdl2 INTERFACE -sUSE_SDL=2)
  target_link_options(sdl2 INTERFACE -sUSE_SDL=2)
else()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
  target_link_libraries(sdl2 INTERFACE PkgConfig::SDL2)
endif()

# Pure C, no SDL: ROM and asset loading, framebuffer drawing.
add_library(galaxian_core STATIC
  src/assets/inflate.c
  src/assets/png.c
  src/assets/zip.c
  src/assets/rom.c
  src/assets/assets.c
  src/assets/savefile.c
  src/assets/extract.c
  src/gfx/fb.c
  src/gfx/blit.c
  src/gfx/scale.c
  src/gfx/enhance.c
  src/gfx/debugfont.c
  src/gfx/galaxian.c
  src/emu/z80.c
  src/emu/board.c
  src/emu/sound.c
  src/sim/game.c
  src/sim/util.c
  src/sim/swarm.c
  src/sim/shots.c
  src/sim/aliens.c
  src/sim/player.c
  src/sim/attack.c
  src/sim/sounds.c
  src/sim/coins.c
  src/sim/text.c
  src/sim/demo.c
  src/sim/screen.c
  src/sim/flow.c
  src/sim/boot.c
  src/sim/frame.c
)
target_include_directories(galaxian_core PUBLIC src)
target_link_libraries(galaxian_core PUBLIC m)
# Development builds also find assets/ in the source tree; releases ship it next to the program.
option(GALAXIAN_DEV_ASSETS "Fall back to the source tree's assets/ at run time" ON)
if(GALAXIAN_DEV_ASSETS)
  target_compile_definitions(galaxian_core PRIVATE GALAXIAN_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
endif()

add_executable(galaxian
  src/main.c
  src/platform/video.c
  src/platform/input.c
  src/platform/audio.c
)
target_link_libraries(galaxian PRIVATE galaxian_core sdl2 m)

# The user's ROM set (never committed; see .gitignore): roms/galaxian.zip or roms/galaxian/.
set(rom_default "${CMAKE_SOURCE_DIR}/roms/galaxian.zip")
if(NOT EXISTS "${rom_default}" AND EXISTS "${CMAKE_SOURCE_DIR}/roms/galaxian")
  set(rom_default "${CMAKE_SOURCE_DIR}/roms/galaxian")
endif()
set(GALAXIAN_ROM "${rom_default}" CACHE FILEPATH "The MAME galaxian ROM set: galaxian.zip or a folder")

# Web build: emcmake cmake -S . -B build-web && cmake --build build-web --target serve
# writes <build>/web/index.{html,js,wasm} and serves it on localhost. The ROM
# set is never bundled; GALAXIAN_WEB_ROM can add the user's own for local play
# only (do not publish such a build).
if(EMSCRIPTEN)
  set(GALAXIAN_WEB_ROM "" CACHE FILEPATH "Bundle this galaxian.zip for LOCAL testing only")
  set(GALAXIAN_WEB_PORT 8000 CACHE STRING "Port for the serve target")
  set(web_preload)
  if(GALAXIAN_WEB_ROM)
    list(APPEND web_preload "SHELL:--preload-file '${GALAXIAN_WEB_ROM}@/galaxian.zip'")
    message(WARNING "Web build bundles ${GALAXIAN_WEB_ROM}: for local use only, do not publish it")
  endif()
  if(EXISTS "${CMAKE_SOURCE_DIR}/assets")
    list(APPEND web_preload "SHELL:--preload-file '${CMAKE_SOURCE_DIR}/assets@/assets'")
  endif()
  set(shell "${CMAKE_SOURCE_DIR}/src/web/shell.html")
  set_target_properties(galaxian PROPERTIES
    OUTPUT_NAME index SUFFIX .html
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/web"
    LINK_DEPENDS "${shell}")
  target_link_options(galaxian PRIVATE -sALLOW_MEMORY_GROWTH=1 "SHELL:--shell-file '${shell}'" ${web_preload})

  find_program(PYTHON3_EXECUTABLE NAMES python3 python)
  add_custom_target(serve
    COMMAND ${CMAKE_COMMAND} -E echo "Galaxian: http://localhost:${GALAXIAN_WEB_PORT}/"
    COMMAND ${PYTHON3_EXECUTABLE} -m http.server ${GALAXIAN_WEB_PORT} --bind 127.0.0.1
      --directory "${CMAKE_BINARY_DIR}/web"
    DEPENDS galaxian USES_TERMINAL)
  return()  # no packages or tests for the web build
endif()

# Release archives: cmake --build build --target package_linux / package_portmaster /
# package_windows / package_macos (configure release builds with -DGALAXIAN_DEV_ASSETS=OFF)
foreach(kind linux portmaster windows macos)
  add_custom_target(package_${kind}
    COMMAND ${CMAKE_COMMAND} -DKIND=${kind} -DSRC=${CMAKE_SOURCE_DIR} -DBIN=${CMAKE_BINARY_DIR}
      -DGAME=$<TARGET_FILE:galaxian> -DARCH=${CMAKE_SYSTEM_PROCESSOR} -DVERSION=${PROJECT_VERSION}
      -P ${CMAKE_SOURCE_DIR}/src/packaging/package.cmake
    DEPENDS galaxian
    COMMENT "Packaging ${kind} release")
endforeach()

enable_testing()
add_executable(test_enhance src/tests/test_enhance.c)
target_link_libraries(test_enhance PRIVATE galaxian_core)
add_test(NAME enhance COMMAND test_enhance)
add_executable(test_z80 src/tests/test_z80.c)
target_link_libraries(test_z80 PRIVATE galaxian_core)
add_test(NAME z80 COMMAND test_z80)
add_executable(test_capture src/tests/test_capture.c)
target_link_libraries(test_capture PRIVATE galaxian_core)
add_executable(test_emu src/tests/test_emu.c)
target_link_libraries(test_emu PRIVATE galaxian_core)

if(EXISTS "${GALAXIAN_ROM}")
  add_executable(test_rom src/tests/test_rom.c)
  target_link_libraries(test_rom PRIVATE galaxian_core)
  add_test(NAME rom_load COMMAND test_rom "${GALAXIAN_ROM}" "${CMAKE_BINARY_DIR}/rom_test")
  add_executable(test_extract src/tests/test_extract.c)
  target_link_libraries(test_extract PRIVATE galaxian_core)
  add_test(NAME extract COMMAND test_extract "${GALAXIAN_ROM}" "${CMAKE_BINARY_DIR}/extract_test")
  add_executable(test_sim src/tests/test_sim.c)
  target_link_libraries(test_sim PRIVATE galaxian_core)
  add_test(NAME sim COMMAND test_sim "${GALAXIAN_ROM}")
  add_executable(test_frame src/tests/test_frame.c)
  target_link_libraries(test_frame PRIVATE galaxian_core)
  add_test(NAME frame COMMAND test_frame "${GALAXIAN_ROM}")
  add_executable(test_routines src/tests/test_routines.c)
  target_link_libraries(test_routines PRIVATE galaxian_core)
  add_test(NAME routines COMMAND test_routines "${GALAXIAN_ROM}")
  set_tests_properties(rom_load extract sim frame routines PROPERTIES LABELS rom)
  # Development builds extract the assets next to the program after each build.
  if(NOT CMAKE_CROSSCOMPILING)
    add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/assets/manifest.txt"
      COMMAND galaxian --extract "${GALAXIAN_ROM}" "${CMAKE_BINARY_DIR}/assets"
      DEPENDS galaxian
      COMMENT "Extracting assets from ${GALAXIAN_ROM}")
    add_custom_target(assets ALL DEPENDS "${CMAKE_BINARY_DIR}/assets/manifest.txt")
  endif()
  message(STATUS "ROM set found at ${GALAXIAN_ROM}: ROM tests enabled")
else()
  message(STATUS "No ROM set at ${GALAXIAN_ROM}: ROM tests disabled (put galaxian.zip in roms/)")
endif()

# MAME as the reference: its view of the ROMs must match ours.
find_program(MAME_EXECUTABLE mame)
if(MAME_EXECUTABLE AND EXISTS "${GALAXIAN_ROM}")
  get_filename_component(rom_parent "${GALAXIAN_ROM}" DIRECTORY)
  add_test(NAME mame_rom_map
    COMMAND ${CMAKE_COMMAND} -DMAME=${MAME_EXECUTABLE} -DROMPATH=${rom_parent} -DGAME=$<TARGET_FILE:galaxian>
      -DROM=${GALAXIAN_ROM} -DWORKDIR=${CMAKE_BINARY_DIR}/mame
      -DPROBE=${CMAKE_SOURCE_DIR}/src/tests/mame/rom_map_probe.lua
      -P ${CMAKE_SOURCE_DIR}/src/tests/mame/rom_map.cmake)
  set_tests_properties(mame_rom_map PROPERTIES LABELS mame TIMEOUT 300 RESOURCE_LOCK mame_workdir)
  add_test(NAME mame_capture
    COMMAND ${CMAKE_COMMAND} -DMAME=${MAME_EXECUTABLE} -DROMPATH=${rom_parent} -DWORKDIR=${CMAKE_BINARY_DIR}/mame
      -DROM=${GALAXIAN_ROM} -DPROBE=${CMAKE_SOURCE_DIR}/src/tests/mame/capture_probe.lua -DCHECK=$<TARGET_FILE:test_capture>
      -P ${CMAKE_SOURCE_DIR}/src/tests/mame/capture.cmake)
  add_test(NAME mame_emu
    COMMAND ${CMAKE_COMMAND} -DMAME=${MAME_EXECUTABLE} -DROMPATH=${rom_parent} -DWORKDIR=${CMAKE_BINARY_DIR}/mame
      -DROM=${GALAXIAN_ROM} -DPROBE=${CMAKE_SOURCE_DIR}/src/tests/mame/trace_probe.lua -DCHECK=$<TARGET_FILE:test_emu>
      -P ${CMAKE_SOURCE_DIR}/src/tests/mame/emu.cmake)
  set_tests_properties(mame_capture mame_emu PROPERTIES LABELS mame TIMEOUT 600 RESOURCE_LOCK mame_workdir)
  set_tests_properties(mame_emu PROPERTIES DEPENDS mame_capture)
  message(STATUS "MAME reference tests enabled (${MAME_EXECUTABLE})")
else()
  message(STATUS "MAME reference tests disabled (need mame and ${GALAXIAN_ROM})")
endif()
