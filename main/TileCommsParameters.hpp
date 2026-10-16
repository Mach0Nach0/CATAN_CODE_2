// TileParameters.hpp

// only tilecomms gets this

#pragma once

#include <array>
#include <cstddef>
#include <cmath>
#include "driver/gpio.h"
#include "esp_timer.h"

#include "TilePacket.hpp"
#include "SharedParameters.hpp"

// Number of edges in each tile (hexagon: 6)
inline constexpr std::size_t MAX_NUM_EDGES = 6;
inline constexpr std::size_t BASIC_NUM_EDGES = 6;
inline constexpr std::size_t MASTER_NUM_EDGES = 1;

// Basic Edge GPIO pins array
inline constexpr std::array<gpio_num_t, BASIC_NUM_EDGES> EDGE_PINS = {
    GPIO_NUM_0, // The master's comm pin
    GPIO_NUM_1,
    GPIO_NUM_3,
    GPIO_NUM_4,
    GPIO_NUM_5,
    GPIO_NUM_6
};

// Serialized message queue
struct SerializedMessage {
    std::array<uint8_t, MAX_PACKET_SIZE> bytes;
    std::size_t length;
};
inline constexpr std::size_t MAX_QUEUE_SIZE = 50;

// Sentinel meaning "don't exclude any edge" when queuing a message - used so a
// relayed message isn't echoed back out the same edge it was received from.
inline constexpr uint8_t NO_EXCLUDED_EDGE = 0xFF;

// Tile commands queue
struct TileCommand {
    PacketType type;
    std::array<uint8_t, MAX_PAYLOAD_SIZE> bytes;
    std::size_t length;
};
inline constexpr std::size_t MAX_COMMANDS_QUEUE_SIZE = 10;

// Bit period
inline constexpr int64_t BIT_PERIOD = 10'000; // 10 ms

// Minimum continuous LOW duration (microseconds) before a receiver commits to
// "a new message is starting." Every real message's first bit is held LOW for
// close to a full BIT_PERIOD (the packetId's MSB is always forced to 1), so this
// just needs to be comfortably shorter than that to filter out brief noise glitches.
inline constexpr int64_t RECEIVE_DEBOUNCE_US = 3'000; // 3 ms

// Credentials
struct PacketCredentials {
    uint16_t packetId;
    uint8_t sourceId;
};
inline constexpr std::size_t MAX_STORED_CREDENTIALS = 32;

// MASTER //

// Master's tile id
inline constexpr uint8_t MASTER_TILE_ID = 255; // for source id

// MAX number of tiles
inline constexpr uint8_t MAX_NUM_TILES = 100;

// Struct storing every tile's info
struct TileState {
    bool connected;
    TileInfo tileInfo;
    std::array<EdgeInfo, BASIC_NUM_EDGES> edgeInfo;
};

// Duration to listen for all tile and edge infos 
inline constexpr int64_t SYNC_DURATION = 5'000; // 5000 ms

// Time to gather all edge handhsakes
inline constexpr int64_t EDGE_WAIT_RESPONSE_DURATION = 500; // 500 ms