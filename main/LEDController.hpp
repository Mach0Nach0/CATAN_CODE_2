// LEDController.hpp

#pragma once

#include <cstdint>
#include <array>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "SharedParameters.hpp"
#include "LEDControllerParameters.hpp"

class LEDController {
public:
    LEDController();

    void begin();

    // FLASH DURATION
    void beginOnForDuration(int64_t duration);

private:
    TaskHandle_t ledTaskHandle_ = nullptr;
    static void ledTaskEntry(void* parameter);
    void ledTask();

    LEDAnimationType curAnimation_;
    int64_t curTime_;
    int64_t animationStartTime_;
    int64_t animationElapsedTime_;
    int64_t animationDuration_;

    // FLASH DURATION
    void updateOnForDuration(int64_t curTime);

    void setAllLeds(bool on);
    void setLed(gpio_num_t ledPin, bool on);

};

