# Galaxian — native SDL2 port — Changelog

---
## Changelog

### v0.2.0 (2026-10-22)

#### Game

- **The game in C by default** — the ROM program rewritten in C
  (`src/sim/`), routine by routine against the same addresses: the NMI and
  its boot modes, the states (attract mode, push start, the players'
  turns), the swarm's sway and survey, the flying aliens' dives, the
  attacks and waves, the ship, the missile and shells, the hit tests,
  scoring and the bonus ship, coins and credits, the sound sequencer, the
  demo's pilot, and the main loop's commands and swarm animation. It reads
  only the assets `--extract` writes (graphics, palette and the tables
  it uses), extracting them on the first start if missing. `--emulate`
  runs the ROM on the emulated board.
- **The emulated board** — a Z80, the Galaxian board's memory map, video
  (tiles with per-column scroll, sprites, shells, the starfield, the
  cocktail flip) and watchdog, checked against MAME cycle for cycle and
  pixel for pixel.
- **Sound** — a model of the board's discrete circuit (tone, swarm, hit,
  fire), measured from MAME's output.

#### Port

- **High score saved** (`galaxian.hiscore`); the DIP switches and the
  cabinet in `galaxian.cfg`; the controls page over the attract mode.
- **Tests** — `z80`, `sim` (every routine at every call, scripted and
  autopilot play), `routines` (random states), `frame` (the C game alone
  against the board), `mame_capture` and `mame_emu` (with sound and a
  cocktail-table run).

### v0.1.0 (2026-10-22)

#### Port

- **Skeleton** — the platform layer, loaders, framebuffer, enhanced
  graphics, packaging, CI and MAME test harness from the sibling Defender
  port, for a 224x256 portrait screen (integer scaling, or fitted with
  sharp bilinear on screens such as 640x480; scanlines down the screen).
- **ROM set** — the MAME `galaxian` set from a zip or folder, matched by
  CRC; `--check-rom`, `--dump-rom` (checked against MAME by `mame_rom_map`).
- **Test card** — ROM status, an input tester for Galaxian's panel, the
  graphics presets.
