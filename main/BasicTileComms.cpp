// BasicTileComms.cpp

#include "BasicTileComms.hpp"

BasicTileComms::BasicTileComms(uint8_t tileId) :
    TileComms(BASIC_NUM_EDGES), 
    tileId_(tileId)
{}

// Public
void BasicTileComms::begin() {
    TileComms::begin();

    resetAllEdgeInfosUpdated();

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

void BasicTileComms::syncTileInfo(uint8_t& updatedNumber, ResourceType& updatedResource) {
    tileInfo_.tileNumber = updatedNumber;
    tileInfo_.tileResource = updatedResource;
}

bool BasicTileComms::getNewCommand(TileCommand& buffer) {
    if (commandQueueSize_ <= 0) {
        return false;
    }

    buffer = tileCommandQueue_[commandQueueHead_];
    dequeueCommand();
    return true;
}

bool BasicTileComms::queueEdgeInfo(int64_t curTime) {
    SerializedMessage message;
    constructEdgeInfo(message);
    ESP_LOGI("Attempting to queue edge info", "Attempting to queue edge info");
    if (queueMessage(message)) {
        ESP_LOGI("Queued edge info", "Queued edge info");
        // Start the edge wait timer
        edgeStartWaiting(curTime);

        return true;
    }
    return false;
}

bool BasicTileComms::queueTileStateToMaster() {
    SerializedMessage message;
    constructTileStateToMaster(message);
    if (queueMessage(message)) {
        return true;
    }
    return false;
}

// bool BasicTileComms::queueTileInfoToMaster() {
//     SerializedMessage message;
//     constructTileInfoToMaster(message);
//     if (queueMessage(message)) {
//         return true;
//     }
//     return false;
// }

bool BasicTileComms::allEdgeInfosUpdated() {
    if (edgeWaitDone_) {
        edgeWaitDone_ = false;
        return true;
    }

    for (std::size_t i = 0; i < BASIC_NUM_EDGES; i++) {
        if (adjEdgeInfoUpdated_[i] == false) {
            return false;
        }
    }
    return true;
}

void BasicTileComms::resetAllEdgeInfosUpdated() {
    for (std::size_t i = 0; i < BASIC_NUM_EDGES; i++) {
        adjEdgeInfoUpdated_[i] = false;
    }
}

// Private
void BasicTileComms::constructEdgeInfo(SerializedMessage& buffer) {
    TilePacket packet;
    packet.packetId = useCurMsgId();
    packet.sourceId = tileId_;
    packet.edgeId = ARBRITARY_EDGE_ID; // doesnt matter

    packet.destinationType = DestinationType::All;
    packet.destinationId = 0; // doesnt matter
    packet.packetType = PacketType::TileState;
    packet.ttl = 0;
    packet.payloadSize = EDGE_INFO_PAYLOAD_SIZE;
    packet.checksum = calculateChecksum(packet);

    serializePacket(packet, buffer);

    return;
}

void BasicTileComms::constructTileStateToMaster(SerializedMessage& buffer) {
    TilePacket packet;
    packet.packetId = useCurMsgId();
    packet.sourceId = tileId_;
    packet.edgeId = ARBRITARY_EDGE_ID; // doesnt matter

    packet.destinationType = DestinationType::Master;
    packet.destinationId = 0; // doesnt matter
    packet.packetType = PacketType::TileState;
    packet.ttl = 10;
    packet.payloadSize = TILE_STATE_TO_MASTER_PAYLOAD_SIZE;
    
    std::size_t curByte = 0;

    packet.payload[curByte++] = tileInfo_.tileNumber;
    packet.payload[curByte++] = static_cast<uint8_t>(tileInfo_.tileResource);

    for (std::size_t i = 0; i < BASIC_NUM_EDGES; i++) {
        packet.payload[curByte++] = adjEdgeInfo_[i].edgeIdA;
        packet.payload[curByte++] = adjEdgeInfo_[i].edgeIdB;
        packet.payload[curByte++] = adjEdgeInfo_[i].tileIdB;
    }

    packet.checksum = calculateChecksum(packet);

    serializePacket(packet, buffer);

    return;
}

// void BasicTileComms::constructTileInfoToMaster(SerializedMessage& buffer) {
//     TilePacket packet;
//     packet.packetId = useCurMsgId();
//     packet.sourceId = tileId_;
//     packet.edgeId = ARBRITARY_EDGE_ID; // doesnt matter

//     packet.destinationType = DestinationType::Master;
//     packet.destinationId = 0; // doesnt matter
//     packet.packetType = PacketType::TileInfo;
//     packet.ttl = 10;
//     packet.payloadSize = TILE_INFO_TO_MASTER_PAYLOAD_SIZE;
    
//     packet.payload[0] = tileInfo_.tileNumber;
//     packet.payload[1] = static_cast<uint8_t>(tileInfo_.tileResource);

//     packet.checksum = calculateChecksum(packet);

//     serializePacket(packet, buffer);

//     return;
// }

void BasicTileComms::edgeStartWaiting(int64_t curTime) {
    edgeIsWaiting_ = true;
    edgeWaitStartTime_ = curTime;
}

void BasicTileComms::communicationTaskEntry(void* parameter) {
    auto* comms = static_cast<BasicTileComms*>(parameter);
    comms->communicationTask();
}

void BasicTileComms::communicationTask() {
    while (true) {
        TileComms::communicationTask();

        for (int i = 0; i < BASIC_NUM_EDGES; i++) {
            edges_[i].communicate();
            
            SerializedMessage newMsg;
            if (edges_[i].getNewMessage(newMsg)) {
                ESP_LOGI("EDGE_RX", "Edge %d got %d bytes", i, (int)newMsg.length);
                // Deserealize packet
                TilePacket newPacket;
                if (deserializePacket(newPacket, newMsg)) {
                    ESP_LOGI("EDGE_RX", "  deserialized OK: src=%u type=%u ttl=%u", newPacket.sourceId, (unsigned)newPacket.packetType, newPacket.ttl);
                    processNewPacket(newPacket, i);
                }
                else {
                    ESP_LOGI("EDGE_RX", "  deserialize FAILED");
                }
            }
            
        }

        // Check the edge is waiting condition
        if (edgeIsWaiting_) {
            int64_t curTime = esp_timer_get_time() / 1000; // ms
            if ((curTime - edgeWaitStartTime_) >= EDGE_WAIT_RESPONSE_DURATION) {
                edgeIsWaiting_ = false;
                edgeWaitDone_ = true;
            }
        }

        vTaskDelay(1);
    }
}

bool BasicTileComms::queueCommand(const TileCommand& command) {
    
    // Check if there is still space in the queue
    if (commandQueueSize_ >= tileCommandQueue_.size()) {
        return false;
    }

    // Append message to the end of the queue
    tileCommandQueue_[commandQueueTail_] = command;

    commandQueueTail_ = (commandQueueTail_ + 1) % tileCommandQueue_.size();
    commandQueueSize_++;

    ESP_LOGI("COMMAND QUEUED", "COMMAND QUEUED");

    return true;
}

bool BasicTileComms::dequeueCommand() {
    
    // Check if there are messages in the queue
    if (commandQueueSize_ == 0) {
        return false;
    }

    // Dequeue the message
    commandQueueHead_ = (commandQueueHead_ + 1) % tileCommandQueue_.size();
    commandQueueSize_--;

    return true;
}

bool BasicTileComms::mustReceivePacket(const TilePacket& packet) {
    if (packet.destinationType == DestinationType::All) {
        return true;
    }

    if (packet.destinationType == DestinationType::Single && packet.destinationId == tileId_) {
        return true;
    }

    if (packet.destinationType == DestinationType::Number && packet.destinationId == tileInfo_.tileNumber) {
        return true;
    }

    if (packet.destinationType == DestinationType::Resource && packet.destinationId == static_cast<uint8_t>(tileInfo_.tileResource)) {
        return true;
    }

    if (packet.destinationType == DestinationType::Group && ((0x01u << packet.destinationId) & groups_) != 0x0u) {
        return true;
    }

    return false;
}

void BasicTileComms::processNewPacket(TilePacket& packet, uint8_t recevingEdgeId) {
    // Perform checksum
    ESP_LOGI("Processing", "Processing");
    if (!confirmChecksum(packet)) {
        // Packet is corrupted
        uint16_t computed = calculateChecksum(packet);
        ESP_LOGI("TILE_RX", "Checksum FAILED src=%u type=%u destType=%u ttl=%u payloadSize=%u received=0x%04X computed=0x%04X",
                 packet.sourceId, (unsigned)packet.packetType, (unsigned)packet.destinationType,
                 packet.ttl, packet.payloadSize, packet.checksum, computed);
        ESP_LOG_BUFFER_HEX("TILE_RX_PAYLOAD", packet.payload.data(), packet.payloadSize);
        return;
    }
    ESP_LOGI("Checksum confirmed", "Checksum confirmed");
    
    // If the packet is new, store as "seen" and do the command
    if (!packetSeen(packet)) {
        // Store packet credentials as "seen"
        addCreds(packet);

        // If the packet targets this tile, continue processing it
        if (mustReceivePacket(packet)) {
            ESP_LOGI("RECEIVED", "RECEIVED");
            executePacketRequest(packet, recevingEdgeId);

            // Terminate message if it is a single target and this tile is the target
            if (packet.destinationType == DestinationType::Single) {
                return;
            }
        }

        // If packet ttl is already 0, do not queue it
        if (packet.ttl <= 0) {
            return;
        }

        // Decrease ttl and put in queue, skipping the edge it arrived on -
        // no need to echo a relayed message back the way it came.
        packet.ttl -= 1;
        packet.checksum = calculateChecksum(packet);
        SerializedMessage msgToQueue;
        if (serializePacket(packet, msgToQueue)) {
            queueMessage(msgToQueue, recevingEdgeId);
        }
        return;
    }
}

void BasicTileComms::executePacketRequest(TilePacket& packet, uint8_t recevingEdgeId) {

    ESP_LOGI("EXECUTING", "EXECUTING");
    // If it is a group assign/unassignment
    if (packet.packetType == PacketType::AssignGroup) {
        if (!assignGroup(packet.packetId)) {
            abort();
        }
        return;
    }
    if (packet.packetType == PacketType::UnassignGroup) {
        unassignGroup(packet.packetId);
        return;
    }

    // If it is adjacent edge information
    if (packet.packetType == PacketType::TileState) {
        // Update the edge info array
        adjEdgeInfo_[recevingEdgeId].edgeIdA = recevingEdgeId;
        adjEdgeInfo_[recevingEdgeId].edgeIdB = packet.edgeId;

        adjEdgeInfo_[recevingEdgeId].tileIdB = packet.sourceId;

        // Update the bool array
        adjEdgeInfoUpdated_[recevingEdgeId] = true;

        return;
    }

    // If it is a command for the tile, queue it
    ESP_LOGI("IS COMMAND", "IS COMMAND");
    TileCommand newTileCommand;
    newTileCommand.type = packet.packetType;
    newTileCommand.bytes = packet.payload;
    newTileCommand.length = packet.payloadSize;
    queueCommand(newTileCommand);

    return;
}

bool BasicTileComms::assignGroup(uint16_t groupId) {
    if (groupId >= 16) {
        return false;
    }

    if (numGroups_ >= 16) {
        return false;
    }

    groups_ |= static_cast<uint16_t>(0x01u << groupId);
    numGroups_++;
    return true;
}

bool BasicTileComms::unassignGroup(uint16_t groupId) {
    if (groupId >= 16) {
        return false;
    }

    if (numGroups_ <= 0) {
        return false;
    }

    groups_ &= static_cast<uint16_t>(~(0x01u << groupId));
    numGroups_--;
    return true;
}