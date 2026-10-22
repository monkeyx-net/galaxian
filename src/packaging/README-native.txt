Galaxian (native SDL2 build)
============================

Converted by monkeyx.

Run ./galaxian, or galaxian.exe on Windows (add --windowed for a window on a
desktop). Work in progress: this version shows the test card (ROM status
and an input tester); the game follows.
./galaxian --test-card shows the ROM status and an input tester.

ROM set
  The game needs your own MAME galaxian ROM set. Put galaxian.zip (or a
  galaxian/ folder) next to the program, set GALAXIAN_ROM to its path, or
  start with --rom /path/to/galaxian.zip. FBNeo sets and old file names
  work too: ROMs are matched by checksum. Check a set with:
  ./galaxian --check-rom galaxian.zip

Controls (one control panel: the players take turns, as on the cabinet)
  Left/right: left stick / D-pad; keyboard Left Right
  Fire: A or B; keyboard Space or Ctrl
  Coin: Select / 5
  Start: Start / 1 or Enter;  2-player start: L1+Start / 2
  Graphics: Select+L1 / F3 steps the presets, F4 glow, F5 CRT scanlines
  Exit: hold Select+Start for 1 second / Esc
  Every connected pad drives the panel.

Graphics presets (--gfx <preset>)
  classic, classic-crt, crisp, smooth, hd-crisp, hd-smooth, hd-glow, hd-glow-crt

See NOTICE for the third-party material this project refers to.
