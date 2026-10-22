# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

A native C11 / SDL2 port of Galaxian (Namco, 1979; MAME `galaxian`) for
Linux handhelds (aarch64: ArkOS, ROCKNIX, muOS, Knulli), x86_64 (Steam
Deck, desktops), Windows, macOS and the web. **The plan, decisions, hardware
notes and test plan live in [GALAXIAN_PORT_PLAN.md](GALAXIAN_PORT_PLAN.md)**;
read it before larger changes and keep its phase table and hardware section
up to date.

The repo started as a copy of the sibling Defender port's skeleton
(`../defender`: platform layer, loaders, framebuffer, enhanced graphics,
packaging, CI, MAME harness). Improvements to that shared code may be worth
carrying across (the plan lists some), but ask first.

## Ground Rules

- **The game is being rewritten in C** (plan decision 6): game logic from
  the ROM's behaviour, checked frame by frame against the emulated board
  (`src/emu/`). Until it reaches parity the emulated board runs the game.
  MAME is the reference for the emulation: changes to `emu/` must keep the
  `mame` tests passing. Write all emulation from the hardware's behaviour,
  not from MAME's source.
- **The ROM set is read once** (plan decision 1): `galaxian --extract`
  writes tiles, sprites, font, palette and tables to `assets/`, which the C
  game reads instead of the ROM. No ROM-derived data in the repo, the
  binary or the packages; the extractor's tables name ROM addresses only.
  Sound is a model of the discrete circuit: nothing is extracted for it.
- **All new code and tooling is C** (built with CMake). The one exception
  is MAME probe scripts, which must be Lua; they only dump to text files,
  and C or CMake does the comparing.
- New files go in `src/`, `CMakeLists.txt`, `.github/` or
  `GALAXIAN_PORT_PLAN.md`. Ask before adding files elsewhere.
- The user's ROM set lives in `roms/` (git-ignored); never commit ROMs or
  zips, never send them anywhere, and never overwrite or replace the files
  there.

## Commands

```bash
cmake -S . -B build && cmake --build build -j     # build
ctest --test-dir build                            # all tests
ctest --test-dir build -L rom                     # tests needing the ROM set (roms/galaxian.zip)
ctest --test-dir build -L mame                    # tests needing MAME + the ROM set
build/galaxian --windowed --test-card             # the test card (ROM status, input tester)
build/galaxian --check-rom roms/galaxian.zip      # verify a ROM set
build/galaxian --dump-rom roms/galaxian.zip 0000 27FF   # program ROM, graphics and PROM as hex lines
SDL_VIDEODRIVER=dummy build/galaxian --frames 60 --screenshot out.png   # headless screenshot
cmake --build build --target package_linux package_portmaster   # release archives
cmake -S . -B build-san -DGALAXIAN_SANITIZE=ON    # ASan/UBSan build
emcmake cmake -S . -B build-web && cmake --build build-web --target serve   # web build on http://localhost:8000/
```

Release builds: `-DGALAXIAN_DEV_ASSETS=OFF` (no source-tree fallbacks).
Later phases add `--emulate` (the ROM on the emulated board), `--input`,
`--extract` and the C game (see the plan's phase table).

## Layout (`src/`)

- `main.c` - SDL setup, the fixed 60 Hz loop, the graphics pipeline, the
  test card (`--test-card`, or no ROM set), `--frames`, `--screenshot`,
  `--rom`, `--check-rom`, `--dump-rom`
- `platform/` - the only code that includes SDL: `video.c` (224x256
  portrait framebuffer, integer scaling or fitted with sharp bilinear,
  scanlines down the screen), `input.c` (Galaxian's control panel; pads
  and keyboard), `audio.c` (streams generated samples)
- `gfx/` - draws into the framebuffer in arcade coordinates (`fb.scale` > 1
  for HD): `fb.c`, `blit.c`, `scale.c` (Scale2x/3x, xBR), `enhance.c`
  (graphics presets, HD image registry, whole-frame filter, glow),
  `debugfont.c` (3x5 font for diagnostics, not the ROM's)
- `assets/` - `rom.c` (the galaxian set from a zip or folder, matched by
  CRC), `zip.c`, `inflate.c`, `png.c`, `assets.c` (asset folder lookup),
  `savefile.c` (the save folder)
- `emu/` - the emulated board (E1: Z80, memory map, video, NMI)
- `sim/` - the game in C (C0 on)
- `tests/` - C tests, MAME probes (`tests/mame/`)
- `packaging/` - release layout script, PortMaster launcher and metadata
- `web/` - page shell for the Emscripten build

## Conventions

- C11, `-Wall -Wextra`, no warnings. Builds must also pass in Ubuntu 20.04
  (GCC 9, SDL 2.0.10, the PortMaster base): use only the SDL 2.0.10 API.
- The board is deterministic: one frame per tick, no wall clock.
- Enhanced graphics are cosmetic: classic output stays the arcade frame.
- Match the surrounding code: comment density, naming, idiom.
- Each phase gets its own commit; nothing is pushed or published without
  asking.

## Testing

Every change keeps `ctest` green.

- `enhance` - always runs (scalers, presets, glow)
- `rom_load` - label `rom`: loads the set from the zip, from renamed copies
  in a folder (CRC matching), and fails cleanly on an empty folder
- `mame_rom_map` - label `mame`: MAME's view of the program ROM, graphics
  and PROM equals `galaxian --dump-rom`
- MAME tests share `build/mame` (one at a time); tests that run the game
  use their own save dir; never write to the user's home folder.

## Reference

- ROM: MAME `galaxian` (0.289 names); CRCs in `src/assets/rom.c`.
- Hardware: Namco Galaxian board (Z80 at 3.072 MHz, tilemap with per-row
  scroll and colour, 8 sprites, shells, starfield, discrete sound). Notes
  go in the plan as they are confirmed.
- MAME 0.289 runs it headless; see `src/tests/mame/mame_common.cmake` for
  the options.
