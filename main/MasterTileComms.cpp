// MasterTileComms.cpp

#include "MasterTileComms.hpp"

MasterTileComms::MasterTileComms() : TileComms(MASTER_NUM_EDGES) {}

// Public
void MasterTileComms::begin() {
    TileComms::begin();

    resetTileConnections();
    // resetAllEdgeInfosUpdated();
    resetAllTileInfosUpdated();

    // Start the communication task
    BaseType_t result = xTaskCreate(
        communicationTaskEntry,
        "tile_comms",
        4096,
        this,
        8,
        &communicationTaskHandle_
    );
    if (result != pdPASS) {
        // Task creation failed
        abort();
    }
}

void MasterTileComms::resetAllTileInfosUpdated() {
    for (std::size_t i = 0; i < MAX_NUM_TILES; i++) {
        tileInfosUpdated_[i] = false;
    }
}

// void MasterTileComms::resetAllEdgeInfosUpdated() {
//     for (std::size_t i = 0; i < MAX_NUM_TILES; i++) {
//         edgeInfosUpdated_[i] = false;
//     }
// }

bool MasterTileComms::infosMatch() {
    for (std::size_t i = 0; i < MAX_NUM_TILES; i++) {
        if (!(tileInfosUpdated_[i] == tiles_[i].connected)) {
            return false;
        }
    }
    return true;
}

void MasterTileComms::resetTileConnections() {
    for (uint8_t i = 0; i < MAX_NUM_TILES; i++) {
        tiles_[i].connected = false;
    }
    curNumTiles = 0;
    return;
}

bool MasterTileComms::queueSyncCommand() {
    SerializedMessage message;

    constructSyncCommand(message);
    if (!queueMessage(message)) {
        return false;
    }

    // constructTileInfoCommand(message);
    // if (!queueMessage(message)) {
    //     return false;
    // }

    return true;
}

bool MasterTileComms::queueLedTestCommand() {
    SerializedMessage message;
    constructLedTestCommand(message);
    if (!queueMessage(message)) {
        return false;
    }
    return true;
}

// Private

void MasterTileComms::communicationTaskEntry(void* parameter) {
    auto* comms = static_cast<MasterTileComms*>(parameter);
    comms->communicationTask();
}

void MasterTileComms::communicationTask() {
    while (true) {
        TileComms::communicationTask();

        // static int lastLoggedBit[MASTER_NUM_EDGES] = { -1 };
        // static bool lastLoggedAvail[MASTER_NUM_EDGES] = { false };
        for (int i = 0; i < MASTER_NUM_EDGES; i++) {
            // if ((int)edges_[i].transmitCurBit_ != lastLoggedBit[i] || edges_[i].available_ != lastLoggedAvail[i]) {
            //     lastLoggedBit[i] = (int)edges_[i].transmitCurBit_;
            //     lastLoggedAvail[i] = edges_[i].available_;
            //     ESP_LOGI("EDGE_AVAIL", "Edge %d available=%s receiving=%s transmitting=%s curBit=%d msgLen=%d queueSize=%d", i,
            //              edges_[i].available_ ? "true" : "false",
            //              edges_[i].receiving_ ? "true" : "false",
            //              edges_[i].transmitting_ ? "true" : "false",
            //              (int)edges_[i].transmitCurBit_,
            //              (int)edges_[i].curMsgToSend_.length,
            //              (int)queueSize_);
            // }

            edges_[i].communicate();
            SerializedMessage newMsg;
            if (edges_[i].getNewMessage(newMsg)) {
                ESP_LOGI("EDGE_RX", "Edge %d got %d bytes", i, (int)newMsg.length);
                // Deserealize packet
                TilePacket newPacket;
                if (deserializePacket(newPacket, newMsg)) {
                    ESP_LOGI("EDGE_RX", "  deserialized OK: src=%u type=%u ttl=%u", newPacket.sourceId, (unsigned)newPacket.packetType, newPacket.ttl);
                    processNewPacket(newPacket, i);
                } else {
                    ESP_LOGI("EDGE_RX", "  deserialize FAILED");
                }
            }

        }

        vTaskDelay(1);
    }
}

bool MasterTileComms::mustReceivePacket(const TilePacket& packet) {
    if (packet.destinationType == DestinationType::Master) {
        return true;
    }
    return false;
}

void MasterTileComms::processNewPacket(TilePacket& packet, uint8_t recevingEdgeId) {
    // Perform checksum
    if (!confirmChecksum(packet)) {
        // Packet is corrupted
        uint16_t computed = calculateChecksum(packet);
        ESP_LOGI("MASTER_RX", "Checksum FAILED src=%u type=%u destType=%u ttl=%u payloadSize=%u received=0x%04X computed=0x%04X",
                 packet.sourceId, (unsigned)packet.packetType, (unsigned)packet.destinationType,
                 packet.ttl, packet.payloadSize, packet.checksum, computed);
        ESP_LOG_BUFFER_HEX("MASTER_RX_PAYLOAD", packet.payload.data(), packet.payloadSize);
        return;
    }
    ESP_LOGI("MASTER_RX", "Checksum confirmed for packet from src=%u type=%u", packet.sourceId, (unsigned)packet.packetType);

    // If the packet is new, store as "seen" and do the command
    if (!packetSeen(packet)) {
        // Store packet credentials as "seen"
        addCreds(packet);

        // If the packet targets this tile, continue processing it
        if (mustReceivePacket(packet)) {
            ESP_LOGI("MASTER_RX", "Executing packet request for src=%u type=%u", packet.sourceId, (unsigned)packet.packetType);
            executePacketRequest(packet, recevingEdgeId);
        }
    }

    // Terminate all messages (the master does not bounce messages)
    return;
}

