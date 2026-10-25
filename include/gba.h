/*
 * gba.h - minimal Game Boy Advance hardware definitions.
 *
 * Only what Mini Minecraft GBA needs. No libtonc / libgba required, so the
 * project builds with a plain devkitARM install.
 *
 * When compiled with -DHOST_TEST the hardware memory regions are replaced by
 * ordinary arrays so the game logic can be unit-tested on a PC.
 */
#ifndef GBA_H
#define GBA_H

#include <stdint.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;

/* ---- Memory map ------------------------------------------------------ */
#ifdef HOST_TEST
extern u8 sim_io[0x400];
extern u8 sim_pal[0x400];
extern u8 sim_vram[0x18000];
extern u8 sim_oam[0x400];
#define MEM_IO    ((uintptr_t)sim_io)
#define MEM_PAL   ((uintptr_t)sim_pal)
#define MEM_VRAM  ((uintptr_t)sim_vram)
#define MEM_OAM   ((uintptr_t)sim_oam)
#else
#define MEM_IO    0x04000000u
#define MEM_PAL   0x05000000u
#define MEM_VRAM  0x06000000u
#define MEM_OAM   0x07000000u
#endif

/* ---- I/O registers --------------------------------------------------- */
#define REG16(off)     (*(volatile u16 *)(MEM_IO + (off)))
#define REG_DISPCNT    REG16(0x000)
#define REG_VCOUNT     REG16(0x006)
#define REG_BG0CNT     REG16(0x008)
#define REG_BG1CNT     REG16(0x00A)
#define REG_BG0HOFS    REG16(0x010)
#define REG_BG0VOFS    REG16(0x012)
#define REG_BG1HOFS    REG16(0x014)
#define REG_BG1VOFS    REG16(0x016)
#define REG_KEYINPUT   REG16(0x130)

/* DISPCNT bits */
#define DCNT_MODE0     0x0000
#define DCNT_OBJ_1D    0x0040
#define DCNT_BLANK     0x0080   /* forced blank: safe to touch VRAM anytime */
#define DCNT_BG0       0x0100
#define DCNT_BG1       0x0200
#define DCNT_OBJ       0x1000

/* Keys (REG_KEYINPUT is active-low; game.c inverts it) */
#define KEY_A          0x0001
#define KEY_B          0x0002
#define KEY_SELECT     0x0004
#define KEY_START      0x0008
#define KEY_RIGHT      0x0010
#define KEY_LEFT       0x0020
#define KEY_UP         0x0040
#define KEY_DOWN       0x0080
#define KEY_R          0x0100
#define KEY_L          0x0200
#define KEY_MASK       0x03FF

#define SCREEN_W       240
#define SCREEN_H       160

/* ---- Palettes (RGB555) ----------------------------------------------- */
#define RGB15(r, g, b) ((u16)((r) | ((g) << 5) | ((b) << 10)))
#define PAL_BG(i)      (*(volatile u16 *)(MEM_PAL + 2 * (i)))
#define PAL_OBJ(i)     (*(volatile u16 *)(MEM_PAL + 0x200 + 2 * (i)))

/* ---- VRAM ------------------------------------------------------------ */
/* 4bpp tile n of BG charblock cb: 8 x u32 (one u32 per pixel row) */
#define TILE_BG(cb, n) ((volatile u32 *)(MEM_VRAM + (cb) * 0x4000 + (n) * 32))
/* Screenblock sb, entry i (u16 map entries) */
#define SCREEN_ENTRY(sb, i) ((volatile u16 *)(MEM_VRAM + (sb) * 0x800 + (i) * 2))
/* Sprite tile n (OBJ tiles live at 0x06010000 in bitmap-less modes) */
#define TILE_OBJ(n)    ((volatile u32 *)(MEM_VRAM + 0x10000 + (n) * 32))

/* ---- OAM ------------------------------------------------------------- */
#define OAM_ATTR(i, k) (*(volatile u16 *)(MEM_OAM + (i) * 8 + (k) * 2))
#define OAM_HIDE       0x0200   /* attr0 value that disables a sprite */

#endif /* GBA_H */
