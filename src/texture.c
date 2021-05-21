/*
 * Pacman Clone
 *
 * Copyright (c) 2021 Amanch Esmailzadeh
 */

#include "texture.h"
#include "game.h"

#include <stdio.h>
#include <stddef.h>
#include <string.h>

#pragma pack(push, 1)
typedef struct {
    struct {
        uint16_t type;
        uint32_t size;
        uint16_t reserved[2];
        uint32_t offset;
    } file_header;
    struct {
        uint32_t size;
        int32_t width;
        int32_t height;
        uint16_t planes;
        uint16_t bits_per_pixel;
        uint32_t compression;
        uint32_t size_of_bmp;
        int32_t horizontal_resolution;
        int32_t vertical_resolution;
        uint32_t colors_used;
        uint32_t colors_important;
    } bitmap_header;
} BitmapFile;
#pragma pack(pop)

static inline uint32_t get_y_bottom_up(uint32_t y, uint32_t height) { return height - 1 - y; }
static inline uint32_t get_y_top_down(uint32_t y, uint32_t height) { IGNORED_VARIABLE(height); return y; }

static inline void assign_color_opaque(unsigned char *data, uint32_t color, uint32_t chroma_key) {
    IGNORED_VARIABLE(chroma_key);
    data[0] = (color >> 16) & 0xff;
    data[1] = (color >> 8) & 0xff;
    data[2] = color & 0xff;
    data[3] = 0xff;
}

static void assign_color_transparent(unsigned char *data, uint32_t color, uint32_t chroma_key) {
    if(color != chroma_key) {
        assign_color_opaque(data, color, chroma_key);
    } else {
        memset(data, 0, sizeof(color));
    }
}

static unsigned char * load_bmp_file(const char *path, uint32_t chroma_key, uint32_t *w, uint32_t *h) {
    if(path) {
        FILE *f = fopen(path, "rb");
        if(f) {
            BitmapFile bmp = { 0 };
            fread(&bmp, sizeof(bmp), 1, f);

            unsigned char *data = NULL;

            // We only support bitmaps that have the Windows header type and are at most v3
            if(bmp.file_header.type == 0x4d42 && bmp.bitmap_header.size == 40 &&
               (bmp.bitmap_header.bits_per_pixel == 24 || bmp.bitmap_header.bits_per_pixel == 32)) {

                uint32_t bytes_per_pixel = bmp.bitmap_header.bits_per_pixel / 8;

                uint32_t width = bmp.bitmap_header.width * bytes_per_pixel;
                uint32_t height = ABSOLUTE_VAL(bmp.bitmap_header.height);

                uint32_t scanline = (width + 3) & ~3;
                uint32_t pixel_row_size = bmp.bitmap_header.width * CHANNEL_COUNT;

                data = malloc(sizeof(*data) * pixel_row_size * height);
                unsigned char *row_buffer = malloc(sizeof(*row_buffer) * scanline); // TODO: Perhaps change to alloca()?

                uint32_t (*get_row_op)(uint32_t, uint32_t) = (bmp.bitmap_header.height >= 0) ?
                    get_y_bottom_up : get_y_top_down;
                void (*assign_color_op)(unsigned char *, uint32_t, uint32_t) = (chroma_key == CHROMA_KEY_UNUSED) ?
                    assign_color_opaque : assign_color_transparent;

                fseek(f, bmp.file_header.offset, SEEK_SET);

                for(uint32_t y = 0; y < height; y++) {
                    fread(row_buffer, sizeof(*row_buffer) * scanline, 1, f);
                    uint32_t row_start = get_row_op(y, height) * pixel_row_size;

                    for(uint32_t xb = 0, xd = 0; xb < width; xb += bytes_per_pixel, xd += CHANNEL_COUNT) {
                        uint32_t data_index = row_start + xd;

                        uint32_t color = 0;
                        memcpy(&color, &row_buffer[xb], bytes_per_pixel);

                        assign_color_op(&data[data_index], color, chroma_key);
                    }
                }

                if(w) {
                    *w = bmp.bitmap_header.width;
                }

                if(h) {
                    *h = ABSOLUTE_VAL(bmp.bitmap_header.height);
                }

                free(row_buffer);
            }

            fclose(f);
            if(data) {
                return data;
            }
        }
    }

    return NULL;
}

Texture2D * load_texture(const char *path, uint32_t chroma_key) {
    Texture2D *tex = NULL;

    uint32_t w, h;
    unsigned char *data = load_bmp_file(path, chroma_key, &w, &h);
    if(data) {
        tex = malloc(offsetof(struct Texture2D, data) + (w * h * CHANNEL_COUNT));
        tex->width = w;
        tex->height = h;
        memcpy(&tex->data, data, w * h * CHANNEL_COUNT);

        free(data);
    }

    return tex;
}

void destroy_texture(Texture2D **texture) {
    if(texture) {
        free(*texture);
        *texture = NULL;
    }
}
