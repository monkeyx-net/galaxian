Galaxian (native SDL2 build)
============================

Converted by monkeyx.

Run ./galaxian, or galaxian.exe on Windows (add --windowed for a window on a
desktop). The game is rewritten in C from the arcade's (checked frame by
frame against the ROM running on an emulated Galaxian board); --emulate
runs your ROM's own code on that board instead. The sound is a model of
the board's circuit. The port adds the graphics presets, pad controls, the
DIP switches and high-score saving, in $GALAXIAN_SAVE_DIR, else
$XDG_DATA_HOME/galaxian, %APPDATA%\galaxian on Windows, or
~/.local/share/galaxian:
  galaxian.cfg      coinage, bonus ship, lives, cabinet (upright or
                    cocktail: player two's turns turned round)
  galaxian.hiscore  the high score (the arcade forgets it at power-off)
./galaxian --test-card shows the ROM status and an input tester.
The game reads its tiles, palette and tables from assets/ next to the
program: ./galaxian --extract galaxian.zip writes them from your set (the
PortMaster launcher does this on the first run; the game also does it on
its first start if it finds your set). After that the set is not needed.
In the attract mode, when nobody touches the controls for 30 seconds, a
page lists the controls; any button hides it.

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
