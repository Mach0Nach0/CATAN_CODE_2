// TileComms.cpp

#include "TileComms.hpp"

namespace {

const char* packetTypeName(PacketType type) {
    switch (type) {
        case PacketType::AssignGroup: return "AssignGroup";
        case PacketType::UnassignGroup: return "UnassignGroup";
        case PacketType::TileState: return "TileState";
        case PacketType::COMMANDSync: return "COMMANDSync";
        case PacketType::COMMANDLEDTest: return "COMMANDLEDTest";
        default: return "UNKNOWN";
    }
}

const char* destinationTypeName(DestinationType type) {
    switch (type) {
        case DestinationType::Master: return "Master";
        case DestinationType::Single: return "Single";
        case DestinationType::All: return "All";
        case DestinationType::Number: return "Number";
        case DestinationType::Resource: return "Resource";
        case DestinationType::Group: return "Group";
        default: return "UNKNOWN";
    }
}

} // namespace

TileComms::TileComms(uint8_t numEdges) : numEdges_(numEdges) {
    queueMutex_ = xSemaphoreCreateMutex();
}

// Public
void TileComms::begin() {
    for (int i = 0; i < numEdges_; i++) {
        edges_[i].begin(i, EDGE_PINS[i]);
    }

    msgSentToEdge_.fill(false);
}

void TileComms::communicationTask() {
    // Peek which edge (if any) the current head-of-queue message should skip -
    // e.g. a relayed message shouldn't be echoed back out the edge it arrived on.
    xSemaphoreTake(queueMutex_, portMAX_DELAY);
    uint8_t excludeEdge = (queueSize_ > 0) ? queueExcludeEdge_[queueHead_] : NO_EXCLUDED_EDGE;
    xSemaphoreGive(queueMutex_);

    // Load the queue to all edges. Only move on to next message once all edges have received
    // or if the timer has ran out
    bool msgLoadedToAllEdges = true;
    for (int i = 0; i < numEdges_; i++) {
        if (i == excludeEdge) {
            continue;
        }
        if (!msgSentToEdge_[i]) {
            if (sendMessageToEdge(edges_[i])) { // sendMessageToEdge will fail when the edge is unavailable
                msgSentToEdge_[i] = true;

                xSemaphoreTake(queueMutex_, portMAX_DELAY);
                SerializedMessage headMsg = sendQueue_[queueHead_];
                xSemaphoreGive(queueMutex_);

                TilePacket packet;
                if (deserializePacket(packet, headMsg)) {
                    ESP_LOGI("SEND", "Sent %s to edge %d (dest=%s id=%u, src=%u, ttl=%u)",
                             packetTypeName(packet.packetType),
                             edges_[i].edgeId,
                             destinationTypeName(packet.destinationType),
                             packet.destinationId,
                             packet.sourceId,
                             packet.ttl);
                } else {
                    ESP_LOGI("SEND", "Sent a message to edge %d (couldn't decode it for logging)", edges_[i].edgeId);
                }
            }
            else {
                msgLoadedToAllEdges = false;
            }
        }
    }
    if (msgLoadedToAllEdges) {
        dequeueMessage();
        msgSentToEdge_.fill(false);
    }
}

bool TileComms::queueMessage(const SerializedMessage& message, uint8_t excludeEdge) {
    xSemaphoreTake(queueMutex_, portMAX_DELAY);

    // Check if there is still space in the queue
    if (queueSize_ >= sendQueue_.size()) {
        xSemaphoreGive(queueMutex_);
        return false;
    }

    // Append message to the end of the queue
    sendQueue_[queueTail_] = message;
    queueExcludeEdge_[queueTail_] = excludeEdge;

    queueTail_ = (queueTail_ + 1) % sendQueue_.size();
    queueSize_++;

    xSemaphoreGive(queueMutex_);
    return true;
}

