/*
 * Pacman Clone
 *
 * Copyright (c) 2021 Amanch Esmailzadeh
 */

#include <gl/gl.h>
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>

#include "game.h"

#include "level.c"
#include "texture.c"
#include "render.c"

#define EPSILON 0.1f
#define DEFAULT_MOVEMENT_SPEED 5.0f
#define FRIGHTENED_SPEED_MOD 0.65f
#define SCATTER_MODE_TIME 5.0f
#define CHASE_MODE_TIME 20.0f
#define FRIGHTENED_MODE_TIME 10.0f
#define INPUT_QUEUE_TIME_MAX 1.0f

static const Rect atlas_sprites[ATLAS_SPRITE_COUNT] = {
    [ATLAS_SPRITE_PLAYER_F1] = { .x = 128, .y = 0, .width = TILE_SIZE, .height = TILE_SIZE },
    [ATLAS_SPRITE_PLAYER_F2] = { .x = 160, .y = 0, .width = TILE_SIZE, .height = TILE_SIZE },
    [ATLAS_SPRITE_WALL] = { .x = 192, .y = 0, .width = TILE_SIZE, .height = TILE_SIZE },
    [ATLAS_SPRITE_PELLET] = { .x = 224, .y = 0, .width = TILE_SIZE, .height = TILE_SIZE },
    [ATLAS_SPRITE_POWER_PELLET] = { .x = 256, .y = 0, .width = TILE_SIZE, .height = TILE_SIZE },
    [ATLAS_SPRITE_CYAN_GHOST_UP] = { .x = 0, .y = 32, .width = TILE_SIZE, .height = TILE_SIZE },
    [ATLAS_SPRITE_CYAN_GHOST_LEFT] = { .x = 32, .y = 0, .width = TILE_SIZE, .height = TILE_SIZE },
    [ATLAS_SPRITE_CYAN_GHOST_DOWN] = { .x = 0, .y = 0, .width = TILE_SIZE, .height = TILE_SIZE },
    [ATLAS_SPRITE_CYAN_GHOST_RIGHT] = { .x = 32, .y = 32, .width = TILE_SIZE, .height = TILE_SIZE },
    [ATLAS_SPRITE_CYAN_GHOST_FRIGHTENED] = { .x = 0, .y = 128, .width = TILE_SIZE, .height = TILE_SIZE },
    [ATLAS_SPRITE_PINK_GHOST_UP] = { .x = 64, .y = 32, .width = TILE_SIZE, .height = TILE_SIZE },
    [ATLAS_SPRITE_PINK_GHOST_LEFT] = { .x = 96, .y = 0, .width = TILE_SIZE, .height = TILE_SIZE },
    [ATLAS_SPRITE_PINK_GHOST_DOWN] = { .x = 64, .y = 0, .width = TILE_SIZE, .height = TILE_SIZE },
    [ATLAS_SPRITE_PINK_GHOST_RIGHT] = { .x = 96, .y = 32, .width = TILE_SIZE, .height = TILE_SIZE },
    [ATLAS_SPRITE_PINK_GHOST_FRIGHTENED] = { .x = 32, .y = 128, .width = TILE_SIZE, .height = TILE_SIZE },
    [ATLAS_SPRITE_RED_GHOST_UP] = { .x = 0, .y = 96, .width = TILE_SIZE, .height = TILE_SIZE },
    [ATLAS_SPRITE_RED_GHOST_LEFT] = { .x = 32, .y = 64, .width = TILE_SIZE, .height = TILE_SIZE },
    [ATLAS_SPRITE_RED_GHOST_DOWN] = { .x = 0, .y = 64, .width = TILE_SIZE, .height = TILE_SIZE },
    [ATLAS_SPRITE_RED_GHOST_RIGHT] = { .x = 32, .y = 64, .width = TILE_SIZE, .height = TILE_SIZE },
    [ATLAS_SPRITE_RED_GHOST_FRIGHTENED] = { .x = 0, .y = 160, .width = TILE_SIZE, .height = TILE_SIZE },
    [ATLAS_SPRITE_ORANGE_GHOST_UP] = { .x = 64, .y = 96, .width = TILE_SIZE, .height = TILE_SIZE },
    [ATLAS_SPRITE_ORANGE_GHOST_LEFT] = { .x = 96, .y = 64, .width = TILE_SIZE, .height = TILE_SIZE },
    [ATLAS_SPRITE_ORANGE_GHOST_DOWN] = { .x = 64, .y = 64, .width = TILE_SIZE, .height = TILE_SIZE },
    [ATLAS_SPRITE_ORANGE_GHOST_RIGHT] = { .x = 96, .y = 96, .width = TILE_SIZE, .height = TILE_SIZE },
    [ATLAS_SPRITE_ORANGE_GHOST_FRIGHTENED] = { .x = 32, .y = 160, .width = TILE_SIZE, .height = TILE_SIZE },
    [ATLAS_SPRITE_GHOST_EATEN_UP] = { .x = 64, .y = 160, .width = TILE_SIZE, .height = TILE_SIZE },
    [ATLAS_SPRITE_GHOST_EATEN_LEFT] = { .x = 96, .y = 128, .width = TILE_SIZE, .height = TILE_SIZE },
    [ATLAS_SPRITE_GHOST_EATEN_DOWN] = { .x = 64, .y = 128, .width = TILE_SIZE, .height = TILE_SIZE },
    [ATLAS_SPRITE_GHOST_EATEN_RIGHT] = { .x = 96, .y = 160, .width = TILE_SIZE, .height = TILE_SIZE },
    [ATLAS_SPRITE_GHOST_HOUSE_GATE] = { .x = 160, .y = 32, .width = TILE_SIZE, .height = TILE_SIZE },
};

