#!/usr/bin/env python3
"""
Comprehensive Technical Flowchart for Seizure Detection Project
Generates a detailed system flowchart showing all components and data flow
"""

import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch, Circle, Rectangle
import matplotlib.lines as mlines
import numpy as np

# Create figure
fig, ax = plt.subplots(figsize=(24, 32))
ax.set_xlim(0, 24)
ax.set_ylim(0, 32)
ax.axis('off')
ax.set_aspect('equal')

# Color scheme
COLORS = {
    'sensor': '#3498db',        # Blue
    'process': '#2ecc71',       # Green
    'ml': '#9b59b6',            # Purple
    'decision': '#e74c3c',      # Red
    'output': '#f39c12',        # Orange
    'data': '#1abc9c',          # Teal
    'hardware': '#34495e',      # Dark gray
    'title': '#2c3e50',         # Dark blue
    'arrow': '#7f8c8d',         # Gray
    'highlight': '#e74c3c',     # Red for important
}

def draw_box(ax, x, y, width, height, text, color, text_color='white', fontsize=9, bold=False):
    """Draw a rounded rectangle box with text"""
    box = FancyBboxPatch((x - width/2, y - height/2), width, height,
                         boxstyle="round,pad=0.02,rounding_size=0.15",
                         facecolor=color, edgecolor='black', linewidth=1.5)
    ax.add_patch(box)
    weight = 'bold' if bold else 'normal'
    ax.text(x, y, text, ha='center', va='center', fontsize=fontsize, 
            color=text_color, fontweight=weight, wrap=True)
    return box

def draw_diamond(ax, x, y, size, text, color):
    """Draw a diamond shape for decisions"""
    diamond = plt.Polygon([(x, y + size), (x + size, y), (x, y - size), (x - size, y)],
                          facecolor=color, edgecolor='black', linewidth=1.5)
    ax.add_patch(diamond)
    ax.text(x, y, text, ha='center', va='center', fontsize=8, color='white', fontweight='bold')
    return diamond

def draw_arrow(ax, start, end, color='#7f8c8d', style='->', linewidth=1.5):
    """Draw an arrow between two points"""
    ax.annotate('', xy=end, xytext=start,
                arrowprops=dict(arrowstyle=style, color=color, lw=linewidth))

def draw_cylinder(ax, x, y, width, height, text, color):
    """Draw a cylinder shape for data storage"""
    # Draw cylinder body
    ellipse_height = height * 0.15
    rect = Rectangle((x - width/2, y - height/2 + ellipse_height/2), width, height - ellipse_height,
                     facecolor=color, edgecolor='black', linewidth=1.5)
    ax.add_patch(rect)
    
    # Top ellipse
    from matplotlib.patches import Ellipse
    top_ellipse = Ellipse((x, y + height/2 - ellipse_height/2), width, ellipse_height,
                          facecolor=color, edgecolor='black', linewidth=1.5)
    ax.add_patch(top_ellipse)
    
    # Bottom ellipse (partial)
    bottom_ellipse = Ellipse((x, y - height/2 + ellipse_height/2), width, ellipse_height,
                             facecolor=color, edgecolor='black', linewidth=1.5)
    ax.add_patch(bottom_ellipse)
    
    ax.text(x, y, text, ha='center', va='center', fontsize=8, color='white', fontweight='bold')

# ============================================================================
# TITLE
# ============================================================================
ax.text(12, 31.5, 'MULTI-SENSOR SEIZURE DETECTION SYSTEM', 
        ha='center', va='center', fontsize=18, fontweight='bold', color=COLORS['title'])
ax.text(12, 31, 'Complete Technical Flowchart', 
        ha='center', va='center', fontsize=12, color=COLORS['title'])

# ============================================================================
# SECTION 1: HARDWARE LAYER (Top)
# ============================================================================
ax.text(12, 30.2, '━━━━━━━━━━━━━━━━━━━━━━  HARDWARE LAYER  ━━━━━━━━━━━━━━━━━━━━━━', 
        ha='center', va='center', fontsize=10, color=COLORS['hardware'])

# ESP32-S3 Main Controller
draw_box(ax, 12, 29, 6, 1.2, 'ESP32-S3 N16R8\n(Dual-Core 240MHz, 16MB Flash, 8MB PSRAM)', 
         COLORS['hardware'], fontsize=9, bold=True)

# Sensors
draw_box(ax, 4, 27, 4, 1.5, 'MPU6050\n━━━━━━━━━\nAccelerometer (±2g)\nGyroscope (±250°/s)\nI2C @ 0x68', 
         COLORS['sensor'], fontsize=8)
draw_box(ax, 12, 27, 4, 1.5, 'MAX30102\n━━━━━━━━━\nPPG Sensor\nIR + Red LEDs\nI2C @ 0x57', 
         COLORS['sensor'], fontsize=8)
