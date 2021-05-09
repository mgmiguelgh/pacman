/*
 * Pacman Clone
 *
 * Copyright (c) 2021 Amanch Esmailzadeh
 */

#ifndef RENDER_H
#define RENDER_H

#include <assert.h>
#include <math.h>
#include <stdint.h>

#include "texture.h"

#define DEFAULT_WINDOW_WIDTH 2560
#define DEFAULT_WINDOW_HEIGHT 1440
#define DEFAULT_FRAMEBUFFER_WIDTH 512
#define DEFAULT_FRAMEBUFFER_HEIGHT 288

#define MIN(a, b) (((a) <= (b)) ? (a) : (b))
#define MAX(a, b) (((a) >= (b)) ? (a) : (b))

typedef struct {
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
} Rect;

typedef struct {
    float x;
    float y;
} Vector2;

typedef struct {
    float x;
    float y;
    float z;
} Vector3;

typedef struct {
    float m00, m01, m02;
    float m10, m11, m12;
    float m20, m21, m22;
} Matrix3x3;

typedef struct {
    int32_t x;
    int32_t y;
} Vector2i;

typedef struct {
    float r, g, b, a;
} Color;

static inline Vector3 mat3_vec3_mul(const Matrix3x3 *mat, const Vector3 *vec) {
    assert(mat && vec);
    return (Vector3) {
        .x = mat->m00 * vec->x + mat->m10 * vec->y + mat->m20 * vec->z,
        .y = mat->m01 * vec->x + mat->m11 * vec->y + mat->m21 * vec->z,
        .z = mat->m02 * vec->x + mat->m12 * vec->y + mat->m22 * vec->z,
    };
}

static inline Matrix3x3 mat3_mul(const Matrix3x3 *a, const Matrix3x3 *b) {
    assert(a && b);
    return (Matrix3x3) {
        .m00 = a->m00 * b->m00 + a->m10 * b->m01 + a->m20 * b->m02,
        .m01 = a->m01 * b->m00 + a->m11 * b->m01 + a->m21 * b->m02,
        .m02 = a->m02 * b->m00 + a->m12 * b->m01 + a->m22 * b->m02,
        .m10 = a->m00 * b->m10 + a->m10 * b->m11 + a->m20 * b->m12,
        .m11 = a->m01 * b->m10 + a->m11 * b->m11 + a->m21 * b->m12,
        .m12 = a->m02 * b->m10 + a->m12 * b->m11 + a->m22 * b->m12,
        .m20 = a->m00 * b->m20 + a->m10 * b->m21 + a->m20 * b->m22,
        .m21 = a->m01 * b->m20 + a->m11 * b->m21 + a->m21 * b->m22,
        .m22 = a->m02 * b->m20 + a->m12 * b->m21 + a->m22 * b->m22
    };
}

static inline Matrix3x3 get_translation_mat3(float x, float y) {
    return (Matrix3x3) {
        .m00 = 1.0f, .m01 = 0.0f, .m02 = 0.0f,
        .m10 = 0.0f, .m11 = 1.0f, .m12 = 0.0f,
        .m20 =    x, .m21 =    y, .m22 = 1.0f
    };
}

static inline Matrix3x3 get_scaling_mat3(float x, float y) {
    return (Matrix3x3) {
        .m00 =    x, .m01 = 0.0f, .m02 = 0.0f,
        .m10 = 0.0f, .m11 =    y, .m12 = 0.0f,
        .m20 = 0.0f, .m21 = 0.0f, .m22 = 1.0f
    };
}

static inline Matrix3x3 get_rotation_mat3(float angle) {
    float c = cosf(angle);
    float s = sinf(angle);

    return (Matrix3x3) {
        .m00 =    c, .m01 =   -s, .m02 = 0.0f,
        .m10 =    s, .m11 =    c, .m12 = 0.0f,
        .m20 = 0.0f, .m21 = 0.0f, .m22 = 1.0f
    };
}

static Matrix3x3 get_inverse_matrix(const Matrix3x3 *mat) {
    assert(mat);

    float determinant =
        mat->m00 * (mat->m11 * mat->m22 - mat->m12 * mat->m21) -
        mat->m01 * (mat->m10 * mat->m22 - mat->m12 * mat->m20) +
        mat->m02 * (mat->m10 * mat->m21 - mat->m11 * mat->m20);

    if(determinant == 0.0f) {
        return (Matrix3x3) {
            .m00 = 1.0f, .m01 = 0.0f, .m02 = 0.0f,
            .m10 = 0.0f, .m11 = 1.0f, .m12 = 0.0f,
            .m20 = 0.0f, .m21 = 0.0f, .m22 = 1.0f
        };
    }

    float inv_det = 1.0f / determinant;

    Matrix3x3 m;
    m.m00 = (mat->m11 * mat->m22 - mat->m12 * mat->m21) * inv_det;
    m.m01 = (-(mat->m01 * mat->m22 - mat->m02 * mat->m21)) * inv_det;
    m.m02 = (mat->m01 * mat->m12 - mat->m02 * mat->m11) * inv_det;

    m.m10 = (-(mat->m10 * mat->m22 - mat->m12 * mat->m20)) * inv_det;
    m.m11 = (mat->m00 * mat->m22 - mat->m02 * mat->m20) * inv_det;
    m.m12 = (-(mat->m00 * mat->m12 - mat->m02 * mat->m10)) * inv_det;

    m.m20 = (mat->m10 * mat->m21 - mat->m11 * mat->m20) * inv_det;
    m.m21 = (-(mat->m00 * mat->m21 - mat->m01 * mat->m20)) * inv_det;
    m.m22 = (mat->m00 * mat->m11 - mat->m01 * mat->m10) * inv_det;

    return m;
}

Color * get_framebuffer(void);
void set_pixel(int32_t x, int32_t y, Color color);
void blit_texture(const Texture2D *texture, int32_t dx, int32_t dy, const Rect *rect, const Matrix3x3 *transform);

#endif /* RENDER_H */
