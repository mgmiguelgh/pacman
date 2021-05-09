/*
 * Pacman Clone
 *
 * Copyright (c) 2021 Amanch Esmailzadeh
 */

#ifndef TEXTURE_H
#define TEXTURE_H

#include <stdint.h>

typedef struct Texture2D {
    uint32_t width;
    uint32_t height;
    unsigned char data[];
} Texture2D;

#define CHROMA_KEY_UNUSED 0xffffffff
#define CHANNEL_COUNT 4
#define ABSOLUTE_VAL(v) (((v) >= 0) ? (v) : -(v))


Texture2D * load_texture(const char *path, uint32_t chroma_key);
void destroy_texture(Texture2D **texture);

#endif /* TEXTURE_H */
