#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "../util/image.h"
#include "world.h"

World emptyWorld (int height, int width) {
    World world = {
        .size_y = height,
        .size_x = width,
        .tiles = malloc(sizeof(Tile) * height),
        .players = malloc(sizeof(Player) * MAX_PLAYERS),
        .beasts = malloc(sizeof(Player) * MAX_BEASTS),
    };

    for (int y = 0; y < height; y++) {
        world.tiles[y] = malloc(sizeof(Tile) * width);
    }

    for (int i = 0; i < MAX_PLAYERS; i++) {
        world.players[i] = NULL;
    }

    for (int i = 0; i < MAX_BEASTS; i++) {
        world.beasts[i] = NULL;
    }

    return world;
}

int populateWorld (World* world, Image map) {
    int ntiles = 0;

    for (int y = 0; y < map.height; y++) {
        for (int x = 0; x < map.width; x++) {
            Tile tile;
            switch (pixelValue(map.pixels[y][x])) {
                case 0xffffff:
                    tile = tiles[AIR];
                    break;
                case 0x000000:
                    tile = tiles[WALL];
                    break;
                case 0x00ff00:
                    tile = tiles[BUSH];
                    break;
                case 0xff00ff:
                    tile = tiles[CAMPSITE];
                    break;
            }
            world->tiles[y][x] = tile;
            ntiles++;
        }
    }

    return ntiles;
}

int populateWorldWithAir (World* world) {
    int ntiles = 0;

    for (int y = 0; y < world->size_y; y++) {
        for (int x = 0; x < world->size_x; x++) {
            world->tiles[y][x] = tiles[AIR];
            ntiles++;
        }
    }

    return ntiles;
}

void printWorld (World world) {
    for (int y = 0; y < world.size_y; y++) {
        for (int x = 0; x < world.size_x; x++) {
            bool printed_entity = false;
            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (world.players[i] == NULL) {
                    continue;
                }
                if (world.players[i]->pos_y == y && world.players[i]->pos_x == x) {
                    printf("%s%d ", FG_BLUE, i + 1);
                    printed_entity = true;
                    break;
                }
            }

            if (!printed_entity) {
                for (int i = 0; i < MAX_BEASTS; i++) {
                    if (world.beasts[i] == NULL) {
                        continue;
                    }
                    if (world.beasts[i]->pos_y == y && world.beasts[i]->pos_x == x) {
                        printf("%s* ", FG_RED);
                        printed_entity = true;
                        break;
                    }
                }
            }

            if (!printed_entity) {
                printf("%s%s ", world.tiles[y][x].color, world.tiles[y][x].symbol);
            }
        }
        puts(FG_NORMAL);
    }
}

char* playerTypeString (PlayerType type) {
    switch ((PlayerType) type) {
        case HUMAN:
            return "HUMAN";
        case CPU:
            return "CPU";
        case BEAST:
            return "BEAST";
    }
}

void printOnePlayerDetails (Player* player) {
    printf("Y/X \tCarry \tBudget \tDeaths \tType\n");
    printf("%d/%d \t%d \t%d \t%d \t%s\n",
           player->pos_y, player->pos_x, player->carrying,
           player->budget, player->deaths, playerTypeString(player->type));
}

void printPlayerDetails (World world) {
    printf("Player \tPort \tY/X \tCarry \tBudget \tDeaths \tType\n");

    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (world.players[i] == NULL) {
            printf("[%d] \t????? \t?/? \t? \t? \t? \t?\n", i + 1);
            continue;
        }

        Player player = *world.players[i];

        printf("[%d] \t%d \t%d/%d \t%d \t%d \t%d \t%s\n",
               i + 1, player.port, player.pos_y, player.pos_x, player.carrying,
               player.budget, player.deaths, playerTypeString(player.type));
    }
}

void randomCoordinates (World world, int* pos_y, int* pos_x) {
    *pos_y = rand() % world.size_y;
    *pos_x = rand() % world.size_x;
}

void randomFreeCoordinates (World world, int* pos_y, int* pos_x) {
    int y, x;

    // Don't try more than 100 coordinates
    for (int iter = 0; iter <= 100; iter++) {
        randomCoordinates(world, &y, &x);

        bool player_on_tile = false;
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (world.players[i] == NULL) {
                continue;
            } else if (world.players[i]->pos_y == y && world.players[i]->pos_x == x) {
                player_on_tile = true;
            }
        }

        bool beast_on_tile = false;
        for (int i = 0; i < MAX_BEASTS; i++) {
            if (world.beasts[i] == NULL) {
                continue;
            } else if (world.beasts[i]->pos_y == y && world.beasts[i]->pos_x == x) {
                beast_on_tile = true;
            }
        }

        if (world.tiles[y][x].id == AIR && !player_on_tile && !beast_on_tile) {
            break;
        }
    }

    *pos_y = y;
    *pos_x = x;
}

Player* addPlayer (World* world, int pos_y, int pos_x, PlayerType type) {
    if (pos_y > world->size_y || pos_x > world->size_x) {
        return NULL;
    }

    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (world->players[i] == NULL) {
            world->players[i] = malloc(sizeof(Player));
            Player* player = world->players[i];
            *player = (Player) {
                    .pos_y = pos_y,
                    .pos_x = pos_x,
                    // Save the original spawn location as the respawn point
                    .res_y = pos_y,
                    .res_x = pos_x,
                    .bush_timer = 0,
                    .carrying = 0,
                    .budget = 0,
                    .deaths = 0,
                    .type = type,
                    .queued_action = NULL,
            };

            return player;
        }
    }

    return NULL;
}

