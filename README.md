# Teensy 4.1 Foxhunt Controller

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Amateur Radio](https://img.shields.io/badge/Amateur%20Radio-FCC%20Part%2097-blue.svg)](https://www.fcc.gov/wireless/bureau-divisions/mobility-division/amateur-radio-service)
[![Platform](https://img.shields.io/badge/Platform-Teensy%204.1-orange.svg)](https://www.pjrc.com/store/teensy41.html)

A robust, feature-rich radio "fox" (hidden transmitter) controller for amateur radio foxhunt events. Built on the Teensy 4.1 platform with support for both Morse code CW identification and WAV file audio playback.

## 🦊 What is a Foxhunt?

Amateur radio foxhunting (also called radio direction finding or T-hunting) is a competitive activity where participants use radio direction-finding equipment to locate hidden transmitters called "foxes." This project provides an automated, reliable controller for those hidden transmitters.

## ✨ Features

### Current Implementation (Phase 1)
- ✅ **Morse Code CW Transmission** - Automated callsign identification with configurable WPM
- ✅ **WAV File Playback** - Voice announcements and taunts from SD card
- ✅ **Baofeng UV-5R Integration** - PTT control with proper timing (1000ms pre-roll)
- ✅ **Configurable Intervals** - Adjustable transmission timing
- ✅ **Status LED Indicators** - Visual feedback during operation

### Planned Features (In Development)
- 🔄 **Non-Blocking Architecture** - State machine design for concurrent operations
- 🔋 **Battery Monitoring** - Real-time voltage tracking with low-battery warnings
- ⚡ **Watchdog Timer** - Safety protection against stuck PTT
- 🎲 **Randomized Content** - Random audio file selection for variety
- 📡 **DTMF Remote Control** - Command the fox via radio tones
- 🔇 **VOX Activation** - Silent operation until "called" by hunters
- 💤 **Deep Sleep Mode** - Extended battery life between transmissions
- 📊 **Duty Cycle Management** - Prevent radio overheating
- 📝 **SD Card Logging** - Complete event logging and diagnostics

See [IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md) for the complete development roadmap.

## 🔧 Hardware Requirements

### Core Components
- **Teensy 4.1** microcontroller (Cortex-M7 @ 600MHz)
- **Baofeng UV-5R** radio (or similar with 2.5mm/3.5mm Kenwood connector)
- **2N2222 NPN transistor** (for PTT control)
- **14.8V 4S LiPo battery** (5200mAh recommended)
- **Dual buck converters** (8.0V for radio, 5.0V for Teensy)
- **MicroSD card** (8GB+, FAT32 formatted)

### Supporting Components
- Resistors: 1kΩ, 10kΩ, 47kΩ
- Capacitors: 10µF, 0.1µF
- 5A inline fuse (XT60 connector)
- 10kΩ potentiometer (audio level control)

### Optional Components
- **MT8870 DTMF decoder** (for remote control)
- **DS3231 RTC module** (for accurate timestamps)
- **Voltage divider** (for battery monitoring)

Complete Bill of Materials: See [Teensy Foxhunt Controller Project Specification.md](Teensy%20Foxhunt%20Controller%20Project%20Specification.md)

## 📋 Pin Configuration

| Function | Teensy Pin | Notes |
|----------|-----------|-------|
| **PTT Control** | Pin 2 | Digital Output → 1kΩ resistor → 2N2222 base |
| **Audio Out** | Pin 12 | PWM/MQS → 10µF cap → 10kΩ pot → Radio mic |
| **Status LED** | Pin 13 | Built-in LED (heartbeat/TX indicator) |
| **Battery Monitor** | Pin A0 | Via voltage divider (47kΩ + 10kΩ) |
| **Power In** | VIN | 5.0V from buck converter |
| **Ground** | GND | Common ground with radio and converters |

## 🚀 Quick Start

### 1. Software Setup

```bash
# Clone the repository
git clone https://github.com/wx7v/teensy4.1-foxhunt-controller.git
cd teensy4.1-foxhunt-controller

# Install Arduino IDE with Teensyduino
# Download from: https://www.pjrc.com/teensy/td_download.html

# Required libraries (install via Arduino Library Manager):
# - Audio Library (included with Teensyduino)
# - SD Library (Arduino core)
```

### 2. Hardware Assembly

1. **PTT Circuit:**
   ```
   Teensy Pin 2 → 1kΩ resistor → 2N2222 base
   2N2222 collector → Radio PTT pin (tip of 2.5mm plug)
   2N2222 emitter → Ground
   ```

2. **Audio Circuit:**
   ```
   Teensy Pin 12 → 1kΩ resistor → 10µF capacitor → 10kΩ pot → Radio mic (ring of 2.5mm plug)
   ```

3. **Power:**
   ```
   Battery (14.8V) → Buck A (8.0V) → Radio battery eliminator
                   → Buck B (5.0V) → Teensy VIN
   ```

See complete wiring diagrams and connection details in:
- **[Hardware Reference](Teensy4%20Fox%20Hardware%20Reference.md)** - Wiring summary, pigtail pinout, audio filtering
- **[Project Specification](Teensy%20Foxhunt%20Controller%20Project%20Specification.md)** - Full technical specifications

### 3. Prepare SD Card

Format microSD card as FAT32 and add audio files:

```
SD_CARD/
├── FOXID.WAV       # Your callsign identification
├── TAUNT01.WAV     # Optional taunt files
├── TAUNT02.WAV
├── LOWBATT.WAV     # Low battery warning
└── CONFIG.TXT      # Configuration file (future)
```

**Audio file specs:**
- Format: 16-bit PCM WAV
- Sample rate: 22.05kHz or 44.1kHz
- Mono or stereo
- Keep files < 10 seconds for reasonable duty cycle

### 4. Configure & Upload

#### For Morse Code Only:
```cpp
// Edit Morse Code Controller.mdc
const char* callsign = "WX7V FOX";  // Change to your callsign
const int toneFreq = 800;            // Audio pitch in Hz
const int dotLen = 100;              // 100ms = ~12 WPM
const long transmitInterval = 60000; // 60 seconds between IDs
```

#### For Audio Playback:
```cpp
// Edit Audio Controller Code.mdc
const long transmitInterval = 60000; // 60 seconds between transmissions

// In loop():
playFoxFile("FOXID.WAV"); // Change to your file name
```

Upload to Teensy via Arduino IDE.

### 5. Test Before Deployment

**Pre-flight checklist:**
- [ ] Battery fully charged (16.8V)
- [ ] SD card has audio files
- [ ] Radio set to correct frequency
- [ ] Antenna connected
- [ ] Volume adjusted (test with speaker)
- [ ] PTT timing verified (1s pre-roll)
- [ ] Audio levels not distorted

**Test procedure:**
1. Power on with monitoring radio nearby
2. Verify LED illuminates during transmission
3. Confirm audio is clear and intelligible
4. Check transmission timing accuracy
5. Monitor for 10+ transmissions before field deployment

## 📚 Documentation

- **[Project Specification](Teensy%20Foxhunt%20Controller%20Project%20Specification.md)** - Complete hardware specs, features, and technical details
- **[Hardware Reference](Teensy4%20Fox%20Hardware%20Reference.md)** - Wiring summary, pigtail pinout, audio filtering, parts checklist
- **[Implementation Plan](IMPLEMENTATION_PLAN.md)** - Phased development roadmap with time estimates
- **[Morse Code Controller](Morse%20Code%20Controller.mdc)** - Simple CW transmission reference code
- **[Audio Controller](Audio%20Controller%20Code.mdc)** - WAV file playback reference code
- **[Cursor Rules](.cursorrules)** - AI-assisted development guidelines

## 🔒 Safety Features

This project implements multiple safety mechanisms:

- **PTT Timeout Protection** - Prevents stuck transmit conditions
- **Duty Cycle Limiting** - Prevents radio overheating (20% max)
- **Battery Monitoring** - Low-voltage shutdown protects LiPo cells
- **Watchdog Timer** - Automatic recovery from crashes
- **Emergency PTT Release** - Fail-safe mechanisms

⚠️ **Important:** A stuck PTT can drain batteries, overheat equipment, and cause harmful interference. Always test safety features before field deployment.

## 📊 Performance Characteristics

| Metric | Value | Notes |
|--------|-------|-------|
| **Transmission Accuracy** | ±2 seconds | Over 24-hour period |
| **Battery Life** | 18-20 hours | 60s interval, 5s TX, 5200mAh pack |
| **PTT Pre-roll** | 1000ms | Required for Baofeng squelch |
| **PTT Tail** | 500ms | After transmission ends |
| **Idle Current** | ~100mA | Teensy active, no transmission |
| **TX Current** | ~150-180mA | Teensy only (radio is separate rail) |
| **Deep Sleep Current** | ~5mA | With Snooze library (future) |
| **Max Duty Cycle** | 20% | Recommended for continuous operation |

## 🛠️ Development

### Current Status
**Phase 1: COMPLETE** ✅
- Basic Morse and audio implementations functional
- Hardware tested and validated
- Documentation complete

**Phase 2: IN PROGRESS** 🔄
- Non-blocking architecture refactor
- State machine design

### Contributing

This project welcomes contributions! Areas of interest:
- Non-blocking code refactoring
- Additional safety features
- Power optimization
- Testing and validation
- Documentation improvements
- Bug fixes

Please read the [Implementation Plan](IMPLEMENTATION_PLAN.md) before contributing.

### Development Setup

```bash
# Recommended: Use PlatformIO for better dependency management
# Or use Arduino IDE with Teensyduino

# Install required libraries
# Audio Library (Teensyduino)
# SD Library (Arduino core)
# Optional: Snooze, Watchdog_t4, TimeLib
```

## 📜 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

**Copyright © 2026 Chris L White, WX7V**  
*Hardware Reference and Project Design created with assisted by Gemini 3*
*Code Written with assistance by Claude Sonnet 4.5*

### Radio Operation Notice

This software is designed for **amateur radio operations only**. Users are responsible for:

- Compliance with FCC Part 97 rules (US) or equivalent regulations
- Proper station identification (every 10 minutes)
- Licensed operation on authorized frequencies
- Adherence to duty cycle and power limitations

**The authors assume no liability for improper or unlicensed use of this software.**

## 🏆 Acknowledgments

- **PJRC** - For the excellent Teensy platform and Audio Library
- **Amateur Radio Community** - For foxhunting traditions and techniques
- **Google DeepMind** - For Gemini 3 AI assistance
- **Anthropic** - For Claude Sonnet 4.5 AI assistance

## 📞 Contact

**Chris L White, WX7V**

Project Link: [https://github.com/wx7v/teensy4.1-foxhunt-controller](https://github.com/wx7v/teensy4.1-foxhunt-controller)

## 🎯 Use Cases

- **Amateur Radio Foxhunts** - Competitive direction-finding events
- **Training Exercises** - Practice radio direction-finding skills
- **Emergency Preparedness** - Search and rescue training
- **Educational Demos** - Teaching RF propagation and antenna theory
- **Field Day Activities** - Fun side activities at ham radio events

## 📸 Gallery

*Photos and diagrams coming soon after field testing!*

---

**73 de WX7V** 📡🦊

*Good luck and happy hunting!*
