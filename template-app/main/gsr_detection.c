// GSR (Galvanic Skin Response) Detection Implementation
// Seeed Grove GSR Sensor on ADC1 Channel 4 (GPIO5)

#include "gsr_detection.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <math.h>

static const char *TAG = "GSR_Detection";

// ADC handles
static adc_oneshot_unit_handle_t adc_handle = NULL;
static adc_cali_handle_t cali_handle = NULL;
static bool calibration_enabled = false;

// Baseline tracking
static float baseline_sum = 0;
static int baseline_count = 0;
static float baseline_value = 0;
static bool baseline_established = false;

// SCR peak detection
#define SCR_BUFFER_SIZE 60  // 1 minute of samples at 4Hz = 240, but we track recent peaks
static float recent_values[16];
static int recent_index = 0;
static int scr_peak_count = 0;
static uint32_t peak_timestamps[10];
static int peak_index = 0;

// Timing
static uint32_t calibration_start_time = 0;

bool gsr_init(void)
{
    ESP_LOGI(TAG, "Initializing GSR sensor on ADC1 Channel 4");
    
    // Configure ADC unit
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = GSR_ADC_UNIT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    
    esp_err_t ret = adc_oneshot_new_unit(&unit_cfg, &adc_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ADC unit: %s", esp_err_to_name(ret));
        return false;
    }
    
    // Configure ADC channel
    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = GSR_ADC_ATTEN,
        .bitwidth = GSR_ADC_WIDTH,
    };
    
    ret = adc_oneshot_config_channel(adc_handle, GSR_ADC_CHANNEL, &chan_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure ADC channel: %s", esp_err_to_name(ret));
        return false;
    }
    
    // Try to setup calibration
    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = GSR_ADC_UNIT,
        .chan = GSR_ADC_CHANNEL,
        .atten = GSR_ADC_ATTEN,
        .bitwidth = GSR_ADC_WIDTH,
    };
    
    ret = adc_cali_create_scheme_curve_fitting(&cali_cfg, &cali_handle);
    if (ret == ESP_OK) {
        calibration_enabled = true;
        ESP_LOGI(TAG, "ADC calibration enabled");
    } else {
        ESP_LOGW(TAG, "ADC calibration not available, using raw values");
    }
    
    // Initialize buffers
    memset(recent_values, 0, sizeof(recent_values));
    memset(peak_timestamps, 0, sizeof(peak_timestamps));
    
    calibration_start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    ESP_LOGI(TAG, "GSR sensor initialized. Baseline will be established in %d seconds.", GSR_CALIBRATION_SEC);
    return true;
}

static float read_gsr_raw(void)
{
    int raw_value = 0;
    esp_err_t ret = adc_oneshot_read(adc_handle, GSR_ADC_CHANNEL, &raw_value);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read ADC: %s", esp_err_to_name(ret));
        return 0;
    }
    
    // Apply calibration if available
    if (calibration_enabled && cali_handle != NULL) {
        int voltage_mv = 0;
        adc_cali_raw_to_voltage(cali_handle, raw_value, &voltage_mv);
        // Convert back to normalized 0-4095 scale based on voltage
        return (voltage_mv / 3300.0f) * 4095.0f;
    }
    
    return (float)raw_value;
}

static void detect_scr_peak(float current_value, uint32_t current_time)
{
    // Store recent value
    recent_values[recent_index] = current_value;
    recent_index = (recent_index + 1) % 16;
    
    // Need at least a few samples to detect peaks
    static float prev_value = 0;
    static float prev_prev_value = 0;
    static bool rising = false;
    
    // Detect peak: value was rising and now falling
    float diff = current_value - prev_value;
    float prev_diff = prev_value - prev_prev_value;
    
    if (prev_diff > GSR_PEAK_MIN_AMPLITUDE && diff < -GSR_PEAK_MIN_AMPLITUDE / 2) {
        // This is a peak - record timestamp
        peak_timestamps[peak_index] = current_time;
        peak_index = (peak_index + 1) % 10;
    }
    
    prev_prev_value = prev_value;
    prev_value = current_value;
}

static int count_recent_peaks(uint32_t current_time)
{
    // Count peaks in the last 60 seconds
    int count = 0;
    uint32_t window_start = current_time > 60000 ? current_time - 60000 : 0;
    
    for (int i = 0; i < 10; i++) {
        if (peak_timestamps[i] > window_start && peak_timestamps[i] <= current_time) {
            count++;
        }
    }
    
    return count;
}

void gsr_read_and_analyze(gsr_result_t *result)
{
    if (result == NULL || adc_handle == NULL) {
        return;
    }
    
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    uint32_t elapsed_ms = current_time - calibration_start_time;
    
    // Read raw GSR value
    float raw_value = read_gsr_raw();
    result->raw_value = raw_value;
    
    // Convert to approximate microsiemens (rough approximation)
    // Grove GSR: Conductance = 1 / Resistance, voltage proportional to conductance
    result->normalized_value = raw_value / 4095.0f * 10.0f; // 0-10 μS range
    
    // Calibration phase
    if (elapsed_ms < GSR_CALIBRATION_SEC * 1000) {
        baseline_sum += raw_value;
        baseline_count++;
        result->is_calibrating = true;
        result->baseline = 0;
        result->percent_above_baseline = 0;
        result->scr_peaks_per_minute = 0;
        result->stress_detected = false;
        return;
    }
    
    // Establish baseline after calibration period
    if (!baseline_established && baseline_count > 0) {
        baseline_value = baseline_sum / baseline_count;
        baseline_established = true;
        ESP_LOGI(TAG, "GSR baseline established: %.1f (ADC units)", baseline_value);
    }
    
    result->is_calibrating = false;
    result->baseline = baseline_value;
    
    // Calculate percentage above baseline
    if (baseline_value > 0) {
        result->percent_above_baseline = ((raw_value - baseline_value) / baseline_value) * 100.0f;
    } else {
        result->percent_above_baseline = 0;
    }
    
    // Detect SCR peaks
    detect_scr_peak(raw_value, current_time);
    result->scr_peaks_per_minute = count_recent_peaks(current_time);
    
    // Determine stress detection
    // Stress = significantly elevated GSR OR frequent SCR peaks
    bool elevated_gsr = (result->percent_above_baseline > GSR_STRESS_THRESHOLD_PCT);
    bool frequent_peaks = (result->scr_peaks_per_minute >= GSR_SCR_PEAK_THRESHOLD);
    
    result->stress_detected = elevated_gsr || frequent_peaks;
    
    // Update baseline slowly (adaptive baseline)
    // Only update if not in stress state (to avoid raising baseline during events)
    if (!result->stress_detected && baseline_established) {
        // Slow adaptation: 0.1% weight to new values
        baseline_value = baseline_value * 0.999f + raw_value * 0.001f;
    }
}

bool gsr_is_calibrated(void)
{
    return baseline_established;
}

float gsr_get_baseline(void)
{
    return baseline_value;
}

