// HRV (Heart Rate Variability) Detection Implementation
// MAX30102 PPG Sensor for ictal tachycardia and HRV reduction detection

#include "hrv_detection.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <math.h>

static const char *TAG = "HRV_Detection";

// I2C port (shared with MPU6050)
#define I2C_MASTER_NUM  I2C_NUM_0

// PPG data buffer for peak detection
#define PPG_BUFFER_SIZE 256
static uint32_t ppg_buffer[PPG_BUFFER_SIZE];
static int ppg_index = 0;
static int ppg_count = 0;

// R-R interval buffer for HRV calculation
#define RR_BUFFER_SIZE 64
static float rr_intervals[RR_BUFFER_SIZE];
static int rr_index = 0;
static int rr_count = 0;

// Peak detection state
static uint32_t last_peak_time = 0;
static uint32_t peak_threshold = 50000;  // Adaptive threshold
static bool sensor_initialized = false;

// Personal baseline tracking
static float personal_baseline_hr = HRV_BASELINE_HR;
static float personal_baseline_rmssd = HRV_BASELINE_RMSSD;
static float hr_sum = 0;
static float rmssd_sum = 0;
static int baseline_sample_count = 0;
static bool baseline_established = false;
static uint32_t init_time = 0;

// Helper: Write to MAX30102 register
static esp_err_t max30102_write_reg(uint8_t reg, uint8_t value)
{
    uint8_t data[2] = {reg, value};
    return i2c_master_write_to_device(I2C_MASTER_NUM, MAX30102_I2C_ADDR,
                                       data, sizeof(data),
                                       100 / portTICK_PERIOD_MS);
}

// Helper: Read from MAX30102 register
static esp_err_t max30102_read_reg(uint8_t reg, uint8_t *value)
{
    return i2c_master_write_read_device(I2C_MASTER_NUM, MAX30102_I2C_ADDR,
                                         &reg, 1, value, 1,
                                         100 / portTICK_PERIOD_MS);
}

// Helper: Read FIFO data
static esp_err_t max30102_read_fifo(uint32_t *ir_value, uint32_t *red_value)
{
    uint8_t fifo_data[6];
    uint8_t reg = MAX30102_REG_FIFO_DATA;
    
    esp_err_t ret = i2c_master_write_read_device(I2C_MASTER_NUM, MAX30102_I2C_ADDR,
                                                  &reg, 1, fifo_data, 6,
                                                  100 / portTICK_PERIOD_MS);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // IR: bytes 0-2, Red: bytes 3-5 (18-bit values)
    *ir_value = ((uint32_t)fifo_data[0] << 16) | ((uint32_t)fifo_data[1] << 8) | fifo_data[2];
    *ir_value &= 0x3FFFF;  // 18-bit mask
    
    *red_value = ((uint32_t)fifo_data[3] << 16) | ((uint32_t)fifo_data[4] << 8) | fifo_data[5];
    *red_value &= 0x3FFFF;
    
    return ESP_OK;
}

