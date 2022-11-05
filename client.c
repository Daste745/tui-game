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
pthread_t render_thread;
int client_socket;

void render() {
    char map_data[MAP_DATA_SIZE(world)];
    Player *player = malloc(sizeof(Player));

    while (connected) {
        sendCommand(client_socket, MAP, 0);

        memset(&map_data, 0, sizeof(map_data));
        recv(client_socket, map_data, sizeof(map_data), 0);

        loadMapData(map_data, &world, player);

        system("clear");
        printWorld(world);
        printOnePlayerDetails(player);

        sleep(1);
    }

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
    sendCommand(client_socket, JOIN, HUMAN);
    recv(client_socket, buffer, sizeof(buffer), 0);
    if ((Response)buffer[0] == SERVER_FULL) {
        ERROR("Server is full, cannot join")
    }

    // Get world info and build an empty world
    char world_size[2];
    sendCommand(client_socket, WORLD_SIZE, 0);
    recv(client_socket, world_size, sizeof(world_size), 0);
    // World size is sent as 1 lower, so we need to adjust for it here
    world = emptyWorld((int)world_size[0] + 1, (int)world_size[1] + 1);
    populateWorldWithAir(&world);

    // Render loop
    pthread_create(&render_thread, NULL, (void *)render, NULL);

    // Keyboard handler
    struct termios old_term_settings, new_term_settings;
    tcgetattr(STDIN_FILENO, &old_term_settings);
    new_term_settings = old_term_settings;
    // Disable terminal canonical mode
    // This sends stdin immediately without buffering
    new_term_settings.c_lflag &= (~ICANON & ~ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_term_settings);

    while (connected) {
        unsigned char key = getchar();

        switch (key) {
            case 'Q':
            case 'q':
                sendCommand(client_socket, LEAVE, 0);
                connected = false;
                break;
            case 'd':
                sendCommand(client_socket, MOVE, RIGHT);
                break;
            case 'a':
                sendCommand(client_socket, MOVE, LEFT);
                break;
            case 's':
                sendCommand(client_socket, MOVE, DOWN);
                break;
            case 'w':
                sendCommand(client_socket, MOVE, UP);
                break;
            default:
                continue;
        }

        // Wait for a response
        memset(buffer, 0, sizeof(buffer));
        recv(client_socket, buffer, sizeof(buffer), 0);

        Response response = (Response)buffer[0];

        if (response == REMOVED) {
            connected = false;
        }

        usleep(10000);
    }

    // Clean up
    tcsetattr(STDIN_FILENO, TCSANOW, &old_term_settings);
    close(client_socket);
    printf("Disconnected\n");

    return 0;
}
