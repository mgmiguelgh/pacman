/*
 * Pacman Clone
 *
 * Copyright (c) 2021 Amanch Esmailzadeh
 */

#ifndef PLATFORM_H
#define PLATFORM_H

#include "level.h"

void init_level_names(LevelFileData *data);
void destroy_level_names(LevelFileData *data);

static int level_name_compare(const void *lhs, const void *rhs) {
    return strcmp(lhs, rhs) > 0;
}

#endif /* PLATFORM_H */
