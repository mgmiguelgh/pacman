/*
 * Pacman Clone
 *
 * Copyright (c) 2021 Amanch Esmailzadeh
 */

#ifdef _WIN32
#include <gl/gl.h>
#elif __linux__
#include <GL/gl.h>
#endif
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>

#include "game.h"

#include "level.c"
#include "texture.c"
#include "render.c"

#define EPSILON 0.05f
#define DEFAULT_MOVEMENT_SPEED 5.0f
#define FRIGHTENED_SPEED_MOD 0.65f
#define SCATTER_MODE_TIME 5.0f
#define CHASE_MODE_TIME 20.0f
#define FRIGHTENED_MODE_TIME 10.0f
#define INPUT_QUEUE_TIME_MAX 0.5f
#define DEFAULT_EATEN_ANIM_TIMER_TARGET 1.0f

static const Vector2 direction_vectors[4] = {
    { .x = 0.0f, .y = -1.0f }, // Up
    { .x = -1.0f, .y = 0.0f }, // Left
    { .x = 0.0f, .y = 1.0f },  // Down
    { .x = 1.0f, .y = 0.0f },  // Right
};

static struct {
    enum MenuItem {
        MENU_ITEM_CONTINUE,
        MENU_ITEM_EXIT,

        MENU_ITEM_COUNT
    } id;
    const char *str;
} menu_items[MENU_ITEM_COUNT] = {
    { .id = MENU_ITEM_CONTINUE, "CONTINUE" },
    { .id = MENU_ITEM_EXIT, "EXIT" }
};

#define LIVES_COUNT_START   3
#define LIVES_MAX           9

#define SCORE_PELLET_EATEN          10
#define SCORE_POWER_PELLET_EATEN    100
#define SCORE_GHOST_EATEN           500
#define SCORE_GHOST_EXTRA_LIFE      10000
#define SCORE_MAX                   99999

typedef enum {
    GAME_STATE_EMPTY,
    GAME_STATE_NORMAL,
    GAME_STATE_READY,
    GAME_STATE_MENU
} GameState;

static struct {
    Texture2D *atlas;
    Level *level;

    GameState previous_state;
    GameState current_state;

    GhostEntity ghosts[GHOST_COUNT];
    PlayerEntity player;

    Timer ready_timer;
    Timer ghost_mode_timer;
    Timer frightened_timer;

    int32_t lives;
    uint32_t score;

    enum MenuItem selected_menu_id;

    enum {
        GAME_MODE_SCATTER,
        GAME_MODE_CHASE
    } mode;
} game_data = { 0 };

static inline void set_game_state(GameState state) {
    switch(state) {
        case GAME_STATE_READY:
            game_data.ready_timer.running = true;
            game_data.ready_timer.elapsed = 0.0f;
            game_data.ready_timer.target = 3.0f;
            break;
        default:
            break;
    }
    game_data.previous_state = game_data.current_state;
    game_data.current_state = state;
}

static inline void increase_player_score(uint32_t points) {
    uint32_t value = game_data.score / SCORE_GHOST_EXTRA_LIFE;
    game_data.score += points;
    game_data.score = MIN(SCORE_MAX, game_data.score);

    if((game_data.score / SCORE_GHOST_EXTRA_LIFE) > value) {
        game_data.lives++;
        game_data.lives = MIN(LIVES_MAX, game_data.lives);
    }
}

static GLuint gl_program;

static struct {
    Vector2i scroll;
    Vector2 offset;
    float zoom;
} game_camera;

static inline void camera_set_default_offset(void) {
    game_camera.offset.x = 0.5f;
    game_camera.offset.y = 0.5f;
}

// Obligatory camera shake function
static inline void camera_shake(void) {
    const float SHAKE_FACTOR = 0.0025f;

    camera_set_default_offset();

    Vector2 intensity = {
        .x = LERP(((float)(rand() % RAND_MAX) / (float)RAND_MAX) * SHAKE_FACTOR, -SHAKE_FACTOR, SHAKE_FACTOR),
        .y = LERP(((float)(rand() % RAND_MAX) / (float)RAND_MAX) * SHAKE_FACTOR, -SHAKE_FACTOR, SHAKE_FACTOR)
    };

    game_camera.offset.x += intensity.x;
    game_camera.offset.y += intensity.y;
}

#define TILE_COUNT_X (int32_t)game_data.level->columns
#define TILE_COUNT_Y (int32_t)game_data.level->rows
#define LAST_TILE_X (int32_t)(game_data.level->columns - 1)
#define LAST_TILE_Y (int32_t)(game_data.level->rows - 1)

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
    return fabsf(a - b) <= EPSILON;
}

static inline float dot_product(MovementDirection dir1, MovementDirection dir2) {
    assert(dir1 != MOVEMENT_DIR_NONE && dir2 != MOVEMENT_DIR_NONE);
    return direction_vectors[dir1].x * direction_vectors[dir2].x +
        direction_vectors[dir1].y * direction_vectors[dir2].y;
}

static inline float pellets_eaten_percentage(void) {
    return (float)game_data.level->pellets_eaten / (float)game_data.level->pellet_count;
}

static inline bool ghost_can_pass_gate(GhostEntity *ghost) {
    return ghost && (ghost->state == GHOST_STATE_EATEN ||
        (ghost->in_ghost_house && ghost->gate_pass_percentage <= pellets_eaten_percentage()));
}

static bool tile_is_wall(const TileCoord *coord, GhostEntity *ghost) {
    if(coord && coords_within_bounds(coord)) {
        uint32_t type = get_level_tile_data(game_data.level, coord);

        if(ghost_can_pass_gate(ghost)) {
            ghost->target = game_data.level->gate_tile;
            return type == ATLAS_SPRITE_WALL_NORMAL || type == ATLAS_SPRITE_WALL_BOTTOM;
        }

        return type == ATLAS_SPRITE_WALL_NORMAL || type == ATLAS_SPRITE_WALL_BOTTOM || type == ATLAS_SPRITE_GHOST_HOUSE_GATE;
    }

    return false;
}

