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

Two working reference implementations demonstrating core functionality:

**1. Morse Code Controller** (`Morse Code Controller.mdc`)
- Blocking implementation for CW identification
- Standard ITU Morse timing
- Configurable WPM and tone frequency
- Pin 2 PTT control, Pin 12 audio output
- 1000ms pre-roll, 500ms tail timing

**2. Audio Controller** (`Audio Controller Code.mdc`)
- Blocking implementation for WAV playback
- Teensy Audio Library integration
- SD card support (built-in Teensy 4.1 slot)
- MQS audio output on Pin 12
- Automatic playback completion detection

These reference implementations prove the concept and provide working code patterns for the production system.

### Phase 2: Production System (Next)

Convert blocking reference code to production-ready non-blocking architecture:

**Core Features:**
- State machine framework (non-blocking)
- Concurrent operations (LED, battery monitoring, timing)
- Watchdog timer protection (30s timeout)
- Dual-threshold battery watchdog:
  - Soft warning at 13.6V (3.4V per cell)
  - Hard shutdown at 12.8V (3.2V per cell)
- Alternating Morse/Audio transmission
- Comprehensive error handling

**Implementation Approach:**
1. Create state machine with states: IDLE, PTT_PREROLL, TRANSMITTING, PTT_TAIL
2. Replace all `delay()` calls with `millis()` timing
3. Implement concurrent LED heartbeat and battery monitoring
4. Add safety interlocks (watchdog, battery protection, PTT timeout)
5. Integrate battery watchdog system from specifications

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

- **Teensy 4.1** microcontroller (Cortex-M7 @ 600MHz, underclocked to 24MHz)
- **Baofeng UV-5R** radio (set to LOW power: 1W)
- **2N2222 NPN transistor** (PTT control)
- **14.8V 4S LiPo battery** (SOCOKIN 5200mAh recommended)
- **Dual buck converters:**
  - Buck A: 8.0V - 12.0V for radio (via battery eliminator)
  - Buck B: 5.0V for Teensy VIN
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
- 0.1µF ceramic capacitor (104) - soldered across Buck A output
- 3× snap-on ferrite cores:
  - Two (2) on radio power lines
  - One (1) on Teensy power lines

**Purpose:** Prevents RF coupling during 1W transmission (discovered during field testing)

### Complete Documentation

- **[Hardware_Reference.md](Hardware_Reference.md)** - Complete wiring diagrams, pinout, voltage divider calculations, grounding architecture, troubleshooting
- **[Software_Reference.md](Software_Reference.md)** - Programming requirements, state machine examples, battery protection code, timing specifications

---

## 📋 Pin Configuration

| Function | Teensy Pin | Connection |
|----------|-----------|------------|
| **PTT Control** | Pin 2 | Digital Output → 1kΩ → 2N2222 base |
| **Audio Out** | Pin 12 | MQS → 1kΩ → 10µF cap → 10kΩ pot → Radio mic |
| **Status LED** | Pin 13 | Built-in LED (heartbeat/TX/warning/SOS) |
| **Battery Monitor** | Pin A9 | Via voltage divider (10kΩ + 2.0kΩ + 100nF cap) |
| **Power In** | VIN | 5.0V from buck converter B |
| **Ground** | GND | Star ground configuration (all grounds meet at ONE point) |

---

## 🚀 Getting Started

### 1. Review Documentation

**Start here:**
1. **[Hardware_Reference.md](Hardware_Reference.md)** - Complete hardware specifications
2. **[Software_Reference.md](Software_Reference.md)** - Programming requirements and examples

**Reference implementations:**
- `Morse Code Controller.mdc` - Working Morse code example
- `Audio Controller Code.mdc` - Working audio playback example

### 2. Build Hardware

Follow wiring diagrams in `Hardware_Reference.md`:

1. **Power system** - Dual buck converters with RF suppression
2. **PTT circuit** - 2N2222 transistor with 1kΩ base resistor
3. **Audio circuit** - 1kΩ resistor + 10µF cap + 10kΩ pot
4. **Battery monitor** - Voltage divider on Pin A9
5. **Star ground** - All grounds meet at one common point

**Critical:** Install RF suppression components (ferrites + ceramic cap) or system may reboot during transmission.

### 3. Prepare SD Card

Format microSD card as FAT32 and add:

```
SD_CARD/
├── FOXID.WAV       # Your callsign identification (required)
├── LOWBATT.WAV     # Low battery warning (optional)
└── TAUNT01.WAV     # Optional taunt files
```

**Audio file specs:**
- Format: 16-bit PCM WAV
- Sample rate: 22.05kHz or 44.1kHz
- Mono or stereo
- Keep files < 10 seconds for reasonable duty cycle

### 4. Test Reference Implementations

**Start with simple blocking code:**

1. Upload `Morse Code Controller.mdc` to test PTT and Morse generation
2. Upload `Audio Controller Code.mdc` to test SD card and audio playback
3. Verify timing, audio quality, and battery voltage readings
4. Adjust potentiometer for clean audio (not distorted)

### 5. Calibrate Battery Monitor

