/*
 * Pacman Clone
 *
 * Copyright (c) 2021 Amanch Esmailzadeh
 */

#include "level.h"

Level * load_next_level(void) {
    Level *level = get_next_level_from_disk();
    if(level) {
        return level;
    }

    return NULL;
}

void unload_level(Level **level) {
    if(level) {
        free(*level);
        *level = NULL;
    }
}

static const Rect atlas_sprites[ATLAS_SPRITE_COUNT] = {
    [ATLAS_SPRITE_EMPTY] = { .x = 0, .y = 0, .width = 32, .height = 32 },
    [ATLAS_SPRITE_PLAYER_FRAME1] = { .x = 32, .y = 0, .width = 32, .height = 32 },
    [ATLAS_SPRITE_PLAYER_FRAME2] = { .x = 64, .y = 0, .width = 32, .height = 32 },
    [ATLAS_SPRITE_BLINKY_FRAME1] = { .x = 96, .y = 0, .width = 32, .height = 32 },
    [ATLAS_SPRITE_BLINKY_FRAME2] = { .x = 128, .y = 0, .width = 32, .height = 32 },
    [ATLAS_SPRITE_PINKY_FRAME1] = { .x = 160, .y = 0, .width = 32, .height = 32 },
    [ATLAS_SPRITE_PINKY_FRAME2] = { .x = 192, .y = 0, .width = 32, .height = 32 },
    [ATLAS_SPRITE_INKY_FRAME1] = { .x = 224, .y = 0, .width = 32, .height = 32 },
    [ATLAS_SPRITE_INKY_FRAME2] = { .x = 0, .y = 32, .width = 32, .height = 32 },
    [ATLAS_SPRITE_CLYDE_FRAME1] = { .x = 32, .y = 32, .width = 32, .height = 32 },
    [ATLAS_SPRITE_CLYDE_FRAME2] = { .x = 64, .y = 32, .width = 32, .height = 32 },
    [ATLAS_SPRITE_BLINKY_FRIGHTENED_FRAME1] = { .x = 96, .y = 32, .width = 32, .height = 32 },
    [ATLAS_SPRITE_BLINKY_FRIGHTENED_FRAME2] = { .x = 128, .y = 32, .width = 32, .height = 32 },
    [ATLAS_SPRITE_PINKY_FRIGHTENED_FRAME1] = { .x = 160, .y = 32, .width = 32, .height = 32 },
    [ATLAS_SPRITE_PINKY_FRIGHTENED_FRAME2] = { .x = 192, .y = 32, .width = 32, .height = 32 },
    [ATLAS_SPRITE_INKY_FRIGHTENED_FRAME1] = { .x = 224, .y = 32, .width = 32, .height = 32 },
    [ATLAS_SPRITE_INKY_FRIGHTENED_FRAME2] = { .x = 0, .y = 64, .width = 32, .height = 32 },
    [ATLAS_SPRITE_CLYDE_FRIGHTENED_FRAME1] = { .x = 32, .y = 64, .width = 32, .height = 32 },
    [ATLAS_SPRITE_CLYDE_FRIGHTENED_FRAME2] = { .x = 64, .y = 64, .width = 32, .height = 32 },
    [ATLAS_SPRITE_GHOST_EATEN_UP] = { .x = 96, .y = 64, .width = 32, .height = 32 },
    [ATLAS_SPRITE_GHOST_EATEN_LEFT] = { .x = 128, .y = 64, .width = 32, .height = 32 },
    [ATLAS_SPRITE_GHOST_EATEN_DOWN] = { .x = 160, .y = 64, .width = 32, .height = 32 },
    [ATLAS_SPRITE_GHOST_EATEN_RIGHT] = { .x = 192, .y = 64, .width = 32, .height = 32 },
    [ATLAS_SPRITE_WALL_NORMAL] = { .x = 224, .y = 64, .width = 32, .height = 32 },
    [ATLAS_SPRITE_WALL_BOTTOM] = { .x = 0, .y = 96, .width = 32, .height = 32 },
    [ATLAS_SPRITE_GHOST_HOUSE_GATE] = { .x = 32, .y = 96, .width = 32, .height = 32 },
    [ATLAS_SPRITE_PELLET] = { .x = 64, .y = 96, .width = 32, .height = 32 },
    [ATLAS_SPRITE_POWER_PELLET] = { .x = 96, .y = 96, .width = 32, .height = 32 }
};

void get_atlas_sprite(AtlasSprite id, Rect *r) {
    assert(r && id >= 0 && id < ATLAS_SPRITE_COUNT);
    memcpy(r, &atlas_sprites[id], sizeof(*r));
}
