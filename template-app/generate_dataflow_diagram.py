#!/usr/bin/env python3
"""
Detailed Data Flow Diagram for Seizure Detection Project
Shows internal processing steps with technical specifications
"""

import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from matplotlib.patches import FancyBboxPatch, Rectangle, Polygon
import numpy as np

# Create figure with subplots for different aspects
fig = plt.figure(figsize=(20, 28))

# Color scheme
COLORS = {
    'input': '#3498db',
    'process': '#2ecc71', 
    'ml': '#9b59b6',
    'decision': '#e74c3c',
    'output': '#f39c12',
    'data': '#1abc9c',
    'hw': '#34495e',
    'bg': '#ecf0f1',
}

def draw_box(ax, x, y, w, h, text, color, fontsize=8, text_color='white'):
    box = FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0.02,rounding_size=0.1",
                         facecolor=color, edgecolor='black', linewidth=1.2)
    ax.add_patch(box)
    ax.text(x + w/2, y + h/2, text, ha='center', va='center', fontsize=fontsize,
            color=text_color, fontweight='bold', wrap=True)

def draw_arrow(ax, start, end, text='', color='#7f8c8d'):
    ax.annotate('', xy=end, xytext=start,
                arrowprops=dict(arrowstyle='->', color=color, lw=1.5))
    if text:
        mid = ((start[0]+end[0])/2, (start[1]+end[1])/2)
        ax.text(mid[0], mid[1]+0.15, text, ha='center', fontsize=7, color='gray')

# ============================================================================
# SUBPLOT 1: Motion Detection Pipeline
# ============================================================================
ax1 = fig.add_subplot(311)
ax1.set_xlim(0, 20)
ax1.set_ylim(0, 8)
ax1.axis('off')
ax1.set_title('MOTION DETECTION PIPELINE (ML-Based)', fontsize=14, fontweight='bold', pad=10)

# Background
ax1.add_patch(Rectangle((0, 0), 20, 8, facecolor=COLORS['bg'], alpha=0.3))

# Sensor
draw_box(ax1, 0.2, 3, 2.5, 2, 'MPU6050\n━━━━━\nACC: X,Y,Z\nGYR: X,Y,Z\n±2g, ±250°/s', COLORS['input'])

# I2C Read
draw_box(ax1, 3.2, 3.5, 2, 1, 'I2C Read\n@400kHz', COLORS['process'])
draw_arrow(ax1, (2.7, 4), (3.2, 4), '20Hz')

# Raw Data
draw_box(ax1, 5.7, 3, 2.5, 2, 'Raw Data\n━━━━━\nacc[3]\ngyr[3]\nfloat32', COLORS['data'])
draw_arrow(ax1, (5.2, 4), (5.7, 4))

# Normalization
draw_box(ax1, 8.7, 3, 3, 2, 'Normalization\n━━━━━━━━━\nx\' = (x - μ) / σ\nμ = -0.048\nσ = 0.195', COLORS['process'])
draw_arrow(ax1, (8.2, 4), (8.7, 4))

# Quantization
draw_box(ax1, 12.2, 3, 2.8, 2, 'Quantization\n━━━━━━━━\nINT8 = x\'/scale\n        + zero_pt\nscale=0.0318\nzp=11', COLORS['process'])
draw_arrow(ax1, (11.7, 4), (12.2, 4))

# Buffer
draw_box(ax1, 15.5, 3, 2.3, 2, 'Ring Buffer\n━━━━━━\n206 × 6\n= 1236 INT8\n10.3 sec', COLORS['data'])
draw_arrow(ax1, (15, 4), (15.5, 4))

# TFLite
draw_box(ax1, 18.3, 2.5, 1.5, 3, 'TFLite\nMicro\n━━━\nCNN\nINT8', COLORS['ml'])
draw_arrow(ax1, (17.8, 4), (18.3, 4))

# Output probability
draw_box(ax1, 15.5, 0.5, 4.3, 1.5, 'Output: P(seizure)\nprob = (out + 128) × 0.00391\nThreshold: prob > 0.5', COLORS['decision'])
draw_arrow(ax1, (19, 2.5), (19, 2), '')
draw_arrow(ax1, (19, 2), (19.8, 2), '')

