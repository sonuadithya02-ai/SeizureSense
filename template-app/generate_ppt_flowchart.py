#!/usr/bin/env python3
"""
PPT-Optimized Flowchart for Seizure Detection Project
Landscape format (16:9) with large readable text
"""

import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch, Rectangle, Polygon
import numpy as np

# Create figure - 16:9 aspect ratio for PPT
fig, ax = plt.subplots(figsize=(16, 9))
ax.set_xlim(0, 16)
ax.set_ylim(0, 9)
ax.axis('off')

# Color scheme - vibrant for presentations
COLORS = {
    'sensor': '#3498db',      # Blue
    'process': '#27ae60',     # Green  
    'ml': '#8e44ad',          # Purple
    'decision': '#c0392b',    # Red
    'output': '#d35400',      # Orange
    'fusion': '#16a085',      # Teal
    'arrow': '#2c3e50',       # Dark
    'bg_light': '#ecf0f1',    # Light gray
}

def draw_box(ax, x, y, w, h, text, color, fontsize=10, text_color='white', alpha=1.0):
    box = FancyBboxPatch((x - w/2, y - h/2), w, h,
                         boxstyle="round,pad=0.03,rounding_size=0.2",
                         facecolor=color, edgecolor='black', linewidth=2, alpha=alpha)
    ax.add_patch(box)
    ax.text(x, y, text, ha='center', va='center', fontsize=fontsize,
            color=text_color, fontweight='bold', linespacing=1.2)

def draw_arrow(ax, start, end, color='#2c3e50', width=2):
    ax.annotate('', xy=end, xytext=start,
                arrowprops=dict(arrowstyle='-|>', color=color, lw=width,
                               mutation_scale=15))

def draw_diamond(ax, x, y, size, text, color):
    diamond = Polygon([(x, y + size), (x + size*0.8, y), (x, y - size), (x - size*0.8, y)],
                      facecolor=color, edgecolor='black', linewidth=2)
    ax.add_patch(diamond)
    ax.text(x, y, text, ha='center', va='center', fontsize=9, color='white', fontweight='bold')

# ============================================================================
# TITLE
# ============================================================================
ax.text(8, 8.5, 'MULTI-SENSOR SEIZURE DETECTION SYSTEM', 
        ha='center', fontsize=18, fontweight='bold', color='#2c3e50')

# ============================================================================
# ROW 1: SENSORS (Top)
# ============================================================================
# Sensor boxes
draw_box(ax, 2.5, 7, 3.5, 1.2, 'MPU6050\nAccelerometer + Gyroscope', COLORS['sensor'], fontsize=11)
draw_box(ax, 8, 7, 3.5, 1.2, 'MAX30102\nPPG / Heart Rate', COLORS['sensor'], fontsize=11)
draw_box(ax, 13.5, 7, 3.5, 1.2, 'Grove GSR\nSkin Conductance', COLORS['sensor'], fontsize=11)

# Sampling rates
ax.text(2.5, 6.2, '20 Hz', ha='center', fontsize=9, color='gray', style='italic')
ax.text(8, 6.2, '100 Hz', ha='center', fontsize=9, color='gray', style='italic')
ax.text(13.5, 6.2, '10 Hz', ha='center', fontsize=9, color='gray', style='italic')

# ============================================================================
# ROW 2: PROCESSING
# ============================================================================
draw_box(ax, 2.5, 5.2, 3.5, 1, 'Preprocessing\nNormalize + Quantize', COLORS['process'], fontsize=10)
draw_box(ax, 8, 5.2, 3.5, 1, 'Peak Detection\nR-R Intervals', COLORS['process'], fontsize=10)
draw_box(ax, 13.5, 5.2, 3.5, 1, 'Baseline Tracking\n% Deviation', COLORS['process'], fontsize=10)

# Arrows from sensors to processing
draw_arrow(ax, (2.5, 6.4), (2.5, 5.7))
draw_arrow(ax, (8, 6.4), (8, 5.7))
draw_arrow(ax, (13.5, 6.4), (13.5, 5.7))

# ============================================================================
# ROW 3: DETECTION ALGORITHMS
# ============================================================================
draw_box(ax, 2.5, 3.8, 3.5, 1.2, 'TensorFlow Lite\n1D CNN (INT8)\nSeizure Classifier', COLORS['ml'], fontsize=10)
draw_box(ax, 8, 3.8, 3.5, 1.2, 'HRV Heuristics\nHR + RMSSD\nThreshold Check', COLORS['ml'], fontsize=10)
draw_box(ax, 13.5, 3.8, 3.5, 1.2, 'GSR Heuristics\nStress Detection\n>20% Increase', COLORS['ml'], fontsize=10)