bool TileComms::dequeueMessage() {
    xSemaphoreTake(queueMutex_, portMAX_DELAY);

    // Check if there are messages in the queue
    if (queueSize_ == 0) {
        xSemaphoreGive(queueMutex_);
        return false;
    }

    // Dequeue the message
    queueHead_ = (queueHead_ + 1) % sendQueue_.size();
    queueSize_--;

    xSemaphoreGive(queueMutex_);
    return true;
}

bool TileComms::sendMessageToEdge(Edge& edge) {
    xSemaphoreTake(queueMutex_, portMAX_DELAY);

    // Check if there are messages in the queue
    if (queueSize_ == 0) {
        xSemaphoreGive(queueMutex_);
        return false;
    }

    // Add edge ID
    SerializedMessage msgToEdge = sendQueue_[queueHead_];

    xSemaphoreGive(queueMutex_);

    uint8_t curEdgeId = edge.edgeId;
    msgToEdge.bytes[PACKET_EDGE_ID_BYTE_INDEX] = curEdgeId;

    // Send message to edge
    if (!edge.loadMsgFromQueue(msgToEdge)) {
        return false;
    }

    return true;
}

uint16_t TileComms::useCurMsgId() {
    uint16_t id = currentMessageId_;
    currentMessageId_++;
    return id;
}

void TileComms::addCreds(const TilePacket& packet) {
    PacketCredentials newCreds;
    newCreds.packetId = packet.packetId;
    newCreds.sourceId = packet.sourceId;

    storedCredentials_[credsIndex_++] = newCreds;

    if (credsIndex_ >= storedCredentials_.size()) {
        credsIndex_ = 0;
    }
}

bool TileComms::packetSeen(const TilePacket& packet) {
    for (std::size_t i = 0; i < credsIndex_ + 1; i++) {
        if (packet.packetId == storedCredentials_[i].packetId && 
                packet.sourceId == storedCredentials_[i].sourceId) {
            return true;
        }
    }
    return false;
}

bool TileComms::serializePacket(const TilePacket& packet, SerializedMessage& message) {

    // Check if the packet's stated payload size will fit 
    if (packet.payloadSize > packet.payload.size()) {
        return false;
    }

    // Check if the message bytes buffer can contain the whole message
    std::size_t requiredCapacity = HEADER_SIZE + packet.payloadSize + CHECKSUM_SIZE;
    if (requiredCapacity > message.bytes.size()) {
        return false;
    }

    // Serialize packet
    std::size_t curByte = 0;

    // Packet ID
    uint16_t msbHighPacketId = packet.packetId + std::pow(2, 15); // Make the MSB is 1
    for (std::size_t i = 0; i < sizeof(packet.packetId); i++) {
        std::size_t shift = BYTE_SIZE * (sizeof(packet.packetId) - i - 1);
        message.bytes[curByte++] = static_cast<uint8_t>((msbHighPacketId >> shift) & 0xFF);
    }
    
    // Source ID
    message.bytes[curByte++] = packet.sourceId;

    // Source edge ID
    message.bytes[curByte++] = packet.edgeId;

    // Destination type
    message.bytes[curByte++] = static_cast<uint8_t>(packet.destinationType);

    // Destination ID
    message.bytes[curByte++] = packet.destinationId;

    // Packet Type
    message.bytes[curByte++] = static_cast<uint8_t>(packet.packetType);

    // TTL
    message.bytes[curByte++] = packet.ttl;

    // Payload Size
    message.bytes[curByte++] = packet.payloadSize;

    // PAYLOAD
    for (std::size_t i = 0; i < packet.payloadSize; i++) {
        message.bytes[curByte++] = static_cast<uint8_t>(packet.payload[i]);
    }

    // Checksum 
    for (std::size_t i = 0; i < sizeof(packet.checksum); i++) {
        std::size_t shift = BYTE_SIZE * (sizeof(packet.checksum) - i - 1);
        message.bytes[curByte++] = static_cast<uint8_t>((packet.checksum >> shift) & 0xFF);
    }

    message.length = curByte;
    return true;
}

