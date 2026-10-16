// MasterTileComms.hpp

#pragma once

#include <cstdint>
#include <array>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "TileComms.hpp"
#include "SharedParameters.hpp"
#include "TileCommsParameters.hpp"
#include "TilePacket.hpp"
#include "Edge.hpp"

class MasterTileComms : public TileComms {
public:
    MasterTileComms();

    void begin() override;

    void resetAllTileInfosUpdated();
    // void resetAllEdgeInfosUpdated();
    bool infosMatch();

    uint8_t curNumTiles;
    std::array<TileState, MAX_NUM_TILES> tiles_; // .connected is only set true if both its tileinfo and edgeinfo are updated
    void resetTileConnections();

    bool queueSyncCommand();
    bool queueLedTestCommand();

    
    // TEST printing
    void printTileStates();

private:

    // LOOPING
    TaskHandle_t communicationTaskHandle_ = nullptr;
    static void communicationTaskEntry(void* parameter);
    void communicationTask() override;

    // TILE INFOS
    std::array<bool, MAX_NUM_TILES> tileInfosUpdated_;

    // // EDGE INFOS
    // std::array<bool, MAX_NUM_TILES> edgeInfosUpdated_;
    
    // PACKET PROCESSING
    bool mustReceivePacket(const TilePacket& packet);
    void processNewPacket(TilePacket& packet, uint8_t recevingEdgeId);
    void executePacketRequest(TilePacket& packet, uint8_t recevingEdgeId);

    // SYNC PACKET CONSTRUCTIONS
    void constructSyncCommand(SerializedMessage& buffer);
    void constructTileInfoCommand(SerializedMessage& buffer);
    void constructLedTestCommand(SerializedMessage& buffer);

};

