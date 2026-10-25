/*
 * game.h - Mini Minecraft GBA game state and entry points.
 */
#ifndef GAME_H
#define GAME_H

#include "gba.h"
#include "art.h"

#define WORLD_W    24      /* world size in blocks */
#define WORLD_H    20
#define BLOCK_PX   16      /* one block = 16x16 pixels */
#define MOVE_SPEED 2       /* pixels per frame while walking (8 frames/block) */
#define MAX_DIRT   99

enum { STATE_TITLE, STATE_PLAY, STATE_PAUSE };
enum { DIR_UP, DIR_RIGHT, DIR_DOWN, DIR_LEFT };   /* clockwise order */

typedef struct {
    int state;
    int tx, ty;          /* tile the player occupies (destination while walking) */
    int px, py;          /* player position in world pixels (top-left of sprite) */
    int facing;          /* DIR_* - the "selected" target direction */
    int moving;          /* 1 while sliding between two tiles */
    int anim_t;          /* walk animation timer */
    int dirt;            /* inventory: collected dirt blocks */
    int show_help;       /* SELECT toggles the controls bar */
    int want_place;      /* A pressed, waiting for the player to stop */
    int want_break;      /* B pressed, waiting for the player to stop */
    int cam_x, cam_y;    /* camera scroll in world pixels */
    u16 prev_keys;
} Game;

extern Game game;
extern u8 world[WORLD_H][WORLD_W];

void game_init(void);    /* set up video + world; call once */
void game_frame(void);   /* run one frame; call once per VBlank */

#endif /* GAME_H */