static const Vector2 direction_vectors[4] = {
    { .x = 0.0f, .y = -1.0f }, // Up
    { .x = -1.0f, .y = 0.0f }, // Left
    { .x = 0.0f, .y = 1.0f },  // Down
    { .x = 1.0f, .y = 0.0f },  // Right
};

static struct {
    Texture2D *atlas;
    Level *level;

    GhostEntity ghosts[GHOST_COUNT];
    PlayerEntity player;

    Timer ghost_mode_timer;
    Timer frightened_timer;

    GLuint gl_program;

    enum {
        GAME_MODE_SCATTER,
        GAME_MODE_CHASE
    } mode;
} game_state = {
    .atlas = NULL,
    .level = NULL
};

static struct {
    Vector2i scroll;
    Vector2 offset;
    float zoom;
} game_camera;

#define TILE_COUNT_X (int32_t)game_state.level->columns
#define TILE_COUNT_Y (int32_t)game_state.level->rows
#define LAST_TILE_X (int32_t)(game_state.level->columns - 1)
#define LAST_TILE_Y (int32_t)(game_state.level->rows - 1)

// Util functions
static bool rect_aabb_test(const Rect *a, const Rect *b) {
    if(a && b) {
        int32_t ax2 = a->x + (int32_t)a->width;
        int32_t ay2 = a->y + (int32_t)a->height;
        int32_t bx2 = b->x + (int32_t)b->width;
        int32_t by2 = b->y + (int32_t)b->height;

        return a->x < bx2 &&
            ax2 > b->x &&
            a->y < by2 &&
            ay2 > b->y;
    }

    return false;
}

static inline bool coords_within_bounds(const TileCoord *coord) {
    return (coord) ? coord->x >= 0 && coord->x <= LAST_TILE_X &&
        coord->y >= 0 && coord->y <= LAST_TILE_Y : 0;
}

static inline bool float_nearly_equal(float a, float b) {
    return fabsf(a - b) < EPSILON;
}

static inline float dot_product(MovementDirection dir1, MovementDirection dir2) {
    assert(dir1 != MOVEMENT_DIR_NONE && dir2 != MOVEMENT_DIR_NONE);
    return direction_vectors[dir1].x * direction_vectors[dir2].x +
        direction_vectors[dir1].y * direction_vectors[dir2].y;
}

static inline float pellets_eaten_percentage(void) {
    return (float)game_state.level->pellets_eaten / (float)game_state.level->pellet_count;
}

static inline bool ghost_can_pass_gate(GhostEntity *ghost) {
    return ghost && (ghost->state == GHOST_STATE_EATEN ||
        (ghost->in_ghost_house && ghost->gate_pass_percentage <= pellets_eaten_percentage()));
}

static bool tile_is_wall(const TileCoord *coord, GhostEntity *ghost) {
    if(coord) {
        uint32_t type = get_level_tile_data(game_state.level, coord);

        if(ghost_can_pass_gate(ghost)) {
            ghost->target = game_state.level->gate_tile;
            return type == TILE_TYPE_WALL;
        }

        return type == TILE_TYPE_WALL || type == TILE_TYPE_GHOST_HOUSE_GATE;
    }

    return false;
}

static Vector2i get_entity_abs_position(const GameEntity *entity) {
    static const float TILE_SIZE_F = (float)TILE_SIZE;

    if(entity) {
        return (Vector2i) {
            .x = (entity->coord.x * TILE_SIZE) + (int32_t)(entity->coord.sub.x * TILE_SIZE_F),
            .y = (entity->coord.y * TILE_SIZE) + (int32_t)(entity->coord.sub.y * TILE_SIZE_F)
        };
    }

    return (Vector2i) { 0 };
}

// Function prototypes
static void tilecoord_to_rect(const TileCoord *coord, Rect *rect, float scale);
static void move_entity(GameEntity *entity, TileCoord *destination, float dt);
static void set_ghost_behavior(GhostEntity *ghost, uint32_t type);
static void set_ghost_speed(GhostEntity *ghost);
static void wrap_tile_coords(TileCoord *coord, bool outside_area);
static void tile_coords_from_direction(TileCoord *coord, MovementDirection direction);

