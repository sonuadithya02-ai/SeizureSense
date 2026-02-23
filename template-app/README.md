# SeizureSense  
### Edge-Based Multisensor Seizure Detection Wearable (ESP32-S3 + Quantized TFLite)

SeizureSense is an embedded edge-AI system designed to detect seizure events in real-time using multisensor fusion and an optimized INT8 TensorFlow Lite model deployed on ESP32-S3.

The system performs fully on-device inference without cloud dependency and is designed for low-power wearable deployment.

---

## 🚀 Key Features

- 🧠 Edge AI inference using quantized INT8 TensorFlow Lite model  
- 📡 Multisensor fusion (GSR + HRV + motion data)  
- ⚡ Real-time signal preprocessing and filtering  
- 📉 Memory-optimized deployment for ESP32-S3  
- 📊 Evaluated using confusion matrix and ROC metrics  
- 🔌 Fully offline inference (no internet required)

---

## 🏗 System Architecture

![System Flowchart](report_graphs/00_complete_system_flowchart.png)

### System Pipeline

1. Sensor acquisition (GSR, HRV, motion)
2. Signal preprocessing and filtering
3. Feature extraction
4. Quantized ML inference (INT8 TFLite)
5. Decision logic
6. Alert triggering mechanism

---

## 🧠 Embedded ML Deployment

- Model converted to TensorFlow Lite
- Quantized to INT8 for memory efficiency
- Deployed using ESP-IDF
- Integrated using C/C++ inference pipeline
- Optimized for real-time execution constraints

Deployed model file:
template-app/seizure_model_int8.tflite


---

## 📊 Model Performance

### Confusion Matrix
![Confusion Matrix](report_graphs/07_confusion_matrix.png)

### ROC Curve
![ROC Curve](report_graphs/08_roc_curve.png)

The model demonstrates strong classification performance for seizure detection scenarios.

---

## 📂 Repository Structure
template-app/
│
├── main/ # ESP-IDF firmware source
│ ├── main.c
│ ├── gsr_detection.c
│ ├── hrv_detection.c
│ ├── seizure_filter.c
│ ├── detection_responder.cc
│ └── model_settings.*
│
├── report_graphs/ # Evaluation visuals
│
├── seizure_model_int8.tflite # Quantized deployment model
│
└── CMakeLists.txt


---

## ⚙️ Build Environment

- ESP-IDF
- C / C++
- TensorFlow Lite (quantized model deployment)
- CMake-based build system

---

## 🔒 Notes

- Training dataset and preprocessing pipelines are intentionally not included.
- Detailed training configurations and hyperparameters are excluded.
- This repository focuses on embedded deployment and system integration.

---

## 🎯 Project Objective

To demonstrate a complete embedded edge-AI pipeline:
- Sensor-level acquisition
- Real-time signal processing
- Quantized ML inference
- Deployment on constrained hardware

This project reflects practical embedded ML engineering for wearable health monitoring applications.

---

## 👨‍💻 Author

**Sonu Adithya**  
Embedded Systems & Edge AI Developer