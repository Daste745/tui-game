#pragma once


#include <stdbool.h>

#include "../util/color.h"
#include "../util/image.h"
#include "entity.h"
#include "tile.h"

#define MAX_PLAYERS  4
#define MAX_BEASTS   8
#define VISION_RANGE 2

// Map data buffer needs to hold at most:
// - 128x128 tiles
// - Player position (y, x)
// - MAX_PLAYERS players (y, x, deaths, type, carrying (4 bytes), budget (4 bytes))
// - MAX_BEASTS beasts (y, x)
#define MAP_DATA_SIZE(world)                                                    \
    world.size_y* world.size_x + 2 + MAX_PLAYERS*(4 + ((int)sizeof(int)) * 2) + \
        MAX_BEASTS * 2

typedef struct World {
    int size_y;
    int size_x;
    Tile** tiles;
    Player* players[MAX_PLAYERS];
    Player* beasts[MAX_BEASTS];
} World;

World emptyWorld(int height, int width);
int populateWorld(World* world, Image map);
int populateWorldWithAir(World* world);

void printWorld(World world);
char* playerTypeString(PlayerType type);
void printOnePlayerDetails(Player* player);
void printPlayerDetails(World world);

void randomCoordinates(World world, int* pos_y, int* pos_x);
void randomFreeCoordinates(World world, int* pos_y, int* pos_x);

Player* addPlayer(World* world, int pos_y, int pos_x, PlayerType type);
Player* addBeast(World* world, int pos_y, int pos_x);
int removePlayer(World* world, Player* player);
int removeBeast(World* world, Player* beast);
void killPlayer(Player* player);
int countPlayers(World world);
int countBeasts(World world);
bool inVisionRange(int pos_y, int pos_x, int y, int x);
int movePlayer(World world, Player* player, Direction direction);

int addCollectible(World* world, int pos_y, int pos_x, TileID collectible);
int addCollectibleRandomly(World* world, TileID collectible);
