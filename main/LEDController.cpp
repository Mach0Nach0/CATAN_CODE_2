// LEDController.cpp

#include "LEDController.hpp"

LEDController::LEDController() {}

// Public
void LEDController::begin() {
    // Set all LED pins to output mode and turn them off
    for (int i = 0; i < NUM_LEDS; i++) {
        gpio_set_direction(LED_PINS[i], GPIO_MODE_OUTPUT);
        setLed(LED_PINS[i], LOW);
    }

    curAnimation_ = LEDAnimationType::NONE;
    curTime_ = 0;
    animationStartTime_ = 0;
    animationElapsedTime_ = 0;
    animationDuration_ = 0;
}

void LEDController::beginOnForDuration(int64_t duration) {
    if (curAnimation_ != LEDAnimationType::NONE) {
        return;
    }

    // Initialising animations
    curAnimation_ = LEDAnimationType::OnForDuration;
    animationStartTime_ = curTime_;
    animationElapsedTime_ = 0;
    animationDuration_ = duration;

    // Animation specific commands
    setAllLeds(HIGH);

    return;
}

// Private
void LEDController::ledTaskEntry(void* parameter) {
    auto* ledController = static_cast<LEDController*>(parameter);
    ledController->ledTask();
}

void LEDController::ledTask() {
    while (true) {
        curTime_ = esp_timer_get_time() / 1000;

        if (curAnimation_ == LEDAnimationType::NONE) {
            setAllLeds(LOW);
        }

        if (curAnimation_ == LEDAnimationType::OnForDuration) {
            updateOnForDuration(curTime_);
        }

        vTaskDelay(1);
    }
}

void LEDController::updateOnForDuration(int64_t curTime) {
    // End animation if past duration
    if (animationElapsedTime_ >= animationDuration_) {
        curAnimation_ = LEDAnimationType::NONE;
        return;
    }

    animationElapsedTime_ = curTime - animationStartTime_;

}


void LEDController::setAllLeds(bool on) {
    for (int i = 0; i < NUM_LEDS; i++) {
        setLed(LED_PINS[i], on);
    }
}

void LEDController::setLed(gpio_num_t ledPin, bool on) {
    gpio_set_level(ledPin, on);
}

