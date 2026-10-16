// Edge.cpp

#include "Edge.hpp"
#include "esp_random.h"

Edge::Edge() //: 
    // edgeId(edgeId), 
    // commPin_(EDGE_PINS[edgeId])
{}

// Public
void Edge::begin(uint8_t id, gpio_num_t pin) {
    edgeId = id;
    commPin_ = pin;
    gpio_set_pull_mode(pin, GPIO_PULLUP_ONLY);
    setLine(LOW);
}

bool Edge::loadMsgFromQueue(SerializedMessage &message) {
    if (available_) {
        curMsgToSend_ = message;
        available_ = false;
        finishedTransmitting_ = false;
        return true;
    }
    return false;
}

void Edge::communicate() {
    // Get current time
    int64_t curTime = esp_timer_get_time();

    // If its not currently receiving, transmit messages
    if (!receiving_ && !finishedTransmitting_) {
        transmit(curTime);
    }

    // If its not currently transmitting, listen for messages
    if (!transmitting_) {
        receive(curTime);
    }

    // Set availability depending on receiving and transmitting, after they've
    // been updated above, so a message just loaded this cycle isn't reported
    // as available again before the caller sees transmitting_ flip true.
    available_ = (!receiving_) && (!transmitting_);
}

bool Edge::getNewMessage(SerializedMessage& buffer) {
    if (receivedNewMessage_) {
        buffer = receivedMsg_;
        receivedNewMessage_ = false;
        return true;
    }
    return false;
}

// Private
void Edge::transmit(int64_t curTime) {
    // If just started transmitting, save current time and set transmitting to true
    if (!transmitting_) {
        transmitLastBitTime_ = curTime;
        transmitCurBit_ = 0;
        transmitting_ = true;

        // return;
    }

    std::size_t byteIndex = transmitCurBit_ / BYTE_SIZE;
    std::size_t bitIndex  = transmitCurBit_ % BYTE_SIZE;

    // While the bits are not fully traversed,
    if (byteIndex < curMsgToSend_.length) {  
        bool bit = (curMsgToSend_.bytes[byteIndex] >> (BYTE_SIZE - 1 - bitIndex)) & 0x01;
        setLine(bit);
        // if it's past the bit's period, increment bit index and update the last bit time
        if (curTime - transmitLastBitTime_ >= BIT_PERIOD) {
            transmitCurBit_++;
            transmitLastBitTime_ += BIT_PERIOD;
        }
    }
    else {
        transmitting_ = false;
        transmitCurBit_ = 0;
        finishedTransmitting_ = true;
        setLine(LOW);
    }
}

void Edge::receive(int64_t curTime) {
    // Debounce the start of a new reception: a real message's first bit is always
    // held LOW for close to a full BIT_PERIOD (the packetId's MSB is forced to 1),
    // so require the line to stay continuously LOW for RECEIVE_DEBOUNCE_US before
    // committing, instead of triggering on the very first LOW sample. This filters
    // out brief noise glitches that would otherwise anchor the bit-sampling clock
    // to the wrong start time.
    if (!receiving_) {
        if (readLine()) {
            if (!lowPulsePending_) {
                lowPulsePending_ = true;
                lowPulseStartTime_ = curTime;
                return;
            }

            if (curTime - lowPulseStartTime_ < RECEIVE_DEBOUNCE_US) {
                return;
            }

            // Confirmed: commit to a new reception, anchored to when the pulse
            // actually started so bit sampling stays correctly centered.
            lowPulsePending_ = false;

            receiveCurBit_ = 0;
            receiving_ = true;

            receivedMsg_.bytes.fill(0);
            receivedMsg_.length = 0;

            receiveNextSampleTime_ = lowPulseStartTime_ + (BIT_PERIOD / 2);

            receivedNewMessage_ = false;

            // Fallback until the real payloadSize byte has been received
            receiveTargetBits_ = MAX_BITS;
        }
        else {
            // Line went back high before the debounce window elapsed - just a glitch.
            lowPulsePending_ = false;
        }

        return;
    }

    if (curTime < receiveNextSampleTime_) {
        return;
    }

    std::size_t byteIndex = receiveCurBit_ / BYTE_SIZE;
    std::size_t bitIndex  = receiveCurBit_ % BYTE_SIZE;

    receivedMsg_.bytes[byteIndex] |= static_cast<uint8_t>(readLine() << (BYTE_SIZE - 1 - bitIndex));
    receiveCurBit_++;
    receiveNextSampleTime_ += BIT_PERIOD;

    // As soon as the payloadSize byte is fully received, compute the packet's real length
    // instead of waiting for the fixed maximum-size window.
    if (byteIndex == PAYLOAD_SIZE_BYTE_INDEX && bitIndex == BYTE_SIZE - 1) {
        std::size_t payloadSize = receivedMsg_.bytes[PAYLOAD_SIZE_BYTE_INDEX];
        if (payloadSize > MAX_PAYLOAD_SIZE) {
            payloadSize = MAX_PAYLOAD_SIZE;
        }
        receiveTargetBits_ = (HEADER_SIZE + payloadSize + CHECKSUM_SIZE) * BYTE_SIZE;
    }

    if (receiveCurBit_ >= receiveTargetBits_) {
        // Give this back to tilecomms
        receivedMsg_.length = receiveCurBit_ / BYTE_SIZE;
        receiving_ = false;

        receivedNewMessage_ = true;

        return;
    }

}

void Edge::setLine(bool on) {
    if (on) {
        gpio_set_direction(commPin_, GPIO_MODE_OUTPUT);
        gpio_set_level(commPin_, LOW);
    }
    else {
        gpio_set_direction(commPin_, GPIO_MODE_INPUT);
    }
}

bool Edge::readLine() {
    return gpio_get_level(commPin_) == LOW;
}
