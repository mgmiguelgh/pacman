/*
 * Pacman Clone
 *
 * Copyright (c) 2021 Amanch Esmailzadeh
 */

#include "render.h"
#include <float.h>
#include <stdbool.h>

static Color framebuffer[DEFAULT_FRAMEBUFFER_WIDTH * DEFAULT_FRAMEBUFFER_HEIGHT];
static float light_buffer[DEFAULT_FRAMEBUFFER_WIDTH * DEFAULT_FRAMEBUFFER_HEIGHT];

Color * get_framebuffer(void) {
    return framebuffer;
}

static inline int in_bounds(int32_t x, int32_t y, int32_t width, int32_t height) {
    return x >= 0 && x < width &&
        y >= 0 && y < height;
}

static inline void set_pixel(int32_t x, int32_t y, Color color) {
    bool should_render = color.a > 0.0f;
    if(should_render && in_bounds(x, y, DEFAULT_FRAMEBUFFER_WIDTH, DEFAULT_FRAMEBUFFER_HEIGHT)) {
        framebuffer[y * DEFAULT_FRAMEBUFFER_WIDTH + x] = color;
    }
}

static inline void add_light(int32_t x, int32_t y, float intensity) {
    if(intensity > 0.0f && in_bounds(x, y, DEFAULT_FRAMEBUFFER_WIDTH, DEFAULT_FRAMEBUFFER_HEIGHT)) {
        float level = light_buffer[y * DEFAULT_FRAMEBUFFER_WIDTH + x];
        level += intensity;
        level = CLAMP(level, 0.0f, 1.0f);
        light_buffer[y * DEFAULT_FRAMEBUFFER_WIDTH + x] = level;
    }
}

static inline Color convert_to_float_color(uint32_t color) {
    return (Color) {
        .r = (float)(color & 0xff) / 255.0f,
        .g = (float)((color >> 8) & 0xff) / 255.0f,
        .b = (float)((color >> 16) & 0xff) / 255.0f,
        .a = (float)(!!((color >> 24) & 0xff))
    };
}

static void simple_forward_blit(const Texture2D *texture, int32_t dx, int32_t dy, const Rect *rect) {
    assert(texture && rect && in_bounds(rect->x, rect->y, texture->width, texture->height));

    int32_t xstart = MAX(0, dx);
    int32_t xend = MIN((dx + rect->width), DEFAULT_FRAMEBUFFER_WIDTH);
    int32_t ystart = MAX(0, dy);
    int32_t yend = MIN((dy + rect->height), DEFAULT_FRAMEBUFFER_HEIGHT);

    int32_t sy = rect->y + ((dy < 0) ? ABSOLUTE_VAL(rect->height - yend) : 0);
    int32_t tex_startx = rect->x + ((dx < 0) ? ABSOLUTE_VAL(rect->width - xend) : 0);

    for(; ystart < yend; ystart++, sy++) {
        int32_t sx = tex_startx;

        for(int32_t x0 = xstart; x0 < xend; x0++, sx++) {
            uint32_t texture_index = sy * (texture->width * CHANNEL_COUNT) + (sx * CHANNEL_COUNT);
            uint32_t tex_color;
            memcpy(&tex_color, &texture->data[texture_index], sizeof(uint32_t));

            Color c = convert_to_float_color(tex_color);

            set_pixel(x0, ystart, c);
        }
    }
}

void blit_texture(const Texture2D *texture, int32_t dx, int32_t dy, const Rect *rect, const Matrix3x3 *transform) {
    if(texture) {
        int32_t x, y;
        int32_t width, height;

        if(rect) {
            x = rect->x;
            y = rect->y;
            width = rect->width;
            height = rect->height;
        } else {
            x = y = 0;
            width = texture->width;
            height = texture->height;
        }

        if(!transform) {
            Rect r = { .x = x, .y = y, .width = width, .height = height };
            simple_forward_blit(texture, dx, dy, &r);
            return;
        }

        Matrix3x3 t;
        Rect bounding_box;

        float x_offset = (float)(-x - (width >> 1));
        float y_offset = (float)(-y - (height >> 1));

        t = get_translation_mat3(x_offset, y_offset);
        Matrix3x3 t2 = get_translation_mat3((float)(dx + (width >> 1)), (float)(dy + (height >> 1)));

        t = mat3_mul(transform, &t);
        t = mat3_mul(&t2, &t);

        Vector3 pos = { .x = (float)x, .y = (float)y, .z = 1.0f };

        Vector3 corners[4];
        corners[0] = mat3_vec3_mul(&t, &pos);

        pos.x = (float)(x + width);
        corners[1] = mat3_vec3_mul(&t, &pos);

        pos.y = (float)(y + height);
        corners[2] = mat3_vec3_mul(&t, &pos);

        pos.x = (float)x;
        corners[3] = mat3_vec3_mul(&t, &pos);

        float min_x = FLT_MAX, max_x = -FLT_MAX;
        float min_y = FLT_MAX, max_y = -FLT_MAX;
        for(int i = 0; i < 4; i++) {
            min_x = MIN(min_x, corners[i].x);
            max_x = MAX(max_x, corners[i].x);
            min_y = MIN(min_y, corners[i].y);
            max_y = MAX(max_y, corners[i].y);
        }

        bounding_box.x = (int32_t)min_x;
        bounding_box.y = (int32_t)min_y;
        bounding_box.width = (int32_t)max_x;
        bounding_box.height = (int32_t)max_y;

        uint32_t bounds = in_bounds(bounding_box.x, bounding_box.y, DEFAULT_FRAMEBUFFER_WIDTH, DEFAULT_FRAMEBUFFER_HEIGHT);
        bounds |= in_bounds(bounding_box.width, bounding_box.y, DEFAULT_FRAMEBUFFER_WIDTH, DEFAULT_FRAMEBUFFER_HEIGHT);
        bounds |= in_bounds(bounding_box.x, bounding_box.height, DEFAULT_FRAMEBUFFER_WIDTH, DEFAULT_FRAMEBUFFER_HEIGHT);
        bounds |= in_bounds(bounding_box.width, bounding_box.height, DEFAULT_FRAMEBUFFER_WIDTH, DEFAULT_FRAMEBUFFER_HEIGHT);

        if(bounds != 0) {
            t = get_inverse_matrix(&t);
            transform = &t;

            int32_t xstart = MAX(0, bounding_box.x);
            int32_t xend = MIN(bounding_box.width, DEFAULT_FRAMEBUFFER_WIDTH);
            int32_t ystart = MAX(0, bounding_box.y);
            int32_t yend = MIN(bounding_box.height, DEFAULT_FRAMEBUFFER_HEIGHT);

            Vector3 point = { .z = 1.0f };
            for(int32_t y0 = ystart; y0 < yend; y0++) {
                for(int32_t x0 = xstart; x0 < xend; x0++) {
                    point.x = (float)x0 + 0.5f;
                    point.y = (float)y0 + 0.5f;

                    point = mat3_vec3_mul(transform, &point);

                    int32_t px = (int32_t)point.x, py = (int32_t)point.y;
                    if(px >= x && px < (x + width) && py >= y && py < (y + height)) {
                        uint32_t texture_index = py * (texture->width * CHANNEL_COUNT) + (px * CHANNEL_COUNT);
                        uint32_t tex_color;
                        memcpy(&tex_color, &texture->data[texture_index], sizeof(uint32_t));

                        Color c = convert_to_float_color(tex_color);

                        set_pixel(x0, y0, c);
                    }
                }
            }
        }
    }
}