Player* addBeast (World* world, int pos_y, int pos_x) {
    if (pos_y > world->size_y || pos_x > world->size_x) {
        return NULL;
    }

    for (int i = 0; i < MAX_BEASTS; i++) {
        if (world->beasts[i] == NULL) {
            world->beasts[i] = malloc(sizeof(Player));
            Player* beast = world->beasts[i];
            *beast = (Player) {
                .pos_y = pos_y,
                .pos_x = pos_x,
                .type = BEAST,
                .queued_action = NULL,
            };

            return beast;
        }
    }

    return NULL;
}

int removePlayer (World* world, Player* player) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (world->players[i] == NULL) {
            continue;
        }
        if (world->players[i] == player) {
            world->players[i] = NULL;
        }
    }

    free(player);

    return 0;
}

int removeBeast (World* world, Player* beast) {
    for (int i = 0; i < MAX_BEASTS; i++) {
        if (world->beasts[i] == NULL) {
            continue;
        }
        if (world->beasts[i] == beast) {
            world->beasts[i] = NULL;
        }
    }

    free(beast);

    return 0;
}

void killPlayer (Player* player) {
    player->pos_y = player->res_y;
    player->pos_x = player->res_x;
    player->carrying = 0;
    player->deaths++;
}

int countPlayers (World world) {
    int n = 0;

    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (world.players[i] != NULL) {
            n++;
        }
    }

    return n;
}

int countBeasts (World world) {
    int n = 0;

    for (int i = 0; i < MAX_BEASTS; i++) {
        if (world.beasts[i] != NULL) {
            n++;
        }
    }

    return n;
}

bool inVisionRange (int pos_y, int pos_x, int y, int x) {
    return (y <= pos_y + VISION_RANGE
         && y >= pos_y - VISION_RANGE
         && x <= pos_x + VISION_RANGE
         && x >= pos_x - VISION_RANGE);
}

int movePlayer (World world, Player* player, Direction direction) {
    int y = player->pos_y, x = player->pos_x;

    switch (direction) {
        case RIGHT:
            x++;
            break;
        case LEFT:
            x--;
            break;
        case DOWN:
            y++;
            break;
        case UP:
            y--;
            break;
        default:
            break;
    }

    // The tile at desired location
    Tile *tile = &world.tiles[y][x];

    // Don't let players through walls
    if (tile->id == WALL) {
        return -1;
    }

    // The entity that our player will collide with (nullable)
    Player *colliding_entity = NULL;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (world.players[i] == NULL) continue;
        if (world.players[i]->pos_y == y && world.players[i]->pos_x == x) {
            colliding_entity = world.players[i];
            break;
        }
    }
    for (int i = 0; i < MAX_BEASTS; i++) {
        if (world.beasts[i] == NULL) continue;
        if (world.beasts[i]->pos_y == y && world.beasts[i]->pos_x == x) {
            colliding_entity = world.beasts[i];
            break;
        }
    }

    bool died = false;
    if (colliding_entity) {
        if (tile->id == CAMPSITE) {
            // Don't allow players into occupied campsites
            return -2;
        }
        if (tile->id == BUSH) {
            // Don't allow players into occupied bushes
            return -3;
        }
        if (colliding_entity->res_y == colliding_entity->pos_y
        &&  colliding_entity->res_x == colliding_entity->pos_x) {
            // Don't allow spawn kills
            return -4;
        }

        int death_box_value = player->carrying + colliding_entity->carrying;
        if (death_box_value > 0) {
            // Don't spawn a death box when it has no value
            *tile = tiles[DEATH_BOX];
            tile->value = death_box_value;
        }
        // Beasts don't die
        if (colliding_entity->type != BEAST) {
            killPlayer(colliding_entity);
        }
        if (player->type != BEAST) {
            died = true;
        }
    } else if (player->type != BEAST) {
        // Beasts can't collect stuff
        switch (tile->id) {
            case COIN:
            case TREASURE:
            case LARGE_TREASURE:
            case DEATH_BOX:
                if (player->bush_timer == 2) break;
                player->carrying += tile->value;
                *tile = tiles[AIR];
                break;
            case CAMPSITE:
                if (player->bush_timer == 2) break;
                player->budget += player->carrying;
                player->carrying = 0;
                break;
        }
    }

    if (died) {
        killPlayer(player);
    } else if (player->bush_timer != 1 && player->bush_timer != 2) {
        // bush_timer=1 is the state whenever a player has just entered a bush, so
        // 0 means they are moving normally and >2 means they have waited at least one tick
        player->pos_y = y;
        player->pos_x = x;
    }

    if (player->bush_timer > 2) {
        player->bush_timer = 0;
    }

    // Start the bush timer, it's later incremented during the game loop
    // and reset whenever a player with a counter 0 or >=2 moves
    if (tile->id == BUSH) {
        player->bush_timer++;
    }

    return 0;
}

int addCollectible (World* world, int pos_y, int pos_x, TileID collectible) {
    if (collectible != COIN
    &&  collectible != TREASURE
    &&  collectible != LARGE_TREASURE) {
        return -1;
    }

    Tile* tile = &world->tiles[pos_y][pos_x];
    if (tile->id != AIR) {
        return -2;
    }

    *tile = tiles[collectible];

    return 0;
}

int addCollectibleRandomly (World* world, TileID collectible) {
    int y, x;
    randomFreeCoordinates(*world, &y, &x);

    return addCollectible(world, y, x, collectible);
}