static void reset_game(void) {
    if(!game_state.atlas) {
        game_state.atlas = load_texture("data/texture_atlas2x.bmp", 0xff00ff);
    }
    memset(&game_state.player, 0, sizeof(game_state.player));
    memset(&game_state.ghosts, 0, sizeof(game_state.ghosts));

    game_state.mode = GAME_MODE_SCATTER;
    game_state.ghost_mode_timer.running = true;
    game_state.ghost_mode_timer.elapsed = 0.0f;
    game_state.ghost_mode_timer.target = SCATTER_MODE_TIME;
    game_state.frightened_timer.running = false;
    game_state.frightened_timer.elapsed = 0.0f;
    game_state.frightened_timer.target = FRIGHTENED_MODE_TIME;

    game_state.level = load_next_level();
    memset(&game_state.level->gate_tile, 0, sizeof(game_state.level->gate_tile));
    game_state.level->pellet_count = 0;
    game_state.level->pellets_eaten = 0;

    for(int32_t y = 0; y < LAST_TILE_Y; y++) {
        for(int32_t x = 0; x < LAST_TILE_X; x++) {
            uint32_t *tile = get_level_tile(game_state.level, x, y);

            if(!tile) {
                continue;
            }

            TileType type = *tile;
            switch(type) {
                case TILE_TYPE_GHOST_HOUSE_GATE:
                    game_state.level->gate_tile.x = x;
                    game_state.level->gate_tile.y = y;
                    break;
                case TILE_TYPE_STARTING_POSITION:
                    game_state.player.entity.coord.x = x;
                    game_state.player.entity.coord.y = y;
                    break;
                case TILE_TYPE_RED_GHOST_START:
                    game_state.ghosts[RED_GHOST].entity.coord.x = x;
                    game_state.ghosts[RED_GHOST].entity.coord.y = y;
                    break;
                case TILE_TYPE_PINK_GHOST_START:
                    game_state.ghosts[PINK_GHOST].entity.coord.x = x;
                    game_state.ghosts[PINK_GHOST].entity.coord.y = y;
                    break;
                case TILE_TYPE_ORANGE_GHOST_START:
                    game_state.ghosts[ORANGE_GHOST].entity.coord.x = x;
                    game_state.ghosts[ORANGE_GHOST].entity.coord.y = y;
                    break;
                case TILE_TYPE_CYAN_GHOST_START:
                    game_state.ghosts[CYAN_GHOST].entity.coord.x = x;
                    game_state.ghosts[CYAN_GHOST].entity.coord.y = y;
                    break;
                default:
                    break;
            }

            if(type == TILE_TYPE_PELLET ||
               type == TILE_TYPE_POWER_PELLET) {
                game_state.level->pellet_count++;
            }
        }
    }

    game_state.player.input_queue.target = INPUT_QUEUE_TIME_MAX;
    game_state.player.entity.dir = MOVEMENT_DIR_NONE;
    game_state.player.entity.default_speed = DEFAULT_MOVEMENT_SPEED;
    game_state.player.entity.speed = DEFAULT_MOVEMENT_SPEED;

    for(int32_t i = 0; i < GHOST_COUNT; i++) {
        game_state.ghosts[i].entity.dir = MOVEMENT_DIR_UP;
        game_state.ghosts[i].state = GHOST_STATE_SCATTER;
        game_state.ghosts[i].entity.coord.sub.x = 0.0f;
        game_state.ghosts[i].entity.coord.sub.y = 0.0f;
        game_state.ghosts[i].can_pass_gate = false;
        game_state.ghosts[i].in_ghost_house = true;
    }

    game_state.ghosts[RED_GHOST].entity.dir = MOVEMENT_DIR_RIGHT;
    game_state.ghosts[RED_GHOST].gate_pass_percentage = 0.0f;
    game_state.ghosts[RED_GHOST].in_ghost_house = false;
    game_state.ghosts[PINK_GHOST].gate_pass_percentage = 0.15f;
    game_state.ghosts[ORANGE_GHOST].gate_pass_percentage = 0.5f;
    game_state.ghosts[CYAN_GHOST].gate_pass_percentage = 0.3f;

    game_state.ghosts[RED_GHOST].entity.default_speed = DEFAULT_MOVEMENT_SPEED - 0.25f;
    game_state.ghosts[RED_GHOST].entity.speed = game_state.ghosts[RED_GHOST].entity.default_speed;
    game_state.ghosts[PINK_GHOST].entity.default_speed = DEFAULT_MOVEMENT_SPEED - 0.5f;
    game_state.ghosts[PINK_GHOST].entity.speed = game_state.ghosts[PINK_GHOST].entity.default_speed;
    game_state.ghosts[ORANGE_GHOST].entity.default_speed = DEFAULT_MOVEMENT_SPEED - 1.0f;
    game_state.ghosts[ORANGE_GHOST].entity.speed = game_state.ghosts[ORANGE_GHOST].entity.default_speed;
    game_state.ghosts[CYAN_GHOST].entity.default_speed = DEFAULT_MOVEMENT_SPEED - 0.75f;
    game_state.ghosts[CYAN_GHOST].entity.speed = game_state.ghosts[CYAN_GHOST].entity.default_speed;

    game_camera.scroll.x = 0;
    game_camera.scroll.y = 0;
    game_camera.offset.x = 0.5f;
    game_camera.offset.y = 0.5f;
    game_camera.zoom = 1.0f;
}

static inline void check_gl_status(GLuint id, GLenum parameter) {
    GLint status;
    static char error_log[512];

    if(parameter == GL_COMPILE_STATUS) {
        glGetShaderiv(id, parameter, &status);
        if(status != GL_TRUE) {
            glGetShaderInfoLog(id, sizeof(error_log), NULL, error_log);
            if(status == 0) {
                fprintf(stderr, "Could not compile the shader!\n");
                exit(-1);
            }
        }
    } else if(parameter == GL_LINK_STATUS) {
        glGetProgramiv(id, parameter, &status);
        if(status != GL_TRUE) {
            glGetProgramInfoLog(id, sizeof(error_log), NULL, error_log);
            if(status == 0) {
                fprintf(stderr, "Could not link the GL program!\n");
                exit(-1);
            }
        }
    }
}

