# Galaxian - native SDL2 port (work in progress)

A C11 / SDL2 port of the 1979 Namco arcade game for Linux handhelds
(PortMaster: ArkOS, ROCKNIX, muOS, Knulli), desktops (Linux, Windows, macOS) and the
web, checked against the original ROM running in MAME.

**Status:** playable. The game is rewritten in C, routine by routine,
checked against the ROM running on an emulated Galaxian board (Z80, video,
starfield; itself checked against MAME cycle for cycle and pixel for
pixel); the C game alone equals the board frame for frame through
scripted games. It reads only the assets `--extract` writes once from
your ROM set. The sound is a model of the board's discrete circuit,
measured from MAME. `--emulate` runs the ROM's own code on the emulated
board instead. The port adds enhanced graphics presets, pad controls,
the DIP switches in an operator menu (saved to `galaxian.cfg`) and
high-score saving. See
[GALAXIAN_PORT_PLAN.md](GALAXIAN_PORT_PLAN.md).

You need your own MAME `galaxian` ROM set (`galaxian.zip`); none is included.

Converted by monkeyx.

```bash
cmake -S . -B build && cmake --build build -j
build/galaxian --windowed --rom /path/to/galaxian.zip
```

## Controls

One control panel, as on the upright cabinet: the players take turns, and
every connected pad drives the panel.

| Action | Pad | Keyboard |
|---|---|---|
| Left / right | left stick or D-pad | Left Right |
| Fire | A or B | Space or Ctrl |
| Coin | Select | 5 |
| 1-player start | Start | 1 or Enter |
| 2-player start | L1 + Start | 2 |
| Next graphics preset | Select + L1 | F3 |
| Glow on / off | | F4 |
| CRT scanlines on / off | | F5 |
| Operator menu | Select + R1 | F2 |
| Quit | hold Select + Start for 1 s | Esc |

## Building

Needs CMake 3.13+, a C11 compiler and SDL 2.0.10+.

```bash
ctest --test-dir build              # tests (ROM and MAME tests need roms/galaxian.zip and mame)
cmake --build build --target package_linux package_portmaster
emcmake cmake -S . -B build-web && cmake --build build-web --target serve
```

## License

Apache 2.0 for the source; see [NOTICE](NOTICE) for the third-party material
this project refers to. No ROM data is included or distributed.
