// MasterTile.hpp

#pragma once

#include <cstdint>
#include <array>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "SharedParameters.hpp"
#include "MasterTileComms.hpp"

#define BUTTON_PIN GPIO_NUM_17

class MasterTile {
public:
    MasterTile();

    void begin();

private:
    TaskHandle_t masterTileTaskHandle_ = nullptr;
    static void masterTileTaskEntry(void* parameter);
    void masterTileTask();

    MasterTileComms comms_;

    // SYNC INFO
    bool mustSync_ = false;
    bool isSyncing_ = false;
    int64_t syncStartTime_ = 0; // ms
    void syncInfo(int64_t curTime);
    void updateTileMap();

    // TILES
    int8_t numTiles_ = 0;
    
};