void draw_spotlight(int32_t dx, int32_t dy, uint32_t radius, float gradient_length) {
    assert(radius > 0);

    int32_t start_x = dx - radius;
    int32_t start_y = dy - radius;
    int32_t end_x = dx + radius;
    int32_t end_y = dy + radius;

    int32_t r2 = radius * radius;

    float r_inv = 1.0f / (float)radius;

    for(int32_t y = start_y; y < end_y; y++) {
        int32_t dist_y = y - dy;
        float y0 = (float)dist_y;

        for(int32_t x = start_x; x < end_x; x++) {
            int32_t dist_x = x - dx;
            float x0 = (float)dist_x;

            // TODO: Change the sqrtf to the rsqrt intrinsic
            float n = (1.0f - (sqrtf(x0 * x0 + y0 * y0) * r_inv)) * gradient_length;

            if((dist_x * dist_x + dist_y * dist_y) < r2) {
                add_light(x, y, n);
            }
        }
    }
}

void clear_spotlights(void) {
    memset(light_buffer, 0, sizeof(light_buffer));
}

void submit_spotlights(void) {
    static const float dither_map[2][2] = {
        { 0.25f, 0.5f },
        { 0.75f, 0.0f }
    };

    for(int32_t y = 0; y < DEFAULT_FRAMEBUFFER_HEIGHT; y++) {
        for(int32_t x = 0; x < DEFAULT_FRAMEBUFFER_WIDTH; x++) {
            Color *pixel = &framebuffer[y * DEFAULT_FRAMEBUFFER_WIDTH + x];

            float intensity = light_buffer[y * DEFAULT_FRAMEBUFFER_WIDTH + x];
            intensity = (intensity > dither_map[y%2][x%2]) ? 1.0f : 0.15f;

            pixel->r = CLAMP((pixel->r * intensity), 0.0f, 1.0f);
            pixel->g = CLAMP((pixel->g * intensity), 0.0f, 1.0f);
            pixel->b = CLAMP((pixel->b * intensity), 0.0f, 1.0f);
            pixel->a = CLAMP((pixel->a * intensity), 0.0f, 1.0f);
        }
    }
}

#define LETTER_SPACING 2
#define SPACE_PIXELS 16

void draw_text(const Texture2D *texture, const char *text, int32_t x, int32_t y) {
    if(texture && text) {
        char c;
        int index = 0;
        int32_t xoff = x;
        Rect sprite_rect;
        while((c = text[index++]) != '\0') {
            if((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
                c &= ~(1 << 5); // Convert to uppercase
                get_atlas_sprite_rect(ATLAS_SPRITE_A + (c - 'A'), &sprite_rect);
                blit_texture(texture, xoff, y, &sprite_rect, NULL);
                xoff += sprite_rect.width + LETTER_SPACING;
            } else if(c >= '0' && c <= '9') {
                get_atlas_sprite_rect(ATLAS_SPRITE_0 + (c - '0'), &sprite_rect);
                blit_texture(texture, xoff, y, &sprite_rect, NULL);
                xoff += sprite_rect.width + LETTER_SPACING;
            } else if(c == ' ') {
                xoff += SPACE_PIXELS;
            }
        }
    }
}

#undef LETTER_SPACING
#undef SPACE_PIXELS

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
    [ATLAS_SPRITE_9] = { .x = 241, .y = 128, .width = 15, .height = 16 }
};

void get_atlas_sprite_rect(AtlasSprite id, Rect *r) {
    assert(r && id >= 0 && id < ATLAS_SPRITE_COUNT);
    memcpy(r, &atlas_sprites[id], sizeof(*r));
}
