#ifndef GAME_TILE_H
#define GAME_TILE_H

typedef struct Tile {
    int id;
    char* symbol;
    char* color;
    int value;
} Tile;

// clang-format off
typedef enum TileID {
    AIR            = 0,
    WALL           = 1,
    BUSH           = 2,
    COIN           = 3,
    TREASURE       = 4,
    LARGE_TREASURE = 5,
    DEATH_BOX      = 6,
    CAMPSITE       = 7,
} TileID;

static const Tile tiles[] = {
    [AIR]            = { AIR,            " ", FG_NORMAL,  0  },
    [WALL]           = { WALL,           "█", FG_GRAY,    0  },
    [BUSH]           = { BUSH,           "░", FG_GREEN,   0  },
    [COIN]           = { COIN,           "c", FG_YELLOW,  1  },
    [TREASURE]       = { TREASURE,       "t", FG_YELLOW,  10 },
    [LARGE_TREASURE] = { LARGE_TREASURE, "T", FG_YELLOW,  50 },
    [DEATH_BOX]      = { DEATH_BOX,      "D", FG_YELLOW,  0  },
    [CAMPSITE]       = { CAMPSITE,       "A", FG_MAGENTA, 0  },
};
// clang-format on

#endif  // GAME_TILE_H
