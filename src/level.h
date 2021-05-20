/*
 * Pacman Clone
 *
 * Copyright (c) 2021 Amanch Esmailzadeh
 */

#ifndef LEVEL_H
#define LEVEL_H

#include <assert.h>
#include <stdint.h>
#include "common.h"
#include "render.h"

typedef struct {
    Vector2 sub;
    int32_t x;
    int32_t y;
} TileCoord;

typedef struct Level {
    TileCoord ghost_start[GHOST_COUNT];
    TileCoord player_start;
    TileCoord gate_tile;
    uint32_t pellet_count;
    uint32_t pellets_eaten;
    uint32_t rows;
    uint32_t columns;
    uint32_t data[];
} Level;

#define X_COORDS_VALID(xcoord, level) ((xcoord) >= 0 && (xcoord) < (int32_t)(level)->columns)
#define Y_COORDS_VALID(ycoord, level) ((ycoord) >= 0 && (ycoord) < (int32_t)(level)->rows)

static inline uint32_t * get_level_tile(Level *level, int32_t x, int32_t y) {
    if(level && x >= 0 && X_COORDS_VALID(x, level) && Y_COORDS_VALID(y, level)) {
        return &level->data[y * level->columns + x];
    }

    return NULL;
}

static inline uint32_t get_level_tile_data(const Level *level, const TileCoord *coord) {
    assert(level && coord);
    return level->data[coord->y * level->columns + coord->x];
}

typedef enum {
    TILE_NEIGHBOR_TOP,
    TILE_NEIGHBOR_LEFT,
    TILE_NEIGHBOR_BOTTOM,
    TILE_NEIGHBOR_RIGHT
} TileNeighbor;

static inline int32_t get_neighboring_tile_index(const Level *level, int32_t x, int32_t y, TileNeighbor dir) {
    int32_t index = -1;

    if(level) {
#define NEIGHBOR_TILE_INDEX(neighbor_dir, xn, yn) \
        case neighbor_dir: \
            if(X_COORDS_VALID((xn), level) && Y_COORDS_VALID((yn), level)) { \
                index = (yn) * level->columns + (xn); \
            } \
            break

        switch(dir) {
            NEIGHBOR_TILE_INDEX(TILE_NEIGHBOR_TOP, x, (y-1));
            NEIGHBOR_TILE_INDEX(TILE_NEIGHBOR_LEFT, (x-1), y);
            NEIGHBOR_TILE_INDEX(TILE_NEIGHBOR_BOTTOM, x, (y+1));
            NEIGHBOR_TILE_INDEX(TILE_NEIGHBOR_RIGHT, (x+1), y);
        }

#undef NEIGHBOR_TILE_INDEX
    }

    return index;
}

Level * load_next_level(void);
Level * load_first_level(void);
void unload_level(Level **level);

#undef X_COORDS_VALID
#undef Y_COORDS_VALID

#endif /* LEVEL_H */
