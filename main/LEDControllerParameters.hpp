// TileParameters.hpp

// only LEDController class gets this

#pragma once

#include <array>
#include <cstddef>
#include <cmath>
#include "driver/gpio.h"
#include "esp_timer.h"

enum class LEDAnimationType : uint8_t {
    NONE,
    OnForDuration, // Turns all leds on for a set duration
    MAX
};

// Number of LEDs in each tile (temporary)
inline constexpr std::size_t NUM_LEDS = 1;

// LED GPIO pins array
inline constexpr std::array<gpio_num_t, NUM_LEDS> LED_PINS = {
    GPIO_NUM_7
};

