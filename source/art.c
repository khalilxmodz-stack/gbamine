/*
 * art.c - all pixel art lives here as ASCII art, so there are no external
 * asset files. Each character is a palette index:
 *   '.' = 0 (transparent), '1'..'9' = 1..9, 'a'..'f' = 10..15
 * 16x16 images become four 8x8 4bpp tiles (TL, TR, BL, BR), which is both
 * the order of a 16x16 sprite in 1D mapping and of a 2x2 block on a BG.
 */
#include "art.h"

/* ====================================================================== */
/*  World blocks (BG palette bank 0)                                       */
/*  1 grass  2 dark grass  3 light grass                                   */
/*  4 dirt   5 dark dirt   6 light dirt                                    */
/*  7 water  8 light water 9 dark water                                    */
/* ====================================================================== */
static const char *const ART_GRASS[16] = {
    "1131111111211111",
    "1111121111111311",
    "3111111311111111",
    "1112111111121111",
    "1111111211111131",
    "1311111111311111",
    "1111211111111121",
    "1111111131111111",
    "2111113111211111",
    "1111111111111311",
    "1311121111111111",
    "1111111113111211",
    "1121111111111111",
    "1111131111211113",
    "1111111111111111",
    "3111211311111111",
};

static const char *const ART_DIRT[16] = {
    "6666666666666665",
    "6444444454444445",
    "6445444444444645",
    "6444444644454445",
    "6454444444444445",
    "6444464444444445",
    "6444444445444645",
    "6446444444444445",
    "6444444464444445",
    "6445444444454445",
    "6444444444444645",
    "6464444544444445",
    "6444444444464445",
    "6445444444444445",
    "6444444644445445",
    "5555555555555555",
};

static const char *const ART_WATER[16] = {
    "7777777777777777",
    "7788777777778877",
    "7777887777777777",
    "7777777777887777",
    "7777777777778877",
    "7777777777777777",
    "7788777777777777",
    "7777887777887777",
    "7777777777778877",
    "7777777777777777",
    "7777778877777777",
    "7788777788777777",
    "7777777777777777",
    "7777777777887777",
    "7777887777777777",
    "7777777777777777",
};

/* ====================================================================== */
/*  Player sprite (OBJ palette bank 0)                                     */
/*  1 skin 2 hair 3 shirt 4 shirt dark 5 pants 6 shoes 7 white 8 eye      */
/*  Built from head (8 rows) + torso (4 rows) + legs (4 rows).             */
/* ====================================================================== */
static const char *const HEAD_FRONT[8] = {
    "................",
    "....22222222....",
    "....22222222....",
    "....21111112....",
    "....17811871....",
    "....11111111....",
    "....11222211....",
    "....11111111....",
};
static const char *const HEAD_BACK[8] = {
    "................",
    "....22222222....",
    "....22222222....",
    "....22222222....",
    "....22222222....",
    "....22222222....",
    "....22222222....",
    "....11111111....",
};
static const char *const HEAD_LEFT[8] = {   /* right is built by mirroring */
    "................",
    "....22222222....",
    "....22222222....",
    "....21111122....",
    "....18111122....",
    "....11111122....",
    "....11111122....",
    "....11111122....",
};
static const char *const TORSO_FB[4] = {    /* front and back */
    "...3333333333...",
    "...3433333343...",
    "...3433333343...",
    "...1433333341...",
};
static const char *const TORSO_SIDE[4] = {
    "....33333333....",
    "....33344333....",
    "....33344333....",
    "....33311333....",
};
static const char *const LEGS_APART[4] = {
    "....55555555....",
    "....55555555....",
    "....555..555....",
    "....666..666....",
};
static const char *const LEGS_TOGETHER[4] = {
    "....55555555....",
    "....55555555....",
    "....55555555....",
    "....66666666....",
};

/* Target cursor (OBJ palette bank 1: 1 = yellow) */
static const char *const ART_CURSOR[16] = {
    "1111........1111",
    "1111........1111",
    "11............11",
    "11............11",
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "11............11",
    "11............11",
    "1111........1111",
    "1111........1111",
};

/* Dirt icon for the HUD (BG palette bank 1: 2 dark, 3 base, 4 dark, 5 light) */
static const char *const ART_ICON[8] = {
    "22222222",
    "25555552",
    "25333342",
    "25343342",
    "25333342",
    "25334342",
    "24444442",
    "22222222",
};

