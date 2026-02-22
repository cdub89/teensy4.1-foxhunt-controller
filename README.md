# WX7V Foxhunt Controller

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Amateur Radio](https://img.shields.io/badge/Amateur%20Radio-FCC%20Part%2097-blue.svg)](https://www.fcc.gov/wireless/bureau-divisions/mobility-division/amateur-radio-service)
[![Platform](https://img.shields.io/badge/Platform-Teensy%204.1-orange.svg)](https://www.pjrc.com/store/teensy41.html)

A robust radio "fox" (hidden transmitter) controller for amateur radio foxhunt events. Built on the Teensy 4.1 platform with support for both Morse code CW identification and WAV file audio playback using a Baofeng UV-5R radio.

---

## 🦊 What is a Foxhunt?

Amateur radio foxhunting (also called radio direction finding or T-hunting) is a competitive activity where participants use radio direction-finding equipment to locate hidden transmitters called "foxes." This project provides an automated, reliable controller for those hidden transmitters.

---

## ✨ Project Goals

Build a production-ready fox controller with:

- **Non-blocking architecture** - State machine design for concurrent operations
- **Morse code CW transmission** - Automated callsign identification
- **WAV file playback** - Voice announcements and taunts from SD card
- **Safety-first design** - Watchdog timer, battery protection, PTT timeout
- **Baofeng UV-5R integration** - Proper PTT timing (1000ms pre-roll, 500ms tail)
- **Battery watchdog system** - Dual-threshold protection (soft warning + hard shutdown)
- **Extended battery life** - Deep sleep between transmissions
- **Field-proven reliability** - 24+ hour continuous operation

---

## 📋 Current Status

### Phase 1: Foundation ✅ COMPLETE

Two working reference implementations demonstrating core functionality (now archived):

**1. Morse Code Controller** (`archived-files/Morse Code Controller.mdc`)
- Blocking implementation for CW identification
- Standard ITU Morse timing
- Configurable WPM and tone frequency
- Pin 2 PTT control, Pin 12 audio output
- 1000ms pre-roll, 500ms tail timing

**2. Audio Controller** (`archived-files/Audio Controller Code.mdc`)
- Blocking implementation for WAV playback
- Teensy Audio Library integration
- SD card support (built-in Teensy 4.1 slot)
- MQS audio output on Pin 12
- Automatic playback completion detection

These reference implementations proved the concept and provided working code patterns for the production system.

### Phase 2: Production System ✅ COMPLETE

**Production Controller** (`controller/controller-production/controller-production.ino`)

**Version 1.3** - Documentation alignment with Project Specification v2.0:

**Core Features Implemented:**
- ✅ Random WAV file selection (automatic SD card scanning)
- ✅ Intelligent playback (loop if <30s, play to completion if >=30s)
- ✅ Battery monitoring with 5-state system (GOOD/LOW/VERY_LOW/SHUTDOWN/CRITICAL)
- ✅ Morse code station ID (FCC compliant)
- ✅ LED status indicators (Morse battery patterns)
- ✅ Dual logging (Serial Monitor + SD card)
- ✅ PTT timeout watchdog (90s max)
- ✅ Reset cause detection for troubleshooting
- ✅ Audio memory optimization (40 blocks)

**Recent Updates (v1.3):**
- ✅ Documentation aligned with Project Specification v2.0
- ✅ Clarified power architecture (single buck for Teensy, optional buck for radio)
- ✅ Updated performance metrics (60s TX cycle, 24.5 hour runtime)
- ✅ Added Pin 10 MQS configuration details
- ✅ Documented foil tape SD card fix
- ✅ Standardized on 150 MHz minimum CPU speed

**Field-Tested Performance:**
- 8+ consecutive cycles with zero errors
- Stable operation with random file selection
- Complete transmission cycles (PTT + Morse + WAV + ID)
- Peak audio memory: 2-5 blocks (40 allocated for safety margin)
- No reboots or crashes during normal operation
- Clean audio quality with RF shielding

**Hardware Requirements for Stability:**
- Ferrite beads on radio and Teensy power lines (tight/snug fit)
- **Foil tape over SD card slot** (covers card slot opening - critical fix from field testing)
- Class 10 or better SD card, freshly formatted FAT32

**Ready for field deployment!**

### Phase 3: Advanced Features (Future)

After production system is stable:

- **Power management** - Deep sleep during idle periods (~5mA vs ~100mA)
- **Duty cycle management** - Prevent radio overheating (20% max)
- **Randomized audio** - Multiple taunt files with random selection
- **Configuration system** - SD card based config file
- **Remote control** - DTMF commands (optional hardware)
- **VOX activation** - Silent operation until "called" (optional)

---

## 🔧 Hardware Requirements

### Core Components

- **Teensy 4.1** microcontroller (Cortex-M7 @ 600MHz, configured to 150 MHz minimum)
- **Baofeng UV-5R** radio (set to LOW power: 1W)
- **2N2222 NPN transistor** (PTT control)
- **12V LiFePO4 battery** (Bioenno 4.5Ah recommended)
- **Buck converter:** 5.0V for Teensy VIN
- **BL-5 Battery Eliminator** for radio (direct from 12V battery)
  - **Optional Buck A (8.0V):** May be required for cheaper off-market battery eliminators (field testing recommended)
  - Quality BL-5 eliminators work directly from 12V battery
- **MicroSD card** (8GB+, FAT32 formatted)
- **5A inline fuse** (XT60 main battery lead)

### Audio Filter Circuit (REQUIRED)

**Components:**
- 1kΩ resistor (low-pass filter with capacitor)
- 10µF electrolytic capacitor (DC blocking)
- 10kΩ linear potentiometer (volume control)

**Purpose:** Smooths MQS PWM output and prevents radio distortion

### Battery Monitor Circuit (CRITICAL)

**Components:**
- 10kΩ resistor (R1) - Battery+ to Pin A9
- 2.0kΩ resistor (R2) - Pin A9 to Ground
- 100nF ceramic capacitor (noise filter, parallel with R2)

**Purpose:** Real-time voltage monitoring for dual-threshold battery protection

### RF Suppression (FIELD-PROVEN)

**Components:**
- 0.1µF ceramic capacitor (104) - soldered across buck converter output
- 3× snap-on ferrite cores:
  - Two (2) on radio power lines
  - One (1) on Teensy power lines
- **Foil tape over SD card slot** - covers the card slot opening

**Purpose:** Prevents RF coupling during 1W transmission and eliminates SD card read/write errors (discovered during field testing)

### Complete Documentation

- **[Project Specification v2.0.md](Project%20Specification%20v2.0.md)** - Complete wiring specifications, pinout, voltage calculations, power architecture, operational logic, and reference implementation examples

---

## 📋 Pin Configuration

| Function | Teensy Pin | Connection |
|----------|-----------|------------|
| **PTT Control** | Pin 2 | Digital Output → 1kΩ → 2N2222 base |
| **Audio Out (Primary)** | Pin 12 | MQS → 10µF cap → 1kΩ → 10kΩ pot → Radio mic |
| **Audio Out (Secondary)** | Pin 10 | MQS → 10µF cap → 1kΩ → Ground (filter path) |
| **Status LED** | Pin 13 | Built-in LED (heartbeat/TX/warning/SOS) |
| **Battery Monitor** | Pin A9 | Via voltage divider (10kΩ + 2.0kΩ + 100nF cap) |
| **Power In** | VIN | 5.0V from buck converter |
| **Ground** | GND | Star ground configuration (all grounds meet at ONE point) |

---

## 🚀 Getting Started

### 1. Review Documentation

**Start here:**
1. **[Project Specification v2.0.md](Project%20Specification%20v2.0.md)** - Master reference document (hardware specs, power architecture, operational requirements)

**Reference implementations:**
- `audio_wav_test/audio_wav_test.ino` - Audio and transmission control reference
- `battery_monitor_test/battery_monitor_test.ino` - Battery monitoring reference

### 2. Build Hardware

Follow wiring specifications in `Project Specification v2.0.md`:

1. **Power system** - 5V buck converter for Teensy, direct 12V to BL-5 eliminator (optional 8.0V buck for cheaper eliminators)
2. **PTT circuit** - 2N2222 transistor with 1kΩ base resistor
3. **Audio circuit** - Dual MQS output (Pin 12 + Pin 10) with 1kΩ resistor + 10µF cap + 10kΩ pot
4. **Battery monitor** - Voltage divider on Pin A9 (10kΩ + 2.0kΩ)
5. **Star ground** - All grounds meet at one common point

**Critical RF Suppression:** 
- 2× ferrite beads on radio power lines
- 1× ferrite bead on Teensy power line
- 0.1µF ceramic capacitor across buck converter output
- **Foil tape over SD card slot** (covers card slot opening, eliminates read/write errors during transmission)

### 3. Prepare SD Card

Format microSD card as FAT32 and add WAV files:

```
SD_CARD/
├── Apollo8a.wav      # NASA Apollo 8 audio
├── JFKmoon1.wav      # JFK Moon speech
├── Skylab02.wav      # Skylab audio
├── WWVBBL01.wav      # WWV time station
└── WWVMarks.wav      # WWV time marks
```

**Audio file specs:**
- Format: 16-bit PCM WAV
- Sample rate: 22.05kHz or 44.1kHz
- Mono (recommended for best compatibility)
- Remove all metadata from files
- Use 8.3 filename format (8 chars + .WAV extension)

**Automatic Selection:**
- Controller scans SD card root directory at startup
- Randomly selects one file per transmission cycle
- Files < 30s automatically loop to reach 30s minimum
- Files >= 30s play to completion (no looping)

### 4. Upload Production Controller

**Location:** `controller/controller-production/controller-production.ino`

**Configuration:**
1. Open file in Arduino IDE 2.3.7
2. Update `CALLSIGN_ID` constant with your call sign (e.g., "WX7V/F")
3. Verify CPU speed: Tools → CPU Speed → **150 MHz or higher** (critical!)
4. Select board: Tools → Board → Teensy 4.1
5. Upload to Teensy

**Important:** CPU speed must be 150 MHz or higher for stable WAV playback. Lower speeds will cause audio glitches or crashes.

### 5. Test and Deploy

**Initial Testing:**
1. Open Serial Monitor (115200 baud)
2. Observe startup sequence and reset cause
3. Verify WAV files are detected
4. Watch first transmission cycle complete
5. Check audio quality and battery voltage

**Field Deployment Checklist:**
- ✅ Multiple transmission cycles completed without errors
- ✅ Battery voltage reading accurately
- ✅ Audio output clean (no distortion)
- ✅ PTT timing correct (1000ms pre-roll, 500ms tail)
- ✅ RF suppression installed (ferrites + capacitors)
- ✅ Station ID transmitting correctly

### 6. Calibrate Battery Monitor

**Critical for battery protection:**

1. Connect fully charged battery (14.6V)
2. Measure voltage at Pin A9 with multimeter
3. Calculate ratio: `actual_ratio = 14.6V / measured_voltage`
4. Update `VOLTAGE_DIVIDER_RATIO` constant in code
5. Test at 13.0V and 12.4V to verify accuracy

See `Project Specification v2.0.md` for voltage divider specifications and `battery_monitor_test/battery_monitor_test.ino` for calibration reference code.

---

## 🔒 Safety Features

This is **safety-critical code** - a stuck PTT can drain batteries, overheat equipment, or cause interference.

### Mandatory Safety Systems

**1. Watchdog Timer**
- 30-second timeout with automatic PTT release
- Resets Teensy if code hangs
- Prevents stuck PTT condition

**2. Dual-Threshold Battery Watchdog**
- **Soft Warning (12.4V / 3.1V per cell):**
  - Plays "LOW BATTERY" warning
  - LED changes to rapid blink (200ms)
  - Continues operation (you have time to finish hunt)
  - Clears at 13.0V (hysteresis prevents spam)
  
- **Hard Shutdown (10.8V / 2.7V per cell - LVP threshold):**
  - **IMMEDIATE** PTT disable (set to INPUT mode)
  - SOS LED pattern (...---...)
  - **PERMANENT** until power cycle
  - Conservative threshold protects LiFePO4 battery cycle life

**3. PTT Timeout**
- Maximum 90 seconds per transmission
- Emergency PTT release on timeout
- Multiple fail-safe mechanisms

**4. Non-Blocking Architecture**
- Responsive to all safety checks
- Concurrent monitoring (battery, watchdog, timing)
- No `delay()` calls in production code

**Why 10.8V hard shutdown?** This conservative LVP (Low Voltage Protection) threshold (2.7V per cell) protects LiFePO4 battery cycle life and ensures reliable operation per Project Specification v2.0.

---

## 📊 Performance Characteristics

### Power Configuration

**CPU Speed:** 150 MHz minimum (REQUIRED for v1.3)
- Production controller requires 150 MHz minimum for stable WAV playback with SD card operations
- 396 MHz recommended for optimal performance and power balance
- Lower speeds will cause WAV playback to fail due to insufficient SD card read/decode performance
- Set in Arduino IDE: Tools → CPU Speed → 150 MHz or higher

**Radio Power:** LOW (1W)
- Reduces current from ~1.5A to ~0.35-0.4A (75% savings)
- Minimizes RF interference with Teensy
- Sufficient range for typical foxhunts (0.5-2 miles)
- Set via radio menu: Menu → 2 (TXP) → LOW

**Combined savings enable >24 hour continuous operation**

### Expected Performance

| Metric | Value | Notes |
|--------|-------|-------|
| **Battery Life** | ~24.5 hours | 4.5Ah LiFePO4 @ 60s TX / 240s idle (5-min cycle) |
| **Transmit Duration** | 60 seconds | Per cycle (Morse + WAV + Station ID) |
| **PTT Pre-roll** | 1000ms | Required for Baofeng squelch opening |
| **PTT Tail** | 500ms | After transmission ends |
| **Duty Cycle** | 20% | 60s TX / 300s total cycle (safe for continuous operation) |
| **Average Current** | ~164mA | Combined system average across TX/idle cycles |
| **TX Current** | 40-80mA | Teensy only (radio on separate 12V rail) |
| **Deep Sleep** | ~5mA | With Snooze library (future implementation) |

### Battery Voltage Monitoring

| Battery Voltage | Cell Voltage | Pin A9 Voltage | Status |
|----------------|--------------|----------------|---------|
| 13.8V | 3.45V | 2.30V | Fully Charged (LiFePO4 max) |
| 13.0V | 3.25V | 2.17V | Nominal |
| 12.4V | 3.10V | 2.07V | ⚠️ Soft Warning |
| 12.0V | 3.00V | 2.00V | ⚠️ Hard Warning |
| 10.8V | 2.70V | 1.80V | 🛑 Hard Shutdown (LVP threshold) |

**Voltage Divider:** 10kΩ (R_High) + 2.0kΩ (R_Low) on Pin A9

---

## 📚 Documentation

### Primary Reference

- **[Project Specification v2.0.md](Project%20Specification%20v2.0.md)** - **SINGLE SOURCE OF TRUTH** for hardware specs, power architecture, pinout, operational requirements, and implementation examples

### Production Code

- **[controller-production.ino](controller/controller-production/controller-production.ino)** - Production controller v1.3 (field-ready)

### Reference Implementations (from Project Specification v2.0)

- **[audio_wav_test/audio_wav_test.ino](audio_wav_test/audio_wav_test.ino)** - Audio & transmission control reference (WAV playback, Morse, PTT timing, MQS configuration)
- **[battery_monitor_test/battery_monitor_test.ino](battery_monitor_test/battery_monitor_test.ino)** - Battery monitoring reference (voltage divider, 5-state system, debouncing, LED patterns)

### Version History

- **[CHANGELOG.md](CHANGELOG.md)** - Detailed change log and version history

### Archived Code (Phase 1 - Historical Reference Only)

- **[archived-files/](archived-files/)** - Phase 1 blocking implementations (not for production use)

### Project Guidelines

- **[.cursorrules](.cursorrules)** - AI-assisted development guidelines and project rules

---

## 🛠️ Development Roadmap

### Phase 1: Foundation ✅ COMPLETE

- [x] Morse code CW transmission (reference implementation)
- [x] WAV file audio playback (reference implementation)
- [x] Basic PTT control with proper timing
- [x] Hardware specifications documented
- [x] Software requirements documented

### Phase 2: Production System ✅ COMPLETE

**Core Implementation:**
- [x] Random WAV file selection with automatic SD scanning
- [x] Intelligent playback (loop if <30s, play to completion if >=30s)
- [x] Battery monitoring with 5-state system
- [x] PTT timeout watchdog (90s max)
- [x] Morse code station ID (FCC compliant)
- [x] LED status indicators (Morse battery patterns)
- [x] Dual logging (Serial Monitor + SD card)
- [x] Reset cause detection
- [x] Audio memory optimization
- [x] SD card contention handling
- [x] WAV file validation

**Testing Complete:**
- [x] No blocking operations during WAV playback
- [x] PTT timing accurate (1000ms pre-roll, 500ms tail)
- [x] Battery protection triggers correctly
- [x] Stable operation through multiple transmission cycles
- [x] Field-tested with RF transmission

### Phase 3: Advanced Features (Future)

- [ ] Deep sleep power management
- [ ] Duty cycle tracking and enforcement
- [ ] Randomized audio file selection
- [ ] Configuration system (CONFIG.TXT on SD card)
- [ ] DTMF remote control (optional hardware)
- [ ] VOX activation mode (optional hardware)

---

## 🎯 Use Cases

- **Amateur Radio Foxhunts** - Competitive direction-finding events
- **Training Exercises** - Practice radio direction-finding skills
- **Emergency Preparedness** - Search and rescue training
- **Educational Demos** - Teaching RF propagation and antenna theory
- **Field Day Activities** - Fun side activities at ham radio events

---

## 📜 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

**Copyright © 2026 Chris L White, WX7V**  
*Hardware specifications created with assistance from Gemini 3*  
*Code development with assistance from Claude Sonnet 4.5*

### Radio Operation Notice

This software is designed for **amateur radio operations only**. Users are responsible for:

- Compliance with FCC Part 97 rules (US) or equivalent regulations
- Proper station identification (every 10 minutes minimum)
- Licensed operation on authorized frequencies
- Adherence to duty cycle and power limitations

**The authors assume no liability for improper or unlicensed use of this software.**

---

## 🏆 Acknowledgments

- **PJRC** - For the excellent Teensy platform and Audio Library
- **Amateur Radio Community** - For foxhunting traditions and techniques
- **Google DeepMind** - For Gemini 3 AI assistance in hardware design
- **Anthropic** - For Claude Sonnet 4.5 AI assistance in code development

---

## 📞 Contact

**Chris L White, WX7V**

Project repository: [GitHub - teensy4.1 foxhunt controller](https://github.com/cdub89/teensy4.1-foxhunt-controller)

---

**73 de WX7V** 📡🦊

*Good luck and happy hunting!*
