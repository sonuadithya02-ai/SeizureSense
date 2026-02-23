#include "detection_responder.h"
#include "esp_log.h"
#include "model_settings.h"

static const char* TAG = "SeizureResponder";

extern "C" void RespondToDetection(int8_t output) {
    // Convert int8 output to probability for logging
    float probability = (static_cast<float>(output) - OUTPUT_ZERO_POINT) * OUTPUT_SCALE;
    
    if (output >= SEIZURE_THRESHOLD) {
        ESP_LOGW(TAG, "⚠️  SEIZURE DETECTED! Output: %d (prob: %.2f)", output, probability);
    } else {
        ESP_LOGI(TAG, "Normal activity. Output: %d (prob: %.2f)", output, probability);
    }
}

