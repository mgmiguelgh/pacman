/*
 * Pacman Clone
 *
 * Copyright (c) 2021 Amanch Esmailzadeh
 */

#include "render.h"
#include <float.h>

static uint32_t framebuffer[DEFAULT_FRAMEBUFFER_WIDTH * DEFAULT_FRAMEBUFFER_HEIGHT];

uint32_t * get_framebuffer(void) {
    return framebuffer;
}

static inline int in_bounds(int32_t x, int32_t y, int32_t width, int32_t height) {
    return x >= 0 && x < width &&
        y >= 0 && y < height;
}

void set_pixel(int32_t x, int32_t y, uint32_t color) {
    int has_alpha = (color >> 24) & 0xff;
    if(has_alpha && in_bounds(x, y, DEFAULT_FRAMEBUFFER_WIDTH, DEFAULT_FRAMEBUFFER_HEIGHT)) {
        framebuffer[y * DEFAULT_FRAMEBUFFER_WIDTH + x] = color;
    }
}

static void simple_forward_blit(const Texture2D *texture, int32_t dx, int32_t dy, const Rect *rect) {
    assert(texture && rect && in_bounds(rect->x, rect->y, texture->width, texture->height));

    uint32_t bounds = in_bounds(dx, dy, DEFAULT_FRAMEBUFFER_WIDTH, DEFAULT_FRAMEBUFFER_HEIGHT);
    bounds |= in_bounds(dx + rect->width, dy, DEFAULT_FRAMEBUFFER_WIDTH, DEFAULT_FRAMEBUFFER_HEIGHT);
    bounds |= in_bounds(dx, dy + rect->height, DEFAULT_FRAMEBUFFER_WIDTH, DEFAULT_FRAMEBUFFER_HEIGHT);
    bounds |= in_bounds(dx + rect->height, dy + rect->height, DEFAULT_FRAMEBUFFER_WIDTH, DEFAULT_FRAMEBUFFER_HEIGHT);

    if(bounds == 0) {
        return;
    }

    int32_t sy = rect->y;
    for(int32_t y0 = 0; y0 < rect->height; y0++, sy++) {
        int32_t sx = rect->x;

        for(int32_t x0 = 0; x0 < rect->width; x0++, sx++) {
            uint32_t texture_index = sy * (texture->width * CHANNEL_COUNT) + (sx * CHANNEL_COUNT);
            uint32_t color;
            memcpy(&color, &texture->data[texture_index], sizeof(uint32_t));

            set_pixel(dx + x0, dy + y0, color);
        }
    }
}

void blit_texture(const Texture2D *texture, int32_t dx, int32_t dy, const Rect *rect, const Matrix3x3 *transform) {
    if(texture) {
        int32_t x, y;
        int32_t width, height;

        static const Matrix3x3 identity_mat = {
            .m00 = 1.0f, .m01 = 0.0f, .m02 = 0.0f,
            .m10 = 0.0f, .m11 = 1.0f, .m12 = 0.0f,
            .m20 = 0.0f, .m21 = 0.0f, .m22 = 1.0f
        };

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
        if(bounds == 0) {
            return;
        }

        t = get_inverse_matrix(&t);

        transform = &t;

        Vector3 point = { .z = 1.0f };
        for(int32_t y0 = bounding_box.y; y0 < bounding_box.height; y0++) {
            for(int32_t x0 = bounding_box.x; x0 < bounding_box.width; x0++) {
                point.x = (float)x0 + 0.5f;
                point.y = (float)y0 + 0.5f;

                point = mat3_vec3_mul(transform, &point);

                int32_t px = (int32_t)point.x, py = (int32_t)point.y;
                if(px >= x && px < (x + width) && py >= y && py < (y + height)) {
                    uint32_t texture_index = py * (texture->width * CHANNEL_COUNT) + (px * CHANNEL_COUNT);
                    uint32_t color;
                    memcpy(&color, &texture->data[texture_index], sizeof(uint32_t));

                    set_pixel(x0, y0, color);
                }
            }
        }
    }
}
