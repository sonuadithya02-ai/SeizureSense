// Multi-Modal Seizure Detection System with Comprehensive Filtering
// ==================================================================
// 
// Combines three detection modalities:
// 1. Motion (ACC + GYR): ML model trained on SeizeIT2 dataset
// 2. HRV (MAX30102): Heuristic ictal tachycardia + reduced HRV detection
// 3. GSR (Grove): Heuristic stress/arousal detection
//
// Sensor Placements (Glove Format):
// - MPU6050: Center of deltoid muscle (upper arm)
// - GSR electrodes: Fingertips
// - MAX30102 (PPG): Another fingertip
//
// Comprehensive Filtering:
// - Activity recognition (walking, brushing teeth, etc.)
// - Environmental vibration detection (vehicle, machinery)
// - Postural transition filtering (standing, sitting)
// - Temporal consistency (sustained detection required)
// - Sensor quality monitoring
//
// ESP32-S3 N16R8 platform with NimBLE Bluetooth alerting


#include "seizure_model_new.cc"
#include "model_settings.h"
#include "detection_responder.h"
#include "hrv_detection.h"
#include "gsr_detection.h"
#include "seizure_filter.h"

#include "esp_log.h"
#include "driver/i2c.h"
#include "esp_heap_caps.h"

// NimBLE Beacon headers
extern "C" {
#include "common.h"
#include "gap.h"
}

#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"

// ============ I2C Configuration ============
#define I2C_MASTER_NUM      I2C_NUM_0
#define I2C_MASTER_SDA_IO   8   // Change if your wiring differs
#define I2C_MASTER_SCL_IO   9
#define I2C_MASTER_FREQ_HZ  400000

// ============ MPU6050 Configuration ============
#define MPU6050_SENSOR_ADDR 0x68
#define MPU6050_REG_PWR_MGMT_1   0x6B
#define MPU6050_REG_ACCEL_XOUT_H 0x3B
#define MPU6050_REG_GYRO_XOUT_H  0x43

static const char *LOCAL_TAG = "SeizureDetection";

// ============ Motion Data Buffer ============
float motion_window[NUM_FRAMES][NUM_CHANNELS] = {0};
int window_index = 0;

// Separate ACC and GYR buffers for motion analysis
float acc_buffer[NUM_FRAMES][3] = {0};
float gyr_buffer[NUM_FRAMES][3] = {0};

// ============ TFLite Configuration ============
constexpr int kTensorArenaSize = 80 * 1024;
uint8_t *tensor_arena = nullptr;

// ============ Detection State ============
static bool seizure_ble_triggered = false;
static int motion_confidence = 0;  // 0-100
static int hrv_confidence = 0;     // 0-100  
static int gsr_confidence = 0;     // 0-100

// ============ Filtering State ============
static motion_analysis_t motion_analysis = {};
static temporal_state_t temporal_state = {};
static sensor_quality_t sensor_quality = {};
static filtered_result_t filtered_result = {};

// ============ Sensor Connection State ============
static bool mpu6050_connected = false;

// ============ I2C Initialization ============
static void i2c_master_init()
{
    i2c_config_t conf = {};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;

    ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0));
    
    ESP_LOGI(LOCAL_TAG, "I2C master initialized (SDA=%d, SCL=%d)", 
             I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO);
}

// ============ MPU6050 Functions ============
static bool mpu6050_init()
{
    uint8_t wake_cmd[] = { MPU6050_REG_PWR_MGMT_1, 0x00 };
    esp_err_t ret = i2c_master_write_to_device(I2C_MASTER_NUM, MPU6050_SENSOR_ADDR,
                                               wake_cmd, sizeof(wake_cmd),
                                               1000 / portTICK_PERIOD_MS);
    
    if (ret == ESP_OK) {
        ESP_LOGI(LOCAL_TAG, "MPU6050 initialized (ACC + GYR)");
        ESP_LOGI(LOCAL_TAG, "  Placement: Center of deltoid muscle");
        mpu6050_connected = true;
        return true;
    } else {
        ESP_LOGW(LOCAL_TAG, "MPU6050 not found (err=%s) - Motion ML disabled", esp_err_to_name(ret));
        mpu6050_connected = false;
        return false;
    }
}

