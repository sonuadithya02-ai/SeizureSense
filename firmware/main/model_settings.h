// Model Settings for SeizeIT2-trained Seizure Detection Model
// 6-channel input: Accelerometer (X,Y,Z) + Gyroscope (X,Y,Z)
// Binary classification: Seizure vs Non-Seizure

#pragma once

// Model input dimensions
constexpr int NUM_FRAMES = 206;      // ~10.3 seconds at 20Hz
constexpr int NUM_CHANNELS = 6;      // ACC_X, ACC_Y, ACC_Z, GYR_X, GYR_Y, GYR_Z
constexpr int NUM_AXES = NUM_CHANNELS;  // Alias for compatibility
constexpr int MODEL_INPUT_LEN = NUM_FRAMES * NUM_CHANNELS;

// Quantization parameters for input (from TFLite model conversion)
constexpr float INPUT_SCALE = 0.031753093004226685f;
constexpr int INPUT_ZERO_POINT = 11;

// Quantization parameters for output
constexpr float OUTPUT_SCALE = 0.00390625f;
constexpr int OUTPUT_ZERO_POINT = -128;

// Normalization parameters (apply BEFORE quantization)
// These are the mean and std of the SeizeIT2 training data
constexpr float NORM_MEAN = -0.048031838015383486f;
constexpr float NORM_STD = 0.19500309866112805f;

// Detection threshold
// Output is int8: -128 to +127
// Probability = (output + 128) * OUTPUT_SCALE
// Threshold of 0 = 50% probability
// We use 0 as the decision boundary (50% confidence)
#define SEIZURE_THRESHOLD 0

// Model performance metrics (from training)
// Sensitivity: 92.4%
// Specificity: 95.7%
// Precision: 87.6%
// Accuracy: 95.0%
