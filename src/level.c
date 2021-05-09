/*
 * Pacman Clone
 *
 * Copyright (c) 2021 Amanch Esmailzadeh
 */

#include "level.h"

Level * load_next_level(void) {
    Level *level = get_next_level_from_disk();
    if(level) {
        return level;
    }

    return NULL;
}

void unload_level(Level **level) {
    if(level) {
        free(*level);
        *level = NULL;
    }
}
