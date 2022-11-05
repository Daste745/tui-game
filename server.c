#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "beast-manager.h"
#include "config.h"
#include "game/entity.h"
#include "game/world.h"
#include "util/api.h"
#include "util/image.h"
#include "util/socket.h"

#define ERROR(msg)          \
    {                       \
        perror(msg);        \
        exit(EXIT_FAILURE); \
    }

World world;
int tick = 0;
bool server_active = true;
pthread_t keyboard_thread, game_thread, waiter_thread;
int server_socket;
int beast_handler_pid;

// Changes state of `server_active` and stops any running threads/processes
void endGame() {
    pthread_cancel(waiter_thread);
    kill(beast_handler_pid, SIGUSR2);

    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (world.players[i] == NULL) {
            continue;
        }
        pthread_cancel(world.players[i]->thread);
    }

    server_active = false;
}

// Runs in keyboard_thread
void keyboardHandler() {
    struct termios old_term_settings, new_term_settings;

    tcgetattr(STDIN_FILENO, &old_term_settings);
    new_term_settings = old_term_settings;
    // Disable terminal canonical mode
    // This sends stdin immediately without buffering
    new_term_settings.c_lflag &= (~ICANON & ~ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_term_settings);

    while (server_active) {
        unsigned char key = getchar();

        pthread_t a;

        switch (key) {
            case 'Q':
            case 'q':
                endGame();
                break;
            case 'c':
                addCollectibleRandomly(&world, COIN);
                break;
            case 't':
                addCollectibleRandomly(&world, TREASURE);
                break;
            case 'T':
                addCollectibleRandomly(&world, LARGE_TREASURE);
                break;
            case 'B':
            case 'b':
                // The SIGUSR1 signal is handled as "add a new beast" in the beast
                // manager process
                kill(beast_handler_pid, SIGUSR1);
                break;
            default:
                putchar(key);
                break;
        }

        usleep(1000);
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &old_term_settings);
    pthread_exit(NULL);
}

// Runs in game_thread
void gameLoop() {
    while (server_active) {
        // Logic
        for (int i = 0; i < MAX_PLAYERS + MAX_BEASTS; i++) {
            // Combined for players and beasts so we don't have to write this code twice
            Player* entity;
            if (i < MAX_PLAYERS) {
                entity = world.players[i];
            } else {
                entity = world.beasts[i - MAX_PLAYERS];
            }

            if (entity == NULL) {
                continue;
            }

            if (entity->bush_timer != 0) {
                entity->bush_timer++;
            }

            if (entity->queued_action == NULL) {
                continue;
            }

            movePlayer(world, entity, entity->queued_action->direction);
            // Player was moved to their desired location, so remove the action from the
            // queue
            entity->queued_action = NULL;
        }

        // Render
        system("clear");
        printWorld(world);
        printf("\nTick: %d \tWorld size: %dx%d\nPlayers: %d/%d\tBeasts: %d/%d\n\n",
               tick, world.size_y, world.size_x, countPlayers(world), MAX_PLAYERS,
               countBeasts(world), MAX_BEASTS);
        printPlayerDetails(world);

        // Post render
        tick++;
        sleep(1);
    }
}

