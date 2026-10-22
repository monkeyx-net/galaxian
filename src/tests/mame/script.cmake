# The scripted run the MAME tests capture and test_emu replays (inputs in
# capture_probe.lua's PROBE_SCRIPT format): the attract mode, a coin, a
# 1-player start, then over a minute of play, moving and firing.
set(capture_script "1200:coin 1210: 1300:start 1310:")
set(moves "left,fire" "right" "fire" "left" "right,fire" "fire" "left" "right")
set(f 1400)
set(k 0)
while(f LESS 5200)
  math(EXPR m "${k} % 8")
  list(GET moves ${m} move)
  # hold each move a while, then let go
  math(EXPR hold "15 + (${k} * 7) % 25")
  math(EXPR off "${f} + ${hold}")
  string(APPEND capture_script " ${f}:${move} ${off}:")
  math(EXPR f "${off} + 5")
  math(EXPR k "${k} + 1")
endwhile()
# frames dumped and snapshotted; static_frames are those whose end-of-frame
# memory draws the whole picture (nothing changes during their scan-out)
set(capture_frames 600 900 1250 1400 1600 1800 2100 2400 2700 3000 3600 4200 4800 5200)
set(static_frames 600 900)

# A second run on the cocktail table (the Cabinet switch): two coins, a
# two-player start, play cycling the controls; player two's turns are
# flipped. Frames every 50 from 600.
set(cocktail_script "300:coin 310: 330:coin 340: 400:start2 410:")
set(f 500)
set(k 0)
set(moves "left,fire" "right" "fire" "left" "right,fire")
while(f LESS 7000)
  math(EXPR m "${k} % 5")
  list(GET moves ${m} move)
  math(EXPR hold "15 + (${k} * 7) % 25")
  math(EXPR off "${f} + ${hold}")
  string(APPEND cocktail_script " ${f}:${move} ${off}:")
  math(EXPR f "${off} + 5")
  math(EXPR k "${k} + 1")
endwhile()
set(cocktail_frames)
foreach(f RANGE 600 6500 50)
  list(APPEND cocktail_frames ${f})
endforeach()