/* ====================================================================== */
/*  Font: 5x7 glyphs, one byte per row, bit 4 = leftmost pixel             */
/* ====================================================================== */
static const char FONT_CHARS[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ:-./!";
static const u8 FONT_DATA[][7] = {
    {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}, /* 0 */
    {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}, /* 1 */
    {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F}, /* 2 */
    {0x1F,0x02,0x04,0x02,0x01,0x11,0x0E}, /* 3 */
    {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}, /* 4 */
    {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E}, /* 5 */
    {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}, /* 6 */
    {0x1F,0x01,0x02,0x04,0x08,0x08,0x08}, /* 7 */
    {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}, /* 8 */
    {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}, /* 9 */
    {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}, /* A */
    {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}, /* B */
    {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}, /* C */
    {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E}, /* D */
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}, /* E */
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}, /* F */
    {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F}, /* G */
    {0x11,0x11,0x11,0x1F,0x11,0x11,0x11}, /* H */
    {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E}, /* I */
    {0x07,0x02,0x02,0x02,0x02,0x12,0x0C}, /* J */
    {0x11,0x12,0x14,0x18,0x14,0x12,0x11}, /* K */
    {0x10,0x10,0x10,0x10,0x10,0x10,0x1F}, /* L */
    {0x11,0x1B,0x15,0x15,0x11,0x11,0x11}, /* M */
    {0x11,0x11,0x19,0x15,0x13,0x11,0x11}, /* N */
    {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}, /* O */
    {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}, /* P */
    {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}, /* Q */
    {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}, /* R */
    {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}, /* S */
    {0x1F,0x04,0x04,0x04,0x04,0x04,0x04}, /* T */
    {0x11,0x11,0x11,0x11,0x11,0x11,0x0E}, /* U */
    {0x11,0x11,0x11,0x11,0x11,0x0A,0x04}, /* V */
    {0x11,0x11,0x11,0x15,0x15,0x1B,0x11}, /* W */
    {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11}, /* X */
    {0x11,0x11,0x0A,0x04,0x04,0x04,0x04}, /* Y */
    {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}, /* Z */
    {0x00,0x04,0x00,0x00,0x00,0x04,0x00}, /* : */
    {0x00,0x00,0x00,0x1F,0x00,0x00,0x00}, /* - */
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C}, /* . */
    {0x01,0x01,0x02,0x04,0x08,0x10,0x10}, /* / */
    {0x04,0x04,0x04,0x04,0x04,0x00,0x04}, /* ! */
};

/* ====================================================================== */
/*  Helpers                                                                */
/* ====================================================================== */
static u32 px_index(char c)
{
    if (c >= '0' && c <= '9') return (u32)(c - '0');
    if (c >= 'a' && c <= 'f') return (u32)(10 + (c - 'a'));
    return 0; /* '.' and anything else = transparent */
}

/* Write one 8x8 4bpp tile. Each u32 is one pixel row, lowest nibble = left
 * pixel. VRAM only accepts 16/32-bit writes, hence u32 stores. */
static void put_tile(volatile u32 *dst, const char *const *rows, int ox, int oy)
{
    for (int y = 0; y < 8; y++) {
        u32 v = 0;
        for (int x = 0; x < 8; x++)
            v |= px_index(rows[oy + y][ox + x]) << (4 * x);
        dst[y] = v;
    }
}

/* 16x16 image -> 4 consecutive tiles (TL, TR, BL, BR) */
static void put_art16(volatile u32 *dst, const char *const *rows)
{
    for (int t = 0; t < 4; t++)
        put_tile(dst + t * 8, rows, (t & 1) * 8, (t >> 1) * 8);
}

static void fill_tile(volatile u32 *dst, u32 value)
{
    for (int y = 0; y < 8; y++) dst[y] = value;
}

