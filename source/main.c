/*
 * main.c - Mini Minecraft GBA entry point.
 */
#include "game.h"

/* Wait for the start of the next VBlank (scanline 160). */
static void vsync(void)
{
    while (REG_VCOUNT >= SCREEN_H) { }   /* already in VBlank: wait for it to end */
    while (REG_VCOUNT <  SCREEN_H) { }   /* wait for the next VBlank */
}

int main(void)
{
    game_init();
    for (;;) {
        vsync();
        game_frame();
    }
    return 0;
}
