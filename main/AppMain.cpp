// AppMain.cpp

#include <cstdint>
#include <array>
#include "driver/gpio.h"

#include "SharedParameters.hpp"
#include "Tile.hpp"
#include "MasterTile.hpp"

// Specific tile ID
static constexpr uint8_t TILE_ID = 1;

// Tiles
// static Tile tile(TILE_ID);
static MasterTile masterTile;

extern "C" void app_main()
{
    // tile.begin();
    masterTile.begin();
}

