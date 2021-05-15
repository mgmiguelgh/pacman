/*
 * Pacman Clone
 *
 * Copyright (c) 2021 Amanch Esmailzadeh
 */

#ifndef LEVEL_H
#define LEVEL_H

#include <assert.h>
#include <stdint.h>
#include "render.h"

typedef struct {
    Vector2 sub;
    int32_t x;
    int32_t y;
} TileCoord;

typedef enum {
    ATLAS_SPRITE_EMPTY,
    ATLAS_SPRITE_PLAYER_FRAME1,
    ATLAS_SPRITE_PLAYER_FRAME2,
    ATLAS_SPRITE_BLINKY_FRAME1,
    ATLAS_SPRITE_BLINKY_FRAME2,
    ATLAS_SPRITE_PINKY_FRAME1,
    ATLAS_SPRITE_PINKY_FRAME2,
    ATLAS_SPRITE_INKY_FRAME1,
    ATLAS_SPRITE_INKY_FRAME2,
    ATLAS_SPRITE_CLYDE_FRAME1,
    ATLAS_SPRITE_CLYDE_FRAME2,
    ATLAS_SPRITE_BLINKY_FRIGHTENED_FRAME1,
    ATLAS_SPRITE_BLINKY_FRIGHTENED_FRAME2,
    ATLAS_SPRITE_PINKY_FRIGHTENED_FRAME1,
    ATLAS_SPRITE_PINKY_FRIGHTENED_FRAME2,
    ATLAS_SPRITE_INKY_FRIGHTENED_FRAME1,
    ATLAS_SPRITE_INKY_FRIGHTENED_FRAME2,
    ATLAS_SPRITE_CLYDE_FRIGHTENED_FRAME1,
    ATLAS_SPRITE_CLYDE_FRIGHTENED_FRAME2,
    ATLAS_SPRITE_GHOST_EATEN_UP,
    ATLAS_SPRITE_GHOST_EATEN_LEFT,
    ATLAS_SPRITE_GHOST_EATEN_DOWN,
    ATLAS_SPRITE_GHOST_EATEN_RIGHT,
    ATLAS_SPRITE_WALL_NORMAL,
    ATLAS_SPRITE_WALL_BOTTOM,
    ATLAS_SPRITE_GHOST_HOUSE_GATE,
    ATLAS_SPRITE_PELLET,
    ATLAS_SPRITE_POWER_PELLET,

    ATLAS_SPRITE_COUNT
} AtlasSprite;

typedef struct Level {
    TileCoord gate_tile;
    uint32_t pellet_count;
    uint32_t pellets_eaten;
    uint32_t rows;
    uint32_t columns;
    uint32_t data[];
} Level;

static inline uint32_t * get_level_tile(Level *level, int32_t x, int32_t y) {
    if(level && x >= 0 && x < (int32_t)level->columns &&
       y >= 0 && y < (int32_t)level->rows) {
        return &level->data[y * level->columns + x];
    }

    return NULL;
}

static inline uint32_t get_level_tile_data(const Level *level, const TileCoord *coord) {
    assert(level && coord);
    return level->data[coord->y * level->columns + coord->x];
}

Level * load_next_level(void);
void unload_level(Level **level);
void get_atlas_sprite_rect(AtlasSprite id, Rect *r);

#endif /* LEVEL_H */
