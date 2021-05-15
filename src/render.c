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

void draw_spotlight(int32_t dx, int32_t dy, uint32_t radius) {
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

            float n = 1.0f - (sqrtf(x0 * x0 + y0 * y0) * r_inv);

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
            intensity = (intensity > dither_map[y%2][x%2]) ? 1.0f : 0.35f;

            pixel->r = CLAMP((pixel->r * intensity), 0.0f, 1.0f);
            pixel->g = CLAMP((pixel->g * intensity), 0.0f, 1.0f);
            pixel->b = CLAMP((pixel->b * intensity), 0.0f, 1.0f);
            pixel->a = CLAMP((pixel->a * intensity), 0.0f, 1.0f);
        }
    }
}
