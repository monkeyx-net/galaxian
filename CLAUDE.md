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
build/galaxian --windowed                         # play the game in C (dev builds find build/assets, roms/galaxian.zip)
build/galaxian --windowed --emulate               # the ROM's own code on the emulated board
build/galaxian --windowed --test-card             # the test card (ROM status, input tester)
build/galaxian --check-rom roms/galaxian.zip      # verify a ROM set
build/galaxian --dump-rom roms/galaxian.zip 0000 27FF   # program ROM, graphics and PROM as hex lines
build/galaxian --extract roms/galaxian.zip [dir]  # assets from the ROM set (dev builds: build/assets after each build)
SDL_VIDEODRIVER=dummy build/galaxian --frames 60 --screenshot out.png   # headless screenshot
cmake --build build --target package_linux package_portmaster   # release archives
cmake -S . -B build-san -DGALAXIAN_SANITIZE=ON    # ASan/UBSan build
emcmake cmake -S . -B build-web && cmake --build build-web --target serve   # web build on http://localhost:8000/
```

Release builds: `-DGALAXIAN_DEV_ASSETS=OFF` (no source-tree fallbacks).
The game in C is the default, reading only `assets/` (extracted from the
ROM set on the first start if missing); `--emulate` runs the ROM on the
emulated board. `--input script.txt` replays controls (lines of
`<frame> [coin] [start] [start2] [left] [right] [fire]`).

## Layout (`src/`)

- `main.c` - SDL setup, the fixed 60 Hz loop running a frame of the C game
  per tick (`--emulate`: a board frame), the C game drawn from its memory
  and heard through the sound model, the graphics pipeline, the DIP
  switches (`galaxian.cfg` in the save folder, MAME's defaults), the high
  score saved (`galaxian.hiscore`), the controls page over the attract
  mode (30 s idle), the test card (`--test-card`, or no ROM set),
  `--input`, `--frames`, `--screenshot`, `--rom`, `--check-rom`,
  `--dump-rom`, `--extract`
- `platform/` - the only code that includes SDL: `video.c` (224x256
  portrait framebuffer, integer scaling or fitted with sharp bilinear,
  scanlines down the screen), `input.c` (Galaxian's control panel; pads
  and keyboard), `audio.c` (streams generated samples)
- `gfx/` - draws into the framebuffer in arcade coordinates (`fb.scale` > 1
  for HD): `fb.c`, `blit.c`, `scale.c` (Scale2x/3x, xBR), `enhance.c`
  (graphics presets, HD image registry, whole-frame filter, glow),
  `debugfont.c` (3x5 font for diagnostics, not the ROM's), `galaxian.c`
  (a board line: tiles with per-column scroll and colour, sprites,
  shells, stars, the PROM palette; kept at MAME's 224x768 and reduced to
  224x256)
- `assets/` - `rom.c` (the galaxian set from a zip or folder, matched by
  CRC), `zip.c`, `inflate.c`, `png.c`, `assets.c` (asset folder lookup),
  `savefile.c` (the save folder), `extract.c` (tiles, palette, font,
  previews; `manifest.txt` written last)
- `emu/` - the emulated board: `z80.c` (the CPU, each memory access at
  its T-state), `board.c` (memory map, latches, the NMI at vertical blank,
  lines drawn lazily as MAME does, the starfield generator's restart, the
  watchdog), `sound.c` (the discrete sound, modelled from MAME's output:
  tone, swarm, hit, fire; 792 samples a frame)
- `sim/` - the game in C, each routine mirroring the ROM's arithmetic and
  order of stores, against the same addresses (`game.c`: the board's
  memory as the ROM lays it out): `util.c` (helpers), `swarm.c` (sway,
  survey), `shots.c` (missile, shells, hit tests), `aliens.c` (the flying
  slots' states and sprites), `attack.c` (who dives when, waves),
  `player.c` (the ship), `sounds.c` (the sound sequencer, the swarm's
  sound, the LFO), `coins.c`, `text.c` (the typewriter), `demo.c` (the
  demo's pilot), `screen.c` (the main loop's commands and animation),
  `flow.c` (the states and steps), `boot.c` (power-on, the boot modes),
  `frame.c` (the NMI and a frame)
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
- `extract` - label `rom`: two extractions are byte-identical; the tiles,
  palette and tables read back are the ROM's; the font matches the
  attract mode's text on the emulated board; no manifest without a ROM set
- `sim` - label `rom`: every ported routine checked at every call the ROM
  makes in two scripted runs (one with an autopilot), against a model
  loaded from the board (see the plan)
- `frame` - label `rom`: the C game alone equals the board every frame
  over 60000 frames, given the NMI / main loop race; it reads only the
  extracted tables
- `z80` - always runs: every opcode's T-states and machine cycles, known
  flag results, NMI, IM 1, HALT
- `mame_rom_map` - label `mame`: MAME's view of the program ROM, graphics
  and PROM equals `galaxian --dump-rom`
- `mame_capture` - label `mame`: runs the scripted game in
  `src/tests/mame/script.cmake`, dumping memory and the latches with a
  snapshot at 14 frames; the static frames drawn from the dump equal MAME's
- `mame_emu` - label `mame`: the board replays that script from power-on:
  MAME's trace of frames 1600-1700 cycle for cycle; RAM (bar the stack
  $43C0-$43FF), tile and object RAM, latches and the picture (MAME's
  224x768) at every captured frame; the sound against MAME's recording of
  the run (levels within 3 dB, the tone alone exact)
- To debug a difference: `test_emu` with `DUMP_DIR=dir` (its pictures),
  `OBJECT_LOG=frame`, `LATCH_LOG=1`, `WRITE_LOG=addr`; `trace_probe.lua`
  with `TRACE_FROM`/`TRACE_TO` (needs `-debug -debugger none`) against
  `Board.step_hook`. Traces, dumps and disassembly stay in `build/`.
- MAME tests share `build/mame` (one at a time); tests that run the game
  use their own save dir; never write to the user's home folder.

## Reference

- ROM: MAME `galaxian` (0.289 names); CRCs in `src/assets/rom.c`.
- Hardware: Namco Galaxian board (Z80 at 3.072 MHz, tilemap with per-row
  scroll and colour, 8 sprites, shells, starfield, discrete sound). Notes
  go in the plan as they are confirmed.
- MAME 0.289 runs it headless; see `src/tests/mame/mame_common.cmake` for
  the options.