static bool mpu6050_read_motion(float *buffer /*len=6*/, float *acc_out /*len=3*/, float *gyr_out /*len=3*/)
{
    if (!mpu6050_connected) {
        // Return zeros if sensor not connected
        for (int i = 0; i < 6; i++) buffer[i] = 0.0f;
        if (acc_out) for (int i = 0; i < 3; i++) acc_out[i] = 0.0f;
        if (gyr_out) for (int i = 0; i < 3; i++) gyr_out[i] = 0.0f;
        return false;
    }
    
    // Read accelerometer (6 bytes starting at 0x3B)
    uint8_t accel_data[6];
    uint8_t accel_reg = MPU6050_REG_ACCEL_XOUT_H;
    esp_err_t ret = i2c_master_write_read_device(I2C_MASTER_NUM, MPU6050_SENSOR_ADDR,
                                                  &accel_reg, 1,
                                                  accel_data, sizeof(accel_data),
                                                  1000 / portTICK_PERIOD_MS);
    if (ret != ESP_OK) {
        for (int i = 0; i < 6; i++) buffer[i] = 0.0f;
        return false;
    }
    
    // Read gyroscope (6 bytes starting at 0x43)
    uint8_t gyro_data[6];
    uint8_t gyro_reg = MPU6050_REG_GYRO_XOUT_H;
    ret = i2c_master_write_read_device(I2C_MASTER_NUM, MPU6050_SENSOR_ADDR,
                                        &gyro_reg, 1,
                                        gyro_data, sizeof(gyro_data),
                                        1000 / portTICK_PERIOD_MS);
    if (ret != ESP_OK) {
        for (int i = 0; i < 6; i++) buffer[i] = 0.0f;
        return false;
    }
    
    // Parse accelerometer (convert to g, ±2g range)
    int16_t ax = (accel_data[0] << 8) | accel_data[1];
    int16_t ay = (accel_data[2] << 8) | accel_data[3];
    int16_t az = (accel_data[4] << 8) | accel_data[5];
    buffer[0] = ax / 16384.0f;  // ACC X
    buffer[1] = ay / 16384.0f;  // ACC Y
    buffer[2] = az / 16384.0f;  // ACC Z

    // Parse gyroscope (normalize to similar scale as accelerometer)
    int16_t gx = (gyro_data[0] << 8) | gyro_data[1];
    int16_t gy = (gyro_data[2] << 8) | gyro_data[3];
    int16_t gz = (gyro_data[4] << 8) | gyro_data[5];
    buffer[3] = (gx / 131.0f) / 250.0f;  // GYR X (normalized)
    buffer[4] = (gy / 131.0f) / 250.0f;  // GYR Y (normalized)
    buffer[5] = (gz / 131.0f) / 250.0f;  // GYR Z (normalized)
    
    // Also output separate ACC and GYR for motion analysis
    if (acc_out) {
        acc_out[0] = buffer[0];
        acc_out[1] = buffer[1];
        acc_out[2] = buffer[2];
    }
    if (gyr_out) {
        gyr_out[0] = buffer[3];
        gyr_out[1] = buffer[4];
        gyr_out[2] = buffer[5];
    }
    
    return true;
}

// ============ Calculate Raw Confidence Scores ============
static void calculate_raw_confidences(int8_t ml_output, hrv_result_t *hrv, gsr_result_t *gsr)
{
    // 1. Motion ML confidence (sigmoid output)
    // Output range: -128 to 127
    float ml_prob = (ml_output + 128) * OUTPUT_SCALE;  // Convert to 0-1 range
    motion_confidence = (int)(ml_prob * 100);
    
    // 2. HRV confidence (based on deviation from normal)
    if (hrv && !hrv->is_calibrating) {
        // Scale based on HR increase percentage (0-100%)
        float hr_score = hrv->hr_change_pct > 0 ? hrv->hr_change_pct * 2.5f : 0;  // 20% increase = 50%
        if (hr_score > 50) hr_score = 50;
        
        // Scale based on RMSSD decrease (0-50%)
        float rmssd_score = hrv->rmssd_change_pct > 0 ? hrv->rmssd_change_pct * 1.67f : 0;  // 30% decrease = 50%
        if (rmssd_score > 50) rmssd_score = 50;
        
        hrv_confidence = (int)(hr_score + rmssd_score);
        if (hrv_confidence > 100) hrv_confidence = 100;
    } else {
        hrv_confidence = 0;
    }
    
    // 3. GSR confidence (based on percent above baseline)
    if (gsr && !gsr->is_calibrating) {
        // Scale confidence based on how much above baseline
        if (gsr->percent_above_baseline > 0) {
            gsr_confidence = (int)(gsr->percent_above_baseline * 2);
            if (gsr_confidence > 100) gsr_confidence = 100;
        } else {
            gsr_confidence = 0;
        }
    } else {
        gsr_confidence = 0;
    }
}

