/*
 * game.c - Mini Minecraft GBA: world, player, input and rendering.
 *
 * Video setup (Mode 0):
 *   BG0  - the world. 64x64 tile map (512x512 px) in screenblocks 24-27,
 *          tiles in charblock 0. Every 16x16 block = 2x2 tiles. Scrolled by
 *          the camera.
 *   BG1  - text / HUD. 32x32 map in screenblock 28, font in charblock 1.
 *          Never scrolls and sits above sprites.
 *   OBJ  - player (OAM 0) and target cursor (OAM 1), 16x16 sprites.
 */
#include "game.h"

#define BG0_CHARBLOCK    100
#define BG0_SCREENBLOCK  24
#define BG1_CHARBLOCK    1
#define BG1_SCREENBLOCK  28

#define TEXT_COLS        30
#define TEXT_ROWS        20

Game game;
u8 world[WORLD_H][WORLD_W];

static const s8 DX[4] = { 0, 1, 0, -1 };
static const s8 DY[4] = { -1, 0, 1, 0 };
int cheatCode = 0;
int start_keyCode = 0;
//if (cheatCode = 1) == block_infinit

/* ---------------------------------------------------------------------- */
/*  Small helpers                                                          */
/* ---------------------------------------------------------------------- */
static int clampi(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static int in_bounds(int x, int y)
{
    return x >= 0 && y >= 0 && x < WORLD_W && y < WORLD_H;
}

static int walkable(int x, int y)
{
    return in_bounds(x, y) && world[y][x] == BLOCK_GRASS;
}

/* ---------------------------------------------------------------------- */
/*  World                                                                  */
/* ---------------------------------------------------------------------- */
static void fill_rect(int x, int y, int w, int h, u8 type)
{
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++)
            if (in_bounds(i, j)) world[j][i] = type;
}

static void world_init(void)
{
    /* Grass field... */
    fill_rect(0, 0, WORLD_W, WORLD_H, BLOCK_GRASS);

    /* ...surrounded by a one-block water border: an island. */
    fill_rect(0, 0, WORLD_W, 1, BLOCK_WATER);
    fill_rect(0, WORLD_H - 1, WORLD_W, 1, BLOCK_WATER);
    fill_rect(0, 0, 1, WORLD_H, BLOCK_WATER);
    fill_rect(WORLD_W - 1, 0, 1, WORLD_H, BLOCK_WATER);

    /* Some dirt to start collecting (the player starts at 12,10). */
    fill_rect(14, 8, 3, 3, BLOCK_DIRT);
    fill_rect(6, 4, 4, 2, BLOCK_DIRT);
    fill_rect(8, 13, 2, 3, BLOCK_DIRT);
    fill_rect(18, 13, 3, 2, BLOCK_DIRT);
    fill_rect(17, 3, 2, 2, BLOCK_DIRT);
    world[7][12] = BLOCK_DIRT;
    world[12][10] = BLOCK_DIRT;
}

/* Address of the BG0 map entry for tile coordinate (tx, ty) in the 64x64 map.
 * The 64x64 layout is four 32x32 screenblocks: TL, TR, BL, BR. */
static volatile u16 *bg0_entry(int tx, int ty)
{
    int sb = BG0_SCREENBLOCK + (tx >> 5) + ((ty >> 5) << 1);
    return SCREEN_ENTRY(sb, ((ty & 31) << 5) + (tx & 31));
}

/* (Re)draw one world block as 2x2 background tiles. */
static void draw_block(int bx, int by)
{
    int base = BLOCK_TILE(world[by][bx]);
    for (int dy = 0; dy < 2; dy++)
        for (int dx = 0; dx < 2; dx++)
            *bg0_entry(bx * 2 + dx, by * 2 + dy) = (u16)(base + dy * 2 + dx);
}

static void world_draw_all(void)
{
    for (int y = 0; y < WORLD_H; y++)
        for (int x = 0; x < WORLD_W; x++)
            draw_block(x, y);
}

/* ---------------------------------------------------------------------- */
/*  Text layer (BG1)                                                       */
/* ---------------------------------------------------------------------- */
static void text_put(int x, int y, const char *s)
{
    for (; *s && x < TEXT_COLS; s++, x++)
        *SCREEN_ENTRY(BG1_SCREENBLOCK, y * 32 + x) =
            (u16)(art_char_tile(*s) | (1 << 12));      /* palette bank 1 */
}

static void text_put_centered(int y, const char *s)
{
    int len = 0;
    while (s[len]) len++;
    text_put((TEXT_COLS - len) / 2, y, s);
}

/* Make rows y0..y1 (inclusive) transparent again. */
static void text_clear_rows(int y0, int y1)
{
    for (int y = y0; y <= y1; y++)
        for (int x = 0; x < 32; x++)
            *SCREEN_ENTRY(BG1_SCREENBLOCK, y * 32 + x) = 0;
}

