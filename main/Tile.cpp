// Tile.cpp

#include "Tile.hpp"

Tile::Tile(uint8_t tileId) : tileId_(tileId), comms_(tileId) {}

// Public
void Tile::begin() {
    comms_.begin();
    led_.begin();

    // Start the communication task
    BaseType_t result = xTaskCreate(
        tileTaskEntry,
        "tile_comms",
        4096,
        this,
        8,
        &tileTaskHandle_
    );
    if (result != pdPASS) {
        // Task creation failed
        abort();
    }
}

// Private
void Tile::tileTaskEntry(void* parameter) {
    auto* tile = static_cast<Tile*>(parameter);
    tile->tileTask();
}

void Tile::tileTask() {
    while (true) {
        // Read the actual tile number and resource

        // Give TileComms the tile number and resource
        comms_.syncTileInfo(tileNumber_, tileResource_);

        // If there is a command in the command queue, take it and execute it
        if (comms_.getNewCommand(command_)) {
            int64_t curTime = esp_timer_get_time() / 1000; // ms
            executeCommand(curTime);
        }

        // Check for updated edge infos
        if (comms_.allEdgeInfosUpdated()) {
            // Reset the edge info updated bool array
            comms_.resetAllEdgeInfosUpdated();

            // Queue an edge info to master packet
            comms_.queueTileStateToMaster();
        }

        vTaskDelay(1);
    }
}

void Tile::executeCommand(int64_t curTime) {
    ESP_LOGI("EXECUTING COMMAND", "EXECUTING COMMAND");

    if (command_.type == PacketType::COMMANDSync) {
        comms_.queueEdgeInfo(curTime);
    }

    // if (command_.type == PacketType::COMMANDSendTileInfo) {
    //     comms_.queueTileInfoToMaster();
    // }

    if (command_.type == PacketType::COMMANDLEDTest) {
        led_.beginOnForDuration(1000);
    }
}



