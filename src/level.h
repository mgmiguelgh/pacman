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
    TILE_TYPE_EMPTY,
    TILE_TYPE_WALL,
    TILE_TYPE_STARTING_POSITION,
    TILE_TYPE_GHOST_HOUSE_GATE,
    TILE_TYPE_PELLET,
    TILE_TYPE_POWER_PELLET,
    TILE_TYPE_EATEN_PELLET,
    TILE_TYPE_EATEN_POWER_PELLET,
    TILE_TYPE_RED_GHOST_START,
    TILE_TYPE_PINK_GHOST_START,
    TILE_TYPE_ORANGE_GHOST_START,
    TILE_TYPE_CYAN_GHOST_START
} TileType;

enum {
    ATLAS_SPRITE_PLAYER_F1,
    ATLAS_SPRITE_PLAYER_F2,
    ATLAS_SPRITE_WALL,
    ATLAS_SPRITE_PELLET,
    ATLAS_SPRITE_POWER_PELLET,
    ATLAS_SPRITE_CYAN_GHOST_UP,
    ATLAS_SPRITE_CYAN_GHOST_LEFT,
    ATLAS_SPRITE_CYAN_GHOST_DOWN,
    ATLAS_SPRITE_CYAN_GHOST_RIGHT,
    ATLAS_SPRITE_CYAN_GHOST_FRIGHTENED,
    ATLAS_SPRITE_PINK_GHOST_UP,
    ATLAS_SPRITE_PINK_GHOST_LEFT,
    ATLAS_SPRITE_PINK_GHOST_DOWN,
    ATLAS_SPRITE_PINK_GHOST_RIGHT,
    ATLAS_SPRITE_PINK_GHOST_FRIGHTENED,
    ATLAS_SPRITE_RED_GHOST_UP,
    ATLAS_SPRITE_RED_GHOST_LEFT,
    ATLAS_SPRITE_RED_GHOST_DOWN,
    ATLAS_SPRITE_RED_GHOST_RIGHT,
    ATLAS_SPRITE_RED_GHOST_FRIGHTENED,
    ATLAS_SPRITE_ORANGE_GHOST_UP,
    ATLAS_SPRITE_ORANGE_GHOST_LEFT,
    ATLAS_SPRITE_ORANGE_GHOST_DOWN,
    ATLAS_SPRITE_ORANGE_GHOST_RIGHT,
    ATLAS_SPRITE_ORANGE_GHOST_FRIGHTENED,
    ATLAS_SPRITE_GHOST_EATEN_UP,
    ATLAS_SPRITE_GHOST_EATEN_LEFT,
    ATLAS_SPRITE_GHOST_EATEN_DOWN,
    ATLAS_SPRITE_GHOST_EATEN_RIGHT,
    ATLAS_SPRITE_GHOST_HOUSE_GATE,

    ATLAS_SPRITE_COUNT
};

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

#endif /* LEVEL_H */
