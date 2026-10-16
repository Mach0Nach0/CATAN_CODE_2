// Tile.hpp

#pragma once

#include <cstdint>
#include <array>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "SharedParameters.hpp"
#include "BasicTileComms.hpp"
#include "LEDController.hpp"

class Tile {
public:
    Tile(
        uint8_t tileId
    );

    void begin();

private:
    TaskHandle_t tileTaskHandle_ = nullptr;
    static void tileTaskEntry(void* parameter);
    void tileTask();

    uint8_t tileId_;
    uint8_t tileNumber_;
    ResourceType tileResource_;

    BasicTileComms comms_;
    LEDController led_;

    // COMMANDS
    TileCommand command_;
    void executeCommand(int64_t curTime);
    
};

