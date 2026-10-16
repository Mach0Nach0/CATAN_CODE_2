// SharedParameters.hpp

// SHared between master and tile classes

#pragma once

#include <array>
#include <cstddef>
#include <cmath>
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"

// Byte size
#define BYTE_SIZE 8

// Logic definition
inline constexpr bool HIGH = 1;
inline constexpr bool LOW = 0;

// Resource types
enum class ResourceType : uint8_t {
    Stone,
    Clay,
    Wood,
    Sheep,
    Hay,
    Gold,
    MAX
};


