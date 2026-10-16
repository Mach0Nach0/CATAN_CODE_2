// MasterTile.cpp

#include "MasterTile.hpp"

MasterTile::MasterTile() : comms_() {}

// Public
void MasterTile::begin() {
    comms_.begin();

    // 1. Configure the GPIO pin
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUTTON_PIN), // Bit mask for the pin
        .mode = GPIO_MODE_INPUT,              // Set as input mode
        .pull_up_en = GPIO_PULLUP_ENABLE,     // Enable internal pull-up
        .pull_down_en = GPIO_PULLDOWN_DISABLE, // Disable pull-down
        .intr_type = GPIO_INTR_DISABLE        // Disable interrupts for now
    };
    gpio_config(&io_conf);

    // Start the communication task
    BaseType_t result = xTaskCreate(
        masterTileTaskEntry,
        "tile_comms",
        4096,
        this,
        8,
        &masterTileTaskHandle_
    );
    if (result != pdPASS) {
        // Task creation failed
        abort();
    }

}

// Private
void MasterTile::masterTileTaskEntry(void* parameter) {
    auto* masterTile = static_cast<MasterTile*>(parameter);
    masterTile->masterTileTask();
}

void MasterTile::masterTileTask() {
    while (true) {
        int64_t curTime = esp_timer_get_time() / 1000; // ms

        // Read for inputs to sync
        // If the sync button is pressed, set mustSync_ to true; !!! 
        // Read the level (0 = LOW/Pressed, 1 = HIGH/Released)
        bool button_state = gpio_get_level(BUTTON_PIN);

        // ESP_LOGI("BUTTON", "button_state=%s", button_state == LOW ? "LOW(pressed)" : "HIGH(released)");
    

        if (button_state == LOW) {
            mustSync_ = true;
        }
        syncInfo(curTime);

        // Read for inputs to test LED
        // If the button is pressed
        // comms_.queueLedTestCommand(); // uncomment this into the if

        vTaskDelay(1);
    }
}

void MasterTile::syncInfo(int64_t curTime) {
    if (!mustSync_) {
        return;
    }

    if (!isSyncing_) {
        comms_.resetTileConnections();
        comms_.resetAllTileInfosUpdated();
        // comms_.resetAllEdgeInfosUpdated();

        comms_.queueSyncCommand();

        isSyncing_ = true;
        syncStartTime_ = curTime;
        return;
    }

    int64_t elapsedTime = curTime - syncStartTime_;
    if (elapsedTime >= SYNC_DURATION && comms_.infosMatch()) {
        numTiles_ = comms_.curNumTiles;
        isSyncing_ = false;
        mustSync_ = false;
        updateTileMap();
        return;
    }
}

void MasterTile::updateTileMap() {
    comms_.printTileStates();
}

