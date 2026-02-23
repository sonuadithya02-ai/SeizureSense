// Seizure Detection Filtering and Validation Implementation
// Comprehensive false positive reduction for real-world wearable scenarios
//
// Sensor Placements:
// - MPU6050: Center of deltoid muscle (upper arm)
// - GSR electrodes: Fingertips
// - MAX30102: Another fingertip

#include "seizure_filter.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <math.h>

static const char* TAG = "SeizureFilter";

// ============================================================================
// STATIC STATE
// ============================================================================

static bool filter_initialized = false;

// Motion history for pattern detection
#define MOTION_HISTORY_SIZE 100  // 5 seconds at 20Hz
static float acc_history[MOTION_HISTORY_SIZE][3];
static float gyr_history[MOTION_HISTORY_SIZE][3];
static int history_index = 0;
static int history_count = 0;

// Peak detection for walking/rhythm analysis
#define PEAK_BUFFER_SIZE 20
static uint32_t peak_times[PEAK_BUFFER_SIZE];
static int peak_index = 0;
static int peak_count = 0;

// GSR history for rate of change detection
#define GSR_HISTORY_SIZE 30  // 7.5 seconds at 4Hz
static float gsr_history[GSR_HISTORY_SIZE];
static int gsr_history_idx = 0;
static int gsr_history_count = 0;

// PPG quality tracking
static uint32_t ppg_history[20];
static int ppg_history_idx = 0;

// Alert cooldown
#define ALERT_COOLDOWN_MS 30000  // 30 seconds between alerts

// ============================================================================
// INITIALIZATION
// ============================================================================