// ============ Trigger BLE Alert ============
static void trigger_ble_alert_if_needed(const filtered_result_t *result)
{
    if (result->seizure_detected && !seizure_ble_triggered) {
        ESP_LOGW(LOCAL_TAG, "============================================");
        ESP_LOGW(LOCAL_TAG, "🚨 SEIZURE DETECTED! Starting BLE beacon...");
        ESP_LOGW(LOCAL_TAG, "============================================");
        ESP_LOGW(LOCAL_TAG, "Raw:      Motion=%d%% | HRV=%d%% | GSR=%d%%", 
                 motion_confidence, hrv_confidence, gsr_confidence);
        ESP_LOGW(LOCAL_TAG, "Adjusted: Motion=%.0f%% | HRV=%.0f%% | GSR=%.0f%%",
                 result->adjusted_ml_confidence,
                 result->adjusted_hrv_confidence,
                 result->adjusted_gsr_confidence);
        ESP_LOGW(LOCAL_TAG, "Final confidence: %.1f%%", result->final_confidence);
        ESP_LOGW(LOCAL_TAG, "Duration: %lu ms", temporal_state.sustained_duration_ms);
        ESP_LOGW(LOCAL_TAG, "============================================");
        
        start_seizure_advertising();
        seizure_ble_triggered = true;
    } else if (!result->seizure_detected) {
        seizure_ble_triggered = false;
    }
}

// ============ Logging ============
static void log_status_periodically(hrv_result_t *hrv, gsr_result_t *gsr, int8_t ml_output)
{
    static int log_counter = 0;
    static int detailed_log_counter = 0;
    
    if (++log_counter >= 20) {  // Every ~1 second
        log_counter = 0;
        
        // Compact status line
        const char* activity = "NORMAL";
        if (motion_analysis.is_walking) activity = "WALKING";
        else if (motion_analysis.is_rhythmic_activity) activity = "RHYTHMIC";
        else if (motion_analysis.is_environmental_vibration) activity = "VIBRATION";
        else if (motion_analysis.is_static) activity = "STATIC";
        else if (motion_analysis.is_postural_transition) activity = "TRANSITION";
        else if (motion_analysis.motion_consistent_with_seizure) activity = "⚠️ SUSPICIOUS";
        
        ESP_LOGI(LOCAL_TAG, "[%s] ML=%d%%(adj:%.0f%%) HR=%.0f GSR=%.0f%% | Intensity=%.2f Freq=%.1fHz",
                 activity,
                 motion_confidence,
                 filtered_result.adjusted_ml_confidence,
                 hrv ? hrv->heart_rate : 0,
                 gsr ? gsr->percent_above_baseline : 0,
                 motion_analysis.motion_intensity,
                 motion_analysis.dominant_frequency);
        
        // Detailed log every 10 seconds
        if (++detailed_log_counter >= 10) {
            detailed_log_counter = 0;
            
            ESP_LOGI(LOCAL_TAG, "--- Motion Analysis ---");
            ESP_LOGI(LOCAL_TAG, "  ACC mag: %.3f g | GYR mag: %.3f", 
                     motion_analysis.acc_magnitude, motion_analysis.gyr_magnitude);
            ESP_LOGI(LOCAL_TAG, "  Regularity: %.2f | Entropy: %.2f",
                     motion_analysis.motion_regularity, motion_analysis.motion_entropy);
            
            ESP_LOGI(LOCAL_TAG, "--- Sensor Quality ---");
            ESP_LOGI(LOCAL_TAG, "  GSR contact: %s | PPG quality: %.1f%%",
                     sensor_quality.gsr_contact_good ? "OK" : "POOR",
                     sensor_quality.ppg_signal_quality * 100);
            
            ESP_LOGI(LOCAL_TAG, "--- Detection State ---");
            ESP_LOGI(LOCAL_TAG, "  Temporal: %d/%d positive | Duration: %lu ms",
                     temporal_state.positive_count, DETECTION_WINDOW_SIZE,
                     temporal_state.sustained_duration_ms);
            
            if (filtered_result.rejection_reason) {
                ESP_LOGI(LOCAL_TAG, "  Suppression: %s", filtered_result.rejection_reason);
            }
        }
    }
}

