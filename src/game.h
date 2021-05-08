/*
 * Pacman Clone
 *
 * Copyright (c) 2021 Amanch Esmailzadeh
 */

#ifndef GAME_H
#define GAME_H

#include <stdint.h>
#include <stdbool.h>
#include "render.h"

#define IGNORED_VARIABLE(v) (void)v

typedef uint32_t bool32;

enum {
    INPUT_UP = 1 << 0,
    INPUT_LEFT = 1 << 1,
    INPUT_DOWN = 1 << 2,
    INPUT_RIGHT = 1 << 3,
    INPUT_CONFIRM = 1 << 4,
    INPUT_CANCEL = 1 << 5
};

typedef struct {
    float x;
    float y;
} Vector2;

#define TILE_SIZE 16
#define TILE_COUNT_X (DEFAULT_FRAMEBUFFER_WIDTH / TILE_SIZE)
#define TILE_COUNT_Y (DEFAULT_FRAMEBUFFER_HEIGHT / TILE_SIZE)
#define LAST_TILE_X (TILE_COUNT_X - 1)
#define LAST_TILE_Y (TILE_COUNT_Y - 1)

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

typedef enum {
    MOVEMENT_DIR_UP = 0,
    MOVEMENT_DIR_LEFT,
    MOVEMENT_DIR_DOWN,
    MOVEMENT_DIR_RIGHT,
    MOVEMENT_DIR_NONE
} MovementDirection;

typedef struct {
    Vector2 sub;
    int32_t x;
    int32_t y;
} TileCoord;

typedef struct {
    bool32 running;
    float elapsed;
    float target;
} Timer;

typedef struct {
    TileCoord coord;
    MovementDirection dir;
    float default_speed;
    float speed;
} GameEntity;

typedef struct {
    GameEntity entity;
    Timer input_queue;
    uint32_t prev_input;
} PlayerEntity;

typedef struct {
    TileCoord target;
    GameEntity entity;
    enum {
        GHOST_STATE_SCATTER,
        GHOST_STATE_CHASE,
        GHOST_STATE_EATEN
    } state;

    float gate_pass_percentage;
    union {
        struct {
            uint32_t frightened : 1;
            uint32_t can_pass_gate : 1;
            uint32_t in_ghost_house : 1;
        };
        uint32_t flags;
    };
} GhostEntity;

typedef struct {
    TileCoord gate_tile;
    uint32_t pellet_count;
    uint32_t pellets_eaten;
    uint32_t data[TILE_COUNT_Y][TILE_COUNT_X];
} Level;

enum {
    RED_GHOST,
    PINK_GHOST,
    ORANGE_GHOST,
    CYAN_GHOST,

    GHOST_COUNT
};

void initialize_game(void);
void close_game(void);
bool update_timer(Timer *timer, float ms);
void update_loop(float dt, uint32_t input);
void render_loop(void);

#endif /* GAME_H */