void seizure_filter_init(void)
{
    memset(acc_history, 0, sizeof(acc_history));
    memset(gyr_history, 0, sizeof(gyr_history));
    memset(peak_times, 0, sizeof(peak_times));
    memset(gsr_history, 0, sizeof(gsr_history));
    memset(ppg_history, 0, sizeof(ppg_history));
    
    history_index = 0;
    history_count = 0;
    peak_index = 0;
    peak_count = 0;
    gsr_history_idx = 0;
    gsr_history_count = 0;
    ppg_history_idx = 0;
    
    filter_initialized = true;
    
    ESP_LOGI(TAG, "Seizure filter initialized");
    ESP_LOGI(TAG, "  - Motion intensity filter: min %.2fg", MIN_SEIZURE_MOTION_MAGNITUDE);
    ESP_LOGI(TAG, "  - Temporal consistency: %d/%d windows", MIN_POSITIVE_DETECTIONS, DETECTION_WINDOW_SIZE);
    ESP_LOGI(TAG, "  - Alert cooldown: %d seconds", ALERT_COOLDOWN_MS/1000);
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

static float calculate_rms(const float* data, int len)
{
    float sum = 0;
    for (int i = 0; i < len; i++) {
        sum += data[i] * data[i];
    }
    return sqrtf(sum / len);
}

static float calculate_variance(const float* data, int len)
{
    if (len < 2) return 0;
    
    float mean = 0;
    for (int i = 0; i < len; i++) mean += data[i];
    mean /= len;
    
    float var = 0;
    for (int i = 0; i < len; i++) {
        float diff = data[i] - mean;
        var += diff * diff;
    }
    return var / (len - 1);
}

static float calculate_entropy(const float* data, int len)
{
    // Simplified entropy: based on value distribution
    // Higher entropy = more random/complex signal
    if (len < 10) return 0;
    
    // Normalize data to 0-1 range
    float min_val = data[0], max_val = data[0];
    for (int i = 1; i < len; i++) {
        if (data[i] < min_val) min_val = data[i];
        if (data[i] > max_val) max_val = data[i];
    }
    
    float range = max_val - min_val;
    if (range < 0.001f) return 0;  // No variation
    
    // Count values in bins
    #define NUM_BINS 10
    int bins[NUM_BINS] = {0};
    
    for (int i = 0; i < len; i++) {
        int bin = (int)((data[i] - min_val) / range * (NUM_BINS - 1));
        if (bin >= NUM_BINS) bin = NUM_BINS - 1;
        if (bin < 0) bin = 0;
        bins[bin]++;
    }
    
    // Calculate entropy
    float entropy = 0;
    for (int i = 0; i < NUM_BINS; i++) {
        if (bins[i] > 0) {
            float p = (float)bins[i] / len;
            entropy -= p * log2f(p);
        }
    }
    
    // Normalize to 0-1 (max entropy for 10 bins is log2(10) ≈ 3.32)
    return entropy / 3.32f;
}

static float detect_dominant_frequency(const float* data, int len, float sample_rate)
{
    // Simplified frequency detection using zero-crossing rate
    // and peak-to-peak period estimation
    
    if (len < 20) return 0;
    
    // Calculate mean
    float mean = 0;
    for (int i = 0; i < len; i++) mean += data[i];
    mean /= len;
    
    // Count zero crossings (around mean)
    int zero_crossings = 0;
    for (int i = 1; i < len; i++) {
        if ((data[i-1] < mean && data[i] >= mean) ||
            (data[i-1] >= mean && data[i] < mean)) {
            zero_crossings++;
        }
    }
    
    // Frequency ≈ zero_crossings / 2 / duration
    float duration = len / sample_rate;
    float freq = (zero_crossings / 2.0f) / duration;
    
    return freq;
}

static float calculate_regularity(const float* data, int len, float sample_rate)
{
    // Measure how regular/periodic the signal is
    // High regularity = consistent repeating pattern (walking, brushing)
    // Low regularity = random or complex pattern (seizure)
    
    if (len < 40) return 0;
    
    // Calculate autocorrelation at different lags
    float mean = 0;
    for (int i = 0; i < len; i++) mean += data[i];
    mean /= len;
    
    // Find peak autocorrelation (excluding lag 0)
    float max_corr = 0;
    int best_lag = 0;
    
    // Search lags corresponding to 0.5-3 Hz (typical activity range)
    int min_lag = (int)(sample_rate / 3.0f);  // 3 Hz max
    int max_lag = (int)(sample_rate / 0.5f);  // 0.5 Hz min
    if (max_lag > len / 2) max_lag = len / 2;
    
    for (int lag = min_lag; lag < max_lag; lag++) {
        float corr = 0;
        float var1 = 0, var2 = 0;
        
        for (int i = 0; i < len - lag; i++) {
            float v1 = data[i] - mean;
            float v2 = data[i + lag] - mean;
            corr += v1 * v2;
            var1 += v1 * v1;
            var2 += v2 * v2;
        }
        
        if (var1 > 0 && var2 > 0) {
            corr = corr / sqrtf(var1 * var2);
            if (corr > max_corr) {
                max_corr = corr;
                best_lag = lag;
            }
        }
    }
    
    return max_corr;  // 0-1, higher = more regular
}

// ============================================================================
// MOTION PATTERN ANALYSIS
// ============================================================================

void analyze_motion_patterns(const float acc_data[][3], const float gyr_data[][3],
                            int num_frames, motion_analysis_t* result)
{
    if (!result || num_frames < 20) {
        if (result) memset(result, 0, sizeof(motion_analysis_t));
        return;
    }
    
    memset(result, 0, sizeof(motion_analysis_t));
    
    // Store in history
    for (int i = 0; i < num_frames && i < 10; i++) {
        int idx = (history_index + i) % MOTION_HISTORY_SIZE;
        memcpy(acc_history[idx], acc_data[num_frames - 10 + i], 3 * sizeof(float));
        memcpy(gyr_history[idx], gyr_data[num_frames - 10 + i], 3 * sizeof(float));
    }
    history_index = (history_index + 10) % MOTION_HISTORY_SIZE;
    if (history_count < MOTION_HISTORY_SIZE) history_count += 10;
    if (history_count > MOTION_HISTORY_SIZE) history_count = MOTION_HISTORY_SIZE;
    
    // ====== INTENSITY ANALYSIS ======
    
    // Calculate accelerometer magnitude (remove gravity estimate)
    float acc_mag_buffer[MOTION_HISTORY_SIZE];
    float gyr_mag_buffer[MOTION_HISTORY_SIZE];
    float acc_magnitude_sum = 0;
    float gyr_magnitude_sum = 0;
    
    int samples_to_analyze = (history_count < 60) ? history_count : 60;  // Last 3 seconds
    
    for (int i = 0; i < samples_to_analyze; i++) {
        int idx = (history_index - samples_to_analyze + i + MOTION_HISTORY_SIZE) % MOTION_HISTORY_SIZE;
        
        // Accelerometer magnitude (subtract ~1g for gravity)
        float ax = acc_history[idx][0];
        float ay = acc_history[idx][1];
        float az = acc_history[idx][2];
        float acc_mag = sqrtf(ax*ax + ay*ay + az*az);
        float acc_dynamic = fabsf(acc_mag - 1.0f);  // Dynamic component (subtract gravity)
        acc_mag_buffer[i] = acc_dynamic;
        acc_magnitude_sum += acc_dynamic;
        
        // Gyroscope magnitude
        float gx = gyr_history[idx][0];
        float gy = gyr_history[idx][1];
        float gz = gyr_history[idx][2];
        float gyr_mag = sqrtf(gx*gx + gy*gy + gz*gz);
        gyr_mag_buffer[i] = gyr_mag;
        gyr_magnitude_sum += gyr_mag;
    }
    
    result->acc_magnitude = acc_magnitude_sum / samples_to_analyze;
    result->gyr_magnitude = gyr_magnitude_sum / samples_to_analyze;
    result->motion_intensity = (result->acc_magnitude + result->gyr_magnitude) / 2.0f;
    
    // ====== STATIC DETECTION ======
    
    // If motion is very low, person is still
    result->is_static = (result->motion_intensity < STATIC_MOTION_THRESHOLD);
    
    // ====== PATTERN ANALYSIS ======
    
    // Use accelerometer Z (vertical for deltoid) for frequency analysis
    float az_buffer[MOTION_HISTORY_SIZE];
    for (int i = 0; i < samples_to_analyze; i++) {
        int idx = (history_index - samples_to_analyze + i + MOTION_HISTORY_SIZE) % MOTION_HISTORY_SIZE;
        az_buffer[i] = acc_history[idx][2];  // Z-axis
    }
    
    result->dominant_frequency = detect_dominant_frequency(acc_mag_buffer, samples_to_analyze, 20.0f);
    result->motion_regularity = calculate_regularity(acc_mag_buffer, samples_to_analyze, 20.0f);
    result->motion_entropy = calculate_entropy(acc_mag_buffer, samples_to_analyze);
    
    // ====== WALKING DETECTION ======
    // Walking: regular periodic motion at 1-2 Hz, moderate intensity
    
    bool walking_frequency = (result->dominant_frequency >= 0.8f && result->dominant_frequency <= 2.5f);
    bool walking_regularity = (result->motion_regularity > WALKING_REGULARITY_THRESHOLD);
    bool walking_intensity = (result->acc_magnitude >= 0.15f && result->acc_magnitude <= 0.8f);
    
    result->is_walking = walking_frequency && walking_regularity && walking_intensity;
    
    // ====== RHYTHMIC ACTIVITY DETECTION ======
    // Brushing teeth, stirring, etc.: very regular at 1-4 Hz
    
    bool rhythmic_frequency = (result->dominant_frequency >= RHYTHMIC_MIN_FREQUENCY_HZ && 
                               result->dominant_frequency <= RHYTHMIC_MAX_FREQUENCY_HZ);
    bool rhythmic_regularity = (result->motion_regularity > RHYTHMIC_REGULARITY_THRESHOLD);
    bool not_walking = !result->is_walking;
    
    result->is_rhythmic_activity = rhythmic_frequency && rhythmic_regularity && not_walking;
    
    // ====== ENVIRONMENTAL VIBRATION DETECTION ======
    // Vehicle vibration: high frequency (>5 Hz), very uniform
    
    bool high_frequency = (result->dominant_frequency > VIBRATION_MIN_FREQUENCY_HZ);
    bool very_uniform = (result->motion_regularity > VIBRATION_UNIFORMITY_THRESHOLD);
    bool low_gyro = (result->gyr_magnitude < 0.1f);  // Vibration has little rotation
    
    result->is_environmental_vibration = high_frequency && very_uniform && low_gyro;
    
    // ====== POSTURAL TRANSITION DETECTION ======
    // Standing/sitting: brief, high intensity, with gravity vector change
    
    // Check for gravity vector change (arm position)
    static float last_gravity[3] = {0, 0, 1};
    float current_gravity[3] = {0, 0, 0};
    
    // Low-pass estimate of gravity from recent accelerometer
    for (int i = 0; i < 10; i++) {
        int idx = (history_index - 10 + i + MOTION_HISTORY_SIZE) % MOTION_HISTORY_SIZE;
        current_gravity[0] += acc_history[idx][0];
        current_gravity[1] += acc_history[idx][1];
        current_gravity[2] += acc_history[idx][2];
    }
    current_gravity[0] /= 10;
    current_gravity[1] /= 10;
    current_gravity[2] /= 10;
    
    float gravity_change = sqrtf(
        (current_gravity[0] - last_gravity[0]) * (current_gravity[0] - last_gravity[0]) +
        (current_gravity[1] - last_gravity[1]) * (current_gravity[1] - last_gravity[1]) +
        (current_gravity[2] - last_gravity[2]) * (current_gravity[2] - last_gravity[2])
    );
    
    // Update for next iteration
    memcpy(last_gravity, current_gravity, sizeof(last_gravity));
    
    // Transition: moderate motion with gravity change, low entropy (simple motion)
    bool has_gravity_change = (gravity_change > GRAVITY_CHANGE_THRESHOLD);
    bool moderate_motion = (result->motion_intensity >= 0.2f && result->motion_intensity <= 0.7f);
    bool simple_motion = (result->motion_entropy < 0.5f);
    
    result->is_postural_transition = has_gravity_change && moderate_motion && simple_motion;
    
    // ====== SEIZURE MOTION CONSISTENCY ======
    // Seizures: high intensity, moderate regularity (not perfect), sustained
    
    bool sufficient_intensity = (result->acc_magnitude >= MIN_SEIZURE_MOTION_MAGNITUDE ||
                                 result->gyr_magnitude >= MIN_SEIZURE_GYR_MAGNITUDE);
    
    // Seizures have some regularity (clonic phase) but not perfect like intentional activities
    bool seizure_regularity = (result->motion_regularity >= SEIZURE_IRREGULARITY_MIN &&
                               result->motion_regularity <= (1.0f - SEIZURE_IRREGULARITY_MIN));
    
    // Not any detected intentional activity
    bool not_intentional = !result->is_walking && 
                           !result->is_rhythmic_activity && 
                           !result->is_environmental_vibration &&
                           !result->is_postural_transition;
    
    result->motion_consistent_with_seizure = sufficient_intensity && seizure_regularity && not_intentional;
    
    // Calculate seizure motion confidence
    if (result->motion_consistent_with_seizure) {
        float intensity_score = (result->motion_intensity - MIN_SEIZURE_MOTION_MAGNITUDE) / 0.5f;
        if (intensity_score > 1.0f) intensity_score = 1.0f;
        if (intensity_score < 0) intensity_score = 0;
        
        result->seizure_motion_confidence = intensity_score * 100.0f;
    } else {
        result->seizure_motion_confidence = 0;
    }
}

// ============================================================================
// TEMPORAL STATE MANAGEMENT
// ============================================================================

void update_temporal_state(bool detection_positive, float confidence,
                          temporal_state_t* state)
{
    if (!state) return;
    
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    // Update detection history
    state->detection_history[state->history_index] = detection_positive;
    state->history_index = (state->history_index + 1) % DETECTION_WINDOW_SIZE;
    
    // Count positive detections in window
    state->positive_count = 0;
    for (int i = 0; i < DETECTION_WINDOW_SIZE; i++) {
        if (state->detection_history[i]) {
            state->positive_count++;
        }
    }
    
    // Track sustained detection timing
    if (detection_positive) {
        if (state->first_detection_time == 0) {
            state->first_detection_time = current_time;
        }
        state->last_detection_time = current_time;
        state->sustained_duration_ms = state->last_detection_time - state->first_detection_time;
        
        state->cumulative_confidence += confidence;
        state->detection_count++;
    } else {
        // Reset if no detection for a while (gap tolerance: 500ms)
        if (current_time - state->last_detection_time > 500) {
            state->first_detection_time = 0;
            state->sustained_duration_ms = 0;
            state->cumulative_confidence = 0;
            state->detection_count = 0;
        }
    }
    
    // Check if alert should be pending
    bool temporal_consistent = (state->positive_count >= MIN_POSITIVE_DETECTIONS);
    bool duration_sufficient = (state->sustained_duration_ms >= MIN_SEIZURE_DURATION_SAMPLES * 50);
    
    state->alert_pending = temporal_consistent && duration_sufficient;
    
    // Check cooldown
    if (state->last_alert_time > 0 && 
        current_time - state->last_alert_time < state->alert_cooldown_ms) {
        state->alert_pending = false;  // Still in cooldown
    }
}

// ============================================================================
// SENSOR QUALITY CHECKS
// ============================================================================

void check_sensor_quality(float gsr_raw, uint32_t ppg_ir, 
                         const float acc_data[3], sensor_quality_t* quality)
{
    if (!quality) return;
    
    memset(quality, 0, sizeof(sensor_quality_t));
    
    // ====== GSR QUALITY ======
    
    // Store GSR history
    gsr_history[gsr_history_idx] = gsr_raw;
    gsr_history_idx = (gsr_history_idx + 1) % GSR_HISTORY_SIZE;
    if (gsr_history_count < GSR_HISTORY_SIZE) gsr_history_count++;
    
    // Check for valid contact (not too high or too low)
    // No contact: very high impedance = low reading (near 0)
    // Good contact: mid-range values (500-3500 typically)
    quality->gsr_contact_good = (gsr_raw > 200 && gsr_raw < 4000);
    
    // Check for water on fingers (sudden large change or very high value)
    if (gsr_history_count >= 5) {
        float recent_avg = 0;
        for (int i = 0; i < 5; i++) {
            int idx = (gsr_history_idx - 1 - i + GSR_HISTORY_SIZE) % GSR_HISTORY_SIZE;
            recent_avg += gsr_history[idx];
        }
        recent_avg /= 5;
        
        // Large sudden change might indicate water
        float change = fabsf(gsr_raw - recent_avg);
        quality->gsr_likely_wet = (change > 1000 || gsr_raw > 3800);
    }
    
    // Signal stability
    if (gsr_history_count >= 10) {
        float variance = calculate_variance(gsr_history, 
            (gsr_history_count < 10) ? gsr_history_count : 10);
        quality->gsr_signal_stability = 1.0f - (variance / 1000000.0f);
        if (quality->gsr_signal_stability < 0) quality->gsr_signal_stability = 0;
    } else {
        quality->gsr_signal_stability = 0.5f;  // Unknown
    }
    
    // ====== PPG QUALITY ======
    
    // Store PPG history
    ppg_history[ppg_history_idx] = ppg_ir;
    ppg_history_idx = (ppg_history_idx + 1) % 20;
    
    // Check for valid contact (not too high or too low)
    // Saturation (max value): pressing too hard or too much ambient light
    // Too low: no finger contact
    quality->ppg_contact_good = (ppg_ir > 10000 && ppg_ir < 250000);
    
    // Motion artifact detection based on accelerometer
    float acc_mag = sqrtf(acc_data[0]*acc_data[0] + 
                          acc_data[1]*acc_data[1] + 
                          acc_data[2]*acc_data[2]);
    float acc_dynamic = fabsf(acc_mag - 1.0f);
    quality->ppg_motion_artifact = (acc_dynamic > 0.5f);
    
    // Signal quality based on amplitude and variability
    if (ppg_ir > 10000 && ppg_ir < 250000) {
        float base_quality = 0.8f;
        if (quality->ppg_motion_artifact) base_quality -= 0.4f;
        if (ppg_ir > 200000) base_quality -= 0.2f;  // Near saturation
        if (ppg_ir < 30000) base_quality -= 0.2f;   // Weak signal
        quality->ppg_signal_quality = (base_quality > 0) ? base_quality : 0;
    } else {
        quality->ppg_signal_quality = 0;
    }
    
    // ====== MPU6050 QUALITY ======
    
    // Check for saturation (readings at ±2g limit)
    float max_acc = 2.0f;  // ±2g range
    quality->mpu_saturated = (fabsf(acc_data[0]) > 1.95f ||
                              fabsf(acc_data[1]) > 1.95f ||
                              fabsf(acc_data[2]) > 1.95f);
    
    // Data valid if readings are reasonable
    float total_acc = sqrtf(acc_data[0]*acc_data[0] + 
                            acc_data[1]*acc_data[1] + 
                            acc_data[2]*acc_data[2]);
    quality->mpu_data_valid = (total_acc > 0.5f && total_acc < 4.0f);  // Should see gravity
}

// ============================================================================
// COMPREHENSIVE FILTERING
// ============================================================================

void apply_seizure_filter(float ml_conf, float hrv_conf, float gsr_conf,
                         const motion_analysis_t* motion,
                         temporal_state_t* temporal,
                         const sensor_quality_t* quality,
                         filtered_result_t* result)
{
    if (!result) return;
    
    memset(result, 0, sizeof(filtered_result_t));
    
    result->ml_confidence = ml_conf;
    result->hrv_confidence = hrv_conf;
    result->gsr_confidence = gsr_conf;
    
    // Start with raw confidences
    result->adjusted_ml_confidence = ml_conf;
    result->adjusted_hrv_confidence = hrv_conf;
    result->adjusted_gsr_confidence = gsr_conf;
    
    // ====== ACTIVITY-BASED SUPPRESSION ======
    
    if (motion) {
        // Walking detected - very unlikely to be having a seizure while walking normally
        if (motion->is_walking) {
            result->suppressed_by_walking = true;
            result->adjusted_ml_confidence *= 0.2f;  // 80% reduction
            result->rejection_reason = "Walking detected";
        }
        
        // Rhythmic intentional activity (brushing teeth, etc.)
        if (motion->is_rhythmic_activity) {
            result->suppressed_by_rhythmic = true;
            result->adjusted_ml_confidence *= 0.1f;  // 90% reduction
            result->rejection_reason = "Rhythmic activity detected";
        }
        
        // Environmental vibration (vehicle, machinery)
        if (motion->is_environmental_vibration) {
            result->suppressed_by_vibration = true;
            result->adjusted_ml_confidence *= 0.1f;  // 90% reduction
            result->rejection_reason = "Environmental vibration detected";
        }
        
        // Low motion - person is still, not seizing
        if (motion->is_static) {
            result->suppressed_by_low_motion = true;
            result->adjusted_ml_confidence *= 0.3f;  // 70% reduction
            // Don't set rejection_reason - could be absence seizure (but very rare)
        }
        
        // Brief postural transition (standing up, sitting down)
        if (motion->is_postural_transition) {
            result->suppressed_by_transition = true;
            result->adjusted_ml_confidence *= 0.3f;  // 70% reduction
            result->rejection_reason = "Postural transition detected";
        }
        
        // If motion is NOT consistent with seizure, major reduction
        if (!motion->motion_consistent_with_seizure && !motion->is_static) {
            result->adjusted_ml_confidence *= 0.5f;
        }
    }
    
    // ====== SENSOR QUALITY ADJUSTMENTS ======
    
    if (quality) {
        // Poor GSR contact - reduce GSR confidence
        if (!quality->gsr_contact_good || quality->gsr_likely_wet) {
            result->adjusted_gsr_confidence *= 0.2f;
        }
        
        // Poor PPG signal - reduce HRV confidence
        if (!quality->ppg_contact_good || quality->ppg_motion_artifact) {
            result->adjusted_hrv_confidence *= 0.3f;
        }
        
        // MPU issues
        if (!quality->mpu_data_valid || quality->mpu_saturated) {
            result->adjusted_ml_confidence *= 0.5f;
        }
    }
    
    // ====== THRESHOLDING ======
    
    // Higher thresholds than before
    #define ML_HIGH_THRESHOLD       80.0f   // Was 50% (threshold 0), now 80%
    #define ML_MEDIUM_THRESHOLD     60.0f   // For multi-modal confirmation
    #define HRV_CONFIRMATION_THRESH 40.0f   // HRV threshold for confirmation
    #define GSR_CONFIRMATION_THRESH 50.0f   // GSR threshold for confirmation
    
    bool ml_high_confidence = (result->adjusted_ml_confidence >= ML_HIGH_THRESHOLD);
    bool ml_medium_confidence = (result->adjusted_ml_confidence >= ML_MEDIUM_THRESHOLD);
    bool hrv_positive = (result->adjusted_hrv_confidence >= HRV_CONFIRMATION_THRESH);
    bool gsr_positive = (result->adjusted_gsr_confidence >= GSR_CONFIRMATION_THRESH);
    
    // Count positive modalities
    int positive_modalities = (ml_medium_confidence ? 1 : 0) + 
                              (hrv_positive ? 1 : 0) + 
                              (gsr_positive ? 1 : 0);
    
    // ====== DECISION LOGIC ======
    
    // STRICT: Require EITHER:
    // 1. ML very high confidence (>80%) AND at least one other modality positive
    // 2. All three modalities positive
    // 3. ML high AND motion is consistent with seizure
    
    bool detection_positive = false;
    
    if (ml_high_confidence && (hrv_positive || gsr_positive)) {
        // High ML + at least one physiological confirmation
        detection_positive = true;
    } else if (positive_modalities >= 3) {
        // All three modalities agree
        detection_positive = true;
    } else if (ml_high_confidence && motion && motion->motion_consistent_with_seizure) {
        // High ML + motion pattern matches seizure characteristics
        detection_positive = true;
    }
    
    // Any suppression active? Require even stronger evidence
    bool any_suppression = result->suppressed_by_walking ||
                           result->suppressed_by_rhythmic ||
                           result->suppressed_by_vibration ||
                           result->suppressed_by_transition;
    
    if (any_suppression && detection_positive) {
        // Override unless VERY strong evidence
        if (result->adjusted_ml_confidence < 90.0f) {
            detection_positive = false;
        }
    }
    
    // ====== TEMPORAL CONSISTENCY ======
    
    if (temporal) {
        update_temporal_state(detection_positive, result->adjusted_ml_confidence, temporal);
        
        // Final decision requires temporal consistency
        if (detection_positive) {
            // Must have sustained detection
            result->seizure_detected = temporal->alert_pending;
        } else {
            result->seizure_detected = false;
        }
    } else {
        // No temporal state - use instantaneous (less reliable)
        result->seizure_detected = detection_positive;
    }
    
    // ====== FINAL CONFIDENCE ======
    
    // Weighted combination of adjusted confidences
    result->final_confidence = (result->adjusted_ml_confidence * 0.5f +
                                result->adjusted_hrv_confidence * 0.25f +
                                result->adjusted_gsr_confidence * 0.25f);
    
    if (result->seizure_detected && temporal) {
        // Boost confidence if sustained
        float duration_factor = temporal->sustained_duration_ms / 10000.0f;  // Max at 10 seconds
        if (duration_factor > 1.0f) duration_factor = 1.0f;
        result->final_confidence = result->final_confidence * (0.7f + 0.3f * duration_factor);
    }
}

// ============================================================================
// ALERT TRIGGERING WITH COOLDOWN
// ============================================================================

bool should_trigger_alert(const filtered_result_t* result, temporal_state_t* temporal)
{
    if (!result || !temporal) return false;
    
    if (!result->seizure_detected) return false;
    
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    // Check cooldown
    if (temporal->last_alert_time > 0) {
        uint32_t time_since_last = current_time - temporal->last_alert_time;
        if (time_since_last < temporal->alert_cooldown_ms) {
            return false;  // Still in cooldown
        }
    }
    
    // Set cooldown for next alert
    temporal->alert_cooldown_ms = ALERT_COOLDOWN_MS;
    
    // Alert!
    if (!temporal->alert_confirmed) {
        temporal->alert_confirmed = true;
        temporal->last_alert_time = current_time;
        return true;
    }
    
    return false;
}