# CNN Architecture detail
ax1.text(1, 7.2, 'CNN Architecture: Input(206×6) → Conv1D(16,k=5) → Pool → Conv1D(32,k=5) → Pool → Conv1D(32,k=3) → GlobalPool → Dense(1,sigmoid)', 
         fontsize=8, color='gray')
ax1.text(1, 6.6, 'Model Size: ~15KB (INT8 quantized) | Inference Time: ~10ms on ESP32-S3 | Memory: ~50KB tensor arena', 
         fontsize=8, color='gray')

# ============================================================================
# SUBPLOT 2: HRV Detection Pipeline  
# ============================================================================
ax2 = fig.add_subplot(312)
ax2.set_xlim(0, 20)
ax2.set_ylim(0, 8)
ax2.axis('off')
ax2.set_title('HRV DETECTION PIPELINE (Heuristic-Based)', fontsize=14, fontweight='bold', pad=10)

ax2.add_patch(Rectangle((0, 0), 20, 8, facecolor=COLORS['bg'], alpha=0.3))

# Sensor
draw_box(ax2, 0.2, 3, 2.5, 2, 'MAX30102\n━━━━━\nIR LED\nRed LED\nPhotodiode', COLORS['input'])

# FIFO Read
draw_box(ax2, 3.2, 3.5, 2, 1, 'FIFO Read\nI2C', COLORS['process'])
draw_arrow(ax2, (2.7, 4), (3.2, 4), '100Hz')

# Raw IR
draw_box(ax2, 5.7, 3, 2.2, 2, 'Raw PPG\n━━━━━\nIR: uint32\nRed: uint32\n18-bit ADC', COLORS['data'])
draw_arrow(ax2, (5.2, 4), (5.7, 4))

# Peak Detection
draw_box(ax2, 8.4, 3, 3, 2, 'Peak Detection\n━━━━━━━━━\nAdaptive threshold\n= min + 0.5×range\nMin amplitude: 100', COLORS['process'])
draw_arrow(ax2, (7.9, 4), (8.4, 4))

# R-R Calculation
draw_box(ax2, 11.9, 3, 2.8, 2, 'R-R Intervals\n━━━━━━━━\nRR[i] = peak[i]\n      - peak[i-1]\nBuffer: 20 RRs', COLORS['data'])
draw_arrow(ax2, (11.4, 4), (11.9, 4))

# HRV Metrics
draw_box(ax2, 15.2, 2.5, 3.2, 3, 'HRV Metrics\n━━━━━━━━━━\nHR = 60000/RR_avg\nRMSSD = √(Σ(ΔRR)²/N)\nSDNN = σ(RR)', COLORS['process'])
draw_arrow(ax2, (14.7, 4), (15.2, 4))

# Decision
draw_box(ax2, 18.9, 3, 1, 2, 'HRV\nAlert\n?', COLORS['decision'])
draw_arrow(ax2, (18.4, 4), (18.9, 4))

# Thresholds
ax2.text(1, 7.2, 'Seizure Indicators: HR increase > 30% from baseline | RMSSD < 50ms | Sudden HR change > 20 BPM in 10s', 
         fontsize=8, color='gray')
ax2.text(1, 6.6, 'Baseline Period: 30 seconds | Personal Baseline: Adaptive (0.95 × old + 0.05 × new) | Normal HR: 60-100 BPM', 
         fontsize=8, color='gray')

# Detection criteria box
draw_box(ax2, 15.2, 0.5, 4.7, 1.5, 'Alert if: (HR > baseline×1.3) OR\n(RMSSD < 50) OR (HR_change > 20)', COLORS['decision'])

# ============================================================================
# SUBPLOT 3: GSR Detection & Fusion Pipeline
# ============================================================================
ax3 = fig.add_subplot(313)
ax3.set_xlim(0, 20)
ax3.set_ylim(0, 8)
ax3.axis('off')
ax3.set_title('GSR DETECTION & MULTI-SENSOR FUSION', fontsize=14, fontweight='bold', pad=10)

