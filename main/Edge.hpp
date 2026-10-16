// Edge.hpp

#pragma once

#include <cstdint>
#include <array>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "SharedParameters.hpp"
#include "TileCommsParameters.hpp"
#include "TilePacket.hpp"

class Edge {
public:
    Edge();

    void begin(uint8_t id, gpio_num_t pin);

    // Returns true if the edge is available to initialize communication,
    // then store the message in curMsgToSend.
    // Manage availability.
    // Returns false if the edge is currently busy.
    bool loadMsgFromQueue(SerializedMessage &message);
    
    void communicate();
    bool getNewMessage(SerializedMessage& buffer);
    uint8_t edgeId;

private:
    gpio_num_t commPin_;
    
    bool available_ = false;
    bool receiving_ = false;
    bool transmitting_ = false;
    bool finishedTransmitting_ = true;

    int64_t transmitLastBitTime_ = 0;
    std::size_t transmitCurBit_ = 0;
    // int64_t transmitBackoffUntil_ = 0; // Randomized per-message delay before driving the line
    SerializedMessage curMsgToSend_;

    int64_t receiveNextSampleTime_ = 0;
    std::size_t receiveCurBit_ = 0;
    std::size_t receiveTargetBits_ = MAX_BITS; // Updated once the real payloadSize is known
    bool hasReceivedBit_ = false;
    SerializedMessage receivedMsg_;

    bool lowPulsePending_ = false; // A LOW reading is being debounced before committing to a new reception
    int64_t lowPulseStartTime_ = 0;

    bool receivedNewMessage_ = false; // Becomes true if there is a new message, false once the master takes it.

    void transmit(int64_t curTime); // Transmit from the queue, if there is anything.
    void receive(int64_t curTime);

    // Pull the line low (HIGH) or release the line (LOW)
    void setLine(bool on);
    bool readLine();

};


