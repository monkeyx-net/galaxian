-- MAME probe: the Z80's view of the program ROM and the graphics and palette
-- regions, as hex lines in the format of `galaxian --dump-rom`: "ADDR BYTE"
-- for $0000-$27FF, then "G:ADDR BYTE" for region :gfx1 (1h, 1k) and
-- "P:ADDR BYTE" for :proms (6l).
--
--   PROBE_OUT=out.txt mame galaxian -autoboot_script rom_map_probe.lua ...

local out_path = os.getenv("PROBE_OUT") or "rom_map_probe.txt"

local space = manager.machine.devices[":maincpu"].spaces["program"]
local regions = manager.machine.memory.regions

local out = assert(io.open(out_path, "w"))
for addr = 0x0000, 0x27FF do
    out:write(string.format("%04X %02X\n", addr, space:read_u8(addr)))
end
local gfx = regions[":gfx1"]
for i = 0, gfx.size - 1 do
    out:write(string.format("G:%03X %02X\n", i, gfx:read_u8(i)))
end
local proms = regions[":proms"]
for i = 0, proms.size - 1 do
    out:write(string.format("P:%02X %02X\n", i, proms:read_u8(i)))
end
out:close()
manager.machine:exit()
