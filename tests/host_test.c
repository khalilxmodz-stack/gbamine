/*
 * host_test.c - runs the real game code on a PC against simulated GBA memory.
 *
 *   make test          (from the project root)
 *
 * It checks the gameplay rules (move, break, collect, place, boundaries,
 * pause) and dumps the simulated VRAM/palette/OAM so tests/render.py can draw
 * what a GBA would show.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../source/game.h"

u8 sim_io[0x400];
u8 sim_pal[0x400];
u8 sim_vram[0x18000];
u8 sim_oam[0x400];

static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

static void frame(u16 keys)
{
    *(u16 *)(sim_io + 0x130) = (u16)(~keys & KEY_MASK);   /* active-low */
    game_frame();
}
static void frames(u16 keys, int n) { while (n-- > 0) frame(keys); }
static void tap(u16 key)            { frame(key); frame(0); }
static void settle(void)            { int n = 0; while (game.moving && n++ < 100) frame(0); }

/* Walk to tile (x, y) on the given row/column by holding one direction. */
static void walk_to_x(int x, u16 dirkey) { int n = 0; while (game.tx != x && n++ < 400) frame(dirkey); settle(); }
static void walk_to_y(int y, u16 dirkey) { int n = 0; while (game.ty != y && n++ < 400) frame(dirkey); settle(); }

static int count_dirt_in_world(void)
{
    int n = 0;
    for (int y = 0; y < WORLD_H; y++)
        for (int x = 0; x < WORLD_W; x++)
            if (world[y][x] == BLOCK_DIRT) n++;
    return n;
}

static void dump(const char *dir, const char *name)
{
    char path[512];
    const struct { const char *ext; const u8 *p; size_t n; } f[] = {
        { "io", sim_io, sizeof sim_io }, { "pal", sim_pal, sizeof sim_pal },
        { "vram", sim_vram, sizeof sim_vram }, { "oam", sim_oam, sizeof sim_oam },
    };
    for (int i = 0; i < 4; i++) {
        snprintf(path, sizeof path, "%s/%s.%s", dir, name, f[i].ext);
        FILE *fp = fopen(path, "wb");
        if (!fp) { printf("cannot write %s\n", path); fails++; return; }
        fwrite(f[i].p, 1, f[i].n, fp);
        fclose(fp);
    }
}