static void initialize_opengl(void) {
    // Setup core OpenGL
    static const char *vertex_shader_source = {
        "#version 330 core\n"
        "layout(location = 0) in vec2 pos;\n"
        "layout(location = 1) in vec2 uvs;\n"
        "out vec2 texcoords;\n"
        "uniform vec3 camera;\n"
        "void main(void) {\n"
        "    texcoords = ((vec2(uvs.s, 1.0 - uvs.t) - vec2(0.5))* camera.z) + camera.xy;\n"
        "    gl_Position = vec4(pos, 0.0, 1.0);\n"
        "}"
    };
    static const char *fragment_shader_source = {
        "#version 330 core\n"
        "uniform sampler2D sampler;\n"
        "in vec2 texcoords;\n"
        "out vec4 out_color;\n"
        "void main(void) {\n"
        "    out_color = texture(sampler, texcoords);\n"
        "}"
    };
    game_state.gl_program = glCreateProgram();
    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

    glShaderSource(vertex_shader, 1, &vertex_shader_source, NULL);
    glShaderSource(fragment_shader, 1, &fragment_shader_source, NULL);

    glCompileShader(vertex_shader);
    check_gl_status(vertex_shader, GL_COMPILE_STATUS);
    glCompileShader(fragment_shader);
    check_gl_status(fragment_shader, GL_COMPILE_STATUS);

    glAttachShader(game_state.gl_program, vertex_shader);
    glAttachShader(game_state.gl_program, fragment_shader);

    glLinkProgram(game_state.gl_program);
    check_gl_status(game_state.gl_program, GL_LINK_STATUS);

    glUseProgram(game_state.gl_program);

    glDetachShader(game_state.gl_program, vertex_shader);
    glDetachShader(game_state.gl_program, fragment_shader);

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    static const GLfloat vertex_buffer_data[] = {
        // Position       // UV coordinates
        -1.0f, -1.0f,     0.0f, 0.0f,
         1.0f, -1.0f,     1.0f, 0.0f,
         1.0f,  1.0f,     1.0f, 1.0f,

         1.0f,  1.0f,     1.0f, 1.0f,
        -1.0f,  1.0f,     0.0f, 1.0f,
        -1.0f, -1.0f,     0.0f, 0.0f,
    };

    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_buffer_data), vertex_buffer_data, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 4, NULL);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * 4, (const void *)(2 * sizeof(GLfloat)));

    glEnable(GL_TEXTURE_2D);
    GLuint framebuffer_texture;
    glGenTextures(1, &framebuffer_texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, framebuffer_texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, DEFAULT_FRAMEBUFFER_WIDTH, DEFAULT_FRAMEBUFFER_HEIGHT,
                 0, GL_RGBA, GL_FLOAT, get_framebuffer());

    float aspect_ratio = (float)DEFAULT_FRAMEBUFFER_WIDTH / (float)DEFAULT_FRAMEBUFFER_HEIGHT;
    float w = DEFAULT_WINDOW_WIDTH;
    float h = w / aspect_ratio;

    if(h > DEFAULT_WINDOW_HEIGHT) {
        h = DEFAULT_WINDOW_HEIGHT;
        w = h * aspect_ratio;
    }

    GLint x = (DEFAULT_WINDOW_WIDTH - (GLint)w) / 2;
    GLint y = (DEFAULT_WINDOW_HEIGHT - (GLint)h) / 2;

    glViewport(x, y, (GLsizei)w, (GLsizei)h);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
}

void initialize_game(void) {
    initialize_opengl();
    reset_game();
}

void close_game(void) {
    destroy_texture(&game_state.atlas);
}

bool update_timer(Timer *timer, float ms) {
    if(timer) {
        timer->elapsed += ms;

        if(timer->elapsed >= timer->target) {
            timer->elapsed = 0.0f;
            return true;
        }
    }

    return false;
}

