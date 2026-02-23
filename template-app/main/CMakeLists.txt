idf_component_register(
    SRCS
        "main.c"                   # NimBLE_Beacon main (NVS + NimBLE init)
        "gap.c"                    # advertising / GAP logic
        "main_functions_new.cc"    # Multi-modal seizure detection task
        "model_settings.cc"
        "detection_responder.cc"
        "gsr_detection.c"          # GSR sensor for stress/arousal detection
        "hrv_detection.c"          # MAX30102 PPG sensor for HRV detection
        "seizure_filter.c"         # Comprehensive false positive filtering
    INCLUDE_DIRS "."
    REQUIRES
        esp-tflite-micro
        driver
        bt
        nvs_flash
        esp_adc                    # For GSR ADC reading
)
