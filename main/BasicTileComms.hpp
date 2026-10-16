// BasicTileComms.hpp

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

class BasicTileComms : public TileComms {
public:
    BasicTileComms( 
        uint8_t tileID
    );

    void begin() override;

    // From the Tile class:
    void syncTileInfo(uint8_t& updatedNumber, ResourceType& updatedResource);
    bool getNewCommand(TileCommand& buffer);

    // ADJACENT TILE INFO
    bool queueEdgeInfo(int64_t curTime);
    bool queueTileStateToMaster();
    // bool queueTileInfoToMaster();
    bool allEdgeInfosUpdated();
    void resetAllEdgeInfosUpdated();

private:

    // LOOPING
    TaskHandle_t communicationTaskHandle_ = nullptr;
    static void communicationTaskEntry(void* parameter);
    void communicationTask() override;

    // TILE AND EDGE INFO
    uint8_t tileId_;
    TileInfo tileInfo_;
    void constructEdgeInfo(SerializedMessage& buffer); // For edge exchange
    void constructTileStateToMaster(SerializedMessage& buffer); // To send to master
    // void constructTileInfoToMaster(SerializedMessage& buffer); // To send to master: tile number and resource
    bool edgeIsWaiting_ = false;
    bool edgeWaitDone_ = false;
    int64_t edgeWaitStartTime_ = 0; // ms
    void edgeStartWaiting(int64_t curTime);

    // ADJACENT TILE INFO
    std::array<EdgeInfo, BASIC_NUM_EDGES> adjEdgeInfo_;
    std::array<bool, BASIC_NUM_EDGES> adjEdgeInfoUpdated_;

    // Tile commands QUEUE
    std::array<TileCommand, MAX_COMMANDS_QUEUE_SIZE> tileCommandQueue_;
    std::size_t commandQueueSize_ = 0;
    std::size_t commandQueueTail_ = 0;
    std::size_t commandQueueHead_ = 0;
    bool queueCommand(const TileCommand& command);
    bool dequeueCommand();
    
    // PACKET PROCESSING
    bool mustReceivePacket(const TilePacket& packet);
    void processNewPacket(TilePacket& packet, uint8_t recevingEdgeId); // decrease ttl and queue, or receive and terminate, etc. 
    void executePacketRequest(TilePacket& packet, uint8_t recevingEdgeId);

    // GROUPS
    uint16_t groups_ = 0x0u; // The MSB is group number 15, LSB is group id 0
    uint8_t numGroups_ = 0;
    bool assignGroup(uint16_t groupId);
    bool unassignGroup(uint16_t groupId);

};

