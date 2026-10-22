-- MAME probe: RAM, tile RAM, object RAM and the write-only latches at chosen
-- frames, with scripted input. Only dumps; C does the decoding and comparing.
--
--   PROBE_OUT=<dir> PROBE_FRAMES="600 1500" PROBE_LAST=1800 \
--   PROBE_SCRIPT="1200:coin 1215: 1300:start 1400:left,fire ..." mame galaxian -autoboot_script capture_probe.lua
--
-- PROBE_POKE="4007=05" writes memory every frame from PROBE_POKE_FROM on.
--
-- Writes into PROBE_OUT, at each PROBE_FRAMES frame:
--   frame_<n>.txt   "ram" + the 1 KB at $4000, "tiles" + the 1 KB at $5000,
--                   "objects" + the 256 bytes at $5800, "latch" + the last
--                   value written to each of $6000-$6007, $6800-$6807,
--                   $7000-$7007 and $7800 (25 bytes, -1 = never: "--")
--   and a MAME snapshot (snap/galaxian/NNNN.png, in order)

local out_dir = os.getenv("PROBE_OUT") or "."
local last_frame = tonumber(os.getenv("PROBE_LAST") or "1800")

local snap_frames = {}
for n in string.gmatch(os.getenv("PROBE_FRAMES") or "", "%d+") do snap_frames[tonumber(n)] = true end

-- "frame:input,input frame:..." where input is coin, start, start2, left,
-- right, fire; each entry holds until the next one.
local script = {}
for f, what in string.gmatch(os.getenv("PROBE_SCRIPT") or "", "(%d+):([%w,]*)") do
    table.insert(script, { frame = tonumber(f), inputs = what })
end
local poke_from = tonumber(os.getenv("PROBE_POKE_FROM") or "0")
local pokes = {}
for a, v in string.gmatch(os.getenv("PROBE_POKE") or "", "(%x+)=(%x+)") do
    table.insert(pokes, { addr = tonumber(a, 16), value = tonumber(v, 16) })
end

local fields = {}
for _, port in pairs(manager.machine.ioport.ports) do
    for name, field in pairs(port.fields) do fields[name] = field end
end
local NAMES = { coin = "Coin 1", start = "1 Player Start", start2 = "2 Players Start",
                left = "P1 Left", right = "P1 Right", fire = "P1 Button 1" }

local space = manager.machine.devices[":maincpu"].spaces["program"]

-- the latches, by their decoded address (mirrors folded)
local latch = {}
local function latch_index(a)
    local base = a & 0xF800
    if base == 0x7800 then return 24 end
    local i = a & 7
    if base == 0x6000 then return i elseif base == 0x6800 then return 8 + i else return 16 + i end
end
-- Kept in a global: MAME removes a tap once its handle is garbage-collected.
_G.capture_latch_tap = space:install_write_tap(0x6000, 0x7FFF, "capture_latch", function(offset, data, mask)
    latch[latch_index(offset)] = data
end)

local function hex(base, n)
    local row = {}
    for a = base, base + n - 1 do row[#row + 1] = string.format("%02X", space:read_u8(a)) end
    return table.concat(row)
end

local function dump_frame(n)
    local f = assert(io.open(string.format("%s/frame_%d.txt", out_dir, n), "w"))
    f:write("ram " .. hex(0x4000, 0x400) .. "\n")
    f:write("tiles " .. hex(0x5000, 0x400) .. "\n")
    f:write("objects " .. hex(0x5800, 0x100) .. "\n")
    local l = {}
    for i = 0, 24 do l[#l + 1] = latch[i] and string.format("%02X", latch[i]) or "--" end
    f:write("latch " .. table.concat(l) .. "\n")
    f:close()
end

local frame, held = 0, {}
_G.capture_frames = emu.add_machine_frame_notifier(function()
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
    if frame >= poke_from then
        for _, p in ipairs(pokes) do space:write_u8(p.addr, p.value) end
    end
    if snap_frames[frame] then
        dump_frame(frame)
        manager.machine.video:snapshot()
    end
    if frame >= last_frame then manager.machine:exit() end
end)
