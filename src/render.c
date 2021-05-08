/*
 * Pacman Clone
 *
 * Copyright (c) 2021 Amanch Esmailzadeh
 */

#include "render.h"

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

void blit_texture(const Texture2D *texture, int32_t dx, int32_t dy, const Rect *rect) {
    if(texture) {
        uint32_t x, y;
        uint32_t width, height;
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

        uint32_t coords_flags = 0;
        coords_flags |= in_bounds(dx, dy, DEFAULT_FRAMEBUFFER_WIDTH, DEFAULT_FRAMEBUFFER_HEIGHT);
        coords_flags |= in_bounds(dx + width, dy, DEFAULT_FRAMEBUFFER_WIDTH, DEFAULT_FRAMEBUFFER_HEIGHT);
        coords_flags |= in_bounds(dx, dy + height, DEFAULT_FRAMEBUFFER_WIDTH, DEFAULT_FRAMEBUFFER_HEIGHT);
        coords_flags |= in_bounds(dx + width, dy + height, DEFAULT_FRAMEBUFFER_WIDTH, DEFAULT_FRAMEBUFFER_HEIGHT);

        // If none of the corners of the destination bounding box are within the framebuffer, we can skip
        // trying to draw it, because none of the pixels will end up on the framebuffer anyway
        if(coords_flags != 0) {
            uint32_t sy = y;
            uint32_t width_source = texture->width * CHANNEL_COUNT;
            for(uint32_t y0 = 0; y0 < height; y0++, sy++) {
                uint32_t sx = x;

                for(uint32_t x0 = 0; x0 < width; x0++, sx++) {
                    uint32_t texture_index = sy * width_source + (sx * CHANNEL_COUNT);
                    uint32_t color;
                    memcpy(&color, &texture->data[texture_index], sizeof(uint32_t));

                    set_pixel(dx + x0, dy + y0, color);
                }
            }
        }

    }
}
