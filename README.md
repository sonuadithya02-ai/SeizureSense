SeizureSense
Edge-Based Seizure Detection Wearable using ESP32-S3 and Quantized TFLite Micro

SeizureSense is a real-time embedded edge-AI system designed for seizure detection using physiological and motion signals.
A quantized TensorFlow Lite Micro model runs directly on the ESP32-S3 microcontroller, enabling low-latency, fully offline inference on resource-constrained hardware.

This project demonstrates embedded firmware architecture, signal preprocessing, TinyML deployment, and real-time event decision logic.

📸 Hardware Prototype

![Hardware Prototype](Assets/hardware_prototype1.jpg)
![Hardware Prototype](Assets/hardware_prototype2.jpg)


🎥 Live Edge Inference Demo

![Live Demo](Assets/demo.gif)

🚀 System Overview

The wearable device continuously monitors:

Galvanic Skin Response (GSR)

Heart Rate Variability (HRV)

Motion patterns (accelerometer)

A lightweight quantized ML model classifies seizure-related activity directly on-device without requiring cloud connectivity.

🧠 Embedded ML Deployment

Model: Quantized INT8 TensorFlow Lite Micro

Target MCU: ESP32-S3

Framework: ESP-IDF

Languages: C / C++

Inference Mode: Fully on-device (Edge AI)

Optimization focus:

Reduced RAM usage

Flash-efficient model storage

Deterministic real-time inference

Memory-aware tensor allocation

📊 Model Performance
Confusion Matrix

![Confusion Matrix](firmware/report_graphs/07_confusion_matrix.png)

ROC Curve

![ROC Curve](firmware/report_graphs/08_roc_curve.png)

The model demonstrates strong classification capability for seizure detection scenarios under evaluated datasets.

📂 Repository Structure
.
├── README.md
├── firmware/
│   ├── main/                  # Embedded application source
│   ├── seizure_model_int8.tflite
│   ├── report_graphs/         # Evaluation visuals
│   └── CMakeLists.txt

🛠 Build Environment

ESP-IDF

ESP32-S3 Toolchain

CMake

Python (model conversion & evaluation only)

To build and flash firmware:

idf.py build
idf.py flash
idf.py monitor
⚙️ Key Features

Real-time physiological signal processing

Adaptive thresholding logic

Quantized TinyML inference

Fully offline operation

Resource-optimized firmware architecture

Deterministic embedded execution pipeline

🎯 Technical Highlights

Edge AI deployment on microcontroller hardware

Embedded signal preprocessing pipeline

TensorFlow Lite Micro integration with ESP-NN acceleration

Optimized memory allocation strategy

Event-driven detection and response logic

📌 Future Improvements

OTA firmware update support

Larger dataset retraining & validation

Further low-power optimization

GSM-based emergency alert integration

Battery-aware power management system

👨‍💻 Author

Molanguru Sonu Adithya
Embedded Systems & Edge AI Engineer