int main(int argc, char **argv)
{
    const char *out = argc > 1 ? argv[1] : ".";

    game_init();
    frame(0);

    /* ---- boot state ---- */
    CHECK(game.state == STATE_TITLE);
    CHECK(game.tx == 12 && game.ty == 10);
    CHECK(world[10][12] == BLOCK_GRASS);
    CHECK(game.dirt == 0);
    CHECK(REG_DISPCNT == (DCNT_OBJ_1D | DCNT_BG0 | DCNT_BG1 | DCNT_OBJ));
    dump(out, "title");

    const int total_dirt = count_dirt_in_world();
    CHECK(total_dirt == 35);

    /* Title ignores the d-pad; START begins play. */
    frames(KEY_RIGHT, 20);
    CHECK(game.tx == 12);
    tap(KEY_START);
    CHECK(game.state == STATE_PLAY);

    /* ---- walking ---- */
    walk_to_x(13, KEY_RIGHT);
    CHECK(game.tx == 13 && game.ty == 10 && game.px == 13 * 16 && !game.moving);

    /* Dirt is solid: walking into it only turns the player. */
    frames(KEY_RIGHT, 20);
    CHECK(game.tx == 13 && game.facing == DIR_RIGHT);
    CHECK(world[10][14] == BLOCK_DIRT);

    /* ---- break + collect ---- */
    tap(KEY_B);
    CHECK(world[10][14] == BLOCK_GRASS);
    CHECK(game.dirt == 1);
    tap(KEY_B);                              /* nothing left to break there */
    CHECK(game.dirt == 1);

    /* ---- place ---- */
    tap(KEY_A);
    CHECK(world[10][14] == BLOCK_DIRT);
    CHECK(game.dirt == 0);
    tap(KEY_A);                              /* occupied tile */
    CHECK(game.dirt == 0 && world[10][14] == BLOCK_DIRT);
    tap(KEY_B);                              /* take it back */
    CHECK(game.dirt == 1);
    walk_to_x(14, KEY_RIGHT);                /* now it is walkable grass */
    CHECK(game.tx == 14);
    tap(KEY_B);                              /* (15,10) is dirt */
    CHECK(game.dirt == 2 && world[10][15] == BLOCK_GRASS);

    /* Can't place with an empty inventory. */
    game.dirt = 0;
    tap(KEY_A);                              /* facing right: grass at (15,10) */
    CHECK(world[10][15] == BLOCK_GRASS && game.dirt == 0);
    game.dirt = 2;

    /* ---- L / R turn the target ---- */
    CHECK(game.facing == DIR_RIGHT);
    tap(KEY_R); CHECK(game.facing == DIR_DOWN);
    tap(KEY_R); CHECK(game.facing == DIR_LEFT);
    tap(KEY_L); CHECK(game.facing == DIR_DOWN);
    tap(KEY_L); CHECK(game.facing == DIR_RIGHT);
    CHECK(game.tx == 14);                    /* turning never moves */

    /* ---- build a small wall of dirt below the player ---- */
    tap(KEY_R);                              /* face down */
    tap(KEY_A);                              /* (14,11) */
    CHECK(world[11][14] == BLOCK_DIRT && game.dirt == 1);
    tap(KEY_R);                              /* face left */
    CHECK(game.facing == DIR_LEFT);
    tap(KEY_A);                              /* (13,10) */
    CHECK(world[10][13] == BLOCK_DIRT && game.dirt == 0);

    /* A pressed mid-walk is applied once the player stops. */
    game.dirt = 3;
    world[10][16] = BLOCK_GRASS;             /* make room (test setup) */
    frame(KEY_RIGHT);                        /* start walking right... */
    CHECK(game.moving && game.tx == 15);
    frame(KEY_A); frame(0);                  /* ...press A mid-step */
    CHECK(game.moving);
    CHECK(world[10][16] == BLOCK_GRASS);     /* not applied yet */
    settle();
    CHECK(game.tx == 15 && game.ty == 10);
    CHECK(world[10][16] == BLOCK_DIRT && game.dirt == 2);
    dump(out, "play");

    /* ---- pause ---- */
    tap(KEY_START);
    CHECK(game.state == STATE_PAUSE);
    frames(KEY_LEFT, 30);
    CHECK(game.tx == 15 && game.ty == 10);
    dump(out, "pause");
    tap(KEY_START);
    CHECK(game.state == STATE_PLAY);

    /* ---- SELECT toggles the help bar ---- */
    u16 help_first = *SCREEN_ENTRY(28, 19 * 32);
    CHECK(help_first != 0);
    tap(KEY_SELECT);
    CHECK(*SCREEN_ENTRY(28, 19 * 32) == 0);
    tap(KEY_SELECT);
    CHECK(*SCREEN_ENTRY(28, 19 * 32) == help_first);

    /* ---- world boundary: walk into the water border ---- */
    game.dirt = 5;
    world[10][13] = BLOCK_GRASS;             /* clear the way (test setup) */
    int before_world = count_dirt_in_world();
    frames(KEY_LEFT, 400);
    settle();
    CHECK(game.tx == 1);                     /* (0, y) is water */
    CHECK(game.facing == DIR_LEFT);
    tap(KEY_A);                              /* placing on water: refused */
    tap(KEY_B);                              /* breaking water: refused */
    CHECK(world[game.ty][0] == BLOCK_WATER);
    CHECK(game.dirt == 5);
    CHECK(count_dirt_in_world() == before_world);

    walk_to_y(1, KEY_UP);
    CHECK(game.ty == 1);
    frames(KEY_RIGHT, 600); settle();
    CHECK(game.tx >= 1 && game.tx <= WORLD_W - 2);
    CHECK(game.px >= 0 && game.px <= (WORLD_W - 1) * 16);
    dump(out, "corner");

    /* ---- fuzz: random button mashing must never break the rules ---- */
    game_init(); frame(0); tap(KEY_START);
    unsigned seed = 12345;
    int expected_total = count_dirt_in_world();
    for (int i = 0; i < 300000; i++) {
        seed = seed * 1664525u + 1013904223u;
        u16 keys = 0;
        u32 r = seed >> 8;
        if ((r & 3) == 0) keys |= (u16)(KEY_RIGHT << ((r >> 2) & 3));    /* a d-pad bit */
        if (((r >> 4) & 7) == 0) keys |= KEY_A;
        if (((r >> 7) & 7) == 0) keys |= KEY_B;
        if (((r >> 10) & 31) == 0) keys |= KEY_L;
        if (((r >> 15) & 31) == 0) keys |= KEY_R;
        if (((r >> 20) & 255) == 0) keys |= KEY_START;
        if (((r >> 12) & 511) == 0) keys |= KEY_SELECT;
        frame(keys);

        if (!(game.tx >= 1 && game.tx < WORLD_W - 1 && game.ty >= 1 && game.ty < WORLD_H - 1) ||
            world[game.ty][game.tx] != BLOCK_GRASS ||
            game.dirt < 0 || game.dirt > MAX_DIRT ||
            count_dirt_in_world() + game.dirt != expected_total) {
            printf("FAIL fuzz at frame %d (tx=%d ty=%d dirt=%d)\n", i, game.tx, game.ty, game.dirt);
            fails++;
            break;
        }
    }
    for (int i = 0; i < WORLD_W; i++) CHECK(world[0][i] == BLOCK_WATER && world[WORLD_H - 1][i] == BLOCK_WATER);
    for (int i = 0; i < WORLD_H; i++) CHECK(world[i][0] == BLOCK_WATER && world[i][WORLD_W - 1] == BLOCK_WATER);

    printf(fails ? "\n%d check(s) FAILED\n" : "\nAll checks passed.\n", fails);
    return fails ? 1 : 0;
}