**Critical for battery protection:**

1. Connect fully charged battery (16.8V)
2. Measure voltage at Pin A9 with multimeter
3. Calculate ratio: `actual_ratio = 16.8V / measured_voltage`
4. Update `VOLTAGE_DIVIDER_RATIO` constant in code
5. Test at 14.8V and 13.6V to verify accuracy

See `Software_Reference.md` for complete calibration procedure.

---

## 🔒 Safety Features

This is **safety-critical code** - a stuck PTT can drain batteries, overheat equipment, or cause interference.

### Mandatory Safety Systems

**1. Watchdog Timer**
- 30-second timeout with automatic PTT release
- Resets Teensy if code hangs
- Prevents stuck PTT condition

**2. Dual-Threshold Battery Watchdog**
- **Soft Warning (13.6V / 3.4V per cell):**
  - Plays "LOW BATTERY" warning
  - LED changes to rapid blink (200ms)
  - Continues operation (you have time to finish hunt)
  - Clears at 14.0V (hysteresis prevents spam)
  
- **Hard Shutdown (12.8V / 3.2V per cell):**
  - **IMMEDIATE** PTT disable (set to INPUT mode)
  - SOS LED pattern (...---...)
  - **PERMANENT** until power cycle
  - Protects LiPo from over-discharge damage

**3. PTT Timeout**
- Maximum 30 seconds per transmission
- Emergency PTT release on timeout
- Multiple fail-safe mechanisms

**4. Non-Blocking Architecture**
- Responsive to all safety checks
- Concurrent monitoring (battery, watchdog, timing)
- No `delay()` calls in production code

**Why 12.8V hard shutdown?** Below 3.2V per cell, LiPo batteries can be permanently damaged. This is a hard safety limit.

---

## 📊 Performance Characteristics

### Power Configuration

**CPU Speed:** 24MHz (underclocked from 600MHz)
- Reduces idle current from ~100mA to ~20-30mA
- Extends battery life by 70-80%
- Still provides ample processing power
- Set in Arduino IDE: Tools → CPU Speed → 24 MHz

**Radio Power:** LOW (1W)
- Reduces current from ~1.5A to ~0.35-0.4A (75% savings)
- Minimizes RF interference with Teensy
- Sufficient range for typical foxhunts (0.5-2 miles)
- Set via radio menu: Menu → 2 (TXP) → LOW

**Combined savings enable >24 hour continuous operation**

### Expected Performance

| Metric | Value | Notes |
|--------|-------|-------|
| **Battery Life** | >24 hours | 5200mAh pack @ 60s interval, 5s TX |
| **PTT Pre-roll** | 1000ms | Required for Baofeng squelch opening |
| **PTT Tail** | 500ms | After transmission ends |
| **Duty Cycle** | 8.3% | 5s TX / 60s interval (safe for continuous operation) |
| **Idle Current** | 20-30mA | @ 24MHz CPU, no transmission |
| **TX Current** | 40-80mA | Teensy only (radio on separate 8-12V rail) |
| **Deep Sleep** | ~5mA | With Snooze library (future implementation) |

### Battery Voltage Monitoring

| Battery Voltage | Cell Voltage | Pin A9 Voltage | Status |
|----------------|--------------|----------------|---------|
| 16.8V | 4.20V | 2.80V | Fully Charged |
| 14.8V | 3.70V | 2.47V | Nominal |
| 13.6V | 3.40V | 2.27V | ⚠️ Soft Warning |
| 12.8V | 3.20V | 2.13V | 🛑 Hard Shutdown |
| 12.0V | 3.00V | 2.00V | ⚠️ Critical Damage Risk |

---

## 📚 Documentation

### Primary References

- **[Hardware_Reference.md](Hardware_Reference.md)** - Complete hardware specs, wiring, troubleshooting
- **[Software_Reference.md](Software_Reference.md)** - Programming requirements, examples, best practices

### Reference Code (Phase 1)

- **[Morse Code Controller.mdc](Morse%20Code%20Controller.mdc)** - Simple CW transmission (blocking)
- **[Audio Controller Code.mdc](Audio%20Controller%20Code.mdc)** - WAV playback (blocking)

### Test Programs

- **[battery_monitor_test](battery_monitor_test/)** - Battery voltage monitoring with SD logging, Morse LED patterns, and false-alarm prevention (v1.6)
- **[audio_test](audio_test/)** - Audio playback testing

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

### Phase 2: Production System (In Progress)

**Core Implementation:**
- [ ] Non-blocking state machine architecture
- [ ] Concurrent operations (LED, battery monitoring)
- [ ] Watchdog timer with PTT protection
- [ ] Dual-threshold battery watchdog system
- [ ] SD card event logging (standard SD library)
- [ ] Alternating Morse/Audio transmission
- [ ] Comprehensive error handling

**Testing Requirements:**
- [ ] No blocking operations (loop() < 10ms)
- [ ] PTT timing accurate (1000ms pre-roll, 500ms tail)
- [ ] Battery protection triggers correctly
- [ ] Watchdog timer tested and verified
- [ ] 10+ transmission cycles stable

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
