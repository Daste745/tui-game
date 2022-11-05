#pragma once

#include "../game/world.h"

typedef enum Command {
    JOIN,       // Join game
    LEAVE,      // Leave game
    MOVE,       // Move player
    MAP,        // Request map data
    WORLD_SIZE  // World size
} Command;

typedef enum Response {
    SERVER_FULL,
    JOINED,
    REMOVED,
    MOVED,
} Response;

long sendCommand(int sock, Command command, int argument);
long sendResponse(int sock, Response response);

// Loads map_data[MAP_DATA_SIZE(world)] bytes into world and current player into player
int loadMapData(const char* map_data, World* world, Player* player);
