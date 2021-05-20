/*
 * Pacman Clone
 *
 * Copyright (c) 2021 Amanch Esmailzadeh
 */

#include "level.h"

Level * load_next_level(void) {
    Level *level = get_next_level_from_disk();
    if(level) {
        for(uint32_t y = 0; y < level->rows; y++) {
            for(uint32_t x = 0; x < level->columns; x++) {
                uint32_t *tile = get_level_tile(level, x, y);
                if(!tile) {
                    continue;
                }

                AtlasSprite sprite_id = *tile;
                switch(sprite_id) {
                    case ATLAS_SPRITE_GHOST_HOUSE_GATE:
                        level->gate_tile.x = x;
                        level->gate_tile.y = y;
                        break;
                    case ATLAS_SPRITE_PLAYER_FRAME1:
                    case ATLAS_SPRITE_PLAYER_FRAME2:
                        level->player_start.x = x;
                        level->player_start.y = y;
                        *tile = ATLAS_SPRITE_EMPTY;
                        break;
                    case ATLAS_SPRITE_BLINKY_FRAME1:
                    case ATLAS_SPRITE_BLINKY_FRAME2:
                        level->ghost_start[GHOST_BLINKY].x = x;
                        level->ghost_start[GHOST_BLINKY].y = y;
                        *tile = ATLAS_SPRITE_EMPTY;
                        break;
                    case ATLAS_SPRITE_PINKY_FRAME1:
                    case ATLAS_SPRITE_PINKY_FRAME2:
                        level->ghost_start[GHOST_PINKY].x = x;
                        level->ghost_start[GHOST_PINKY].y = y;
                        *tile = ATLAS_SPRITE_EMPTY;
                        break;
                    case ATLAS_SPRITE_CLYDE_FRAME1:
                    case ATLAS_SPRITE_CLYDE_FRAME2:
                        level->ghost_start[GHOST_CLYDE].x = x;
                        level->ghost_start[GHOST_CLYDE].y = y;
                        *tile = ATLAS_SPRITE_EMPTY;
                        break;
                    case ATLAS_SPRITE_INKY_FRAME1:
                    case ATLAS_SPRITE_INKY_FRAME2:
                        level->ghost_start[GHOST_INKY].x = x;
                        level->ghost_start[GHOST_INKY].y = y;
                        *tile = ATLAS_SPRITE_EMPTY;
                        break;
                    default:
                        break;
                }

                if(sprite_id == ATLAS_SPRITE_PELLET || sprite_id == ATLAS_SPRITE_POWER_PELLET) {
                    level->pellet_count++;
                }
            }
        }

        return level;
    }

    return NULL;
}

Level * load_first_level(void) {
    set_next_level_index(0);
    return load_next_level();
}

void unload_level(Level **level) {
    if(level) {
        free(*level);
        *level = NULL;
    }
}
