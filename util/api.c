#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <stdio.h>
#include "api.h"


long sendCommand (int sock, Command command, int argument) {
    char message[2] = { (char) command, (char) argument };
    return send(sock, message, sizeof(message), 0);
}

long sendResponse (int sock, Response response) {
    char message[1] = { (char) response };
    return send(sock, message, sizeof(message), 0);
}

int loadMapData (const char* map_data, World* world, Player* player) {
    int index = 0;

    // Update map with tiles in view distance (5x5 around player)
    for (int y = 0; y < world->size_y; y++) {
        for (int x = 0; x < world->size_x; x++, index++) {
            Tile* tile = &world->tiles[y][x];

            if (map_data[index] == AIR) {
                if (tile->id == COIN
                ||  tile->id == TREASURE
                ||  tile->id == LARGE_TREASURE
                ||  tile->id == DEATH_BOX) {
                    // Stumbled upon a collectible, which has been picked up and is now AIR
                    *tile = tiles[AIR];
                }

                // Tile is AIR, so it's outside our vision range or just doesn't need to be changed
                continue;
            }

            // Other tiles are updated every time, so we can make sure everything except AIR is up to date
            *tile = tiles[(TileID) map_data[index]];
        }
    }

    // Read player position
    int player_pos_y = (int) map_data[index];
    int player_pos_x = (int) map_data[index + 1];
    index += 2;

    // Read player data
    for (int i = 0; i < MAX_PLAYERS; i++, index += 4 + 2 * sizeof(int)) {
        // Players with y=-1 and x=-1 should be treated as non-existent or outside of vision range
        if ((int) map_data[index]     == -1
        &&  (int) map_data[index + 1] == -1) {
            if (world->players[i] != NULL) {
                removePlayer(world, world->players[i]);
            }
            continue;
        }

        int pos_y = (int) map_data[index];
        int pos_x = (int) map_data[index + 1];
        PlayerType type = (PlayerType) map_data[index + 2];
        int deaths = (int) map_data[index + 3];

        // Add player if they are not already present
        Player *curr_player = world->players[i];
        if (curr_player == NULL) {
            // Construct player object and add it to the world
            world->players[i] = malloc(sizeof(Player));
            curr_player = world->players[i];
            *curr_player = (Player) { .type = type };
        }

        // Set player parameters every time
        curr_player->pos_y = pos_y;
        curr_player->pos_x = pos_x;
        curr_player->deaths = deaths;

        // Copy sizeof(int) bytes from chars into approperiate variables
        memcpy(&curr_player->carrying, &map_data[index + 4], sizeof(int));
        memcpy(&curr_player->budget, &map_data[index + 4 + sizeof(int)], sizeof(int));

        if (curr_player->pos_y == player_pos_y
        &&  curr_player->pos_x == player_pos_x) {
            *player = *curr_player;
        }
    }

    // Read beast data
    for (int i = 0; i < MAX_BEASTS; i++, index += 2) {
        // Beasts with y=-1 and x=-1 should be treated as non-existent or outside of vision range
        if ((int) map_data[index]     == -1
        &&  (int) map_data[index + 1] == -1) {
            if (world->beasts[i] != NULL) {
                removeBeast(world, world->beasts[i]);
            }
            continue;
        }

        Player* curr_beast = world->beasts[i];
        if (curr_beast == NULL) {
            world->beasts[i] = malloc(sizeof(Player));
            curr_beast = world->beasts[i];
            *curr_beast = (Player) { .type = BEAST };
        }
        
        curr_beast->pos_y = (int) map_data[index];
        curr_beast->pos_x = (int) map_data[index + 1];

        if (curr_beast->pos_y == player_pos_y
        &&  curr_beast->pos_x == player_pos_x) {
            *player = *curr_beast;
        }
    }

    return 0;
}
