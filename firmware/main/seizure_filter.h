// Seizure Detection Filtering and Validation
// Comprehensive false positive reduction for real-world scenarios
//
// This module addresses gaps in the ML model (trained on hospital bed-rest data)
// by implementing activity recognition, temporal consistency, and multi-modal validation

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// MOTION ANALYSIS CONFIGURATION
// ============================================================================

// Minimum motion intensity for seizure consideration
// Seizures produce significant motion; sitting/standing transitions are brief
#define MIN_SEIZURE_MOTION_MAGNITUDE    0.5f    // Minimum g-force variation
#define MIN_SEIZURE_GYR_MAGNITUDE       0.3f    // Minimum angular velocity variation

// Duration requirements (in samples at 20Hz)
#define MIN_SEIZURE_DURATION_SAMPLES    60      // 3 seconds minimum
#define MAX_TRANSITION_DURATION         20      // 1 second - normal transitions are fast

// Temporal consistency - sustained detection required
#define DETECTION_WINDOW_SIZE           10      // 10 consecutive windows
#define MIN_POSITIVE_DETECTIONS         7       // 70% of windows must be positive

// ============================================================================
// ACTIVITY RECOGNITION THRESHOLDS
// ============================================================================

// Walking detection (regular periodic motion)
#define WALKING_STEP_PERIOD_MIN_MS      300     // 200 BPM max walking
#define WALKING_STEP_PERIOD_MAX_MS      1500    // 40 BPM min walking
#define WALKING_REGULARITY_THRESHOLD    0.7f    // How regular steps should be

// Rhythmic activity detection (brushing teeth, stirring, etc.)
#define RHYTHMIC_MIN_FREQUENCY_HZ       1.0f    // 1 Hz minimum
#define RHYTHMIC_MAX_FREQUENCY_HZ       4.0f    // 4 Hz maximum
#define RHYTHMIC_REGULARITY_THRESHOLD   0.8f    // High regularity = intentional

// Static/rest detection
#define STATIC_MOTION_THRESHOLD         0.1f    // Below this = person is still
#define GRAVITY_CHANGE_THRESHOLD        0.3f    // Arm position change detection

// Vehicle/environmental vibration
#define VIBRATION_MIN_FREQUENCY_HZ      5.0f    // Vehicle vibration typically >5Hz
#define VIBRATION_UNIFORMITY_THRESHOLD  0.85f   // Very uniform = external vibration

// ============================================================================
// SEIZURE CHARACTERISTICS (from literature)
// ============================================================================

// Tonic phase: sustained muscle contraction (3-10 seconds)
#define TONIC_PHASE_MIN_DURATION_MS     3000
#define TONIC_PHASE_MAX_DURATION_MS     15000

// Clonic phase: rhythmic jerking (30-120 seconds)
#define CLONIC_PHASE_MIN_DURATION_MS    30000
#define CLONIC_PHASE_MAX_DURATION_MS    180000
#define CLONIC_FREQUENCY_MIN_HZ         2.0f    // 2-8 Hz is characteristic
#define CLONIC_FREQUENCY_MAX_HZ         8.0f

// Motion irregularity - seizures are NOT perfectly rhythmic
#define SEIZURE_IRREGULARITY_MIN        0.15f   // Some variation expected
#define SEIZURE_IRREGULARITY_MAX        0.6f    // But not random

// ============================================================================
// DATA STRUCTURES
// ============================================================================

// Motion analysis result
typedef struct {
    // Intensity metrics
    float acc_magnitude;            // RMS of accelerometer
    float gyr_magnitude;            // RMS of gyroscope
    float motion_intensity;         // Combined intensity score
    
    // Pattern analysis
    float dominant_frequency;       // Main frequency component
    float frequency_power;          // Power at dominant frequency
    float motion_regularity;        // 0=chaotic, 1=perfectly periodic
    float motion_entropy;           // Measure of randomness/complexity
    
    // Activity classification
    bool is_static;                 // Person not moving
    bool is_walking;                // Walking detected
    bool is_rhythmic_activity;      // Brushing teeth, stirring, etc.
    bool is_environmental_vibration; // Vehicle, machinery
    bool is_postural_transition;    // Standing up, sitting down
    
    // Seizure-specific
    bool motion_consistent_with_seizure;
    float seizure_motion_confidence;
} motion_analysis_t;