void update_loop(float dt, uint32_t input) {
    if(update_timer(&game_state.ghost_mode_timer, dt)) {
        game_state.ghost_mode_timer.elapsed = 0.0f;

        if(game_state.mode == GAME_MODE_SCATTER) {
            game_state.mode = GAME_MODE_CHASE;
            game_state.ghost_mode_timer.target = CHASE_MODE_TIME;
            for(int32_t i = 0; i < GHOST_COUNT; i++) {
                if(game_state.ghosts[i].state == GHOST_STATE_SCATTER) {
                    game_state.ghosts[i].state = GHOST_STATE_CHASE;
                }
            }
        } else {
            game_state.mode = GAME_MODE_SCATTER;
            game_state.ghost_mode_timer.target = SCATTER_MODE_TIME;
            for(int32_t i = 0; i < GHOST_COUNT; i++) {
                if(game_state.ghosts[i].state == GHOST_STATE_CHASE) {
                    game_state.ghosts[i].state = GHOST_STATE_SCATTER;
                }
            }
        }
    }

    if(game_state.frightened_timer.running && update_timer(&game_state.frightened_timer, dt)) {
        game_state.frightened_timer.elapsed = 0.0f;
        game_state.frightened_timer.running = false;

        for(int32_t i = 0; i < GHOST_COUNT; i++) {
            game_state.ghosts[i].frightened = false;
        }
    }

    if(input == 0) {
        if(game_state.player.prev_input != 0 && !update_timer(&game_state.player.input_queue, dt)) {
            input = game_state.player.prev_input;
        } else {
            game_state.player.prev_input = 0;
        }
    } else {
        game_state.player.prev_input = input;
    }

#define MAP_INPUT_TO_MOV_DIR(d) if(input & INPUT_##d) { game_state.player.entity.dir = MOVEMENT_DIR_##d; }
    MovementDirection previous_dir = game_state.player.entity.dir;

    MAP_INPUT_TO_MOV_DIR(UP);
    MAP_INPUT_TO_MOV_DIR(DOWN);
    MAP_INPUT_TO_MOV_DIR(LEFT);
    MAP_INPUT_TO_MOV_DIR(RIGHT);

#undef MAP_INPUT_TO_MOV_DIR

    if(previous_dir != MOVEMENT_DIR_NONE) {
        if(coords_within_bounds(&game_state.player.entity.coord)) {
            bool is_quarter_turn = float_nearly_equal(dot_product(previous_dir, game_state.player.entity.dir), 0.0f);
            if(is_quarter_turn) {
                bool near_center = float_nearly_equal(game_state.player.entity.coord.sub.x, 0.0f) &&
                    float_nearly_equal(game_state.player.entity.coord.sub.y, 0.0f);

                if(near_center) {
                    TileCoord projected = game_state.player.entity.coord;
                    tile_coords_from_direction(&projected, game_state.player.entity.dir);
                    if(tile_is_wall(&projected, NULL)) {
                        // Make sure we're not stopped by walls if we try to
                        // move 90 degrees clockwise or counterclockwise
                        game_state.player.entity.dir = previous_dir;
                    }
                } else {
                    game_state.player.entity.dir = previous_dir;
                }
            }
        } else {
            game_state.player.entity.dir = previous_dir;
        }
    }

    TileCoord next_tile;
    move_entity(&game_state.player.entity, &next_tile, dt);
    if(tile_is_wall(&next_tile, NULL)) {
        Rect player_rect, wall_rect;
        tilecoord_to_rect(&game_state.player.entity.coord, &player_rect, 1.0f);
        tilecoord_to_rect(&next_tile, &wall_rect, 1.0f);

        if(rect_aabb_test(&player_rect, &wall_rect)) {
            wrap_tile_coords(&game_state.player.entity.coord, true);
            game_state.player.prev_input = 0;
            game_state.player.input_queue.elapsed = 0.0f;

            game_state.player.entity.coord.sub.x = 0.0f;
            game_state.player.entity.coord.sub.y = 0.0f;
            game_state.player.entity.dir = MOVEMENT_DIR_NONE;
        }
    }

    uint32_t *player_tile = get_level_tile(game_state.level, game_state.player.entity.coord.x,
                                           game_state.player.entity.coord.y);
    if(player_tile) {
        if(*player_tile == TILE_TYPE_PELLET) {
            game_state.level->pellets_eaten++;
            *player_tile = TILE_TYPE_EMPTY;
        } else if(*player_tile == TILE_TYPE_POWER_PELLET) {
            game_state.level->pellets_eaten++;
            *player_tile = TILE_TYPE_EMPTY;

            game_state.frightened_timer.running = true;
            game_state.frightened_timer.elapsed = 0.0f;

            for(int32_t i = 0; i < GHOST_COUNT; i++) {
                if(game_state.ghosts[i].state != GHOST_STATE_EATEN) {
                    game_state.ghosts[i].frightened = true;
                }
            }
        }
    }

    for(int32_t i = 0; i < GHOST_COUNT; i++) {
        TileCoord old_tile = game_state.ghosts[i].entity.coord;
        set_ghost_speed(&game_state.ghosts[i]);
        move_entity(&game_state.ghosts[i].entity, NULL, dt);

        MovementDirection ghost_dir = game_state.ghosts[i].entity.dir;

        if(ghost_dir == MOVEMENT_DIR_NONE ||
           (old_tile.x == game_state.ghosts[i].entity.coord.x &&
            old_tile.y == game_state.ghosts[i].entity.coord.y)) {
            continue;
        }

        game_state.ghosts[i].target.sub.x = 0.0f;
        game_state.ghosts[i].target.sub.y = 0.0f;

        TileCoord current = game_state.ghosts[i].entity.coord;
        if(ghost_can_pass_gate(&game_state.ghosts[i]) && get_level_tile_data(game_state.level, &current) == TILE_TYPE_GHOST_HOUSE_GATE) {
            game_state.ghosts[i].in_ghost_house = game_state.ghosts[i].state == GHOST_STATE_EATEN;
            game_state.ghosts[i].state = (game_state.mode == GAME_MODE_SCATTER) ?
                GHOST_STATE_SCATTER : GHOST_STATE_CHASE;
        }

        set_ghost_behavior(&game_state.ghosts[i], i);

        MovementDirection best_dir = MOVEMENT_DIR_NONE;
        int32_t dist = INT_MAX;
        for(int32_t dir = 0; dir < 4; dir++) {
            if(dot_product(ghost_dir, dir) >= 0.0f) {
                int32_t next_x = game_state.ghosts[i].entity.coord.x + (int32_t)direction_vectors[dir].x;
                int32_t next_y = game_state.ghosts[i].entity.coord.y + (int32_t)direction_vectors[dir].y;

                TileCoord temp = {
                    .x = next_x,
                    .y = next_y
                };

                if(next_x >= 0 && next_x < TILE_COUNT_X &&
                   next_y >= 0 && next_y < TILE_COUNT_Y &&
                   !tile_is_wall(&temp, &game_state.ghosts[i])) {

                    int32_t dist_x = game_state.ghosts[i].target.x - next_x;
                    int32_t dist_y = game_state.ghosts[i].target.y - next_y;
                    int32_t squared_dist = dist_x * dist_x + dist_y * dist_y;

                    if(squared_dist < dist) {
                        dist = squared_dist;
                        best_dir = (MovementDirection)dir;
                    }

                }
            }
        }

        if(best_dir != MOVEMENT_DIR_NONE) {
            game_state.ghosts[i].entity.dir = best_dir;
        }
    }

    Rect player_rect;
    tilecoord_to_rect(&game_state.player.entity.coord, &player_rect, 0.75f);
    for(int32_t i = 0; i < GHOST_COUNT; i++) {
        Rect ghost_rect;
        tilecoord_to_rect(&game_state.ghosts[i].entity.coord, &ghost_rect, 0.75f);

        if(rect_aabb_test(&player_rect, &ghost_rect)) {
            if(game_state.ghosts[i].frightened) {
                game_state.ghosts[i].frightened = false;
                game_state.ghosts[i].state = GHOST_STATE_EATEN;
            } else if(game_state.ghosts[i].state != GHOST_STATE_EATEN) {
                tilecoord_to_rect(&game_state.player.entity.coord, &player_rect, 0.35f);
                tilecoord_to_rect(&game_state.ghosts[i].entity.coord, &ghost_rect, 0.35f);

                if(rect_aabb_test(&player_rect, &ghost_rect)) {
                    //reset_game();
                }
            }
        }
    }

    Vector2i abs_pos = get_entity_abs_position(&game_state.player.entity);
    game_camera.scroll.x = abs_pos.x - (DEFAULT_FRAMEBUFFER_WIDTH / 2);
    game_camera.scroll.y = abs_pos.y - (DEFAULT_FRAMEBUFFER_HEIGHT / 2);

    if(game_camera.scroll.x < 0) {
        game_camera.scroll.x = 0;
    } else if((game_camera.scroll.x + DEFAULT_FRAMEBUFFER_WIDTH) > (TILE_COUNT_X * TILE_SIZE)) {
        game_camera.scroll.x = TILE_COUNT_X * TILE_SIZE - DEFAULT_FRAMEBUFFER_WIDTH;
    }

    if(game_camera.scroll.y < 0) {
        game_camera.scroll.y = 0;
    } else if((game_camera.scroll.y + DEFAULT_FRAMEBUFFER_HEIGHT) > (TILE_COUNT_Y * TILE_SIZE)) {
        game_camera.scroll.y = TILE_COUNT_Y * TILE_SIZE - DEFAULT_FRAMEBUFFER_HEIGHT;
    }
}