/* Dirt counter, top-left: [icon] X 12 */
static void hud_draw(void)
{
    char s[6];
    s[0] = '#';
    s[1] = 'X';
    s[2] = game.dirt >= 10 ? (char)('0' + game.dirt / 10) : ' ';
    s[3] = (char)('0' + game.dirt % 10);
    s[4] = ' ';
    s[5] = 0;
    text_put(0, 0, s);
}

/* Controls bar along the bottom (toggled with SELECT). */
static void help_draw(void)
{
    static const char HELP[] = "A:PLACE B:BREAK L/R:TURN      ";   /* 30 chars */
    if (game.show_help && game.state != STATE_TITLE)
        text_put(0, TEXT_ROWS - 1, HELP);
    else
        text_clear_rows(TEXT_ROWS - 1, TEXT_ROWS - 1);
}

static void overlay_clear(void)
{
    text_clear_rows(2, TEXT_ROWS - 3);
}

static void title_draw(void)
{
    /* Rows 10-11 are left free so the player sprite stays visible. */
    text_put_centered(3,  "MINI MINECRAFT GBA");
    text_put_centered(5,  "PRESS START");
    text_put_centered(13, "D-PAD: MOVE");
    text_put_centered(14, "A: PLACE DIRT");
    text_put_centered(15, "B: BREAK DIRT");
    text_put_centered(16, "L/R: TURN");
    text_put_centered(17, "SELECT: HELP BAR");
}

static void pause_draw(void)
{
    text_put_centered(8,  "PAUSED");
    text_put_centered(10, "START TO RESUME");
}

/* ---------------------------------------------------------------------- */
/*  Block editing                                                          */
/* ---------------------------------------------------------------------- */
/* Tile the player is facing; returns 0 if it is outside the world. */
static int target_tile(int *x, int *y)
{
    *x = game.tx + DX[game.facing];
    *y = game.ty + DY[game.facing];
    return in_bounds(*x, *y);
}

static void try_place(void)
{
    int x, y;
    if (!target_tile(&x, &y)) return;                  /* outside the world */
    if (x == game.tx && y == game.ty) return;          /* never on the player */
    if (world[y][x] != BLOCK_GRASS) return;            /* only on empty ground */
    if (game.dirt <= 0) return;                        /* nothing to place */

    world[y][x] = BLOCK_DIRT;
    game.dirt--;
    draw_block(x, y);
    hud_draw();
}

static void try_break(void)
{
    int x, y;
    if (!target_tile(&x, &y)) return;
    if (world[y][x] != BLOCK_DIRT) return;             /* only dirt can be broken */
    if (game.dirt >= MAX_DIRT) return;                 /* inventory full */

    world[y][x] = BLOCK_GRASS;
    game.dirt++;
    draw_block(x, y);
    hud_draw();
}

/* ---------------------------------------------------------------------- */
/*  Rendering                                                              */
/* ---------------------------------------------------------------------- */
static void oam_hide_all(void)
{
    for (int i = 0; i < 128; i++) {
        OAM_ATTR(i, 0) = OAM_HIDE;
        OAM_ATTR(i, 1) = 0;
        OAM_ATTR(i, 2) = 0;
    }
}

/* 16x16 square sprite at screen position (sx, sy). */
static void oam_set(int i, int sx, int sy, int tile, int pal)
{
    OAM_ATTR(i, 0) = (u16)(sy & 0xFF);                         /* 4bpp, square */
    OAM_ATTR(i, 1) = (u16)((sx & 0x1FF) | (1 << 14));          /* size 16x16 */
    OAM_ATTR(i, 2) = (u16)(tile | (1 << 10) | (pal << 12));    /* priority 1 */
}

static void render(void)
{
    /* Camera: centre on the player, clamp to the world. */
    game.cam_x = clampi(game.px + BLOCK_PX / 2 - SCREEN_W / 2, 0, WORLD_W * BLOCK_PX - SCREEN_W);
    game.cam_y = clampi(game.py + BLOCK_PX / 2 - SCREEN_H / 2, 0, WORLD_H * BLOCK_PX - SCREEN_H);
    REG_BG0HOFS = (u16)game.cam_x;
    REG_BG0VOFS = (u16)game.cam_y;

    /* Player */
    int anim = game.moving ? ((game.anim_t >> 2) & 1) : 0;
    oam_set(0, game.px - game.cam_x, game.py - game.cam_y,
            OBJ_TILE_PLAYER(game.facing, anim), 0);

    /* Target cursor: shown while standing still on a valid target tile */
    int cx, cy;
    if (game.state == STATE_PLAY && !game.moving && target_tile(&cx, &cy)) {
        oam_set(1, cx * BLOCK_PX - game.cam_x, cy * BLOCK_PX - game.cam_y,
                OBJ_TILE_CURSOR, 1);
    } else {
        OAM_ATTR(1, 0) = OAM_HIDE;
    }
}