ax3.add_patch(Rectangle((0, 0), 20, 8, facecolor=COLORS['bg'], alpha=0.3))

# GSR Sensor
draw_box(ax3, 0.2, 5.5, 2.2, 2, 'Grove GSR\n━━━━━\nADC Input\nGPIO5\n12-bit', COLORS['input'])

# ADC Read
draw_box(ax3, 2.9, 6, 1.8, 1, 'ADC Read\n10Hz', COLORS['process'])
draw_arrow(ax3, (2.4, 6.5), (2.9, 6.5))

# Smoothing
draw_box(ax3, 5.2, 5.5, 2.2, 2, 'Smoothing\n━━━━━\nEMA filter\nα = 0.3', COLORS['process'])
draw_arrow(ax3, (4.7, 6.5), (5.2, 6.5))

# Baseline
draw_box(ax3, 8, 5.5, 2.5, 2, 'Baseline\n━━━━━━\n30s average\nAdaptive\nupdate', COLORS['data'])
draw_arrow(ax3, (7.4, 6.5), (8, 6.5))

# % Deviation
draw_box(ax3, 11, 5.5, 2.8, 2, '% Deviation\n━━━━━━━━\n% = (curr-base)\n     / base × 100', COLORS['process'])
draw_arrow(ax3, (10.5, 6.5), (11, 6.5))

# GSR Decision
draw_box(ax3, 14.5, 5.5, 2, 2, 'GSR\nAlert?\n━━━\n>20%', COLORS['decision'])
draw_arrow(ax3, (13.8, 6.5), (14.5, 6.5))

# FUSION section
ax3.text(10, 4.3, '━━━━━━━━━━  MULTI-SENSOR FUSION  ━━━━━━━━━━', ha='center', fontsize=10, fontweight='bold')

# Three inputs to fusion
draw_box(ax3, 2, 2.5, 2.5, 1.5, 'Motion\nConfidence\n0-100%', COLORS['ml'])
draw_box(ax3, 5.5, 2.5, 2.5, 1.5, 'HRV\nConfidence\n0-100%', COLORS['ml'])
draw_box(ax3, 9, 2.5, 2.5, 1.5, 'GSR\nConfidence\n0-100%', COLORS['ml'])

# Fusion logic
draw_box(ax3, 12.5, 1.8, 4.5, 3, 'DECISION LOGIC\n━━━━━━━━━━━━━━\n1. Motion > 50% → ALERT\n2. 2+ sensors agree → ALERT\n3. Motion > 30% AND\n   (HRV OR GSR) → ALERT', 
         COLORS['decision'], fontsize=7)

# Arrows to fusion
draw_arrow(ax3, (4.5, 3.25), (12.5, 3.25))
draw_arrow(ax3, (8, 3.25), (12.5, 3.25))
draw_arrow(ax3, (11.5, 3.25), (12.5, 3.25))

# From GSR decision
draw_arrow(ax3, (15.5, 5.5), (15.5, 4.8))

# Output
draw_box(ax3, 17.5, 2, 2.3, 2.5, 'BLE\nALERT\n━━━━\n5 sec\nbeacon', COLORS['output'])
draw_arrow(ax3, (17, 3.25), (17.5, 3.25))

# Weights annotation
ax3.text(1, 0.8, 'Sensor Weights: Motion (ML) ~60-70% | HRV ~20-25% | GSR ~10-15%', fontsize=8, color='gray')
ax3.text(1, 0.3, 'System Latency: < 2 seconds from seizure onset to BLE alert | Loop Rate: 50ms (20 iterations/sec)', fontsize=8, color='gray')

plt.tight_layout()
plt.savefig('/Users/sonuadithya/Documents/seizure_cursor/template-app/report_graphs/00b_detailed_dataflow.png',
            dpi=300, bbox_inches='tight', facecolor='white')
plt.close()

print("✅ Detailed Data Flow Diagram saved!")
print("   Location: report_graphs/00b_detailed_dataflow.png")







