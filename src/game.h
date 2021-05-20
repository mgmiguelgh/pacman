/*
 * Pacman Clone
 *
 * Copyright (c) 2021 Amanch Esmailzadeh
 */

#ifndef GAME_H
#define GAME_H

#include <stdint.h>
#include <stdbool.h>
#include "common.h"
#include "level.h"
#include "render.h"

enum {
    INPUT_UP = 1 << 0,
    INPUT_LEFT = 1 << 1,
    INPUT_DOWN = 1 << 2,
    INPUT_RIGHT = 1 << 3,
    INPUT_CONFIRM = 1 << 4,
    INPUT_MENU = 1 << 5
};

#define TILE_SIZE 32
#define TILE_SIZE_ALT 16

typedef enum {
    MOVEMENT_DIR_UP = 0,
    MOVEMENT_DIR_LEFT,
    MOVEMENT_DIR_DOWN,
    MOVEMENT_DIR_RIGHT,
    MOVEMENT_DIR_NONE
} MovementDirection;

typedef struct {
    TileCoord coord;
    MovementDirection dir;
    MovementDirection facing;
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
    Timer eaten_anim_timer;

    enum {
        GHOST_STATE_SCATTER,
        GHOST_STATE_CHASE,
        GHOST_STATE_EATEN
    } state;

    float gate_pass_percentage;
    union {
        struct {
            uint32_t frightened : 1;
            uint32_t in_ghost_house : 1;
        };
        uint32_t flags;
    };
} GhostEntity;

void initialize_game(void);
void close_game(void);
void signal_window_resize(int32_t new_width, int32_t new_height);

bool update_loop(float dt, uint32_t input);
void render_loop(float dt);

#endif /* GAME_H */