static Vector2i get_entity_abs_position(const GameEntity *entity) {
    const float TILE_SIZE_F = (float)TILE_SIZE;

    if(entity) {
        return (Vector2i) {
            .x = (entity->coord.x * TILE_SIZE) + (int32_t)(entity->coord.sub.x * TILE_SIZE_F),
            .y = (entity->coord.y * TILE_SIZE) + (int32_t)(entity->coord.sub.y * TILE_SIZE_F)
        };
    }

    return (Vector2i) { 0 };
}

// Function prototypes
static void handle_player_ghosts_collisions(float dt);
static void tilecoord_to_rect(const TileCoord *coord, Rect *rect, float scale);
static void move_entity(GameEntity *entity, TileCoord *destination, float dt);
static void set_ghost_behavior(GhostEntity *ghost, uint32_t type);
static void set_ghost_speed(GhostEntity *ghost);
static void wrap_tile_coords(TileCoord *coord, bool outside_area);
static void tile_coords_from_direction(TileCoord *coord, MovementDirection direction);

static void set_starting_data(void) {
    memset(&game_data.player, 0, sizeof(game_data.player));
    memset(&game_data.ghosts, 0, sizeof(game_data.ghosts));

    game_data.player.input_queue.target = INPUT_QUEUE_TIME_MAX;
    game_data.player.entity.dir = MOVEMENT_DIR_NONE;
    game_data.player.entity.facing = MOVEMENT_DIR_RIGHT;
    game_data.player.entity.default_speed = DEFAULT_MOVEMENT_SPEED;
    game_data.player.entity.speed = DEFAULT_MOVEMENT_SPEED;

    for(int32_t i = 0; i < GHOST_COUNT; i++) {
        game_data.ghosts[i].entity.dir = MOVEMENT_DIR_UP;
        game_data.ghosts[i].entity.facing = MOVEMENT_DIR_RIGHT;
        game_data.ghosts[i].state = GHOST_STATE_SCATTER;
        game_data.ghosts[i].entity.coord.sub.x = 0.0f;
        game_data.ghosts[i].entity.coord.sub.y = 0.0f;
        game_data.ghosts[i].in_ghost_house = true;
        game_data.ghosts[i].eaten_anim_timer.running = false;
        game_data.ghosts[i].eaten_anim_timer.elapsed = 0.0f;
        game_data.ghosts[i].eaten_anim_timer.target = DEFAULT_EATEN_ANIM_TIMER_TARGET;
    }

    game_data.ghosts[GHOST_BLINKY].entity.dir = MOVEMENT_DIR_RIGHT;
    game_data.ghosts[GHOST_BLINKY].gate_pass_percentage = 0.0f;
    game_data.ghosts[GHOST_BLINKY].in_ghost_house = false;
    game_data.ghosts[GHOST_PINKY].gate_pass_percentage = 0.15f;
    game_data.ghosts[GHOST_CLYDE].gate_pass_percentage = 0.5f;
    game_data.ghosts[GHOST_INKY].gate_pass_percentage = 0.3f;

    game_data.ghosts[GHOST_BLINKY].entity.default_speed = DEFAULT_MOVEMENT_SPEED - 0.25f;
    game_data.ghosts[GHOST_BLINKY].entity.speed = game_data.ghosts[GHOST_BLINKY].entity.default_speed;
    game_data.ghosts[GHOST_PINKY].entity.default_speed = DEFAULT_MOVEMENT_SPEED - 0.5f;
    game_data.ghosts[GHOST_PINKY].entity.speed = game_data.ghosts[GHOST_PINKY].entity.default_speed;
    game_data.ghosts[GHOST_CLYDE].entity.default_speed = DEFAULT_MOVEMENT_SPEED - 1.0f;
    game_data.ghosts[GHOST_CLYDE].entity.speed = game_data.ghosts[GHOST_CLYDE].entity.default_speed;
    game_data.ghosts[GHOST_INKY].entity.default_speed = DEFAULT_MOVEMENT_SPEED - 0.75f;
    game_data.ghosts[GHOST_INKY].entity.speed = game_data.ghosts[GHOST_INKY].entity.default_speed;

    game_camera.scroll.x = 0;
    game_camera.scroll.y = 0;
    camera_set_default_offset();
    game_camera.zoom = 1.0f;

    set_game_state(GAME_STATE_READY);

    game_data.player.entity.coord = game_data.level->player_start;
    game_data.ghosts[GHOST_BLINKY].entity.coord = game_data.level->ghost_start[GHOST_BLINKY];
    game_data.ghosts[GHOST_PINKY].entity.coord = game_data.level->ghost_start[GHOST_PINKY];
    game_data.ghosts[GHOST_CLYDE].entity.coord = game_data.level->ghost_start[GHOST_CLYDE];
    game_data.ghosts[GHOST_INKY].entity.coord = game_data.level->ghost_start[GHOST_INKY];
}

static void start_next_level(bool reset) {
    if(!game_data.atlas) {
        game_data.atlas = load_texture("data/texture_atlas.bmp", 0xff00ff);
    }

    game_data.mode = GAME_MODE_SCATTER;
    game_data.ghost_mode_timer.running = true;
    game_data.ghost_mode_timer.elapsed = 0.0f;
    game_data.ghost_mode_timer.target = SCATTER_MODE_TIME;
    game_data.frightened_timer.running = false;
    game_data.frightened_timer.elapsed = 0.0f;
    game_data.frightened_timer.target = FRIGHTENED_MODE_TIME;

    if(game_data.level) {
        unload_level(&game_data.level);
    }

    game_data.level = reset ? load_first_level() : load_next_level();
    set_starting_data();
}