// Temporal detection state
typedef struct {
    // Detection history (sliding window)
    bool detection_history[DETECTION_WINDOW_SIZE];
    int history_index;
    int positive_count;
    
    // Sustained detection tracking
    uint32_t first_detection_time;
    uint32_t last_detection_time;
    uint32_t sustained_duration_ms;
    
    // Confidence tracking
    float cumulative_confidence;
    int detection_count;
    
    // Alert state
    bool alert_pending;
    bool alert_confirmed;
    uint32_t last_alert_time;
    uint32_t alert_cooldown_ms;
} temporal_state_t;

// Sensor quality metrics
typedef struct {
    // GSR quality
    bool gsr_contact_good;
    float gsr_signal_stability;
    bool gsr_likely_wet;            // Water on fingers
    
    // PPG quality
    bool ppg_contact_good;
    float ppg_signal_quality;       // 0-1, based on amplitude and noise
    bool ppg_motion_artifact;       // High motion corrupting signal
    
    // MPU6050 quality
    bool mpu_data_valid;
    bool mpu_saturated;             // Clipping at max values
} sensor_quality_t;

// Final filtered result
typedef struct {
    // Raw modality results
    float ml_confidence;
    float hrv_confidence;
    float gsr_confidence;
    
    // Filtered/adjusted confidences
    float adjusted_ml_confidence;
    float adjusted_hrv_confidence;
    float adjusted_gsr_confidence;
    
    // Activity suppression
    bool suppressed_by_walking;
    bool suppressed_by_rhythmic;
    bool suppressed_by_vibration;
    bool suppressed_by_low_motion;
    bool suppressed_by_transition;
    
    // Final decision
    bool seizure_detected;
    float final_confidence;
    const char* rejection_reason;   // Why detection was rejected (if any)
} filtered_result_t;

// ============================================================================
// FUNCTION PROTOTYPES
// ============================================================================

/**
 * @brief Initialize the seizure filter module
 */
void seizure_filter_init(void);

/**
 * @brief Analyze motion data for activity patterns
 * @param acc_data Accelerometer data buffer [frames][3]
 * @param gyr_data Gyroscope data buffer [frames][3]
 * @param num_frames Number of frames in buffer
 * @param result Output motion analysis result
 */
void analyze_motion_patterns(const float acc_data[][3], const float gyr_data[][3],
                            int num_frames, motion_analysis_t* result);

/**
 * @brief Update temporal detection state
 * @param detection_positive Whether current detection is positive
 * @param confidence Current detection confidence (0-100)
 * @param state Temporal state to update
 */
void update_temporal_state(bool detection_positive, float confidence,
                          temporal_state_t* state);

/**
 * @brief Check sensor signal quality
 * @param gsr_raw Raw GSR value
 * @param ppg_ir Raw PPG IR value
 * @param acc_data Current accelerometer reading
 * @param quality Output quality metrics
 */
void check_sensor_quality(float gsr_raw, uint32_t ppg_ir, 
                         const float acc_data[3], sensor_quality_t* quality);

/**
 * @brief Apply comprehensive filtering to detection
 * @param ml_conf ML model confidence (0-100)
 * @param hrv_conf HRV confidence (0-100)
 * @param gsr_conf GSR confidence (0-100)
 * @param motion Motion analysis result
 * @param temporal Temporal state
 * @param quality Sensor quality
 * @param result Output filtered result
 */
void apply_seizure_filter(float ml_conf, float hrv_conf, float gsr_conf,
                         const motion_analysis_t* motion,
                         temporal_state_t* temporal,
                         const sensor_quality_t* quality,
                         filtered_result_t* result);

/**
 * @brief Check if alert should be triggered (with cooldown)
 * @param result Filtered detection result
 * @param temporal Temporal state
 * @return true if alert should be triggered
 */
bool should_trigger_alert(const filtered_result_t* result, temporal_state_t* temporal);

#ifdef __cplusplus
}
#endif