void tilecoord_to_rect(const TileCoord *coord, Rect *rect, float scale) {
    static const float TILE_SIZE_F = (float)TILE_SIZE;

    if(coord && rect && scale > 0.0f) {
        int32_t xpos = TILE_SIZE * coord->x +
            (int32_t)(coord->sub.x * TILE_SIZE_F);
        int32_t ypos = TILE_SIZE * coord->y +
            (int32_t)(coord->sub.y * TILE_SIZE_F);

        int32_t offset = (int32_t)(0.5f * ((1.0f - scale) * TILE_SIZE_F));
        rect->x = xpos + offset;
        rect->y = ypos + offset;
        rect->width = rect->height = (uint32_t)(TILE_SIZE_F * scale); // All tiles are of the same fixed-size
    }
}

void move_entity(GameEntity *entity, TileCoord *destination, float dt) {
    if(entity) {
        int32_t dir_x = 0;
        int32_t dir_y = 0;

        float speed = entity->speed * dt;

        switch(entity->dir) {
            case MOVEMENT_DIR_UP:
                entity->coord.sub.y -= speed;
                entity->coord.sub.x = 0.0f;
                dir_y = -1;
                break;
            case MOVEMENT_DIR_DOWN:
                entity->coord.sub.y += speed;
                entity->coord.sub.x = 0.0f;
                dir_y = 1;
                break;
            case MOVEMENT_DIR_LEFT:
                entity->coord.sub.x -= speed;
                entity->coord.sub.y = 0.0f;
                dir_x = -1;
                break;
            case MOVEMENT_DIR_RIGHT:
                entity->coord.sub.x += speed;
                entity->coord.sub.y = 0.0f;
                dir_x = 1;
                break;
            case MOVEMENT_DIR_NONE:
            default:
                break;
        }

        float intpart;
        entity->coord.sub.x = modff(entity->coord.sub.x, &intpart);
        entity->coord.x += (int32_t)intpart;
        entity->coord.sub.y = modff(entity->coord.sub.y, &intpart);
        entity->coord.y += (int32_t)intpart;

        wrap_tile_coords(&entity->coord, true);

        if(destination) {
            destination->sub.x = 0.0f;
            destination->sub.y = 0.0f;
            destination->x = entity->coord.x + dir_x;
            destination->y = entity->coord.y + dir_y;

            wrap_tile_coords(destination, false);
        }
    }
}

