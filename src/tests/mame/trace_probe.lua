-- MAME probe: an instruction trace of the Z80 over chosen frames of a
-- scripted run (needs -debug -debugger none). Only dumps; test_emu compares.
--
--   PROBE_OUT=<dir> TRACE_FROM=100 TRACE_TO=200 PROBE_SCRIPT="300:coin ..." \
--   mame galaxian -debug -debugger none -autoboot_script trace_probe.lua
--
-- Writes <dir>/main.tr ("<total cycles> <PC>: <disassembly>" per
-- instruction). Traces hold disassembled ROM code: they stay in the build
-- folder, never commit them. Inputs as in capture_probe.lua.

local out_dir = os.getenv("PROBE_OUT") or "."
local from = tonumber(os.getenv("TRACE_FROM") or "100")
local to = tonumber(os.getenv("TRACE_TO") or "200")

local script = {}
for f, what in string.gmatch(os.getenv("PROBE_SCRIPT") or "", "(%d+):([%w,]*)") do
    table.insert(script, { frame = tonumber(f), inputs = what })
end
local fields = {}
for _, port in pairs(manager.machine.ioport.ports) do
    for name, field in pairs(port.fields) do fields[name] = field end
end
local NAMES = { coin = "Coin 1", start = "1 Player Start", start2 = "2 Players Start",
                left = "P1 Left", right = "P1 Right", fire = "P1 Button 1" }

local dbg = manager.machine.debugger
local frame, held = 0, {}
local function start_trace()
    dbg:command('trace ' .. out_dir .. '/main.tr,maincpu,noloop,{tracelog "%d ",totalcycles}')
end
if from == 0 then start_trace() end
_G.trace_frames = emu.add_machine_frame_notifier(function()
    frame = frame + 1
    for _, s in ipairs(script) do
        if s.frame == frame then
            held = {}
            for what in string.gmatch(s.inputs, "%a+") do held[what] = true end
        end
    end
    for what, name in pairs(NAMES) do
        if fields[name] then fields[name]:set_value(held[what] and 1 or 0) end
    end
    if frame == from then
        start_trace()
    elseif frame == to then
        dbg:command("trace off,maincpu")
    elseif frame == to + 2 then
        manager.machine:exit()
    end
end)
