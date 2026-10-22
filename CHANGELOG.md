# Galaxian — native SDL2 port — Changelog

---
## Changelog

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