draw_box(ax, 20, 27, 4, 1.5, 'Grove GSR\n━━━━━━━━━\nGalvanic Skin Response\nAnalog Output\nADC1_CH4 (GPIO5)', 
         COLORS['sensor'], fontsize=8)

# Arrows from ESP32 to sensors
draw_arrow(ax, (12, 28.4), (12, 27.8))
draw_arrow(ax, (10, 28.4), (6, 27.8))
draw_arrow(ax, (14, 28.4), (18, 27.8))

# ============================================================================
# SECTION 2: DATA ACQUISITION LAYER
# ============================================================================
ax.text(12, 25.7, '━━━━━━━━━━━━━━━━━━━━  DATA ACQUISITION LAYER  ━━━━━━━━━━━━━━━━━━━━', 
        ha='center', va='center', fontsize=10, color=COLORS['process'])

# Data acquisition boxes
draw_box(ax, 4, 24.5, 3.5, 1.2, 'Read Motion Data\n━━━━━━━━━\nmpu6050_read_motion()\n20Hz sampling', 
         COLORS['process'], fontsize=8)
draw_box(ax, 12, 24.5, 3.5, 1.2, 'Read PPG Data\n━━━━━━━━━\nhrv_read_and_analyze()\n100Hz → FIFO', 
         COLORS['process'], fontsize=8)
draw_box(ax, 20, 24.5, 3.5, 1.2, 'Read GSR Data\n━━━━━━━━━\ngsr_read_and_analyze()\nADC @ 10Hz', 
         COLORS['process'], fontsize=8)

# Arrows
draw_arrow(ax, (4, 26.2), (4, 25.1))
draw_arrow(ax, (12, 26.2), (12, 25.1))
draw_arrow(ax, (20, 26.2), (20, 25.1))

# ============================================================================
# SECTION 3: PREPROCESSING LAYER
# ============================================================================
ax.text(12, 23.2, '━━━━━━━━━━━━━━━━━━━━━  PREPROCESSING LAYER  ━━━━━━━━━━━━━━━━━━━━━', 
        ha='center', va='center', fontsize=10, color=COLORS['data'])

# Motion preprocessing
draw_box(ax, 4, 21.8, 4.5, 2, 
         'Motion Preprocessing\n━━━━━━━━━━━━━━━\n1. Read ACC_X, ACC_Y, ACC_Z\n2. Read GYR_X, GYR_Y, GYR_Z\n3. Normalize: (x - μ) / σ\n   μ = -0.048, σ = 0.195\n4. Quantize to INT8', 
         COLORS['data'], fontsize=7)

# HRV preprocessing
draw_box(ax, 12, 21.8, 4.5, 2, 
         'HRV Preprocessing\n━━━━━━━━━━━━━━━\n1. Read IR values from FIFO\n2. Adaptive peak detection\n3. Calculate R-R intervals\n4. Compute HR (60/RR_avg)\n5. Calculate RMSSD, SDNN', 
         COLORS['data'], fontsize=7)

# GSR preprocessing
draw_box(ax, 20, 21.8, 4.5, 2, 
         'GSR Preprocessing\n━━━━━━━━━━━━━━━\n1. Read ADC value (12-bit)\n2. Apply smoothing filter\n3. Baseline calibration\n   (30s moving average)\n4. Calculate % deviation', 
         COLORS['data'], fontsize=7)

# Arrows
draw_arrow(ax, (4, 23.9), (4, 22.8))
draw_arrow(ax, (12, 23.9), (12, 22.8))
draw_arrow(ax, (20, 23.9), (20, 22.8))

# ============================================================================
# SECTION 4: BUFFERING & WINDOWING
# ============================================================================
# Motion buffer
draw_box(ax, 4, 19.5, 4, 1.2, 'Circular Buffer\n━━━━━━━━━\n206 frames × 6 axes\n10.3 sec window', 
         COLORS['process'], fontsize=8)

# HRV buffer
draw_box(ax, 12, 19.5, 4, 1.2, 'R-R Interval Buffer\n━━━━━━━━━\n20 intervals\n~20-30 sec window', 
         COLORS['process'], fontsize=8)

# GSR buffer
draw_box(ax, 20, 19.5, 4, 1.2, 'GSR Sample Buffer\n━━━━━━━━━\n100 samples\n10 sec baseline', 
         COLORS['process'], fontsize=8)

# Arrows
draw_arrow(ax, (4, 20.8), (4, 20.1))
draw_arrow(ax, (12, 20.8), (12, 20.1))
draw_arrow(ax, (20, 20.8), (20, 20.1))

