#pragma once

#include <pthread.h>

typedef enum PlayerType {
    HUMAN,
    CPU,
    BEAST,
} PlayerType;

typedef enum Direction {
    RIGHT,
    LEFT,
    DOWN,
    UP,
} Direction;

typedef struct Action {
    Direction direction;
} Action;

typedef struct Player {
    int pos_y;
    int pos_x;
    int res_y;
    int res_x;
    int bush_timer;
    int carrying;
    int budget;
    int deaths;
    PlayerType type;
    // Saves the action the player has requested, e.g. move right, or move down.
    // NULL if there is no queued action this tick.
    Action* queued_action;
    int port;
    pthread_t thread;
} Player;

