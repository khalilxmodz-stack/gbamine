#!/usr/bin/env python3
"""
render.py - tiny software GBA PPU for verifying the game's video output.

Reads the memory dumps written by host_test (name.io/.pal/.vram/.oam) and
draws BG0, BG1 and 16x16 4bpp sprites the way a Mode 0 GBA would.
Usage: python3 tests/render.py DUMP_DIR NAME OUT.png [scale]
"""
import struct
import sys
from PIL import Image


def rd16(b, o): return struct.unpack_from("<H", b, o)[0]
def rgb(c): return tuple(((c >> s) & 31) * 255 // 31 for s in (0, 5, 10))


def render(d, name, scale=3):
    io = open(f"{d}/{name}.io", "rb").read()
    pal = open(f"{d}/{name}.pal", "rb").read()
    vram = open(f"{d}/{name}.vram", "rb").read()
    oam = open(f"{d}/{name}.oam", "rb").read()

    def color(bank_base, idx): return rgb(rd16(pal, bank_base + 2 * idx))
    def tile_px(cb, n, x, y):
        w = struct.unpack_from("<I", vram, cb * 0x4000 + n * 32 + y * 4)[0]
        return (w >> (4 * x)) & 15

    dispcnt = rd16(io, 0)
    img = [[color(0, 0)] * 240 for _ in range(160)]

    def draw_bg(n):
        cnt = rd16(io, 0x08 + 2 * n)
        char = (cnt >> 2) & 3
        sb = (cnt >> 8) & 31
        size = (cnt >> 14) & 3
        hofs = rd16(io, 0x10 + 4 * n) & 0x1FF
        vofs = rd16(io, 0x12 + 4 * n) & 0x1FF
        wtiles = 64 if size & 1 else 32
        htiles = 64 if size & 2 else 32
        for y in range(160):
            for x in range(240):
                wx = (x + hofs) % (wtiles * 8)
                wy = (y + vofs) % (htiles * 8)
                tx, ty = wx // 8, wy // 8
                blk = (tx // 32) + (ty // 32) * (2 if wtiles == 64 else 1)
                ent = rd16(vram, (sb + blk) * 0x800 + ((ty % 32) * 32 + tx % 32) * 2)
                tile, pb = ent & 0x3FF, (ent >> 12) & 15
                px, py = wx % 8, wy % 8
                if ent & 0x400: px = 7 - px
                if ent & 0x800: py = 7 - py
                idx = tile_px(char, tile, px, py)
                if idx: img[y][x] = color(pb * 32, idx)

    def draw_sprites(prio):
        for i in range(127, -1, -1):                     # lower index = on top
            a0, a1, a2 = rd16(oam, i * 8), rd16(oam, i * 8 + 2), rd16(oam, i * 8 + 4)
            if a0 & 0x300 == 0x200: continue             # hidden
            if (a2 >> 10) & 3 != prio: continue
            assert (a0 >> 14) == 0 and (a1 >> 14) == 1 and not (a0 & 0x2000), "only 16x16 4bpp"
            sy, sx = a0 & 0xFF, a1 & 0x1FF
            if sx >= 240: sx -= 512
            if sy >= 160: sy -= 256
            tile, pb = a2 & 0x3FF, (a2 >> 12) & 15
            for y in range(16):
                for x in range(16):
                    t = tile + (y // 8) * 2 + (x // 8)   # 1D mapping, 2 tiles wide
                    idx = tile_px(4, t, x % 8, y % 8)
                    X, Y = sx + x, sy + y
                    if idx and 0 <= X < 240 and 0 <= Y < 160:
                        img[Y][X] = color(0x200 + pb * 32, idx)

    # priority order (back to front): BG0 (prio 1), sprites (prio 1), BG1 (prio 0)
    assert dispcnt & 0x1000 and dispcnt & 0x40 and dispcnt & 0x100 and dispcnt & 0x200
    draw_bg(0)
    draw_sprites(1)
    draw_bg(1)

    out = Image.new("RGB", (240, 160))
    out.putdata([p for row in img for p in row])
    return out.resize((240 * scale, 160 * scale), Image.NEAREST)


if __name__ == "__main__":
    d, name, outp = sys.argv[1:4]
    render(d, name, int(sys.argv[4]) if len(sys.argv) > 4 else 3).save(outp)