# ============================================================================
# SECTION 5: DETECTION ALGORITHMS
# ============================================================================
ax.text(12, 18.2, '━━━━━━━━━━━━━━━━━━━━  DETECTION ALGORITHMS  ━━━━━━━━━━━━━━━━━━━━', 
        ha='center', va='center', fontsize=10, color=COLORS['ml'])

# ML Detection
draw_box(ax, 4, 16.5, 5, 2.5, 
         'TensorFlow Lite Micro\nML INFERENCE\n━━━━━━━━━━━━━━━━━\n\nModel: 1D CNN (INT8)\nInput: [1, 206, 6]\nLayers: Conv1D(16,32,32)\nOutput: Sigmoid probability\n\nThreshold: > 0 (50%)', 
         COLORS['ml'], fontsize=7, bold=True)

# HRV Heuristic
draw_box(ax, 12, 16.5, 5, 2.5, 
         'HRV HEURISTICS\n━━━━━━━━━━━━━━━━━\n\nBaseline: 30s calibration\n\nIctal Indicators:\n• HR increase > 30%\n• RMSSD < 50ms\n• Sudden HR change > 20 BPM\n\nPersonal baseline tracking', 
         COLORS['ml'], fontsize=7, bold=True)

# GSR Heuristic
draw_box(ax, 20, 16.5, 5, 2.5, 
         'GSR HEURISTICS\n━━━━━━━━━━━━━━━━━\n\nBaseline: 30s calibration\n\nStress Indicators:\n• Conductance > 20% above\n• Rapid rise (> 10%/sec)\n• Sustained elevation\n\nSympathetic activation', 
         COLORS['ml'], fontsize=7, bold=True)

# Arrows
draw_arrow(ax, (4, 18.9), (4, 17.8))
draw_arrow(ax, (12, 18.9), (12, 17.8))
draw_arrow(ax, (20, 18.9), (20, 17.8))

# ============================================================================
# SECTION 6: CONFIDENCE CALCULATION
# ============================================================================
draw_box(ax, 4, 13.8, 4, 1.2, 'Motion Confidence\n━━━━━━━━━\nmotion_conf = prob × 100%\nRange: 0-100%', 
         COLORS['process'], fontsize=8)

draw_box(ax, 12, 13.8, 4, 1.2, 'HRV Confidence\n━━━━━━━━━\nhrv_conf = HR_increase × 2\nCapped at 100%', 
         COLORS['process'], fontsize=8)

draw_box(ax, 20, 13.8, 4, 1.2, 'GSR Confidence\n━━━━━━━━━\ngsr_conf = %_above × 2\nCapped at 100%', 
         COLORS['process'], fontsize=8)

# Arrows
draw_arrow(ax, (4, 15.2), (4, 14.4))
draw_arrow(ax, (12, 15.2), (12, 14.4))
draw_arrow(ax, (20, 15.2), (20, 14.4))

# ============================================================================
# SECTION 7: MULTI-SENSOR FUSION
# ============================================================================
ax.text(12, 12.7, '━━━━━━━━━━━━━━━━━━━━  MULTI-SENSOR FUSION  ━━━━━━━━━━━━━━━━━━━━', 
        ha='center', va='center', fontsize=10, color=COLORS['decision'])

# Fusion box
draw_box(ax, 12, 11.2, 8, 2, 
         'evaluate_seizure_detection()\n━━━━━━━━━━━━━━━━━━━━━━━━━━\nInputs: motion_conf, hrv_conf, gsr_conf\n        seizure_detected_ml, hrv_result, gsr_result', 
         COLORS['decision'], fontsize=8, bold=True)

# Arrows converging
draw_arrow(ax, (4, 13.2), (8, 11.8))
draw_arrow(ax, (12, 13.2), (12, 12.2))
draw_arrow(ax, (20, 13.2), (16, 11.8))

# ============================================================================
# SECTION 8: DECISION LOGIC
# ============================================================================
# Decision diamonds
draw_diamond(ax, 6, 9, 0.8, 'Motion\n>50%?', COLORS['decision'])
draw_diamond(ax, 12, 9, 0.8, '2+\nAgree?', COLORS['decision'])
draw_diamond(ax, 18, 9, 0.8, 'Motion>30%\n+ Support?', COLORS['decision'])

# Arrow from fusion to decisions
draw_arrow(ax, (12, 10.2), (12, 9.8))
draw_arrow(ax, (9, 10.5), (6, 9.8))
draw_arrow(ax, (15, 10.5), (18, 9.8))

# Decision outcomes
ax.text(6, 8, 'YES', fontsize=8, ha='center', color='green', fontweight='bold')
ax.text(12, 8, 'YES', fontsize=8, ha='center', color='green', fontweight='bold')
ax.text(18, 8, 'YES', fontsize=8, ha='center', color='green', fontweight='bold')