// ============ Main Seizure Detection Task ============
extern "C" void seizure_task_main(void *pvParameters)
{
    (void)pvParameters;

    esp_log_level_set(LOCAL_TAG, ESP_LOG_INFO);
    
    ESP_LOGI(LOCAL_TAG, "========================================");
    ESP_LOGI(LOCAL_TAG, "Multi-Modal Seizure Detection System");
    ESP_LOGI(LOCAL_TAG, "  with Comprehensive Filtering");
    ESP_LOGI(LOCAL_TAG, "========================================");
    ESP_LOGI(LOCAL_TAG, "Sensors:");
    ESP_LOGI(LOCAL_TAG, "  1. MPU6050 (deltoid): Motion ML");
    ESP_LOGI(LOCAL_TAG, "  2. MAX30102 (fingertip): HRV");
    ESP_LOGI(LOCAL_TAG, "  3. GSR (fingertips): Stress");
    ESP_LOGI(LOCAL_TAG, "========================================");
    ESP_LOGI(LOCAL_TAG, "Filters Active:");
    ESP_LOGI(LOCAL_TAG, "  - Activity recognition (walking, etc.)");
    ESP_LOGI(LOCAL_TAG, "  - Environmental vibration filter");
    ESP_LOGI(LOCAL_TAG, "  - Temporal consistency (3s min)");
    ESP_LOGI(LOCAL_TAG, "  - Multi-modal confirmation");
    ESP_LOGI(LOCAL_TAG, "  - Sensor quality monitoring");
    ESP_LOGI(LOCAL_TAG, "========================================");

    // Initialize I2C bus (shared by MPU6050 and MAX30102)
    i2c_master_init();
    
    // Initialize sensors
    bool motion_enabled = mpu6050_init();
    
    bool hrv_enabled = hrv_init();
    if (!hrv_enabled) {
        ESP_LOGW(LOCAL_TAG, "MAX30102 not found - HRV disabled");
    } else {
        ESP_LOGI(LOCAL_TAG, "MAX30102 placement: Fingertip");
    }
    
    bool gsr_enabled = gsr_init();
    if (!gsr_enabled) {
        ESP_LOGW(LOCAL_TAG, "GSR sensor not found - GSR disabled");
    } else {
        ESP_LOGI(LOCAL_TAG, "GSR placement: Fingertips (electrodes)");
    }
    
    // Initialize seizure filter
    seizure_filter_init();
    
    // Initialize temporal state
    memset(&temporal_state, 0, sizeof(temporal_state));
    temporal_state.alert_cooldown_ms = 30000;  // 30 second cooldown
    
    // Log sensor status summary
    ESP_LOGI(LOCAL_TAG, "----------------------------------------");
    ESP_LOGI(LOCAL_TAG, "Sensor Status:");
    ESP_LOGI(LOCAL_TAG, "  Motion (MPU6050): %s", motion_enabled ? "ENABLED" : "DISABLED");
    ESP_LOGI(LOCAL_TAG, "  HRV (MAX30102):   %s", hrv_enabled ? "ENABLED" : "DISABLED");
    ESP_LOGI(LOCAL_TAG, "  GSR (Grove):      %s", gsr_enabled ? "ENABLED" : "DISABLED");
    ESP_LOGI(LOCAL_TAG, "----------------------------------------");
    
    if (!motion_enabled && !hrv_enabled && !gsr_enabled) {
        ESP_LOGW(LOCAL_TAG, "NO SENSORS DETECTED! Running in demo mode...");
    }

    // Allocate TensorFlow Lite arena
    tensor_arena = (uint8_t*)heap_caps_malloc(
        kTensorArenaSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!tensor_arena) {
        ESP_LOGE(LOCAL_TAG, "Failed to allocate tensor arena");
        vTaskDelete(NULL);
        return;
    }

    const tflite::Model* model = tflite::GetModel(g_model);

    // Op resolver for SeizeIT2 CNN model
    tflite::MicroMutableOpResolver<13> resolver;
    resolver.AddConv2D();
    resolver.AddDepthwiseConv2D();
    resolver.AddMaxPool2D();
    resolver.AddMean();
    resolver.AddFullyConnected();
    resolver.AddReshape();
    resolver.AddLogistic();
    resolver.AddQuantize();
    resolver.AddDequantize();
    resolver.AddMul();
    resolver.AddAdd();
    resolver.AddRelu();
    resolver.AddExpandDims();

    tflite::MicroInterpreter interpreter(
        model, resolver, tensor_arena, kTensorArenaSize);
    
    if (interpreter.AllocateTensors() != kTfLiteOk) {
        ESP_LOGE(LOCAL_TAG, "Failed to allocate tensors");
        vTaskDelete(NULL);
        return;
    }

    TfLiteTensor* input = interpreter.input(0);
    ESP_LOGI(LOCAL_TAG, "Model ready: input [%d, %d, %d]", 
             input->dims->data[0], input->dims->data[1], input->dims->data[2]);

    bool motion_buffer_filled = false;
    int sample_counter = 0;
    float current_acc[3] = {0};  // Current accelerometer for quality check

    ESP_LOGI(LOCAL_TAG, "========================================");
    ESP_LOGI(LOCAL_TAG, "System ready. Starting detection loop...");
    ESP_LOGI(LOCAL_TAG, "========================================");

    while (true) {
        // ============ Read Motion Sensors ============
        float motion_sample[NUM_CHANNELS];
        float acc_sample[3], gyr_sample[3];
        bool motion_read_ok = mpu6050_read_motion(motion_sample, acc_sample, gyr_sample);

        if (motion_enabled && motion_read_ok) {
            // Store in ML input window
            for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
                motion_window[window_index][ch] = motion_sample[ch];
            }
            
            // Store in separate buffers for motion analysis
            memcpy(acc_buffer[window_index], acc_sample, sizeof(acc_sample));
            memcpy(gyr_buffer[window_index], gyr_sample, sizeof(gyr_sample));
            memcpy(current_acc, acc_sample, sizeof(current_acc));
            
            window_index = (window_index + 1) % NUM_FRAMES;

            if (!motion_buffer_filled && window_index == 0) {
                motion_buffer_filled = true;
                ESP_LOGI(LOCAL_TAG, "Motion buffer filled - detection active");
            }
        }

        // ============ Read HRV (every sample) ============
        static hrv_result_t hrv_result = {};
        if (hrv_enabled) {
            hrv_read_and_analyze(&hrv_result);
        }

        // ============ Read GSR (every 250ms = 5 samples) ============
        static gsr_result_t gsr_result = {};
        if (gsr_enabled && (sample_counter % 5 == 0)) {
            gsr_read_and_analyze(&gsr_result);
        }
        sample_counter++;

        // ============ Run ML Inference (when buffer ready) ============
        int8_t ml_output = -128;  // Default: no seizure
        
        if (motion_enabled && motion_buffer_filled) {
            // Prepare input: normalize, then quantize
            for (int i = 0; i < NUM_FRAMES; ++i) {
                for (int ch = 0; ch < NUM_CHANNELS; ++ch) {
                    float raw_val = motion_window[(window_index + i) % NUM_FRAMES][ch];
                    float normalized = (raw_val - NORM_MEAN) / NORM_STD;
                    int quantized = static_cast<int>(normalized / INPUT_SCALE) + INPUT_ZERO_POINT;
                    if (quantized < -128) quantized = -128;
                    if (quantized > 127)  quantized = 127;
                    input->data.int8[i * NUM_CHANNELS + ch] = static_cast<int8_t>(quantized);
                }
            }

            // Run inference
            if (interpreter.Invoke() == kTfLiteOk) {
                ml_output = interpreter.output(0)->data.int8[0];
            }
        }

        // ============ Calculate Raw Confidences ============
        calculate_raw_confidences(
            ml_output, 
            hrv_enabled ? &hrv_result : nullptr,
            gsr_enabled ? &gsr_result : nullptr
        );

        // ============ Motion Pattern Analysis ============
        if (motion_enabled && motion_buffer_filled) {
            // Analyze motion patterns for activity recognition
            analyze_motion_patterns(
                (const float(*)[3])acc_buffer,
                (const float(*)[3])gyr_buffer,
                NUM_FRAMES,
                &motion_analysis
            );
        }

        // ============ Sensor Quality Check ============
        check_sensor_quality(
            gsr_enabled ? gsr_result.raw_value : 0,
            hrv_enabled ? hrv_result.raw_ir : 0,
            current_acc,
            &sensor_quality
        );

        // ============ Apply Comprehensive Filtering ============
        apply_seizure_filter(
            (float)motion_confidence,
            (float)hrv_confidence,
            (float)gsr_confidence,
            &motion_analysis,
            &temporal_state,
            &sensor_quality,
            &filtered_result
        );

        // ============ Log Status ============
        log_status_periodically(
            hrv_enabled ? &hrv_result : nullptr,
            gsr_enabled ? &gsr_result : nullptr,
            ml_output
        );

        // ============ Trigger BLE Alert if Needed ============
        if (should_trigger_alert(&filtered_result, &temporal_state)) {
            trigger_ble_alert_if_needed(&filtered_result);
        }

        // 50ms delay = 20Hz sampling rate (matches SeizeIT2)
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}