/* ====================================================================== */
/*  Loaders                                                                */
/* ====================================================================== */
void art_load_palettes(void)
{
    for (int i = 0; i < 256; i++) { PAL_BG(i) = 0; PAL_OBJ(i) = 0; }

    /* BG palette bank 0: world */
    PAL_BG(0)  = RGB15(2, 4, 10);      /* backdrop */
    PAL_BG(1)  = RGB15(9, 22, 6);      /* grass */
    PAL_BG(2)  = RGB15(6, 16, 4);      /* grass dark */
    PAL_BG(3)  = RGB15(14, 27, 9);     /* grass light */
    PAL_BG(4)  = RGB15(17, 11, 5);     /* dirt */
    PAL_BG(5)  = RGB15(13, 8, 4);      /* dirt dark */
    PAL_BG(6)  = RGB15(21, 14, 8);     /* dirt light */
    PAL_BG(7)  = RGB15(6, 14, 26);     /* water */
    PAL_BG(8)  = RGB15(11, 18, 29);    /* water light */
    PAL_BG(9)  = RGB15(4, 10, 21);     /* water dark */

    /* BG palette bank 1 (entries 16..31): text + HUD icon */
    PAL_BG(16 + 1) = RGB15(31, 31, 31);  /* text white */
    PAL_BG(16 + 2) = RGB15(3, 3, 7);     /* text panel (dark navy) */
    PAL_BG(16 + 3) = RGB15(17, 11, 5);   /* icon dirt */
    PAL_BG(16 + 4) = RGB15(13, 8, 4);
    PAL_BG(16 + 5) = RGB15(21, 14, 8);

    /* OBJ palette bank 0: player */
    PAL_OBJ(1) = RGB15(30, 23, 18);    /* skin */
    PAL_OBJ(2) = RGB15(11, 7, 4);      /* hair */
    PAL_OBJ(3) = RGB15(6, 22, 22);     /* shirt */
    PAL_OBJ(4) = RGB15(4, 17, 17);     /* shirt dark */
    PAL_OBJ(5) = RGB15(7, 7, 20);      /* pants */
    PAL_OBJ(6) = RGB15(12, 12, 12);    /* shoes */
    PAL_OBJ(7) = RGB15(31, 31, 31);    /* eye white */
    PAL_OBJ(8) = RGB15(3, 3, 14);      /* eye pupil */

    /* OBJ palette bank 1 (entries 16..31): cursor */
    PAL_OBJ(16 + 1) = RGB15(31, 29, 4);  /* yellow */
}

void art_load_world_tiles(void)
{
    fill_tile(TILE_BG(0, 0), 0);
    put_art16(TILE_BG(0, BLOCK_TILE(BLOCK_GRASS)), ART_GRASS);
    put_art16(TILE_BG(0, BLOCK_TILE(BLOCK_DIRT)),  ART_DIRT);
    put_art16(TILE_BG(0, BLOCK_TILE(BLOCK_WATER)), ART_WATER);
}

u16 art_char_tile(char c)
{
    if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    if (c == ' ') return 1;
    if (c == '#') return 2;
    for (int i = 0; FONT_CHARS[i]; i++)
        if (FONT_CHARS[i] == c) return (u16)(3 + i);
    return 1;
}

void art_load_font_tiles(void)
{
    fill_tile(TILE_BG(1, 0), 0);            /* transparent */
    fill_tile(TILE_BG(1, 1), 0x22222222u);  /* dark blank */
    put_tile(TILE_BG(1, 2), ART_ICON, 0, 0);

    for (int g = 0; FONT_CHARS[g]; g++) {
        volatile u32 *dst = TILE_BG(1, 3 + g);
        dst[0] = 0x22222222u;               /* top padding row */
        for (int r = 0; r < 7; r++) {
            u32 v = 0;
            for (int x = 0; x < 8; x++) {
                int on = (x >= 1 && x <= 5) && ((FONT_DATA[g][r] >> (5 - x)) & 1);
                v |= (u32)(on ? 1 : 2) << (4 * x);
            }
            dst[1 + r] = v;
        }
    }
}

void art_load_sprite_tiles(void)
{
    const char *rows[16];
    char mir[16][17];

    for (int facing = 0; facing < 4; facing++) {      /* 0 up 1 right 2 down 3 left */
        for (int anim = 0; anim < 2; anim++) {
            const char *const *head;
            const char *const *torso;
            const char *const *legs = anim ? LEGS_TOGETHER : LEGS_APART;
            int mirror = 0;

            switch (facing) {
            case 0:  head = HEAD_BACK;  torso = TORSO_FB;   break;
            case 2:  head = HEAD_FRONT; torso = TORSO_FB;   break;
            case 3:  head = HEAD_LEFT;  torso = TORSO_SIDE; break;
            default: head = HEAD_LEFT;  torso = TORSO_SIDE; mirror = 1; break;
            }

            for (int i = 0; i < 8; i++)  rows[i]      = head[i];
            for (int i = 0; i < 4; i++)  rows[8 + i]  = torso[i];
            for (int i = 0; i < 4; i++)  rows[12 + i] = legs[i];

            if (mirror) {
                for (int y = 0; y < 16; y++) {
                    for (int x = 0; x < 16; x++) mir[y][x] = rows[y][15 - x];
                    mir[y][16] = 0;
                    rows[y] = mir[y];
                }
            }
            put_art16(TILE_OBJ(OBJ_TILE_PLAYER(facing, anim)), rows);
        }
    }
    put_art16(TILE_OBJ(OBJ_TILE_CURSOR), ART_CURSOR);
}
