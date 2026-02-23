// HRV (Heart Rate Variability) Detection Header
// MAX30102 PPG Sensor - Heuristic ictal tachycardia detection
// Derived from SeizeIT2 dataset baseline statistics

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// MAX30102 I2C Configuration
#define MAX30102_I2C_ADDR       0x57
#define MAX30102_REG_INTR_STATUS_1  0x00
#define MAX30102_REG_INTR_STATUS_2  0x01
#define MAX30102_REG_INTR_ENABLE_1  0x02
#define MAX30102_REG_INTR_ENABLE_2  0x03
#define MAX30102_REG_FIFO_WR_PTR    0x04
#define MAX30102_REG_OVF_COUNTER    0x05
#define MAX30102_REG_FIFO_RD_PTR    0x06
#define MAX30102_REG_FIFO_DATA      0x07
#define MAX30102_REG_FIFO_CONFIG    0x08
#define MAX30102_REG_MODE_CONFIG    0x09
#define MAX30102_REG_SPO2_CONFIG    0x0A
#define MAX30102_REG_LED1_PA        0x0C
#define MAX30102_REG_LED2_PA        0x0D
#define MAX30102_REG_PART_ID        0xFF

// HRV Analysis Configuration
#define HRV_WINDOW_SEC          30      // Analysis window in seconds
#define HRV_SAMPLE_RATE         25      // MAX30102 sample rate (Hz)
#define HRV_WINDOW_SAMPLES      (HRV_WINDOW_SEC * HRV_SAMPLE_RATE)

// Baseline parameters from SeizeIT2 dataset
// Note: These are dataset-specific baselines for ictal change detection
#define HRV_BASELINE_HR         72.4f   // Baseline heart rate (BPM)
#define HRV_BASELINE_RMSSD      164.4f  // Baseline RMSSD (ms)
#define HRV_BASELINE_SDNN       118.1f  // Baseline SDNN (ms)

// Detection thresholds
#define HRV_HR_THRESHOLD        100.0f  // Tachycardia threshold (BPM)
#define HRV_RMSSD_THRESHOLD     20.0f   // Low HRV threshold (ms)
#define HRV_HR_INCREASE_PCT     20.0f   // Percent HR increase from baseline
#define HRV_RMSSD_DECREASE_PCT  30.0f   // Percent RMSSD decrease from baseline

// Calibration settings
#define HRV_CALIBRATION_SEC     60      // Personal baseline calibration period
#define HRV_MIN_PEAKS_FOR_HR    5       // Minimum peaks to calculate HR

// Result structure
typedef struct {
    float heart_rate;               // Current heart rate (BPM)
    float rmssd;                    // Root mean square of successive differences (ms)
    float sdnn;                     // Standard deviation of NN intervals (ms)
    float pnn50;                    // Percentage of intervals > 50ms difference
    float hr_change_pct;            // Percent change from baseline HR
    float rmssd_change_pct;         // Percent change from baseline RMSSD
    bool tachycardia_detected;      // Heart rate above threshold
    bool low_hrv_detected;          // HRV below threshold
    bool seizure_hrv_pattern;       // Both tachycardia + low HRV (ictal pattern)
    bool is_calibrating;            // Still establishing personal baseline
    uint32_t raw_ir;                // Raw IR value for debugging
} hrv_result_t;

/**
 * @brief Initialize MAX30102 PPG sensor
 * @return true if initialization successful, false otherwise
 */
bool hrv_init(void);

/**
 * @brief Read PPG data and analyze HRV
 * @param result Pointer to store the analysis result
 */
void hrv_read_and_analyze(hrv_result_t *result);

/**
 * @brief Check if HRV baseline calibration is complete
 * @return true if personal baseline is established
 */
bool hrv_is_calibrated(void);

/**
 * @brief Get current personal baseline HR
 * @return Baseline heart rate in BPM
 */
float hrv_get_baseline_hr(void);

/**
 * @brief Get current personal baseline RMSSD
 * @return Baseline RMSSD in ms
 */
float hrv_get_baseline_rmssd(void);

#ifdef __cplusplus
}
#endif