# ============================================================================
# SECTION 9: ALERT TRIGGER
# ============================================================================
ax.text(12, 7.2, '━━━━━━━━━━━━━━━━━━━━━━  ALERT SYSTEM  ━━━━━━━━━━━━━━━━━━━━━━', 
        ha='center', va='center', fontsize=10, color=COLORS['output'])

# Alert decision
draw_diamond(ax, 12, 6, 1, 'SEIZURE\nDETECTED?', COLORS['highlight'])

# Arrows to alert
draw_arrow(ax, (6, 8.2), (10.5, 6.5), color='green')
draw_arrow(ax, (12, 8.2), (12, 7), color='green')
draw_arrow(ax, (18, 8.2), (13.5, 6.5), color='green')

# YES path
draw_box(ax, 6, 4.5, 4.5, 1.5, 
         'TRIGGER ALERT\n━━━━━━━━━━━━━━━\ntrigger_ble_alert_if_needed(true)\nStart BLE advertising', 
         COLORS['highlight'], fontsize=8, bold=True)

# NO path
draw_box(ax, 18, 4.5, 4.5, 1.5, 
         'NORMAL STATE\n━━━━━━━━━━━━━━━\ntrigger_ble_alert_if_needed(false)\nContinue monitoring', 
         COLORS['process'], fontsize=8)

# Arrows from decision
draw_arrow(ax, (11, 5.5), (8.3, 5))
draw_arrow(ax, (13, 5.5), (15.7, 5))
ax.text(9, 5.5, 'YES', fontsize=9, color='red', fontweight='bold')
ax.text(15, 5.5, 'NO', fontsize=9, color='green', fontweight='bold')

# ============================================================================
# SECTION 10: BLE ADVERTISING
# ============================================================================
draw_box(ax, 6, 2.5, 5, 1.5, 
         'BLE BEACON\n━━━━━━━━━━━━━━━\nDevice Name: "SEIZURE_ALERT"\nAdvertising: 5 seconds\nNimBLE Stack', 
         COLORS['output'], fontsize=8, bold=True)

# Arrow
draw_arrow(ax, (6, 3.7), (6, 3.3))

# Mobile app
draw_box(ax, 6, 0.8, 4, 1, 
         'Caregiver Notification\nMobile App / Smart Watch', 
         COLORS['sensor'], fontsize=9)

draw_arrow(ax, (6, 1.7), (6, 1.3))

# Loop back arrow
ax.annotate('', xy=(18, 3.7), xytext=(18, 2),
            arrowprops=dict(arrowstyle='->', color=COLORS['arrow'], lw=1.5,
                           connectionstyle='arc3,rad=0'))
ax.annotate('', xy=(21, 26), xytext=(21, 3.7),
            arrowprops=dict(arrowstyle='->', color=COLORS['arrow'], lw=1.5,
                           connectionstyle='arc3,rad=0.1'))
ax.text(22, 15, 'LOOP\n(50ms)', fontsize=8, ha='center', color=COLORS['arrow'], rotation=90)

# ============================================================================
# LEGEND
# ============================================================================
ax.text(1, 1.5, 'LEGEND:', fontsize=10, fontweight='bold', color=COLORS['title'])

legend_items = [
    (COLORS['sensor'], 'Sensor/Input'),
    (COLORS['process'], 'Processing'),
    (COLORS['data'], 'Data Transform'),
    (COLORS['ml'], 'ML/Algorithm'),
    (COLORS['decision'], 'Decision'),
    (COLORS['output'], 'Output/Alert'),
]

for i, (color, label) in enumerate(legend_items):
    rect = FancyBboxPatch((1 + i*3.5, 0.3), 0.5, 0.5,
                          boxstyle="round,pad=0.02", facecolor=color, edgecolor='black')
    ax.add_patch(rect)
    ax.text(1.8 + i*3.5, 0.55, label, fontsize=8, va='center')

# ============================================================================
# TIMING ANNOTATIONS
# ============================================================================
ax.text(23, 27, 'I2C\n400kHz', fontsize=7, ha='center', color='gray')
ax.text(23, 24.5, '20Hz', fontsize=7, ha='center', color='gray')
ax.text(23, 21.8, 'Real-time', fontsize=7, ha='center', color='gray')
ax.text(23, 16.5, '~50ms\nlatency', fontsize=7, ha='center', color='gray')
ax.text(23, 11, 'Weighted\nFusion', fontsize=7, ha='center', color='gray')

# Save
plt.tight_layout()
plt.savefig('/Users/sonuadithya/Documents/seizure_cursor/template-app/report_graphs/00_complete_system_flowchart.png', 
            dpi=300, bbox_inches='tight', facecolor='white')
plt.close()

print("✅ Complete System Flowchart saved!")
print("   Location: report_graphs/00_complete_system_flowchart.png")

