/*
 * Pacman Clone
 *
 * Copyright (c) 2021 Amanch Esmailzadeh
 */

#include <assert.h>
#include <string.h>
#include "level.h"

static Level * get_next_level(const char *file_name) {
    assert(file_name);

    static char buf[9999];
    snprintf(buf, sizeof(buf), "data/level/%s", file_name);
    FILE *f = fopen(buf, "rb");

    if(f) {
        static const char *delims = " ,\n\r";

        int row_count = 0;
        int max_column_count = 0;

        // First determine how many tiles there are
        while(fgets(buf, sizeof(buf), f)) {
            row_count++;
            int column_count = 0;

            char *token;
            char *b = buf;
            while((token = strtok(b, delims)) != NULL) {
                b = NULL;
                column_count++;
            }

            if(column_count > max_column_count) {
                max_column_count = column_count;
            }
        }

        fseek(f, 0, SEEK_SET);

        Level *level = calloc(1, offsetof(struct Level, data) + (row_count * max_column_count * sizeof(uint32_t)));
        level->rows = row_count;
        level->columns = max_column_count;

        // Parse the level/tile data
        int index = 0;
        while(fgets(buf, sizeof(buf), f)) {
            char *token;
            char *b = buf;

            while((token = strtok(b, delims)) != NULL) {
                int value = atoi(token);
                switch(value) {
                    case -1:
                        level->data[index] = ATLAS_SPRITE_EMPTY;
                        break;
                    default:
                        level->data[index] = value;
                        break;
                }

                b = NULL;
                index++;
            }
        }

        fclose(f);
        return level;
    }

    return NULL;
}

Level * load_next_level(void) {
    Level *level = get_next_level(get_next_level_name());
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
