/*
 * art.h - pixel art and font data, plus loaders that copy it into
 * GBA palette / VRAM memory.
 */
#ifndef ART_H
#define ART_H

#include "gba.h"

/* Block types stored in the world grid */
#define BLOCK_GRASS 0
#define BLOCK_DIRT  1
#define BLOCK_WATER 2

/*
 * BG0 character memory layout (charblock 0):
 *   tile 0                      : blank (transparent)
 *   tiles 1+4*type .. 4+4*type  : the 4 8x8 tiles of a 16x16 block
 *                                 in order top-left, top-right,
 *                                 bottom-left, bottom-right
 */
#define BLOCK_TILE(type) (1 + 4 * (type))

/*
 * BG1 (text) character memory layout (charblock 1):
 *   tile 0 = transparent, tile 1 = dark blank, tile 2 = dirt icon,
 *   tile 3.. = font glyphs
 */

/* OBJ (sprite) tile layout: 8 player frames x 4 tiles, then the cursor */
#define OBJ_TILE_PLAYER(facing, anim) ((((facing) * 2) + (anim)) * 4)
#define OBJ_TILE_CURSOR 32

void art_load_palettes(void);
void art_load_world_tiles(void);
void art_load_font_tiles(void);
void art_load_sprite_tiles(void);

/* Tile index in the text charblock for an ASCII character */
u16  art_char_tile(char c);

#endif /* ART_H */