static void reset_game(void) {
    game_data.lives = LIVES_COUNT_START;
    game_data.score = 0;

    start_next_level(true);
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

static void resize_window(int32_t new_width, int32_t new_height) {
    new_width = MAX(DEFAULT_WINDOW_WIDTH, new_width);
    new_height = MAX(DEFAULT_WINDOW_HEIGHT, new_height);

    float aspect_ratio = (float)DEFAULT_FRAMEBUFFER_WIDTH / (float)DEFAULT_FRAMEBUFFER_HEIGHT;
    float w = (float)new_width;
    float h = w / aspect_ratio;

    if(h > new_height) {
        h = (float)new_height;
        w = h * aspect_ratio;
    }

    GLint x = (new_width - (GLint)w) / 2;
    GLint y = (new_height - (GLint)h) / 2;

    glViewport(x, y, (GLsizei)w, (GLsizei)h);
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
    gl_program = glCreateProgram();
    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

    glShaderSource(vertex_shader, 1, &vertex_shader_source, NULL);
    glShaderSource(fragment_shader, 1, &fragment_shader_source, NULL);

    glCompileShader(vertex_shader);
    check_gl_status(vertex_shader, GL_COMPILE_STATUS);
    glCompileShader(fragment_shader);
    check_gl_status(fragment_shader, GL_COMPILE_STATUS);

    glAttachShader(gl_program, vertex_shader);
    glAttachShader(gl_program, fragment_shader);

    glLinkProgram(gl_program);
    check_gl_status(gl_program, GL_LINK_STATUS);

    glUseProgram(gl_program);

    glDetachShader(gl_program, vertex_shader);
    glDetachShader(gl_program, fragment_shader);

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

    resize_window(DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
}

void initialize_game(void) {
    initialize_opengl();
    init_levels();
    reset_game();
}

void close_game(void) {
    destroy_texture(&game_data.atlas);
    close_levels();
    unload_level(&game_data.level);
}

static struct {
    int32_t width;
    int32_t height;
    bool should_resize;
} window_box = { 0 };

void signal_window_resize(int32_t new_width, int32_t new_height) {
    window_box.should_resize = true;
    window_box.width = new_width;
    window_box.height = new_height;
}

bool update_timer(Timer *timer, float ms) {
    if(timer && timer->running) {
        timer->elapsed += ms;

        if(timer->elapsed >= timer->target) {
            timer->elapsed = 0.0f;
            timer->running = false;
            return true;
        }
    }

    return false;
}

static void update_camera_pos(void) {
    Vector2i abs_pos = get_entity_abs_position(&game_data.player.entity);
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

static void update_ghost_timers(float dt) {
    if(update_timer(&game_data.ghost_mode_timer, dt)) {
        game_data.ghost_mode_timer.elapsed = 0.0f;
        game_data.ghost_mode_timer.running = true;

        if(game_data.mode == GAME_MODE_SCATTER) {
            game_data.mode = GAME_MODE_CHASE;
            game_data.ghost_mode_timer.target = CHASE_MODE_TIME;
            for(int32_t i = 0; i < GHOST_COUNT; i++) {
                if(game_data.ghosts[i].state == GHOST_STATE_SCATTER) {
                    game_data.ghosts[i].state = GHOST_STATE_CHASE;
                }
            }
        } else {
            game_data.mode = GAME_MODE_SCATTER;
            game_data.ghost_mode_timer.target = SCATTER_MODE_TIME;
            for(int32_t i = 0; i < GHOST_COUNT; i++) {
                if(game_data.ghosts[i].state == GHOST_STATE_CHASE) {
                    game_data.ghosts[i].state = GHOST_STATE_SCATTER;
                }
            }
        }
    }

    if(update_timer(&game_data.frightened_timer, dt)) {
        for(int32_t i = 0; i < GHOST_COUNT; i++) {
            game_data.ghosts[i].frightened = false;
        }
    }
}

#define INPUT_PRESS(action) ((input & action) && !(game_data.player.prev_input & action))

bool update_loop(float dt, uint32_t input) {
    if(window_box.should_resize) {
        resize_window(window_box.width, window_box.height);
        window_box.should_resize = false;
    }

    switch(game_data.current_state) {
        case GAME_STATE_READY:
            update_camera_pos();
            if(update_timer(&game_data.ready_timer, dt)) {
                set_game_state(GAME_STATE_NORMAL);
                break;
            }
            return true;
        case GAME_STATE_NORMAL:
            if(INPUT_PRESS(INPUT_MENU)) {
                set_game_state(GAME_STATE_MENU);
            }
            break;
        case GAME_STATE_MENU:
            if(INPUT_PRESS(INPUT_MENU)) {
                set_game_state(game_data.previous_state);
                break;
            } else if(INPUT_PRESS(INPUT_UP)) {
                game_data.selected_menu_id = (game_data.selected_menu_id - 1) % MENU_ITEM_COUNT;
                game_data.selected_menu_id = game_data.selected_menu_id + ((game_data.selected_menu_id < 0) ? MENU_ITEM_COUNT : 0);
            } else if(INPUT_PRESS(INPUT_DOWN)) {
                game_data.selected_menu_id = (game_data.selected_menu_id + 1) % MENU_ITEM_COUNT;
            }
            if(INPUT_PRESS(INPUT_CONFIRM)) {
                switch(game_data.selected_menu_id) {
                    case MENU_ITEM_CONTINUE:
                        set_game_state(game_data.previous_state);
                        break;
                    case MENU_ITEM_EXIT:
                        return false;
                    default:
                        break;
                }
            }
            game_data.player.prev_input = input;
            return true;
        default:
            break;
    }

    camera_set_default_offset();
    update_ghost_timers(dt);

    if(input == 0) {
        game_data.player.prev_input &= ~INPUT_MENU; // Unset, because the input for bringing up the menu shouldn't be held on to
        if(game_data.player.prev_input != 0 && !update_timer(&game_data.player.input_queue, dt)) {
            input = game_data.player.prev_input;
        } else {
            game_data.player.prev_input = 0;
        }
    } else {
        game_data.player.prev_input = input;
        game_data.player.input_queue.elapsed = 0.0f;
        game_data.player.input_queue.running = true;
    }

#define MAP_INPUT_TO_MOV_DIR(d) if(input & INPUT_##d) { game_data.player.entity.dir = MOVEMENT_DIR_##d; }
    MovementDirection previous_dir = game_data.player.entity.dir;

    MAP_INPUT_TO_MOV_DIR(UP);
    MAP_INPUT_TO_MOV_DIR(DOWN);
    MAP_INPUT_TO_MOV_DIR(LEFT);
    MAP_INPUT_TO_MOV_DIR(RIGHT);

#undef MAP_INPUT_TO_MOV_DIR

    if(previous_dir != MOVEMENT_DIR_NONE) {
        if(coords_within_bounds(&game_data.player.entity.coord)) {
            bool is_quarter_turn = float_nearly_equal(dot_product(previous_dir, game_data.player.entity.dir), 0.0f);
            if(is_quarter_turn) {
                bool near_center = float_nearly_equal(game_data.player.entity.coord.sub.x, 0.0f) &&
                    float_nearly_equal(game_data.player.entity.coord.sub.y, 0.0f);

                if(near_center) {
                    TileCoord projected = game_data.player.entity.coord;
                    tile_coords_from_direction(&projected, game_data.player.entity.dir);
                    if(tile_is_wall(&projected, NULL)) {
                        // Make sure we're not stopped by walls if we try to
                        // move 90 degrees clockwise or counterclockwise
                        game_data.player.entity.dir = previous_dir;
                    }
                } else {
                    game_data.player.entity.dir = previous_dir;
                }
            }
        } else {
            game_data.player.entity.dir = previous_dir;
        }

        game_data.player.entity.facing = game_data.player.entity.dir;
    }

    TileCoord next_tile;
    move_entity(&game_data.player.entity, &next_tile, dt);
    if(tile_is_wall(&next_tile, NULL)) {
        Rect player_rect, wall_rect;
        tilecoord_to_rect(&game_data.player.entity.coord, &player_rect, 1.0f);
        tilecoord_to_rect(&next_tile, &wall_rect, 1.0f);

        if(rect_aabb_test(&player_rect, &wall_rect)) {
            wrap_tile_coords(&game_data.player.entity.coord, true);
            game_data.player.prev_input = 0;
            game_data.player.input_queue.elapsed = 0.0f;

            game_data.player.entity.coord.sub.x = 0.0f;
            game_data.player.entity.coord.sub.y = 0.0f;
            game_data.player.entity.dir = MOVEMENT_DIR_NONE;
        }
    }

    uint32_t *player_tile = get_level_tile(game_data.level, game_data.player.entity.coord.x,
                                           game_data.player.entity.coord.y);
    if(player_tile) {
        if(*player_tile == ATLAS_SPRITE_PELLET) {
            game_data.level->pellets_eaten++;
            *player_tile = ATLAS_SPRITE_EMPTY;

            increase_player_score(SCORE_PELLET_EATEN);
            camera_shake();
        } else if(*player_tile == ATLAS_SPRITE_POWER_PELLET) {
            game_data.level->pellets_eaten++;
            *player_tile = ATLAS_SPRITE_EMPTY;

            game_data.frightened_timer.running = true;
            game_data.frightened_timer.elapsed = 0.0f;

            for(int32_t i = 0; i < GHOST_COUNT; i++) {
                if(game_data.ghosts[i].state != GHOST_STATE_EATEN) {
                    game_data.ghosts[i].frightened = true;
                }
            }

            increase_player_score(SCORE_POWER_PELLET_EATEN);
            camera_shake();
        }

        if(game_data.level->pellets_eaten == game_data.level->pellet_count) {
            start_next_level(false);
        }
    }

    for(int32_t i = 0; i < GHOST_COUNT; i++) {
        TileCoord old_tile = game_data.ghosts[i].entity.coord;
        set_ghost_speed(&game_data.ghosts[i]);
        move_entity(&game_data.ghosts[i].entity, NULL, dt);

        MovementDirection ghost_dir = game_data.ghosts[i].entity.dir;

        if(ghost_dir == MOVEMENT_DIR_NONE ||
           (old_tile.x == game_data.ghosts[i].entity.coord.x &&
            old_tile.y == game_data.ghosts[i].entity.coord.y)) {
            continue;
        }

        game_data.ghosts[i].target.sub.x = 0.0f;
        game_data.ghosts[i].target.sub.y = 0.0f;

        TileCoord current = game_data.ghosts[i].entity.coord;
        if(ghost_can_pass_gate(&game_data.ghosts[i]) &&
           get_level_tile_data(game_data.level, &current) == ATLAS_SPRITE_GHOST_HOUSE_GATE) {

            game_data.ghosts[i].in_ghost_house = game_data.ghosts[i].state == GHOST_STATE_EATEN;
            if(game_data.ghosts[i].in_ghost_house) {
                game_data.ghosts[i].eaten_anim_timer.running = false;
                game_data.ghosts[i].eaten_anim_timer.elapsed = 0.0f;
            }

            game_data.ghosts[i].state = (game_data.mode == GAME_MODE_SCATTER) ?
                GHOST_STATE_SCATTER : GHOST_STATE_CHASE;
        }

        set_ghost_behavior(&game_data.ghosts[i], i);

        MovementDirection best_dir = MOVEMENT_DIR_NONE;
        int32_t dist = INT_MAX;
        for(int32_t dir = 0; dir < 4; dir++) {
            if(dot_product(ghost_dir, dir) >= 0.0f) {
                int32_t next_x = game_data.ghosts[i].entity.coord.x + (int32_t)direction_vectors[dir].x;
                int32_t next_y = game_data.ghosts[i].entity.coord.y + (int32_t)direction_vectors[dir].y;

                TileCoord temp = {
                    .x = next_x,
                    .y = next_y
                };

                if(next_x >= 0 && next_x < TILE_COUNT_X &&
                   next_y >= 0 && next_y < TILE_COUNT_Y &&
                   !tile_is_wall(&temp, &game_data.ghosts[i])) {

                    int32_t dist_x = game_data.ghosts[i].target.x - next_x;
                    int32_t dist_y = game_data.ghosts[i].target.y - next_y;
                    int32_t squared_dist = dist_x * dist_x + dist_y * dist_y;

                    if(squared_dist < dist) {
                        dist = squared_dist;
                        best_dir = (MovementDirection)dir;
                    }
                }
            }
        }

        if(best_dir != MOVEMENT_DIR_NONE) {
            game_data.ghosts[i].entity.dir = best_dir;
            if(best_dir == MOVEMENT_DIR_LEFT ||
               best_dir == MOVEMENT_DIR_RIGHT) {
                game_data.ghosts[i].entity.facing = best_dir;
            }
        }
    }

    handle_player_ghosts_collisions(dt);
    update_camera_pos();

    return true;
}

void handle_player_ghosts_collisions(float dt) {
    Rect player_rect;
    tilecoord_to_rect(&game_data.player.entity.coord, &player_rect, 0.75f);
    for(int32_t i = 0; i < GHOST_COUNT; i++) {
        GhostEntity *ghost = &game_data.ghosts[i];
        Rect ghost_rect;
        tilecoord_to_rect(&ghost->entity.coord, &ghost_rect, 0.75f);

        if(update_timer(&ghost->eaten_anim_timer, dt) && ghost->state == GHOST_STATE_EATEN) {
            ghost->eaten_anim_timer.elapsed = 0.0f;
            ghost->eaten_anim_timer.running = true;
        }

        if(rect_aabb_test(&player_rect, &ghost_rect)) {
            if(ghost->frightened) {
                ghost->frightened = false;
                ghost->state = GHOST_STATE_EATEN;
                ghost->eaten_anim_timer.running = true;

                increase_player_score(SCORE_GHOST_EATEN);
                camera_shake();
            } else if(game_data.ghosts[i].state != GHOST_STATE_EATEN) {
                tilecoord_to_rect(&game_data.player.entity.coord, &player_rect, 0.35f);
                tilecoord_to_rect(&ghost->entity.coord, &ghost_rect, 0.35f);

                if(rect_aabb_test(&player_rect, &ghost_rect)) {
                    game_data.lives--;
                    if(game_data.lives >= 0) {
                        set_starting_data();
                    } else {
                        reset_game();
                    }
                }
            }
        }
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
        rect->width = rect->height = (uint32_t)(TILE_SIZE_F * scale);
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
        case GHOST_BLINKY:
            ghost->target = game_data.player.entity.coord;
            break;
        case GHOST_PINKY:
            ghost->target.x = game_data.player.entity.coord.x +
                ((int32_t)direction_vectors[game_data.player.entity.dir].x * 4);
            ghost->target.y = game_data.player.entity.coord.y +
                ((int32_t)direction_vectors[game_data.player.entity.dir].y * 4);
            break;
        case GHOST_CLYDE: {
            int32_t dist_x = game_data.player.entity.coord.x - ghost->entity.coord.x;
            int32_t dist_y = game_data.player.entity.coord.y - ghost->entity.coord.y;

            if((dist_x * dist_x + dist_y * dist_y) >= 8) {
                ghost->target = game_data.player.entity.coord;
            } else {
                ghost->target.x = 8;
                ghost->target.y = TILE_COUNT_Y;
            }

            break;
        }
        case GHOST_INKY:
            ghost->target.x = game_data.player.entity.coord.x +
                ((int32_t)direction_vectors[game_data.player.entity.dir].x * 2);
            ghost->target.y = game_data.player.entity.coord.y +
                ((int32_t)direction_vectors[game_data.player.entity.dir].y * 2);

            ghost->target.x += (ghost->target.x - game_data.ghosts[GHOST_BLINKY].entity.coord.x);
            ghost->target.y += (ghost->target.y - game_data.ghosts[GHOST_BLINKY].entity.coord.y);

            break;
        default:
            break;
    }
}

static void set_ghost_default_behavior(GhostEntity *ghost, uint32_t type) {
    assert(ghost);

    switch(type) {
        case GHOST_BLINKY:
            ghost->target.x = LAST_TILE_X - 1;
            ghost->target.y = 1;
            break;
        case GHOST_PINKY:
            ghost->target.x = 1;
            ghost->target.y = 1;
            break;
        case GHOST_CLYDE:
            ghost->target.x = 1;
            ghost->target.y = LAST_TILE_Y - 1;
            break;
        case GHOST_INKY:
            ghost->target.x = LAST_TILE_X - 1;
            ghost->target.y = LAST_TILE_Y - 1;
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
                    ghost->target = game_data.level->gate_tile;
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
            default: break;
        }

        wrap_tile_coords(coord, true);
    }
}

static inline AtlasSprite get_entity_frame(GameEntity *entity, AtlasSprite frame1, AtlasSprite frame2) {
    assert(entity);

    float sub = MAX(fabsf(entity->coord.sub.x), fabsf(entity->coord.sub.y));
    return (sub < 0.25f || sub >= 0.75f) ? frame1 : frame2;
}

static inline AtlasSprite get_ghost_eaten_frame(GhostEntity *ghost, AtlasSprite frame1, AtlasSprite frame2) {
    assert(ghost);

    float n = ghost->eaten_anim_timer.elapsed / ghost->eaten_anim_timer.target;
    if((n > 0.25f && n <= 0.5f) || (n > 0.75f)) {
        return frame2;
    } else if(n > 0.5f && n <= 0.75f) {
        return ATLAS_SPRITE_GHOST_EATEN_FRAME3;
    }

    return frame1;
}

void render_loop(float dt) {
    Color4 *fb = get_framebuffer();
    memset(fb, 0, sizeof(*fb) * DEFAULT_FRAMEBUFFER_WIDTH * DEFAULT_FRAMEBUFFER_HEIGHT);

    clear_spotlights();

    set_draw_intensity(game_data.current_state == GAME_STATE_READY ?
                       game_data.ready_timer.elapsed / game_data.ready_timer.target : 1.0f);

    glUniform3f(glGetUniformLocation(gl_program, "camera"),
                game_camera.offset.x, game_camera.offset.y, game_camera.zoom);

    Rect sprite_rect = { 0 };

    float gradient;
    int32_t radius;
    {
        static float bias = 0.0f;
        bias += 0.75f * dt;
        if(bias >= M_PI_2) {
            bias -= M_PI_2;
        }
        float t = (sinf(bias) + 1.0f) * 0.5f;
        gradient = LERP(t, 1.5f, 3.0f);
        radius = (int32_t)(LERP(t, 30.0f, 45.f));
    }

    // Level
    for(int32_t y = 0; y < TILE_COUNT_Y; y++) {
        for(int32_t x = 0; x < TILE_COUNT_X; x++) {
            TileCoord coord = { .x = x, .y = y };
            AtlasSprite sprite = get_level_tile_data(game_data.level, &coord);
            get_atlas_sprite_rect(sprite, &sprite_rect);

            int32_t xpos = x * TILE_SIZE - game_camera.scroll.x;
            int32_t ypos = y * TILE_SIZE - game_camera.scroll.y;
            blit_texture(game_data.atlas, xpos, ypos, &sprite_rect, NULL);

            if(sprite == ATLAS_SPRITE_POWER_PELLET) {
                draw_spotlight(xpos + TILE_SIZE / 2, ypos + TILE_SIZE / 2, radius, gradient);
            }
        }
    }

    Rect rdest;
    // Ghosts
    for(int32_t i = 0; i < GHOST_COUNT; i++) {
        GhostEntity *ghost = &game_data.ghosts[i];
        tilecoord_to_rect(&ghost->entity.coord, &rdest, 1.0f);

#define SELECT_GHOST_SPRITE(ghost_name, f1, f2) \
        case GHOST_##ghost_name: \
            get_atlas_sprite_rect(get_entity_frame(&game_data.ghosts[GHOST_##ghost_name].entity, f1, f2), &sprite_rect); \
            break;

        if(ghost->frightened) {
            switch(i) {
                SELECT_GHOST_SPRITE(BLINKY, ATLAS_SPRITE_GHOST_FRIGHTENED_FRAME1, ATLAS_SPRITE_GHOST_FRIGHTENED_FRAME2);
                SELECT_GHOST_SPRITE(PINKY, ATLAS_SPRITE_GHOST_FRIGHTENED_FRAME1, ATLAS_SPRITE_GHOST_FRIGHTENED_FRAME2);
                SELECT_GHOST_SPRITE(CLYDE, ATLAS_SPRITE_GHOST_FRIGHTENED_FRAME1, ATLAS_SPRITE_GHOST_FRIGHTENED_FRAME2);
                SELECT_GHOST_SPRITE(INKY, ATLAS_SPRITE_GHOST_FRIGHTENED_FRAME1, ATLAS_SPRITE_GHOST_FRIGHTENED_FRAME2);
            }
        } else if(ghost->state == GHOST_STATE_EATEN) {
            AtlasSprite base = ATLAS_SPRITE_GHOST_EATEN_UP_FRAME1 + (ghost->entity.dir * 2);
            get_atlas_sprite_rect(get_ghost_eaten_frame(ghost, base, base + 1), &sprite_rect);
        } else {
            switch(i) {
                SELECT_GHOST_SPRITE(BLINKY, ATLAS_SPRITE_BLINKY_FRAME1, ATLAS_SPRITE_BLINKY_FRAME2);
                SELECT_GHOST_SPRITE(PINKY, ATLAS_SPRITE_PINKY_FRAME1, ATLAS_SPRITE_PINKY_FRAME2);
                SELECT_GHOST_SPRITE(CLYDE, ATLAS_SPRITE_CLYDE_FRAME1, ATLAS_SPRITE_CLYDE_FRAME2);
                SELECT_GHOST_SPRITE(INKY, ATLAS_SPRITE_INKY_FRAME1, ATLAS_SPRITE_INKY_FRAME2);
            }
        }

#undef SELECT_GHOST_SPRITE

        Matrix3x3 transform = get_scaling_mat3((ghost->state != GHOST_STATE_EATEN &&
                                                ghost->entity.facing == MOVEMENT_DIR_LEFT) ?
                                               -1.0f : 1.0f, 1.0f);
        blit_texture(game_data.atlas, rdest.x - game_camera.scroll.x,
                     rdest.y - game_camera.scroll.y, &sprite_rect, &transform);

        draw_spotlight(rdest.x - game_camera.scroll.x + TILE_SIZE / 2,
                       rdest.y - game_camera.scroll.y + TILE_SIZE / 2, radius, gradient);
    }

    // Player
    tilecoord_to_rect(&game_data.player.entity.coord, &rdest, 1.0f);

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

    Matrix3x3 transform;
    switch(game_data.player.entity.facing) {
        case MOVEMENT_DIR_UP:
            transform = get_rotation_mat3(M_PI * 0.5f);
            break;
        case MOVEMENT_DIR_LEFT:
            transform = get_scaling_mat3(-1.0f, 1.0f);
            break;
        case MOVEMENT_DIR_DOWN:
            transform = get_rotation_mat3(M_PI * 1.5f);
            break;
        case MOVEMENT_DIR_RIGHT:
        default:
            transform = get_identity_mat3();
            break;
    }

    get_atlas_sprite_rect(get_entity_frame(&game_data.player.entity, ATLAS_SPRITE_PLAYER_FRAME1, ATLAS_SPRITE_PLAYER_FRAME2),
                          &sprite_rect);
    blit_texture(game_data.atlas, player_x, player_y, &sprite_rect, &transform);

    draw_spotlight(player_x + TILE_SIZE / 2, player_y + TILE_SIZE / 2, radius * 2, gradient);
    submit_spotlights();

    // Score and lives
    int32_t xend = DEFAULT_FRAMEBUFFER_WIDTH - (TILE_SIZE * 3);
    int32_t ypos = DEFAULT_FRAMEBUFFER_HEIGHT - TILE_SIZE;

    draw_formatted_text(game_data.atlas, 16, ypos, "Score %d", game_data.score);

    get_atlas_sprite_rect(ATLAS_SPRITE_PLAYER_FRAME1, &sprite_rect);

    blit_texture(game_data.atlas, xend, ypos - TILE_SIZE_ALT, &sprite_rect, NULL);
    draw_formatted_text(game_data.atlas, xend + TILE_SIZE, ypos, "X%d", game_data.lives);

    switch(game_data.current_state) {
        case GAME_STATE_READY: {
            int32_t xoff = TILE_SIZE * 2;
            int32_t centerx = (DEFAULT_FRAMEBUFFER_WIDTH / 2) - (TILE_SIZE_ALT / 2);
            int32_t centery = (DEFAULT_FRAMEBUFFER_HEIGHT / 2) - (TILE_SIZE_ALT / 2);

            float intpart;
            float fract = modff(game_data.ready_timer.elapsed, &intpart);

            if(fract <= 0.5f) {
                set_draw_intensity(1.0f);
                draw_text(game_data.atlas, centerx - xoff, centery, "GET");
                draw_text(game_data.atlas, centerx + xoff, centery, "READY");
            }
            break;
        }
        case GAME_STATE_MENU: {
#define MENU_TILE_COUNT_WIDTH 10
#define MENU_TILE_COUNT_HEIGHT 3
            int32_t xstart = ((DEFAULT_FRAMEBUFFER_WIDTH / 2) - (TILE_SIZE_ALT * (MENU_TILE_COUNT_WIDTH / 2))) + (TILE_SIZE_ALT / 2);
            int32_t ystart = ((DEFAULT_FRAMEBUFFER_HEIGHT / 2) - (TILE_SIZE_ALT * (MENU_TILE_COUNT_HEIGHT / 2))) + (TILE_SIZE_ALT / 2);
            int32_t ystep = TILE_SIZE_ALT + (TILE_SIZE_ALT / 2);
            Rect r = {
                .x = xstart,
                .y = ystart,
                .width = TILE_SIZE_ALT * MENU_TILE_COUNT_WIDTH,
                .height = TILE_SIZE_ALT * MENU_TILE_COUNT_HEIGHT
            };

            // Draw the corners first
            get_atlas_sprite_rect(ATLAS_SPRITE_MENU_SLICE_TOPLEFT, &sprite_rect);
            blit_texture(game_data.atlas, r.x, r.y, &sprite_rect, NULL);

            get_atlas_sprite_rect(ATLAS_SPRITE_MENU_SLICE_TOPRIGHT, &sprite_rect);
            blit_texture(game_data.atlas, r.x + r.width, r.y, &sprite_rect, NULL);

            get_atlas_sprite_rect(ATLAS_SPRITE_MENU_SLICE_BOTTOMLEFT, &sprite_rect);
            blit_texture(game_data.atlas, r.x, r.y + r.height, &sprite_rect, NULL);

            get_atlas_sprite_rect(ATLAS_SPRITE_MENU_SLICE_BOTTOMRIGHT, &sprite_rect);
            blit_texture(game_data.atlas, r.x + r.width, r.y + r.height, &sprite_rect, NULL);

            // Draw the tiling menu
            get_atlas_sprite_rect(ATLAS_SPRITE_MENU_SLICE_TOP, &sprite_rect);
            for(int32_t i = r.x + TILE_SIZE_ALT; i < (r.x + r.width); i += TILE_SIZE_ALT) {
                blit_texture(game_data.atlas, i, r.y, &sprite_rect, NULL);
            }

            get_atlas_sprite_rect(ATLAS_SPRITE_MENU_SLICE_LEFT, &sprite_rect);
            for(int32_t i = r.y + TILE_SIZE_ALT; i < (r.y + r.height); i += TILE_SIZE_ALT) {
                blit_texture(game_data.atlas, r.x, i, &sprite_rect, NULL);
            }

            get_atlas_sprite_rect(ATLAS_SPRITE_MENU_SLICE_RIGHT, &sprite_rect);
            for(int32_t i = r.y + TILE_SIZE_ALT; i < (r.y + r.height); i += TILE_SIZE_ALT) {
                blit_texture(game_data.atlas, r.x + r.width, i, &sprite_rect, NULL);
            }

            get_atlas_sprite_rect(ATLAS_SPRITE_MENU_SLICE_BOTTOM, &sprite_rect);
            for(int32_t i = r.x + TILE_SIZE_ALT; i < (r.x + r.width); i += TILE_SIZE_ALT) {
                blit_texture(game_data.atlas, i, r.y + r.height, &sprite_rect, NULL);
            }

            get_atlas_sprite_rect(ATLAS_SPRITE_MENU_SLICE_CENTER, &sprite_rect);
            for(int32_t i = r.x + TILE_SIZE_ALT; i < (r.x + r.width); i += TILE_SIZE_ALT) {
                for(int32_t j = r.y + TILE_SIZE_ALT; j < (r.y + r.height); j += TILE_SIZE_ALT) {
                    blit_texture(game_data.atlas, i, j, &sprite_rect, NULL);
                }
            }

            // Draw menu text
            xstart += (TILE_SIZE_ALT / 2);
            ystart += (TILE_SIZE_ALT / 2);

            for(int32_t i = 0; i < MENU_ITEM_COUNT; i++) {
                set_draw_intensity(0.35f);
                if(i == game_data.selected_menu_id) {
                    set_draw_intensity(1.0f);
                }
                draw_text(game_data.atlas, xstart, ystart + (ystep * i), menu_items[i].str);
            }

#undef MENU_TILE_COUNT_WIDTH
#undef MENU_TILE_COUNT_HEIGHT

            break;
        }
        default:
            break;
    }

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
#undef INPUT_PRESS

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
    [ATLAS_SPRITE_GHOST_EATEN_UP_FRAME1] = { .x = 96, .y = 32, .width = 32, .height = 32 },
    [ATLAS_SPRITE_GHOST_EATEN_UP_FRAME2] = { .x = 128, .y = 32, .width = 32, .height = 32 },
    [ATLAS_SPRITE_GHOST_EATEN_LEFT_FRAME1] = { .x = 160, .y = 32, .width = 32, .height = 32 },
    [ATLAS_SPRITE_GHOST_EATEN_LEFT_FRAME2] = { .x = 192, .y = 32, .width = 32, .height = 32 },
    [ATLAS_SPRITE_GHOST_EATEN_DOWN_FRAME1] = { .x = 224, .y = 32, .width = 32, .height = 32 },
    [ATLAS_SPRITE_GHOST_EATEN_DOWN_FRAME2] = { .x = 0, .y = 64, .width = 32, .height = 32 },
    [ATLAS_SPRITE_GHOST_EATEN_RIGHT_FRAME1] = { .x = 32, .y = 64, .width = 32, .height = 32 },
    [ATLAS_SPRITE_GHOST_EATEN_RIGHT_FRAME2] = { .x = 64, .y = 64, .width = 32, .height = 32 },
    [ATLAS_SPRITE_GHOST_EATEN_FRAME3] = { .x = 96, .y = 64, .width = 32, .height = 32 },
    [ATLAS_SPRITE_GHOST_FRIGHTENED_FRAME1] = { .x = 128, .y = 64, .width = 32, .height = 32 },
    [ATLAS_SPRITE_GHOST_FRIGHTENED_FRAME2] = { .x = 160, .y = 64, .width = 32, .height = 32 },
    [ATLAS_SPRITE_GHOST_HOUSE_GATE] = { .x = 192, .y = 64, .width = 32, .height = 32 },
    [ATLAS_SPRITE_WALL_NORMAL] = { .x = 224, .y = 64, .width = 32, .height = 32 },
    [ATLAS_SPRITE_WALL_BOTTOM] = { .x = 0, .y = 96, .width = 32, .height = 32 },
    [ATLAS_SPRITE_PELLET] = { .x = 32, .y = 96, .width = 32, .height = 32 },
    [ATLAS_SPRITE_POWER_PELLET] = { .x = 64, .y = 96, .width = 32, .height = 32 },
    [ATLAS_SPRITE_A] = { .x = 96, .y = 96, .width = 16, .height = 16 },
    [ATLAS_SPRITE_B] = { .x = 112, .y = 96, .width = 16, .height = 16 },
    [ATLAS_SPRITE_C] = { .x = 128, .y = 96, .width = 16, .height = 16 },
    [ATLAS_SPRITE_D] = { .x = 144, .y = 96, .width = 16, .height = 16 },
    [ATLAS_SPRITE_E] = { .x = 160, .y = 96, .width = 16, .height = 16 },
    [ATLAS_SPRITE_F] = { .x = 176, .y = 96, .width = 16, .height = 16 },
    [ATLAS_SPRITE_G] = { .x = 192, .y = 96, .width = 16, .height = 16 },
    [ATLAS_SPRITE_H] = { .x = 208, .y = 96, .width = 16, .height = 16 },
    [ATLAS_SPRITE_I] = { .x = 225, .y = 96, .width = 14, .height = 16 },
    [ATLAS_SPRITE_J] = { .x = 240, .y = 96, .width = 15, .height = 16 },
    [ATLAS_SPRITE_K] = { .x = 96, .y = 112, .width = 16, .height = 16 },
    [ATLAS_SPRITE_L] = { .x = 112, .y = 112, .width = 16, .height = 16 },
    [ATLAS_SPRITE_M] = { .x = 128, .y = 112, .width = 16, .height = 16 },
    [ATLAS_SPRITE_N] = { .x = 144, .y = 112, .width = 16, .height = 16 },
    [ATLAS_SPRITE_O] = { .x = 160, .y = 112, .width = 16, .height = 16 },
    [ATLAS_SPRITE_P] = { .x = 176, .y = 112, .width = 16, .height = 16 },
    [ATLAS_SPRITE_Q] = { .x = 192, .y = 112, .width = 16, .height = 16 },
    [ATLAS_SPRITE_R] = { .x = 208, .y = 112, .width = 16, .height = 16 },
    [ATLAS_SPRITE_S] = { .x = 224, .y = 112, .width = 16, .height = 16 },
    [ATLAS_SPRITE_T] = { .x = 240, .y = 112, .width = 16, .height = 16 },
    [ATLAS_SPRITE_U] = { .x = 0, .y = 128, .width = 16, .height = 16 },
    [ATLAS_SPRITE_V] = { .x = 16, .y = 128, .width = 16, .height = 16 },
    [ATLAS_SPRITE_W] = { .x = 32, .y = 128, .width = 16, .height = 16 },
    [ATLAS_SPRITE_X] = { .x = 48, .y = 128, .width = 16, .height = 16 },
    [ATLAS_SPRITE_Y] = { .x = 64, .y = 128, .width = 16, .height = 16 },
    [ATLAS_SPRITE_Z] = { .x = 80, .y = 128, .width = 16, .height = 16 },
    [ATLAS_SPRITE_0] = { .x = 96, .y = 128, .width = 16, .height = 16 },
    [ATLAS_SPRITE_1] = { .x = 120, .y = 128, .width = 6, .height = 16 },
    [ATLAS_SPRITE_2] = { .x = 128, .y = 128, .width = 16, .height = 16 },
    [ATLAS_SPRITE_3] = { .x = 144, .y = 128, .width = 16, .height = 16 },
    [ATLAS_SPRITE_4] = { .x = 161, .y = 128, .width = 13, .height = 16 },
    [ATLAS_SPRITE_5] = { .x = 176, .y = 128, .width = 16, .height = 16 },
    [ATLAS_SPRITE_6] = { .x = 192, .y = 128, .width = 16, .height = 16 },
    [ATLAS_SPRITE_7] = { .x = 208, .y = 128, .width = 16, .height = 16 },
    [ATLAS_SPRITE_8] = { .x = 224, .y = 128, .width = 16, .height = 16 },
    [ATLAS_SPRITE_9] = { .x = 241, .y = 128, .width = 15, .height = 16 },
    [ATLAS_SPRITE_MENU_SLICE_TOPLEFT] = { .x = 0, .y = 144, .width = 16, .height = 16 },
    [ATLAS_SPRITE_MENU_SLICE_TOP] = { .x = 16, .y = 144, .width = 16, .height = 16 },
    [ATLAS_SPRITE_MENU_SLICE_TOPRIGHT] = { .x = 32, .y = 144, .width = 16, .height = 16 },
    [ATLAS_SPRITE_MENU_SLICE_LEFT] = { .x = 0, .y = 160, .width = 16, .height = 16 },
    [ATLAS_SPRITE_MENU_SLICE_CENTER] = { .x = 16, .y = 160, .width = 16, .height = 16 },
    [ATLAS_SPRITE_MENU_SLICE_RIGHT] = { .x = 32, .y = 160, .width = 16, .height = 16 },
    [ATLAS_SPRITE_MENU_SLICE_BOTTOMLEFT] = { .x = 0, .y = 176, .width = 16, .height = 16 },
    [ATLAS_SPRITE_MENU_SLICE_BOTTOM] = { .x = 16, .y = 176, .width = 16, .height = 16 },
    [ATLAS_SPRITE_MENU_SLICE_BOTTOMRIGHT] = { .x = 32, .y = 176, .width = 16, .height = 16 }
};

void get_atlas_sprite_rect(AtlasSprite id, Rect *r) {
    assert(r && id >= 0 && id < ATLAS_SPRITE_COUNT);
    memcpy(r, &atlas_sprites[id], sizeof(*r));
}