bool hrv_init(void)
{
    ESP_LOGI(TAG, "Initializing MAX30102 PPG sensor...");
    
    // Check if sensor is present
    uint8_t part_id = 0;
    esp_err_t ret = max30102_read_reg(MAX30102_REG_PART_ID, &part_id);
    
    if (ret != ESP_OK || part_id != 0x15) {
        ESP_LOGW(TAG, "MAX30102 not found (ID: 0x%02X, expected 0x15)", part_id);
        return false;
    }
    
    // Reset the sensor
    max30102_write_reg(MAX30102_REG_MODE_CONFIG, 0x40);  // Reset
    vTaskDelay(100 / portTICK_PERIOD_MS);
    
    // Clear FIFO pointers FIRST
    max30102_write_reg(MAX30102_REG_FIFO_WR_PTR, 0x00);
    max30102_write_reg(MAX30102_REG_OVF_COUNTER, 0x00);
    max30102_write_reg(MAX30102_REG_FIFO_RD_PTR, 0x00);
    
    // FIFO config: sample averaging = 1 (no averaging), FIFO rollover enabled
    // Bits 7-5: sample avg = 000 (1), Bit 4: rollover enable = 1, Bits 3-0: almost full = 0
    max30102_write_reg(MAX30102_REG_FIFO_CONFIG, 0x10);
    
    // SpO2 config: ADC range 4096, 100 samples/sec, 411us pulse width
    // Bits 6-5: ADC range = 01 (4096), Bits 4-2: sample rate = 001 (100Hz), Bits 1-0: pulse width = 11 (411us)
    max30102_write_reg(MAX30102_REG_SPO2_CONFIG, 0x27);
    
    // LED pulse amplitude (current) - reduced to prevent saturation
    // 0x1F = ~6mA, 0x24 = ~7mA, 0x50 = ~16mA (too bright for many fingers)
    max30102_write_reg(MAX30102_REG_LED1_PA, 0x1F);  // IR LED ~6mA
    max30102_write_reg(MAX30102_REG_LED2_PA, 0x1F);  // Red LED ~6mA
    
    // Disable interrupts (we poll)
    max30102_write_reg(MAX30102_REG_INTR_ENABLE_1, 0x00);
    max30102_write_reg(MAX30102_REG_INTR_ENABLE_2, 0x00);
    
    // Clear any pending interrupts by reading interrupt status
    uint8_t intr_status;
    max30102_read_reg(0x00, &intr_status);  // INT_STATUS_1
    max30102_read_reg(0x01, &intr_status);  // INT_STATUS_2
    
    // Mode config: SpO2 mode - SET THIS LAST to start sampling
    max30102_write_reg(MAX30102_REG_MODE_CONFIG, 0x03);
    
    // Wait for first samples
    vTaskDelay(50 / portTICK_PERIOD_MS);
    
    // Verify mode is set correctly
    uint8_t mode_val = 0;
    max30102_read_reg(MAX30102_REG_MODE_CONFIG, &mode_val);
    ESP_LOGI(TAG, "MAX30102 mode register: 0x%02X (expected 0x03)", mode_val);
    
    sensor_initialized = true;
    init_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    ESP_LOGI(TAG, "MAX30102 initialized successfully");
    ESP_LOGI(TAG, "Baseline HR: %.1f BPM, RMSSD: %.1f ms", HRV_BASELINE_HR, HRV_BASELINE_RMSSD);
    
    return true;
}

static void detect_peak_and_calculate_rr(uint32_t ir_value, uint32_t current_time)
{
    // Store in circular buffer
    ppg_buffer[ppg_index] = ir_value;
    ppg_index = (ppg_index + 1) % PPG_BUFFER_SIZE;
    if (ppg_count < PPG_BUFFER_SIZE) ppg_count++;
    
    // Simple peak detection using threshold crossing
    // Adaptive threshold based on recent signal range
    static uint32_t min_val = UINT32_MAX;
    static uint32_t max_val = 0;
    static bool above_threshold = false;
    static uint32_t peak_value = 0;
    static uint32_t peak_time = 0;
    
    // Update min/max with decay
    if (ir_value < min_val) min_val = ir_value;
    else min_val = min_val + (ir_value - min_val) * 0.001f;
    
    if (ir_value > max_val) max_val = ir_value;
    else max_val = max_val - (max_val - ir_value) * 0.001f;
    
    // Calculate adaptive threshold (50% of range)
    uint32_t range = max_val - min_val;
    if (range > 100) {  // Lower minimum signal amplitude for better sensitivity
        peak_threshold = min_val + (uint32_t)(range * 0.5f);
    }
    
    // Detect upward threshold crossing (potential peak)
    if (!above_threshold && ir_value > peak_threshold) {
        above_threshold = true;
        peak_value = ir_value;
        peak_time = current_time;
    } 
    else if (above_threshold) {
        // Track peak value
        if (ir_value > peak_value) {
            peak_value = ir_value;
            peak_time = current_time;
        }
        
        // Detect downward threshold crossing (peak confirmed)
        if (ir_value < peak_threshold) {
            above_threshold = false;
            
            // Calculate R-R interval
            if (last_peak_time > 0) {
                uint32_t rr_ms = peak_time - last_peak_time;
                
                // Valid RR interval: 300-2000ms (30-200 BPM)
                if (rr_ms >= 300 && rr_ms <= 2000) {
                    rr_intervals[rr_index] = (float)rr_ms;
                    rr_index = (rr_index + 1) % RR_BUFFER_SIZE;
                    if (rr_count < RR_BUFFER_SIZE) rr_count++;
                }
            }
            
            last_peak_time = peak_time;
        }
    }
}

