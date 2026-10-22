Galaxian (native SDL2 build)
============================

Converted by monkeyx.

Run ./galaxian, or galaxian.exe on Windows (add --windowed for a window on a
desktop). Work in progress: today the game is your ROM's own code on an
emulated Galaxian board (checked against MAME), with its sound modelled
from the board's circuit; the port adds the graphics presets, pad controls
and the DIP switches (galaxian.cfg in $GALAXIAN_SAVE_DIR, else
$XDG_DATA_HOME/galaxian, %APPDATA%\galaxian on Windows, or
~/.local/share/galaxian: coinage, bonus life, lives).
./galaxian --test-card shows the ROM status and an input tester.
./galaxian --extract galaxian.zip writes the game's tiles, palette and font
from your set into assets/ next to the program (the PortMaster launcher
does this on the first run), for the game rewritten in C to come.

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
