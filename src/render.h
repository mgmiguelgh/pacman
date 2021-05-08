/*
 * Pacman Clone
 *
 * Copyright (c) 2021 Amanch Esmailzadeh
 */

#ifndef RENDER_H
#define RENDER_H

#include <stdint.h>

#include "texture.h"

#define DEFAULT_WINDOW_WIDTH 1920
#define DEFAULT_WINDOW_HEIGHT 1080
#define DEFAULT_FRAMEBUFFER_WIDTH 512
#define DEFAULT_FRAMEBUFFER_HEIGHT 288

typedef struct {
    int32_t x;
    int32_t y;
    uint32_t width;
    uint32_t height;
} Rect;

uint32_t * get_framebuffer(void);
void set_pixel(int32_t x, int32_t y, uint32_t color);
void blit_texture(const Texture2D *texture, int32_t dx, int32_t dy, const Rect *rect);

#endif /* RENDER_H */