// Packs the map, players and beasts into a seriazible format and sends to client
long sendMapData(int sock, Player* player) {
    char data[MAP_DATA_SIZE(world)];

    // Serialize tiles
    for (int y = 0; y < world.size_y; y++) {
        for (int x = 0; x < world.size_x; x++) {
            int index = world.size_x * y + x;

            if (inVisionRange(player->pos_y, player->pos_x, y, x)) {
                data[index] = (char)world.tiles[y][x].id;
            } else {
                data[index] = (char)0;
            }
        }
    }

    // Player position
    int position_offset = world.size_y * world.size_x;
    data[position_offset] = (char)player->pos_y;
    data[position_offset + 1] = (char)player->pos_x;

    // Serialize players
    int players_offset = position_offset + 2;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        int index = players_offset + i * 4 + i * ((int)sizeof(int) * 2);

        if (world.players[i] == NULL ||
            !inVisionRange(player->pos_y, player->pos_x, world.players[i]->pos_y,
                           world.players[i]->pos_x)) {
            // Send -1/-1 y/x to indicate that this data is invalid and the requester
            // can't see this player
            data[index] = (char)-1;
            data[index + 1] = (char)-1;
            continue;
        }

        data[index] = (char)world.players[i]->pos_y;
        data[index + 1] = (char)world.players[i]->pos_x;
        data[index + 2] = (char)world.players[i]->type;

        if (player == world.players[i]) {
            // This data is only available to the player which requests it
            // carrying, budget and deaths are unknown to other players

            data[index + 3] = (char)world.players[i]->deaths;

            // Memcpy nicely copies raw bytes of an int to a char
            // Which is necessary here, as we often handle values larger than 128
            memcpy(&data[index + 4], &world.players[i]->carrying, sizeof(int));
            memcpy(&data[index + 4 + sizeof(int)], &world.players[i]->budget,
                   sizeof(int));
        } else {
            // -1 are later printed as '?' to let players know they don't have access to
            // this info
            int data_unavail = -1;

            data[index + 3] = (char)data_unavail;

            memcpy(&data[index + 4], &data_unavail, sizeof(int));
            memcpy(&data[index + 4 + sizeof(int)], &data_unavail, sizeof(int));
        }
    }

    // Serialize beasts
    int beasts_offset = players_offset + MAX_PLAYERS * (4 + ((int)sizeof(int)) * 2);
    for (int i = 0; i < MAX_BEASTS; i++) {
        int index = beasts_offset + i * 2;

        if (world.beasts[i] == NULL ||
            !inVisionRange(player->pos_y, player->pos_x, world.beasts[i]->pos_y,
                           world.beasts[i]->pos_x)) {
            data[index] = (char)-1;
            data[index + 1] = (char)-1;
            continue;
        } else {
            data[index] = (char)world.beasts[i]->pos_y;
            data[index + 1] = (char)world.beasts[i]->pos_x;
        }
    }

    return send(sock, data, sizeof(data), 0);
}

// Runs in waiter_thread
void waitForClientConnection() {
    struct sockaddr_in client;
    socklen_t client_len = sizeof(client);

    // Accept new connection
    int client_socket = accept(server_socket, (struct sockaddr*)&client, &client_len);
    if (client_socket == -1) {
        pthread_exit(NULL);
    }

    // Get client connection info
    char hoststr[NI_MAXHOST];
    char portstr[NI_MAXSERV];
    getnameinfo((struct sockaddr*)&client, client_len, hoststr, sizeof(hoststr),
                portstr, sizeof(portstr), NI_NUMERICHOST | NI_NUMERICSERV);
    int port = (int)strtol(portstr, NULL, 10);
    ClientInfo client_info = {
        .socket = client_socket,
        .port = port,
    };

    pthread_exit(&client_info);
}

