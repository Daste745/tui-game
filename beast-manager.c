#include <stdlib.h>
#include <pthread.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include "util/socket.h"
#include "config.h"
#include "util/api.h"

#define BETWEEN(n, lower, upper) (lower <= n && n <= upper)


bool beast_manager_active = true;
ClientInfo** beast_client_info;


// Calculates the sign of an int
int sign(int x) {
    return (x > 0) - (x < 0);
}

// Runs in beast->thread, simulates a beast
void runBeast (ClientInfo* client_info) {
    int sock = getClientSocket(HOST, PORT);
    if (sock <= 0) {
        pthread_exit(NULL);
    }
    client_info->socket = sock;

    char buffer[1024] = { 0 };

    // Join the game
    sendCommand(sock, JOIN, BEAST);
    recv(sock, buffer, sizeof(buffer), 0);
    if ((Response) buffer[0] == SERVER_FULL) {
        pthread_exit(NULL);
    }

    // Get world info and build empty world
    char world_size[2];
    sendCommand(sock, WORLD_SIZE, 0);
    recv(sock, world_size, sizeof(world_size), 0);
    World _world = emptyWorld((int) world_size[0] + 1, (int) world_size[1] + 1);
    populateWorldWithAir(&_world);

    char map_data[MAP_DATA_SIZE(_world)];
    Player* beast = malloc(sizeof(Player));
    srand(time(NULL) * 100);

    while (beast_manager_active) {
        sendCommand(sock, MAP, 0);

        memset(&map_data, 0, sizeof(map_data));
        recv(sock, map_data, sizeof(map_data), 0);

        loadMapData(map_data, &_world, beast);

        Player* player_in_range = NULL;
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (_world.players[i] == NULL) {
                continue;
            }

            player_in_range = _world.players[i];
            break;
        }

        Direction horizontal_direction;
        Direction vertical_direction;
        bool can_see_player = false;
        if (player_in_range != NULL) {
            // Calculate the offset between player and beast
            // We leave the sign, so it's easier to determine in which direction to look later

            int y_offset = player_in_range->pos_y - beast->pos_y;
            int x_offset = player_in_range->pos_x - beast->pos_x;
            unsigned int abs_y_offset = (sign(y_offset) == 1) ? y_offset : -y_offset;
            unsigned int abs_x_offset = (sign(x_offset) == 1) ? x_offset : -x_offset;

            // Sight calculation
            // 3 4 2 4 3
            // 4 1 1 1 4
            // 2 1 x 1 2
            // 4 1 1 1 4
            // 3 4 2 4 3
            //
            // x - beast (checking only in 5x5 radius)
            // 1 - player next to beast is always in vision range
            // 2 - player in line with beast could be obstructed by a wall
            // 3 - same as 2, but diagonally
            // 4 - check tile next to player that could cover them

            if (BETWEEN(y_offset, -1, 1) && BETWEEN(x_offset, -1, 1)) {  // 1
                vertical_direction   = (sign(y_offset) == 1) ? DOWN : UP;
                horizontal_direction = (sign(x_offset) == 1) ? RIGHT : LEFT;
                can_see_player = true;
            } else if ((abs_y_offset == 2  ^ abs_x_offset == 2)     // 2
                   ||  (abs_y_offset == 2 && abs_x_offset == 2)     // 3
                   ||  (abs_y_offset == 1 && abs_x_offset == 2)     // 4
                   ||  (abs_y_offset == 2 && abs_x_offset == 1)) {  // 4
                int check_y = beast->pos_y + sign(y_offset);
                int check_x = beast->pos_x + sign(x_offset);

                // Check tile in front of player (tile between beast and player)
                if (_world.tiles[check_y][check_x].id != WALL) {
                    vertical_direction   = (sign(y_offset) == 1) ? DOWN : UP;
                    horizontal_direction = (sign(x_offset) == 1) ? RIGHT : LEFT;
                    can_see_player = true;
                }
            }
        }

        Direction move_direction;
        if (!can_see_player) {
            // No player in sight - move randomly
            move_direction = (Direction) (rand() % 4);
        } else {
            int choice = rand() % 2;
            if (choice == 0) {
                move_direction = horizontal_direction;
            } else {
                move_direction = vertical_direction;
            }
        }

        sendCommand(sock, MOVE, move_direction);
        recv(sock, buffer, sizeof(buffer), 0);

        sleep(1);
    }
}

// Spawns new beast thread
void handleSIGUSR1 () {
    for (int i = 0; i < MAX_BEASTS; i++) {
        if (beast_client_info[i] != NULL) {
            continue;
        }

        ClientInfo *client_info = &(ClientInfo) { };
        pthread_create(&client_info->thread, NULL, runBeast, client_info);
        beast_client_info[i] = client_info;

        break;
    }
}

// Cancels all beast threads and sets beast_manager_active=false
void handleSIGUSR2 () {
    for (int i = 0; i < MAX_BEASTS; i++) {
        if (beast_client_info[i] == NULL) {
            continue;
        }

        pthread_cancel(beast_client_info[i]->thread);
        close(beast_client_info[i]->socket);
    }

    beast_manager_active = false;
}

// Runs as beast manager process
void beastManager () {
    srand(time(NULL) * 100);

    beast_client_info = malloc(sizeof(ClientInfo) * MAX_BEASTS);
    for (int i = 0; i < MAX_BEASTS; i++) {
        beast_client_info[i] = NULL;
    }

    signal(SIGUSR1, handleSIGUSR1);
    signal(SIGUSR2, handleSIGUSR2);

    while (beast_manager_active) {
        // Wait for a signal
        pause();
    }
}