static float calculate_heart_rate(void)
{
    if (rr_count < HRV_MIN_PEAKS_FOR_HR) {
        return 0;
    }
    
    // Average of recent RR intervals
    float sum = 0;
    int count = (rr_count < 10) ? rr_count : 10;  // Use last 10 intervals
    
    for (int i = 0; i < count; i++) {
        int idx = (rr_index - 1 - i + RR_BUFFER_SIZE) % RR_BUFFER_SIZE;
        sum += rr_intervals[idx];
    }
    
    float avg_rr = sum / count;
    return 60000.0f / avg_rr;  // Convert ms to BPM
}

static float calculate_rmssd(void)
{
    if (rr_count < 3) {
        return HRV_BASELINE_RMSSD;  // Not enough data
    }
    
    // RMSSD = sqrt(mean of squared successive differences)
    float sum_sq_diff = 0;
    int count = (rr_count < 20) ? rr_count - 1 : 19;
    
    for (int i = 0; i < count; i++) {
        int idx1 = (rr_index - 1 - i + RR_BUFFER_SIZE) % RR_BUFFER_SIZE;
        int idx2 = (rr_index - 2 - i + RR_BUFFER_SIZE) % RR_BUFFER_SIZE;
        float diff = rr_intervals[idx1] - rr_intervals[idx2];
        sum_sq_diff += diff * diff;
    }
    
    return sqrtf(sum_sq_diff / count);
}

static float calculate_sdnn(void)
{
    if (rr_count < 3) {
        return HRV_BASELINE_SDNN;
    }
    
    // Calculate mean
    float sum = 0;
    int count = (rr_count < 20) ? rr_count : 20;
    
    for (int i = 0; i < count; i++) {
        int idx = (rr_index - 1 - i + RR_BUFFER_SIZE) % RR_BUFFER_SIZE;
        sum += rr_intervals[idx];
    }
    float mean = sum / count;
    
    // Calculate variance
    float sum_sq_diff = 0;
    for (int i = 0; i < count; i++) {
        int idx = (rr_index - 1 - i + RR_BUFFER_SIZE) % RR_BUFFER_SIZE;
        float diff = rr_intervals[idx] - mean;
        sum_sq_diff += diff * diff;
    }
    
    return sqrtf(sum_sq_diff / count);
}

static float calculate_pnn50(void)
{
    if (rr_count < 3) {
        return 0;
    }
    
    int count_above_50 = 0;
    int total = (rr_count < 20) ? rr_count - 1 : 19;
    
    for (int i = 0; i < total; i++) {
        int idx1 = (rr_index - 1 - i + RR_BUFFER_SIZE) % RR_BUFFER_SIZE;
        int idx2 = (rr_index - 2 - i + RR_BUFFER_SIZE) % RR_BUFFER_SIZE;
        float diff = fabsf(rr_intervals[idx1] - rr_intervals[idx2]);
        if (diff > 50.0f) {
            count_above_50++;
        }
    }
    
    return (float)count_above_50 / total * 100.0f;
}