/* ---------------------------------------------------------------------- */
/*  Per-frame update                                                       */
/* ---------------------------------------------------------------------- */
static void update_play(u16 keys, u16 pressed)
{
    /* Remember button presses that happen while walking. */
    if (pressed & KEY_A) game.want_place = 1;
    if (pressed & KEY_B) game.want_break = 1;

    /* Slide toward the destination tile. */
    if (game.moving) {
        game.px += DX[game.facing] * MOVE_SPEED;
        game.py += DY[game.facing] * MOVE_SPEED;
        game.anim_t++;
        if (game.px == game.tx * BLOCK_PX && game.py == game.ty * BLOCK_PX) {
            game.moving = 0;
            game.anim_t = 0;
        } else {
            return;
        }
    }

    /* Standing still from here on. */

    /* L / R rotate the target direction without moving. */
    if (pressed & KEY_R) game.facing = (game.facing + 1) & 3;
    if (pressed & KEY_L) game.facing = (game.facing + 3) & 3;

    if (game.want_place) { try_place(); game.want_place = 0; }
    if (game.want_break) { try_break(); game.want_break = 0; }

    /* D-pad: face that way, and walk if the tile ahead is free grass. */
    int dir = -1;
    if      (keys & KEY_UP)    dir = DIR_UP;
    else if (keys & KEY_RIGHT) dir = DIR_RIGHT;
    else if (keys & KEY_DOWN)  dir = DIR_DOWN;
    else if (keys & KEY_LEFT)  dir = DIR_LEFT;

    if (dir >= 0) {
        game.facing = dir;
        int nx = game.tx + DX[dir];
        int ny = game.ty + DY[dir];
        if (walkable(nx, ny)) {
            game.tx = nx;       /* occupy the destination right away */
            game.ty = ny;
            game.moving = 1;
            game.anim_t = 0;
        }
    }
}

void game_frame(void)
{
    u16 keys = (u16)(~REG_KEYINPUT) & KEY_MASK;     /* hardware keys are active-low */
    u16 pressed = keys & (u16)~game.prev_keys;      /* newly pressed this frame */
    game.prev_keys = keys;

    if (pressed & KEY_START) {
        start_keyCode == 1;
        if (game.state == STATE_TITLE) {
            game.state = STATE_PLAY;
            overlay_clear();
            help_draw();
        } else if (game.state == STATE_PLAY) {
            game.state = STATE_PAUSE;
            game.want_place = game.want_break = 0;
            pause_draw();
        } else {                            /* STATE_PAUSE: resume */
            game.state = STATE_PLAY;
            overlay_clear();
        }
    }

    if ((pressed & KEY_SELECT) && game.state != STATE_TITLE) {
        game.show_help = !game.show_help;
        help_draw();
    }

    if (game.state == STATE_PLAY)
        update_play(keys, pressed);

    render();
}

/* ---------------------------------------------------------------------- */
/*  Initialisation                                                         */
/* ---------------------------------------------------------------------- */
static void vram_clear(void)
{
    volatile u32 *p = (volatile u32 *)MEM_VRAM;
    for (int i = 0; i < 0x18000 / 4; i++) p[i] = 0;
}

void game_init(void)
{
    REG_DISPCNT = DCNT_BLANK;            /* forced blank while we fill VRAM */

    vram_clear();
    art_load_palettes();
    art_load_world_tiles();
    art_load_font_tiles();
    art_load_sprite_tiles();
    oam_hide_all();

    world_init();
    world_draw_all();

    /* BG0: priority 1, 4bpp, 64x64 tiles. BG1: priority 0, 32x32 tiles. */
    REG_BG0CNT = (u16)(1 | (BG0_CHARBLOCK << 2) | (BG0_SCREENBLOCK << 8) | (3 << 14));
    REG_BG1CNT = (u16)(0 | (BG1_CHARBLOCK << 2) | (BG1_SCREENBLOCK << 8) | (0 << 14));
    REG_BG1HOFS = 0;
    REG_BG1VOFS = 0;

    /* Player starts near the middle of the island. */
    game.state = STATE_TITLE;
    game.tx = WORLD_W / 2;
    game.ty = WORLD_H / 2;
    game.px = game.tx * BLOCK_PX;
    game.py = game.ty * BLOCK_PX;
    game.facing = DIR_DOWN;
    game.moving = 0;
    game.anim_t = 0;
    game.dirt = 0;
    game.show_help = 1;
    game.want_place = 0;
    game.want_break = 0;
    game.prev_keys = KEY_MASK;/* ignore keys held during boot */

    if (start_keyCode == 1){
        cheatCode == 1;
        if (cheatCode == 1){
        game.dirt += 90;
      }
    }
    
    
    hud_draw();
    title_draw();
    render();

    REG_DISPCNT = DCNT_MODE0 | DCNT_OBJ_1D | DCNT_BG0 | DCNT_BG1 | DCNT_OBJ;
}
