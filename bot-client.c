#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <termios.h>
#include <unistd.h>

#include "config.h"
#include "game/entity.h"
#include "game/world.h"
#include "util/api.h"
#include "util/socket.h"

#define ERROR(msg)          \
    {                       \
        perror(msg);        \
        exit(EXIT_FAILURE); \
    }

World world;
bool connected = true;
pthread_t render_thread, keyboard_thread;
int client_socket;

void render() {
    char map_data[MAP_DATA_SIZE(world)];
    Player* player = malloc(sizeof(Player));
    srand(time(NULL) * 100);

    while (connected) {
        sendCommand(client_socket, MAP, 0);

        memset(&map_data, 0, sizeof(map_data));
        recv(client_socket, map_data, sizeof(map_data), 0);

        loadMapData(map_data, &world, player);

        Player* beast_in_sight = NULL;
        // Find if any beast is in sight
        for (int i = 0; i < MAX_BEASTS; i++) {
            if (world.beasts[i] == NULL) {
                continue;
            }

            beast_in_sight = world.beasts[i];
            break;
        }

        Direction horizontal;
        Direction vertical;
        if (beast_in_sight != NULL) {
            if (beast_in_sight->pos_y > player->pos_y) {
                // Beast below player
                vertical = UP;
            } else if (beast_in_sight->pos_y < player->pos_y) {
                // Beast above player
                vertical = DOWN;
            } else {
                // Choose random vertical direction
                vertical = (Direction)(rand() % 2 + 2);
            }

            if (beast_in_sight->pos_x > player->pos_x) {
                // Beast to the right of the player
                horizontal = LEFT;
            } else if (beast_in_sight->pos_x < player->pos_x) {
                // Beast to the left of the player
                horizontal = RIGHT;
            } else {
                // Choose random horizontal direction
                horizontal = (Direction)(rand() % 2);
            }
        } else {
            // Move randomly
            vertical = (Direction)(rand() % 2 + 2);
            horizontal = (Direction)(rand() % 2);
        }

        // Randomly decide in which direction to run (vertically or horizontally)
        int move_choice = rand() % 2;
        if (move_choice == 0) {
            sendCommand(client_socket, MOVE, vertical);
        } else {
            sendCommand(client_socket, MOVE, horizontal);
        }

        char buffer[1];
        recv(client_socket, buffer, sizeof(buffer), 0);

        // Print to screen
        system("clear");
        printWorld(world);
        printOnePlayerDetails(player);

        sleep(1);
    }

    pthread_exit(NULL);
}

void keyboardHandler() {
    struct termios old_term_settings, new_term_settings;

    tcgetattr(STDIN_FILENO, &old_term_settings);
    new_term_settings = old_term_settings;
    // Disable terminal canonical mode
    // This sends stdin immediately without buffering
    new_term_settings.c_lflag &= (~ICANON & ~ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_term_settings);

    while (connected) {
        unsigned char key = getchar();

        if (key == 'q' || key == 'Q') {
            sendCommand(client_socket, LEAVE, 0);
            pthread_cancel(render_thread);
            connected = false;
        }
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &old_term_settings);
    pthread_exit(NULL);
}

int main() {
    client_socket = getClientSocket(HOST, PORT);
    if (client_socket == -1) {
        ERROR("Error opening socket")
    } else if (client_socket == -2) {
        ERROR("Could not find host")
    } else if (client_socket == -3) {
        ERROR("Error connecting to socket")
    }

    char buffer[1024] = { 0 };

    // Join the game
    sendCommand(client_socket, JOIN, CPU);
    recv(client_socket, buffer, sizeof(buffer), 0);
    if ((Response)buffer[0] == SERVER_FULL) {
        ERROR("Server is full, cannot join")
    }

    // Get world info and build empty world
    char world_size[2];
    sendCommand(client_socket, WORLD_SIZE, 0);
    recv(client_socket, world_size, sizeof(world_size), 0);
    // World size is sent as 1 lower, so we need to adjust for it here
    world = emptyWorld((int)world_size[0] + 1, (int)world_size[1] + 1);
    populateWorldWithAir(&world);

    // Render loop
    pthread_create(&render_thread, NULL, (void*)render, NULL);

    // Keyboard handler (used only for quitting)
    pthread_create(&keyboard_thread, NULL, (void*)keyboardHandler, NULL);

    pthread_join(render_thread, NULL);
    pthread_join(keyboard_thread, NULL);

    // Clean up
    close(client_socket);
    printf("Disconnected\n");

    return 0;
}
