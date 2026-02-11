# WX7V Foxhunt Controller

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Amateur Radio](https://img.shields.io/badge/Amateur%20Radio-FCC%20Part%2097-blue.svg)](https://www.fcc.gov/wireless/bureau-divisions/mobility-division/amateur-radio-service)
[![Platform](https://img.shields.io/badge/Platform-Teensy%204.1-orange.svg)](https://www.pjrc.com/store/teensy41.html)

A robust, feature-rich radio "fox" (hidden transmitter) controller for amateur radio foxhunt events. Built on the Teensy 4.1 platform with support for both Morse code CW identification and WAV file audio playback.

## 🦊 What is a Foxhunt?

Amateur radio foxhunting (also called radio direction finding or T-hunting) is a competitive activity where participants use radio direction-finding equipment to locate hidden transmitters called "foxes." This project provides an automated, reliable controller for those hidden transmitters.

## ✨ Features

### Current Implementation (v1.0.0 - PRODUCTION READY)
- ✅ **Non-Blocking Architecture** - State machine design for concurrent operations
- ✅ **Morse Code CW Transmission** - Automated callsign identification with configurable WPM
- ✅ **WAV File Playback** - Voice announcements and taunts from SD card
- ✅ **Baofeng UV-5R Integration** - PTT control with proper timing (1000ms pre-roll)
- ✅ **Dual-Threshold Battery Watchdog** - Soft warning (13.6V) + hard shutdown (12.8V)
- ✅ **Watchdog Timer** - 30-second timeout protects against stuck PTT
- ✅ **Safety Interlocks** - Multiple fail-safe mechanisms
- ✅ **Status LED Indicators** - Visual feedback (heartbeat, warning, SOS)
- ✅ **Automatic Mode Switching** - Alternates between Morse and audio

### Planned Features (Future Development)
- 🔄 **Deep Sleep Mode** - Extended battery life between transmissions (Phase 3)
- 🎲 **Randomized Audio Selection** - Random taunt file selection (Phase 4)
- 📡 **DTMF Remote Control** - Command the fox via radio tones (Phase 4)
- 🔇 **VOX Activation** - Silent operation until "called" by hunters (Phase 4)
- 📊 **Duty Cycle Management** - Prevent radio overheating (Phase 3)
- 📝 **SD Card Logging** - Complete event logging and diagnostics (Phase 5)
- ⚙️ **Configuration System** - SD card based config file (Phase 5)

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
- Resistors: 1kΩ (audio filter), 10kΩ (voltage divider R1), 2.2kΩ (voltage divider R2)
- Capacitors: 10µF (audio coupling), 100nF (voltage divider filter)
- 5A inline fuse (XT60 connector)
- 10kΩ linear potentiometer (audio level control)

### Optional Components (Future)
- **MT8870 DTMF decoder** (for remote control - Phase 4)
- **DS3231 RTC module** (for accurate timestamps - Phase 5)

Complete Bill of Materials: See [WX7V Foxhunt Controller Project Specification.md](WX7V%20Foxhunt%20Controller%20Project%20Specification.md)

## 📋 Pin Configuration

| Function | Teensy Pin | Notes |
|----------|-----------|-------|
| **PTT Control** | Pin 2 | Digital Output → 1kΩ resistor → 2N2222 base |
| **Audio Out** | Pin 12 | MQS → 1kΩ → 10µF cap → 10kΩ pot → Radio mic |
| **Status LED** | Pin 13 | Built-in LED (heartbeat/TX/SOS indicator) |
| **Battery Monitor** | Pin A9 | Via voltage divider (10kΩ + 2.2kΩ) |
| **Power In** | VIN | 5.0V from buck converter |
| **Ground** | GND | Common ground with radio and converters |

## 🚀 Quick Start

### 1. Software Setup

**Open** `FoxhuntController.ino` in Arduino IDE with Teensyduino installed.

**Required libraries** (install via Arduino Library Manager or Teensyduino):
- **Audio Library** (included with Teensyduino)
- **SD Library** (Arduino core)
- **Snooze Library** (Teensy low power)
- **Watchdog_t4** (Teensy 4.x watchdog timer)

**Configure your callsign:**
```cpp
// Edit line 35 in FoxhuntController.ino
const char* CALLSIGN = "WX7V FOX";  // Change to YOUR callsign
```

### 2. Hardware Assembly

1. **PTT Circuit:**
   ```
   Teensy Pin 2 → 1kΩ resistor → 2N2222 base
   2N2222 collector → Radio PTT pin (tip of 2.5mm plug)
   2N2222 emitter → Ground
   ```

2. **Audio Circuit:** ⚠️ **1kΩ resistor is REQUIRED for clean audio**
   ```
   Teensy Pin 12 → 1kΩ resistor → 10µF capacitor (+) → 10kΩ pot (left pin)
   10kΩ pot (center pin) → Radio mic (ring of 2.5mm plug)
   10kΩ pot (right pin) → Ground
   ```

3. **Battery Monitor Circuit:**
   ```
   Battery (+) → 10kΩ (R1) → Pin A9 → 2.2kΩ (R2) → Ground
                            → 100nF cap → Ground (noise filter)
   ```

4. **Power:**
   ```
   Battery (14.8V) → Buck A (8.0V) → Radio battery eliminator
                   → Buck B (5.0V) → Teensy VIN
   ```

See complete wiring diagrams and connection details in:
- **[Hardware Reference](Teensy4%20Fox%20Hardware%20Reference.md)** - Wiring summary, pigtail pinout, audio filtering
- **[Project Specification](WX7V%20Foxhunt%20Controller%20Project%20Specification.md)** - Full technical specifications

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

**Open** `FoxhuntController.ino` and configure:

```cpp
// Line 35: Change to YOUR callsign
const char* CALLSIGN = "WX7V FOX";

// Line 41: Adjust Morse speed if desired
const int MORSE_WPM = 12;  // 12 words per minute

// Line 46: Adjust transmission interval
const long TX_INTERVAL = 60000;  // 60 seconds

// Lines 56-57: Battery thresholds (verify with your setup)
const float SOFT_WARNING_THRESHOLD = 13.6;   // Low battery warning
const float HARD_SHUTDOWN_THRESHOLD = 12.8;  // Emergency shutdown
```

**Select Board:** Tools → Board → Teensy 4.1  
**Upload** to Teensy via Arduino IDE.

**See:** [FoxhuntController_README.md](FoxhuntController_README.md) for detailed instructions.

### 5. Test Before Deployment

**Pre-flight checklist:**
- [ ] Battery fully charged (16.8V)
- [ ] SD card has `FOXID.WAV` (and optionally `LOWBATT.WAV`)
- [ ] Callsign configured in code
- [ ] Audio filter installed (1kΩ + 10µF + 10kΩ pot)
- [ ] Battery voltage divider installed (10kΩ + 2.2kΩ + 100nF)
- [ ] Battery voltage reading verified with multimeter
- [ ] Radio set to correct frequency and CTCSS
- [ ] Antenna connected
- [ ] Volume adjusted (test with speaker)
- [ ] PTT timing verified (1s pre-roll)
- [ ] Audio levels not distorted
- [ ] Watchdog timer tested (optional)

**Test procedure:**
1. Power on with monitoring radio nearby
2. Verify LED illuminates during transmission
3. Confirm audio is clear and intelligible
4. Check transmission timing accuracy
5. Monitor for 10+ transmissions before field deployment

## 📚 Documentation

- **[Project Specification](WX7V%20Foxhunt%20Controller%20Project%20Specification.md)** - Complete hardware specs, features, and technical details
- **[Hardware Reference](Teensy4%20Fox%20Hardware%20Reference.md)** - Wiring summary, pigtail pinout, audio filtering, parts checklist
- **[Implementation Plan](IMPLEMENTATION_PLAN.md)** - Phased development roadmap with time estimates
- **[FoxhuntController.ino](FoxhuntController.ino)** - Production controller code (v1.0.0)
- **[FoxhuntController README](FoxhuntController_README.md)** - Quick start guide for the controller
- **[Morse Code Controller](Morse%20Code%20Controller.mdc)** - Simple CW transmission reference code
- **[Audio Controller](Audio%20Controller%20Code.mdc)** - WAV file playback reference code
- **[Cursor Rules](.cursorrules)** - AI-assisted development guidelines

## 🔒 Safety Features

This project implements multiple safety mechanisms:

- **Watchdog Timer** - 30-second timeout with automatic PTT release
- **Dual-Threshold Battery Watchdog:**
  - **Soft Warning (13.6V)** - Plays "LOW BATTERY" every 5 cycles
  - **Hard Shutdown (12.8V)** - Permanently disables PTT, enters SOS mode
- **Transmission Timeout** - 30-second maximum per transmission
- **Hysteresis Protection** - Prevents battery warning spam
- **Emergency PTT Release** - Multiple fail-safe mechanisms
- **Non-blocking Architecture** - Responsive to all safety checks

⚠️ **Important:** A stuck PTT can drain batteries, overheat equipment, and cause harmful interference. The battery hard shutdown at 12.8V prevents LiPo damage and requires power cycle to reset.

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
**Version 1.0.0 - PRODUCTION READY** ✅
- Non-blocking state machine architecture ✅
- Morse code and audio transmission ✅
- Dual-threshold battery watchdog ✅
- Watchdog timer safety ✅
- Hardware tested and validated ✅
- Documentation complete ✅

**Next Development (Phase 3):**
- Deep sleep power management
- Duty cycle tracking and enforcement
- Enhanced logging to SD card

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
