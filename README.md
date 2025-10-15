# Miner Health Monitoring System

Real-time health monitoring system for underground miners with intelligent anomaly detection and emergency alerting.

## 🎯 Features

- **Multi-sensor Monitoring**: SpO2, Heart Rate, ECG, Blood Pressure, Temperature, Fall Detection
- **Power Optimized**: Deep sleep between measurements, 35-second monitoring intervals
- **Intelligent Analysis**: Edge-based health analysis with adaptive monitoring
- **Emergency Response**: Automatic alerts to surface via LoRa/BLE gateway
- **Fall Detection**: 6-axis IMU with sophisticated fall detection algorithm

## 📋 Hardware Components

| Component | Part Number | Purpose |
|-----------|-------------|---------|
| MCU | nRF52840 | ARM Cortex-M4F with BLE 5.0 |
| SpO2 & HR | MAX30102 | Optical pulse oximeter |
| ECG & BP | ADS1292R | 24-bit biopotential ADC |
| Temperature | TMP117 | ±0.1°C accurate digital sensor |
| IMU | ICM-42688-P | 6-axis accelerometer + gyroscope |
| Battery | Li-ion 1500mAh | Portable power |
| Gateway | LoRa/BLE | Underground-to-surface communication |

## 🚀 Getting Started

### Prerequisites

```bash
# Install ARM GCC Toolchain
sudo apt-get install gcc-arm-none-eabi

# Download nRF5 SDK 17.1.0
wget https://developer.nordicsemi.com/nRF5_SDK/nRF5_SDK_v17.x.x/nRF5_SDK_17.1.0_ddde560.zip
unzip nRF5_SDK_17.1.0_ddde560.zip
```

### Building

```bash
# Clone repository
git clone https://github.com/Criz-007/miner-health-monitor.git
cd miner-health-monitor

# Update SDK path in Makefile
# Edit Makefile: SDK_ROOT := /path/to/nRF5_SDK_17.1.0_ddde560

# Build
make

# Flash
make flash
```

### Simulation (No Hardware Required)

```bash
# Run Python simulation
python miner_health_monitor_simulation.py
```

## 📊 System Architecture

```
┌─────────────────────────────────────────┐
│         Miner Wearable Device           │
├─────────────────────────────────────────┤
│                                         │
│  ┌─────────────────────────────────┐   │
│  │       nRF52840 MCU              │   │
│  │  ┌──────────────────────────┐   │   │
│  │  │   Power Management       │   │   │
│  │  │   - Sleep/Wake Control   │   │   │
│  │  │   - Sensor Power Control │   │   │
│  │  └──────────────────────────┘   │   │
│  │                                  │   │
│  │  ┌──────────────────────────┐   │   │
│  │  │   Health Analysis Engine │   │   │
│  │  │   - Vital Sign Processing│   │   │
│  │  │   - Anomaly Detection    │   │   │
│  │  │   - Emergency Detection  │   │   │
│  │  └──────────────────────────┘   │   │
│  └─────────────────────────────────┘   │
│           │          │          │      │
│  ┌────────┴─┐  ┌────┴─────┐  ┌─┴────┐ │
│  │ MAX30102 │  │ ADS1292R │  │TMP117│ │
│  │ SpO2/HR  │  │  ECG/BP  │  │ Temp │ │
│  └──────────┘  └──────────┘  └──────┘ │
│                                         │
│  ┌─────────────┐    ┌────────────────┐ │
│  │ ICM-42688   │    │ LoRa/BLE Radio │ │
│  │ Fall Detect │    │ Communication  │ │
│  └─────────────┘    └────────────────┘ │
└─────────────────────────────────────────┘
              │
              │ Wireless
              ▼
    ┌─────────────────┐
    │  Gateway Node   │
    │  (Underground)  │
    └─────────────────┘
              │
              │ LoRa Long Range
              ▼
    ┌─────────────────┐
    │ Surface Station │
    │  & Monitoring   │
    └─────────────────┘
```

## 🔬 Health Monitoring Algorithm

### Normal Operation
- **Interval**: 35 seconds
- **Process**: Sleep → Wake → Measure → Analyze → Sleep

### Warning Detected
- **Trigger**: 1 vital sign outside normal range
- **Action**: Continue normal monitoring, increment anomaly counter

### Critical Condition
- **Trigger**: 2+ warning flags OR sustained anomaly
- **Action**: Switch to extended monitoring (10s intervals)
- **Communication**: Send alert to gateway

### Emergency
- **Trigger**: Fall detection OR critical vital signs
- **Action**: Continuous monitoring (5s intervals)
- **Communication**: Immediate emergency transmission with high priority

## 📈 Vital Sign Thresholds

| Parameter | Normal Range | Warning | Critical |
|-----------|--------------|---------|----------|
| SpO2 | 95-100% | <92% | <85% |
| Heart Rate | 60-100 BPM | <45 or >120 | <40 or >150 |
| Temperature | 36.5-37.5°C | <35.5 or >38.5 | <35 or >40 |
| BP Systolic | 90-140 mmHg | <90 or >160 | <80 or >180 |
| Fall | No fall | Fall detected | Fall + no movement |

## 🔋 Power Consumption

| Mode | Current Draw | Duration |
|------|--------------|----------|
| Deep Sleep | ~5µA | 35s (normal) |
| Sensor Warmup | ~15mA | 2s |
| Measurement | ~25mA | 3s |
| Transmission | ~40mA | 0.5s |

**Estimated Battery Life**: ~48 hours with 1500mAh battery

## 📡 Communication Protocol

### Data Packet Structure
```
[Start][Status][SpO2][HR_H][HR_L][BP_S_H][BP_S_L][BP_D_H][BP_D_L][Temp_H][Temp_L][Flags][End]
  0xAA   1B      1B    1B    1B     1B      1B      1B      1B      1B      1B     1B    0x55
```

### Status Codes
- `0x00`: Normal
- `0x01`: Warning
- `0x02`: Critical
- `0x03`: Emergency

## 🧪 Testing

### Unit Tests
```bash
# Run sensor tests
make test

# Test individual sensors
./test_max30102
./test_ads1292r
./test_tmp117
./test_icm42688
```

### Simulation Testing
The Python simulation allows testing without hardware:
- Realistic vital sign generation
- Anomaly injection for testing detection algorithms
- Full system workflow demonstration

## 📝 Code Structure

```
miner-health-monitor/
├── main.c                          # Main application logic
├── max30102_driver.c/h             # SpO2 & Heart Rate sensor
├── ads1292r_driver.c/h             # ECG & Blood Pressure sensor
├── tmp117_driver.c/h               # Temperature sensor
├── icm42688_driver.c/h             # IMU & Fall detection
├── communication.c/h               # LoRa/BLE communication
├── Makefile                        # Build configuration
├── sdk_config.h                    # nRF SDK configuration
├── miner_health_monitor_simulation.py  # Python simulation
└── README.md                       # This file
```

## 🤝 Contributing

Contributions are welcome! Please:
1. Fork the repository
2. Create a feature branch
3. Submit a pull request

## 📄 License

MIT License - see LICENSE file for details

## 👤 Author

**Criz-007**
- GitHub: [@Criz-007](https://github.com/Criz-007)
- Date: 2025-10-15

## 🙏 Acknowledgments

- nRF5 SDK by Nordic Semiconductor
- Sensor manufacturers: Maxim, TI, TDK InvenSense
- Mining safety research community

---

**Safety Notice**: This is a prototype system. For production deployment in actual mining operations, additional certifications, testing, and regulatory approvals are required.