void MasterTileComms::executePacketRequest(TilePacket& packet, uint8_t recevingEdgeId) {
    // Master actions 
    
    if (packet.packetType == PacketType::TileState) {
        uint8_t tileIdx = packet.sourceId;
        tileInfosUpdated_[tileIdx] = true;

        std::size_t curByte = 0;
        for (int i = 0; i < BASIC_NUM_EDGES; i++) {
            tiles_[tileIdx].edgeInfo[i].edgeIdA = packet.payload[curByte++];
            tiles_[tileIdx].edgeInfo[i].edgeIdB = packet.payload[curByte++];
            tiles_[tileIdx].edgeInfo[i].tileIdB = packet.payload[curByte++];
        }

        if (!tiles_[tileIdx].connected && tileInfosUpdated_[tileIdx]) {
            tiles_[tileIdx].connected = true;
            curNumTiles++;
        }
        
        ESP_LOGI("MASTER_RX", "Done storing adjacent edge info (Tile %u)", packet.sourceId);

        return;
    }

    // if (packet.packetType == PacketType::TileInfo) {
    //     uint8_t tileIdx = packet.sourceId;
    //     tileInfosUpdated_[tileIdx] = true;
        
    //     tiles_[tileIdx].tileInfo.tileNumber = packet.payload[0];
    //     tiles_[tileIdx].tileInfo.tileResource = static_cast<ResourceType>(packet.payload[1]);

    //     if (!tiles_[tileIdx].connected && tileInfosUpdated_[tileIdx] && edgeInfosUpdated_[tileIdx]) {
    //         tiles_[tileIdx].connected = true;
    //         curNumTiles++;
    //     }
        
    //     ESP_LOGI("MASTER_RX", "Done storing tile info (Tile %u)", packet.sourceId);

    //     return;
    // }

    return;
}

void MasterTileComms::constructSyncCommand(SerializedMessage& buffer) {
    TilePacket packet;
    packet.packetId = useCurMsgId();
    packet.sourceId = MASTER_TILE_ID;
    packet.edgeId = ARBRITARY_EDGE_ID; // doesnt matter

    packet.destinationType = DestinationType::All;
    packet.destinationId = 0; // doesnt matter
    packet.packetType = PacketType::COMMANDSync;
    packet.ttl = 10;
    packet.payloadSize = EDGE_INFO_COMMAND_PAYLOAD_SIZE;

    packet.checksum = calculateChecksum(packet);

    serializePacket(packet, buffer);

    return;
}

// void MasterTileComms::constructTileInfoCommand(SerializedMessage& buffer) {
//     TilePacket packet;
//     packet.packetId = useCurMsgId();
//     packet.sourceId = MASTER_TILE_ID;
//     packet.edgeId = ARBRITARY_EDGE_ID; // doesnt matter

//     packet.destinationType = DestinationType::All;
//     packet.destinationId = 0; // doesnt matter
//     packet.packetType = PacketType::COMMANDSendTileInfo;
//     packet.ttl = 10;
//     packet.payloadSize = TILE_INFO_COMMAND_PAYLOAD_SIZE;

//     packet.checksum = calculateChecksum(packet);

//     serializePacket(packet, buffer);

//     return;
// }

void MasterTileComms::constructLedTestCommand(SerializedMessage& buffer) {
    TilePacket packet;
    packet.packetId = useCurMsgId();
    packet.sourceId = MASTER_TILE_ID;
    packet.edgeId = ARBRITARY_EDGE_ID; // doesnt matter

    packet.destinationType = DestinationType::All;
    packet.destinationId = 0; // doesnt matter
    packet.packetType = PacketType::COMMANDLEDTest;
    packet.ttl = 10;
    packet.payloadSize = LED_TEST_COMMAND_PAYLOAD_SIZE;

    packet.checksum = calculateChecksum(packet);

    serializePacket(packet, buffer);

    return;
}

void MasterTileComms::printTileStates()
{
    std::array<TileState, MAX_NUM_TILES>& tileStates = tiles_;
    ESP_LOGI("HERE", "HERE");
    for (std::size_t i = 0; i < tileStates.size(); ++i) {
        const TileState& tile = tileStates[i];
        if (tile.connected) {
            ESP_LOGI("TILE_STATE",
                    "Tile[%u]: connected=%s, tileNumber=%u, resource=%u",
                    static_cast<unsigned>(i),
                    tile.connected ? "true" : "false",
                    static_cast<unsigned>(tile.tileInfo.tileNumber),
                    static_cast<unsigned>(tile.tileInfo.tileResource));

            for (std::size_t j = 0; j < tile.edgeInfo.size(); ++j) {
                const EdgeInfo& edge = tile.edgeInfo[j];

                ESP_LOGI("TILE_STATE",
                        "  Edge[%u]: edgeIdA=%u, edgeIdB=%u, tileIdB=%u",
                        static_cast<unsigned>(j),
                        static_cast<unsigned>(edge.edgeIdA),
                        static_cast<unsigned>(edge.edgeIdB),
                        static_cast<unsigned>(edge.tileIdB));
            }            
        }

    }
}