// Runs in player->thread, processes client commands
void handleClientConnection(ClientInfo* client_info) {
    char buffer[1024] = { 0 };
    bool connected = true;
    Player* player;

    while (connected) {
        // Wait for a message, the client always knocks first and only then we
        // sendResponse
        long bytes_received = recv(client_info->socket, buffer, sizeof(buffer), 0);
        if (bytes_received <= 0) {
            // The transport was dropped for some reason (client was killed or
            // connection interrupted) In this case, we remove the player if they had
            // already joined and terminate the handler
            if (player != NULL) {
                removePlayer(&world, player);
            }
            pthread_exit(NULL);
        }

        // Read command from the first byte and argument from the second byte of the
        // client's message
        Command command = (Command)buffer[0];
        int argument = (int)buffer[1];
        memset(&buffer, 0, sizeof(buffer));

        switch ((Command)command) {
            case JOIN: {
                switch ((PlayerType)argument) {
                    case HUMAN:
                    case CPU:
                        if (countPlayers(world) >= MAX_PLAYERS) {
                            // Server is full, sendResponse with SERVER_FULL and close
                            // the connection
                            sendResponse(client_info->socket, SERVER_FULL);
                            connected = false;
                        } else {
                            int y, x;
                            randomFreeCoordinates(world, &y, &x);

                            // Arguments for JOIN are always of type PlayerType
                            player = addPlayer(&world, y, x, (PlayerType)argument);
                            player->port = client_info->port;
                            player->thread = client_info->thread;
                            sendResponse(client_info->socket, JOINED);
                        }
                        break;
                    case BEAST:
                        if (countBeasts(world) >= MAX_BEASTS) {
                            sendResponse(client_info->socket, SERVER_FULL);
                            connected = false;
                        } else {
                            int y, x;
                            randomFreeCoordinates(world, &y, &x);

                            player = addBeast(&world, y, x);
                            player->port = client_info->port;
                            player->thread = client_info->thread;
                            sendResponse(client_info->socket, JOINED);
                        }
                        break;
                }
                break;
            }
            case LEAVE:
                if (player->type == BEAST) {
                    removeBeast(&world, player);
                } else {
                    removePlayer(&world, player);
                }
                sendResponse(client_info->socket, REMOVED);
                connected = false;
                break;
            case MOVE:
                player->queued_action = &(Action){ .direction = (Direction)argument };
                sendResponse(client_info->socket, MOVED);
                break;
            case WORLD_SIZE: {
                // Sends just char[2] - no Response here
                // Subtract 1, so we can send the max size of the world without adding
                // extra bytes
                char size_data[2] = { (char)(world.size_y - 1),
                                      (char)(world.size_x - 1) };
                send(client_info->socket, size_data, sizeof(size_data), 0);
                break;
            }
            case MAP:
                // Sends just map data - no Response here
                sendMapData(client_info->socket, player);
                break;
            default:
                break;
        }
    }
}

int main() {
    beast_handler_pid = fork();
    if (beast_handler_pid == 0) {
        beastManager();
        exit(EXIT_SUCCESS);
    }

    server_socket = getServerSocket(PORT);
    if (server_socket == -1) {
        ERROR("Error opening socket")
    } else if (server_socket == -2) {
        ERROR("Error binding to socket")
    }

    Image map_image = loadPPM(MAP_IMAGE);
    world = emptyWorld(map_image.height, map_image.width);
    populateWorld(&world, map_image);
    free(map_image.pixels);
    // Add some random collectibles around the map
    srand(time(NULL) * 100);
    for (int i = 0; i < 50; i++) {
        addCollectibleRandomly(&world, (TileID)rand() % 3 + 3);
    }

    // Game loop
    pthread_create(&game_thread, NULL, (void*)gameLoop, NULL);

    // Keyboard handler
    pthread_create(&keyboard_thread, NULL, (void*)keyboardHandler, NULL);

    // This loop waits for a new client connection, then:
    // - Rejects it if the server is full (countPlayers(world) >= MAX_PLAYERS)
    // - Accepts it and starts a new client handler thread2
    while (server_active) {
        // Create a waiter thread and wait for new client connections
        ClientInfo* client_info;
        pthread_create(&waiter_thread, NULL, (void*)waitForClientConnection, NULL);
        pthread_join(waiter_thread, (void**)&client_info);

        if (client_info == NULL) {
            printf("Connection on port %d failed\n", client_info->port);
            continue;
        }

        // Double check if the server is still server_active
        // This prevents a SEGFAULT during server shutdown
        if (server_active) {
            pthread_create(&client_info->thread, NULL, (void*)handleClientConnection,
                           client_info);
        }
    }

    // Wait until beast handler exits
    wait(NULL);

    // Clean up
    close(server_socket);
    printf("Server exit\n");

    return 0;
}