static void set_ghost_chase_behavior(GhostEntity *ghost, uint32_t type) {
    assert(ghost);

    switch(type) {
        case RED_GHOST:
            ghost->target = game_state.player.entity.coord;
            break;
        case PINK_GHOST:
            ghost->target.x = game_state.player.entity.coord.x +
                ((int32_t)direction_vectors[game_state.player.entity.dir].x * 4);
            ghost->target.y = game_state.player.entity.coord.y +
                ((int32_t)direction_vectors[game_state.player.entity.dir].y * 4);
            break;
        case ORANGE_GHOST: {
            int32_t dist_x = game_state.player.entity.coord.x - ghost->entity.coord.x;
            int32_t dist_y = game_state.player.entity.coord.y - ghost->entity.coord.y;

            if((dist_x * dist_x + dist_y * dist_y) >= 8) {
                ghost->target = game_state.player.entity.coord;
            } else {
                ghost->target.x = 8;
                ghost->target.y = TILE_COUNT_Y;
            }

            break;
        }
        case CYAN_GHOST:
            ghost->target.x = game_state.player.entity.coord.x +
                ((int32_t)direction_vectors[game_state.player.entity.dir].x * 2);
            ghost->target.y = game_state.player.entity.coord.y +
                ((int32_t)direction_vectors[game_state.player.entity.dir].y * 2);

            ghost->target.x += (ghost->target.x - game_state.ghosts[RED_GHOST].entity.coord.x);
            ghost->target.y += (ghost->target.y - game_state.ghosts[RED_GHOST].entity.coord.y);

            break;
        default:
            break;
    }
}

static void set_ghost_default_behavior(GhostEntity *ghost, uint32_t type) {
    assert(ghost);

    switch(type) {
        case RED_GHOST:
            ghost->target.x = LAST_TILE_X - 8;
            ghost->target.y = -1;
            break;
        case PINK_GHOST:
            ghost->target.x = 8;
            ghost->target.y = -1;
            break;
        case ORANGE_GHOST:
            ghost->target.x = 8;
            ghost->target.y = TILE_COUNT_Y;
            break;
        case CYAN_GHOST:
            ghost->target.x = LAST_TILE_X - 8;
            ghost->target.y = TILE_COUNT_Y;
            break;
        default:
            break;
    }
}

void set_ghost_behavior(GhostEntity *ghost, uint32_t type) {
    if(ghost && type >= 0 && type < GHOST_COUNT) {
        if(!ghost->frightened) {
            switch(ghost->state) {
                case GHOST_STATE_CHASE:
                    set_ghost_chase_behavior(ghost, type);
                    break;
                case GHOST_STATE_EATEN:
                    // Make the ghost target the ghost house gate
                    ghost->target.x = 16;
                    ghost->target.y = 8;
                    break;
                case GHOST_STATE_SCATTER:
                default:
                    set_ghost_default_behavior(ghost, type);
                    break;
            }
        } else {
            // Frightened ghosts pick a random direction
            // whenever they're at an intersection
            uint32_t potential_targets = 0;
            MovementDirection directions[4] = {0};

            for(int32_t i = 0; i < 4; i++) {
                TileCoord new_coord = {
                    .x = ghost->entity.coord.x + (int32_t)direction_vectors[i].x,
                    .y = ghost->entity.coord.y + (int32_t)direction_vectors[i].y
                };

                if(!tile_is_wall(&new_coord, ghost) && dot_product(i, ghost->entity.dir) >= 0.0f) {
                    potential_targets++;
                    directions[i] = (MovementDirection)i;
                }
            }

            if(potential_targets > 0) {
                int32_t index = rand() % (potential_targets + 1);
                ghost->target.x = ghost->entity.coord.x + (int32_t)direction_vectors[index].x;
                ghost->target.y = ghost->entity.coord.y + (int32_t)direction_vectors[index].y;
            }
        }
    }
}

void set_ghost_speed(GhostEntity *ghost) {
    if(ghost) {
        if(ghost->state == GHOST_STATE_EATEN) {
            ghost->entity.speed = DEFAULT_MOVEMENT_SPEED;
        } else if(ghost->frightened) {
            ghost->entity.speed = ghost->entity.default_speed * FRIGHTENED_SPEED_MOD;
        } else {
            ghost->entity.speed = ghost->entity.default_speed;
        }
    }
}

void wrap_tile_coords(TileCoord *coord, bool outside_area) {
    if(coord) {
        int32_t min, max_x, max_y;
        if(outside_area) {
            min = -1;
            max_x = LAST_TILE_X + 1;
            max_y = LAST_TILE_Y + 1;
        } else {
            min = 0;
            max_x = LAST_TILE_X;
            max_y = LAST_TILE_Y;
        }

        if(coord->x < min) {
            coord->x = max_x;
        } else if(coord->x > max_x) {
            coord->x = min;
        }

        if(coord->y < min) {
            coord->y = max_y;
        } else if(coord->y > max_y) {
            coord->y = min;
        }
    }
}

void tile_coords_from_direction(TileCoord *coord, MovementDirection direction) {
    if(coord && direction != MOVEMENT_DIR_NONE) {
        switch(direction) {
            case MOVEMENT_DIR_UP: coord->y--; break;
            case MOVEMENT_DIR_LEFT: coord->x--; break;
            case MOVEMENT_DIR_RIGHT: coord->x++; break;
            case MOVEMENT_DIR_DOWN: coord->y++; break;
        }

        wrap_tile_coords(coord, true);
    }
}

