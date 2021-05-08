/*
 * Pacman Clone
 *
 * Copyright (c) 2021 Amanch Esmailzadeh
 */

#ifndef GAME_H
#define GAME_H

#include <stdint.h>
#include <stdbool.h>
#include "level.h"
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

#define TILE_SIZE 32

typedef enum {
    MOVEMENT_DIR_UP = 0,
    MOVEMENT_DIR_LEFT,
    MOVEMENT_DIR_DOWN,
    MOVEMENT_DIR_RIGHT,
    MOVEMENT_DIR_NONE
} MovementDirection;

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
