// TileComms.hpp

#pragma once

#include <cstdint>
#include <array>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "SharedParameters.hpp"
#include "TileCommsParameters.hpp"
#include "TilePacket.hpp"
#include "Edge.hpp"

class TileComms {
public:
    TileComms(uint8_t numEdges);

    virtual void begin();

protected:

    // EDGES
    uint8_t numEdges_;
    std::array<Edge, MAX_NUM_EDGES> edges_;

    // LOOPING
    virtual void communicationTask();
    std::array<bool, MAX_NUM_EDGES> msgSentToEdge_;

    // MESSAGE QUEUE
    // Guards sendQueue_/queueSize_/queueHead_/queueTail_, which are touched both
    // by the comms task (dequeue/send) and by whichever task calls queueMessage()
    // (e.g. the button-polling task on the master), which run concurrently.
    SemaphoreHandle_t queueMutex_ = nullptr;
    std::array<SerializedMessage, MAX_QUEUE_SIZE> sendQueue_;
    std::array<uint8_t, MAX_QUEUE_SIZE> queueExcludeEdge_; // Per-message edge to skip when broadcasting (NO_EXCLUDED_EDGE = none)
    std::size_t queueSize_ = 0;
    std::size_t queueTail_ = 0;
    std::size_t queueHead_ = 0;
    bool queueMessage(const SerializedMessage& message, uint8_t excludeEdge = NO_EXCLUDED_EDGE);
    bool dequeueMessage();
    bool sendMessageToEdge(Edge& edge);
    std::size_t currentMessageId_ = 0;
    uint16_t useCurMsgId();

    // STORED CREDENTIALS
    std::array<PacketCredentials, MAX_STORED_CREDENTIALS> storedCredentials_;
    std::size_t credsIndex_ = 0;
    void addCreds(const TilePacket& packet);
    bool packetSeen(const TilePacket& packet);

    // PACKET SERIALIZATION
    bool serializePacket(const TilePacket& packet, SerializedMessage& message);
    bool deserializePacket(TilePacket& packet, const SerializedMessage& message);
    
    // CHECKSUM
    uint16_t calculateChecksum(const TilePacket& packet);
    bool confirmChecksum(const TilePacket& packet);

};