void render_loop(void) {
    Color *fb = get_framebuffer();
    memset(fb, 0, sizeof(*fb) * DEFAULT_FRAMEBUFFER_WIDTH * DEFAULT_FRAMEBUFFER_HEIGHT);

    glUniform3f(glGetUniformLocation(game_state.gl_program, "camera"),
                game_camera.offset.x, game_camera.offset.y, game_camera.zoom);

    // Level
    for(int32_t y = 0; y < TILE_COUNT_Y; y++) {
        for(int32_t x = 0; x < TILE_COUNT_X; x++) {
            TileCoord coord = { .x = x, .y = y };
            uint32_t tile = get_level_tile_data(game_state.level, &coord);

#define ASSIGN_TILE_TYPE(type) case TILE_TYPE_##type: sprite = &atlas_sprites[ATLAS_SPRITE_##type]; break
            const Rect *sprite = NULL;
            switch(tile) {
                ASSIGN_TILE_TYPE(WALL);
                ASSIGN_TILE_TYPE(PELLET);
                ASSIGN_TILE_TYPE(POWER_PELLET);
                ASSIGN_TILE_TYPE(GHOST_HOUSE_GATE);
                default: break;
            }
#undef ASSIGN_TILE_TYPE

            if(sprite) {
                blit_texture(game_state.atlas, x * TILE_SIZE - game_camera.scroll.x,
                             y * TILE_SIZE - game_camera.scroll.y, sprite, NULL);
            }
        }
    }

    Rect rdest;
    // Ghosts
    for(int32_t i = 0; i < GHOST_COUNT; i++) {
        tilecoord_to_rect(&game_state.ghosts[i].entity.coord, &rdest, 1.0f);
        int32_t atlas_index = 0;
        if(game_state.ghosts[i].frightened) {
            switch(i) {
                case RED_GHOST: atlas_index = ATLAS_SPRITE_RED_GHOST_FRIGHTENED; break;
                case PINK_GHOST: atlas_index = ATLAS_SPRITE_PINK_GHOST_FRIGHTENED; break;
                case ORANGE_GHOST: atlas_index = ATLAS_SPRITE_ORANGE_GHOST_FRIGHTENED; break;
                case CYAN_GHOST: atlas_index = ATLAS_SPRITE_CYAN_GHOST_FRIGHTENED; break;
            }
        } else if(game_state.ghosts[i].state == GHOST_STATE_EATEN) {
            atlas_index = ATLAS_SPRITE_GHOST_EATEN_UP + game_state.ghosts[i].entity.dir;
        } else {
            switch(i) {
                case RED_GHOST: atlas_index = ATLAS_SPRITE_RED_GHOST_UP + game_state.ghosts[i].entity.dir; break;
                case PINK_GHOST: atlas_index = ATLAS_SPRITE_PINK_GHOST_UP + game_state.ghosts[i].entity.dir; break;
                case ORANGE_GHOST: atlas_index = ATLAS_SPRITE_ORANGE_GHOST_UP + game_state.ghosts[i].entity.dir; break;
                case CYAN_GHOST: atlas_index = ATLAS_SPRITE_CYAN_GHOST_UP + game_state.ghosts[i].entity.dir; break;
            }
        }

        blit_texture(game_state.atlas, rdest.x - game_camera.scroll.x,
                     rdest.y - game_camera.scroll.y, &atlas_sprites[atlas_index], NULL);
    }

    // Player
    float sub = MAX(fabsf(game_state.player.entity.coord.sub.x), fabsf(game_state.player.entity.coord.sub.y));
    tilecoord_to_rect(&game_state.player.entity.coord, &rdest, 1.0f);

    int32_t player_x, player_y;
    if(game_camera.scroll.x <= 0) {
        player_x = rdest.x;
    } else if((game_camera.scroll.x + DEFAULT_FRAMEBUFFER_WIDTH) >= (TILE_COUNT_X * TILE_SIZE)) {
        player_x = DEFAULT_FRAMEBUFFER_WIDTH - (TILE_COUNT_X * TILE_SIZE - rdest.x);
    } else {
        player_x = DEFAULT_FRAMEBUFFER_WIDTH / 2;
    }

    if(game_camera.scroll.y <= 0) {
        player_y = rdest.y;
    } else if((game_camera.scroll.y + DEFAULT_FRAMEBUFFER_HEIGHT) >= (TILE_COUNT_Y * TILE_SIZE)) {
        player_y = DEFAULT_FRAMEBUFFER_HEIGHT - (TILE_COUNT_Y * TILE_SIZE - rdest.y);
    } else {
        player_y = DEFAULT_FRAMEBUFFER_HEIGHT / 2;
    }

    static float r = 0.0f;
    r += 0.005f;
    Matrix3x3 transform = get_rotation_mat3(r);
    float s = sinf(r) + 1.5f;
    Matrix3x3 scale = get_scaling_mat3(s, s);
    transform = mat3_mul(&transform, &scale);

    blit_texture(game_state.atlas, player_x, player_y,
                 (sub >= 0.25f && sub < 0.75f) ?
                 &atlas_sprites[ATLAS_SPRITE_PLAYER_F2] :
                 &atlas_sprites[ATLAS_SPRITE_PLAYER_F1], &transform);

    // OpenGL stuff
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, DEFAULT_FRAMEBUFFER_WIDTH,
                    DEFAULT_FRAMEBUFFER_HEIGHT, GL_RGBA, GL_FLOAT, get_framebuffer());
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

#undef EPSILON
#undef DEFAULT_MOVEMENT_SPEED
#undef FRIGHTENED_SPEED_MOD
#undef SCATTER_MODE_TIME
#undef CHASE_MODE_TIME
#undef FRIGHTENED_MODE_TIME
#undef INPUT_QUEUE_TIME_MAX
