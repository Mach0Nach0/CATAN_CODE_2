// TilePacket.hpp

// All info about the packets

inline constexpr std::size_t MAX_PAYLOAD_SIZE = 20;

#pragma once

#include <cstdint>
#include <array>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h" 

#include "SharedParameters.hpp"
 
enum class DestinationType : uint8_t {
    Master, // Targets the master
    Single, // Target a specific tile; ID is the tileId
    All, // All tiles; ID does not matter
    Number, // Target tiles with a specific tile number; ID is the number
    Resource, // Target tiles with a specific resource type; ID is the resource (an enum) 
    Group, // Target an assigned group; ID is the group ID (bitmask position index)
    MAX
};

enum class PacketType : uint8_t {
    AssignGroup, // Assign to a custom group; payload is the group bitmask position
    UnassignGroup, // Unassign from a group; payload is the group bitmask position

    TileState, // To ALL: no payload, only tile id and edge id; To MASTER: payload contains [tile number, tile resource, edge info x6]
    
    // ADD SPECIFIC TILE COMMANDS BELOW:
    COMMANDSync, // tiles exchange edge info, then once they are done, send a packet to the master
    // COMMANDSendTileInfo, // tiles send a tile info packet to the master
    COMMANDLEDTest, // Turn an led on for 1 second
    MAX
};

// TILE PACKET
inline constexpr std::size_t PACKET_EDGE_ID_BYTE_INDEX = 3; // Used to add the edge id when sending messages
inline constexpr uint8_t ARBRITARY_EDGE_ID = 7; // For when edge id does not matter

struct TilePacket {
    uint16_t packetId; // When its being transmitted, always make the MSB 1. So max value is 2^15
    uint8_t sourceId; // the tile id
    uint8_t edgeId; // the source's edge id

    DestinationType destinationType;
    uint8_t destinationId;

    PacketType packetType;
    uint8_t ttl;

    uint8_t payloadSize;
    std::array<uint8_t, MAX_PAYLOAD_SIZE> payload;

    uint16_t checksum;
};

// Tile Info
struct TileInfo {
    uint8_t tileNumber;
    ResourceType tileResource;
};

// Edge Connection
struct EdgeInfo {
    uint8_t edgeIdA; // A is the edge belonging to this side
    uint8_t edgeIdB; // B is the edge belonging to the other side
    uint8_t tileIdB;
};



// Sizes in bytes
inline constexpr std::size_t HEADER_SIZE = 
    sizeof(DestinationType) + 
    sizeof(PacketType) + 
    sizeof(uint16_t) * 1 + // packetId
    sizeof(uint8_t) * 5; // sourceId, edgeId, destinationId, ttl, payloadSize
inline constexpr std::size_t CHECKSUM_SIZE = sizeof(uint16_t);
inline constexpr std::size_t PAYLOAD_SIZE_BYTE_INDEX = HEADER_SIZE - 1; // Byte offset of the payloadSize field
const std::size_t MAX_PACKET_SIZE = HEADER_SIZE + MAX_PAYLOAD_SIZE + CHECKSUM_SIZE;
const std::size_t MAX_BITS = MAX_PACKET_SIZE * BYTE_SIZE;

// Size of payloads
inline constexpr std::size_t TILE_STATE_TO_MASTER_PAYLOAD_SIZE = 
    sizeof(uint8_t) + sizeof(ResourceType) +
    sizeof(EdgeInfo) * 6; // Connection info of each edge

inline constexpr std::size_t EDGE_INFO_PAYLOAD_SIZE = 0;

inline constexpr std::size_t EDGE_INFO_COMMAND_PAYLOAD_SIZE = 0;

inline constexpr std::size_t LED_TEST_COMMAND_PAYLOAD_SIZE = 0;
