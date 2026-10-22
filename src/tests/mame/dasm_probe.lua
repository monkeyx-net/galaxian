-- RE helper (not a test): MAME's disassembly of the Z80's program ROM, for
-- reading the game's code. Writes text files only, to DASM_OUT (default
-- build/re); never commit them, they hold the ROM's code.
--
--   DASM_OUT=build/re mame galaxian -debug -debugger none -video none -sound none \
--     -seconds_to_run 30 -autoboot_script src/tests/mame/dasm_probe.lua
--
-- Writes rom.asm ($0000-$27FF). DASM_EXTRA="1a55:40,2000:100" also
-- disassembles from those starts (where data ahead misaligns the whole
-- listing), to rom_<start>.asm.

local out = os.getenv("DASM_OUT") or "build/re"
local dbg = manager.machine.debugger
local extra = {}
for a, n in string.gmatch(os.getenv("DASM_EXTRA") or "", "(%x+):(%x+)") do
    extra[#extra + 1] = { a, n }
end
local frame = 0
_G.dasm_frames = emu.add_machine_frame_notifier(function()
    frame = frame + 1
    if frame == 5 then
        dbg:command("dasm " .. out .. "/rom.asm,0,2800")
    elseif frame >= 6 and frame < 6 + #extra then
        local e = extra[frame - 5]
        dbg:command("dasm " .. out .. "/rom_" .. e[1] .. ".asm," .. e[1] .. "," .. e[2])
    elseif frame == 6 + #extra + 30 then
        manager.machine:exit()
    end
end)
