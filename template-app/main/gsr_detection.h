// GSR (Galvanic Skin Response) Detection Header
// Seeed Grove GSR Sensor - Heuristic stress/arousal detection
// Used as supporting modality for seizure detection

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// GSR Configuration
#define GSR_ADC_CHANNEL         ADC_CHANNEL_4   // GPIO5
#define GSR_ADC_UNIT            ADC_UNIT_1
#define GSR_ADC_ATTEN           ADC_ATTEN_DB_12 // Full range 0-3.3V
#define GSR_ADC_WIDTH           ADC_BITWIDTH_12 // 12-bit resolution (0-4095)

// Calibration settings
#define GSR_CALIBRATION_SEC     30      // Baseline calibration period
#define GSR_SAMPLE_RATE_HZ      4       // Sample rate (every 250ms)
#define GSR_BASELINE_SAMPLES    (GSR_CALIBRATION_SEC * GSR_SAMPLE_RATE_HZ)

// Detection thresholds
#define GSR_STRESS_THRESHOLD_PCT    50.0f   // 50% above baseline = stress
#define GSR_SCR_PEAK_THRESHOLD      3       // SCR peaks per minute for stress
#define GSR_PEAK_MIN_AMPLITUDE      100     // Minimum ADC change for SCR peak

// Result structure
typedef struct {
    float raw_value;                // Raw ADC value (0-4095)
    float normalized_value;         // Normalized to microsiemens (approx)
    float baseline;                 // Established baseline
    float percent_above_baseline;   // Percentage above baseline
    int scr_peaks_per_minute;       // Skin conductance response peaks
    bool stress_detected;           // Stress/arousal detected
    bool is_calibrating;            // Still in calibration period
} gsr_result_t;

/**
 * @brief Initialize the GSR sensor
 * @return true if initialization successful, false otherwise
 */
bool gsr_init(void);

/**
 * @brief Read GSR and analyze for stress detection
 * @param result Pointer to store the analysis result
 */
void gsr_read_and_analyze(gsr_result_t *result);

/**
 * @brief Check if GSR calibration is complete
 * @return true if baseline is established
 */
bool gsr_is_calibrated(void);

/**
 * @brief Get current GSR baseline value
 * @return Baseline ADC value
 */
float gsr_get_baseline(void);

#ifdef __cplusplus
}
#endif