void hrv_read_and_analyze(hrv_result_t *result)
{
    if (result == NULL) {
        return;
    }
    
    // Initialize result
    memset(result, 0, sizeof(hrv_result_t));
    
    if (!sensor_initialized) {
        result->heart_rate = HRV_BASELINE_HR;
        result->rmssd = HRV_BASELINE_RMSSD;
        return;
    }
    
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    uint32_t elapsed = current_time - init_time;
    
    // Read available FIFO samples
    uint8_t wr_ptr = 0, rd_ptr = 0;
    max30102_read_reg(MAX30102_REG_FIFO_WR_PTR, &wr_ptr);
    max30102_read_reg(MAX30102_REG_FIFO_RD_PTR, &rd_ptr);
    
    int samples_available = (wr_ptr >= rd_ptr) ? (wr_ptr - rd_ptr) : (32 - rd_ptr + wr_ptr);
    
    // Debug: Log FIFO status periodically
    static int debug_counter = 0;
    if (++debug_counter >= 100) {  // Every ~5 seconds
        debug_counter = 0;
        ESP_LOGI(TAG, "FIFO: wr=%d rd=%d avail=%d peaks=%d", wr_ptr, rd_ptr, samples_available, rr_count);
    }
    
    // Read all available samples to prevent FIFO overflow
    int samples_to_read = (samples_available > 16) ? 16 : samples_available;
    
    uint32_t ir_value = 0, red_value = 0;
    static uint32_t last_valid_ir = 0;
    int valid_reads = 0;
    
    for (int i = 0; i < samples_to_read; i++) {
        if (max30102_read_fifo(&ir_value, &red_value) == ESP_OK && ir_value > 0) {
            last_valid_ir = ir_value;
            valid_reads++;
            detect_peak_and_calculate_rr(ir_value, current_time);
        }
    }
    
    // Use the last valid IR value for debugging
    result->raw_ir = (ir_value > 0) ? ir_value : last_valid_ir;
    
    // Calculate HRV metrics
    result->heart_rate = calculate_heart_rate();
    result->rmssd = calculate_rmssd();
    result->sdnn = calculate_sdnn();
    result->pnn50 = calculate_pnn50();
    
    // Use dataset baseline if no personal baseline yet
    if (result->heart_rate == 0) {
        result->heart_rate = personal_baseline_hr;
    }
    
    // Calibration phase: establish personal baseline
    if (elapsed < HRV_CALIBRATION_SEC * 1000) {
        result->is_calibrating = true;
        
        if (result->heart_rate > 40 && result->heart_rate < 150) {
            hr_sum += result->heart_rate;
            rmssd_sum += result->rmssd;
            baseline_sample_count++;
        }
        
        result->tachycardia_detected = false;
        result->low_hrv_detected = false;
        result->seizure_hrv_pattern = false;
        return;
    }
    
    // Establish personal baseline after calibration
    if (!baseline_established && baseline_sample_count > 0) {
        personal_baseline_hr = hr_sum / baseline_sample_count;
        personal_baseline_rmssd = rmssd_sum / baseline_sample_count;
        baseline_established = true;
        ESP_LOGI(TAG, "Personal baseline: HR=%.1f BPM, RMSSD=%.1f ms", 
                 personal_baseline_hr, personal_baseline_rmssd);
    }
    
    result->is_calibrating = false;
    
    // Calculate changes from baseline
    result->hr_change_pct = ((result->heart_rate - personal_baseline_hr) / personal_baseline_hr) * 100.0f;
    result->rmssd_change_pct = ((personal_baseline_rmssd - result->rmssd) / personal_baseline_rmssd) * 100.0f;
    
    // Detect ictal tachycardia
    result->tachycardia_detected = (result->heart_rate > HRV_HR_THRESHOLD) ||
                                    (result->hr_change_pct > HRV_HR_INCREASE_PCT);
    
    // Detect low HRV (reduced parasympathetic activity)
    result->low_hrv_detected = (result->rmssd < HRV_RMSSD_THRESHOLD) ||
                                (result->rmssd_change_pct > HRV_RMSSD_DECREASE_PCT);
    
    // Seizure HRV pattern: both tachycardia AND low HRV
    result->seizure_hrv_pattern = result->tachycardia_detected && result->low_hrv_detected;
}

bool hrv_is_calibrated(void)
{
    return baseline_established;
}

float hrv_get_baseline_hr(void)
{
    return personal_baseline_hr;
}

float hrv_get_baseline_rmssd(void)
{
    return personal_baseline_rmssd;
}