bool TileComms::deserializePacket(TilePacket& packet, const SerializedMessage& message) {

    if (message.length < HEADER_SIZE + CHECKSUM_SIZE) {
        ESP_LOGI("DESERIALIZE_FAIL", "message.length (%d) < HEADER_SIZE+CHECKSUM_SIZE (%d)",
                 (int)message.length, (int)(HEADER_SIZE + CHECKSUM_SIZE));
        return false;
    }

    if (message.length > message.bytes.size()) {
        ESP_LOGI("DESERIALIZE_FAIL", "message.length (%d) > message.bytes.size() (%d)",
                 (int)message.length, (int)message.bytes.size());
        return false;
    }

    std::size_t curByte = 0;

    uint16_t highByte = message.bytes[curByte++];
    uint16_t lowByte  = message.bytes[curByte++];
    packet.packetId = (highByte << BYTE_SIZE) | lowByte;
    packet.packetId -= std::pow(2, 15);

    packet.sourceId = message.bytes[curByte++];

    packet.edgeId = message.bytes[curByte++];

    packet.destinationType = static_cast<DestinationType>(message.bytes[curByte++]);
    if (packet.destinationType >= DestinationType::MAX) {
        ESP_LOGI("DESERIALIZE_FAIL", "destinationType (%u) >= MAX (%u)",
                 (unsigned)packet.destinationType, (unsigned)DestinationType::MAX);
        return false;
    }

    packet.destinationId = message.bytes[curByte++];

    packet.packetType = static_cast<PacketType>(message.bytes[curByte++]);
    if (packet.packetType >= PacketType::MAX) {
        ESP_LOGI("DESERIALIZE_FAIL", "packetType (%u) >= MAX (%u)",
                 (unsigned)packet.packetType, (unsigned)PacketType::MAX);
        return false;
    }

    packet.ttl = message.bytes[curByte++];
    // if (packet.ttl <= 0) {
    //     return false;
    // }

    packet.payloadSize = message.bytes[curByte++];

    if (packet.payloadSize > packet.payload.size()) {
        ESP_LOGI("DESERIALIZE_FAIL", "payloadSize (%u) > payload.size() (%u)",
                 (unsigned)packet.payloadSize, (unsigned)packet.payload.size());
        return false;
    }

    const std::size_t expectedLength = HEADER_SIZE + packet.payloadSize + CHECKSUM_SIZE;
    if (message.length != expectedLength) {
        ESP_LOGI("DESERIALIZE_FAIL", "message.length (%d) != expectedLength (%d) [payloadSize=%u src=%u destType=%u type=%u ttl=%u]",
                 (int)message.length, (int)expectedLength,
                 (unsigned)packet.payloadSize, (unsigned)packet.sourceId,
                 (unsigned)packet.destinationType, (unsigned)packet.packetType, (unsigned)packet.ttl);
        return false;
    }

    for (std::size_t i = 0; i < packet.payloadSize; i++) {
        packet.payload[i] = message.bytes[curByte++];
    }

    highByte = message.bytes[curByte++];
    lowByte  = message.bytes[curByte++];
    packet.checksum = (highByte << BYTE_SIZE) | lowByte;
    
    return true;
}

uint16_t TileComms::calculateChecksum(const TilePacket& packet) {
    uint16_t checksum = 0;

    // Packet ID: add both bytes
    checksum += static_cast<uint8_t>((packet.packetId >> BYTE_SIZE) & 0xFF);

    checksum += static_cast<uint8_t>(packet.packetId & 0xFF);

    checksum += packet.sourceId;

    // checksum += packet.edgeId; EXCLUDED

    checksum += static_cast<uint8_t>(packet.destinationType);

    checksum += packet.destinationId;

    checksum += static_cast<uint8_t>(packet.packetType);

    checksum += packet.ttl;
    checksum += packet.payloadSize;

    for (std::size_t i = 0; i < packet.payloadSize; ++i) {
        checksum += packet.payload[i];
    }

    return checksum;
}

bool TileComms::confirmChecksum(const TilePacket& packet) {
    if (packet.checksum == calculateChecksum(packet)) {
        return true;
    }
    return false;
}