# Arrows
draw_arrow(ax, (2.5, 4.7), (2.5, 4.4))
draw_arrow(ax, (8, 4.7), (8, 4.4))
draw_arrow(ax, (13.5, 4.7), (13.5, 4.4))

# Labels under detection
ax.text(2.5, 3.05, 'Motion: 60-70%', ha='center', fontsize=9, color=COLORS['ml'], fontweight='bold')
ax.text(8, 3.05, 'HRV: 20-25%', ha='center', fontsize=9, color=COLORS['ml'], fontweight='bold')
ax.text(13.5, 3.05, 'GSR: 10-15%', ha='center', fontsize=9, color=COLORS['ml'], fontweight='bold')

# ============================================================================
# ROW 4: FUSION
# ============================================================================
# Fusion box (wide)
draw_box(ax, 8, 2.2, 10, 0.9, 'MULTI-SENSOR FUSION - Weighted Decision Algorithm', COLORS['fusion'], fontsize=12)

# Arrows converging to fusion
draw_arrow(ax, (2.5, 3.2), (4, 2.5))
draw_arrow(ax, (8, 3.2), (8, 2.7))
draw_arrow(ax, (13.5, 3.2), (12, 2.5))

# ============================================================================
# ROW 5: DECISION LOGIC
# ============================================================================
# Decision diamond
draw_diamond(ax, 8, 1.2, 0.6, 'SEIZURE?', COLORS['decision'])

draw_arrow(ax, (8, 1.75), (8, 1.55))

# Decision criteria (side annotations)
ax.text(3.5, 1.5, 'Decision Rules:', ha='left', fontsize=9, fontweight='bold', color=COLORS['decision'])
ax.text(3.5, 1.15, '1. Motion > 50%', ha='left', fontsize=9, color='#2c3e50')
ax.text(3.5, 0.85, '2. Two+ sensors agree', ha='left', fontsize=9, color='#2c3e50')
ax.text(3.5, 0.55, '3. Motion > 30% + support', ha='left', fontsize=9, color='#2c3e50')

# YES/NO paths
ax.text(9, 0.95, 'YES', fontsize=10, color='red', fontweight='bold')
ax.text(7.3, 0.95, 'NO', fontsize=10, color='green', fontweight='bold')

# Alert box
draw_box(ax, 10.5, 0.5, 2.2, 0.7, 'BLE ALERT\n5 sec beacon', COLORS['output'], fontsize=9)
draw_arrow(ax, (8.6, 1.0), (9.4, 0.6))

# Phone detection
draw_box(ax, 12.8, 0.5, 2, 0.7, 'Patient\nPhone', COLORS['sensor'], fontsize=9)
draw_arrow(ax, (11.6, 0.5), (11.8, 0.5))

# SMS to caregiver
draw_box(ax, 15, 0.5, 2, 0.7, 'SMS to\nCaregiver', COLORS['decision'], fontsize=9)
draw_arrow(ax, (13.8, 0.5), (14, 0.5))

# Continue monitoring (moved up)
draw_box(ax, 6, 0.5, 2, 0.7, 'Continue\nMonitoring', COLORS['process'], fontsize=9)
draw_arrow(ax, (7.4, 1.2), (7, 0.85))

# Loop arrow (back to top from Continue Monitoring)
ax.annotate('', xy=(1.2, 6.4), xytext=(1.2, 0.5),
            arrowprops=dict(arrowstyle='-|>', color='gray', lw=1.5,
                           connectionstyle='arc3,rad=-0.15'))
draw_arrow(ax, (5, 0.5), (1.2, 0.5))
ax.text(0.8, 3.5, '50ms\nloop', ha='center', fontsize=8, color='gray')

# ============================================================================
# BOTTOM: Key Specs
# ============================================================================
specs = [
    'ESP32-S3 N16R8',
    'Model: 15KB INT8',
    'Latency: <2 sec',
    'Sens: 92.4%',
    'Spec: 95.7%'
]
for i, spec in enumerate(specs):
    ax.text(1 + i*3, 0.15, spec, ha='center', fontsize=8, color='gray',
            bbox=dict(boxstyle='round,pad=0.2', facecolor='#ecf0f1', edgecolor='none'))

# Save
plt.tight_layout()
plt.savefig('/Users/sonuadithya/Documents/seizure_cursor/template-app/report_graphs/00c_ppt_flowchart.png',
            dpi=300, bbox_inches='tight', facecolor='white')
plt.close()

print("✅ PPT-Optimized Flowchart saved!")
print("   Location: report_graphs/00c_ppt_flowchart.png")
print("   Format: 16:9 landscape, optimized for presentations")

