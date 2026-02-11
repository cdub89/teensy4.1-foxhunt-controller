# Changelog

All notable changes to the WX7V Foxhunt Controller project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2026-02-10

### Added - Production Controller Release
- **FoxhuntController.ino** - Complete production-ready controller code
- Non-blocking state machine architecture with 7 distinct states
- Dual-threshold battery watchdog system:
  - Soft warning at 13.6V (3.4V per cell) - plays LOWBATT.WAV
  - Hard shutdown at 12.8V (3.2V per cell) - permanent PTT disable
  - Moving average filter (10 samples) for voltage stability
  - Hysteresis at 14.0V to prevent warning spam
- Watchdog timer integration (30-second timeout with callback)
- Morse code generator with ITU-standard timing
- Teensy Audio Library integration for WAV playback
- Automatic alternation between Morse and audio transmissions
- Emergency shutdown mode with SOS LED pattern
- Comprehensive serial debugging output
- Battery voltage monitoring on Pin A9
- PTT pre-roll (1000ms) and tail (500ms) timing
- Maximum transmission timeout (30 seconds)
- Status LED indicators (heartbeat, warning, transmit, SOS)

### Technical Implementation
- State machine states: STARTUP, IDLE, PTT_PREROLL, TX_MORSE, TX_AUDIO, PTT_TAIL, EMERGENCY_SHUTDOWN
- MorseGenerator class with character-by-character non-blocking generation
- Complete Morse code table (A-Z, 0-9, space)
- Audio files: FOXID.WAV (ID) and LOWBATT.WAV (battery warning)
- Configurable parameters: callsign, WPM, frequency, intervals, thresholds
- Battery voltage divider: 10kΩ/2.2kΩ (ratio 5.545)
- Audio filter circuit: 1kΩ series resistor for MQS smoothing

### Documentation
- **FoxhuntController_README.md** - Quick start guide and troubleshooting
- Updated **README.md** with v1.0.0 features and production status
- Updated **IMPLEMENTATION_PLAN.md** battery watchdog specifications
- Referenced **WX7V Foxhunt Controller Project Specification.md**
- Referenced **Teensy4 Fox Hardware Reference.md** for wiring

### Safety Features
- Emergency PTT release on watchdog timeout
- Battery protection with permanent PTT disable below 12.8V
- High-impedance PTT pin in emergency mode
- Multiple timeout protections
- SD card error handling (operates in Morse-only mode if SD fails)
- Graceful degradation on component failures

### Hardware Requirements
- Teensy 4.1 microcontroller
- Baofeng UV-5R radio (or compatible)
- 14.8V 4S LiPo battery (SOCOKIN 5200mAh)
- Voltage divider: 10kΩ + 2.2kΩ + 100nF capacitor
- Audio filter: 1kΩ resistor + 10µF capacitor + 10kΩ potentiometer
- PTT control: 2N2222 transistor + 1kΩ resistor
- Buck converters: 8.0V (radio) and 5.0V (Teensy)

### Library Dependencies
- Audio Library (Teensyduino)
- SD Library (Arduino core)
- Snooze Library (Teensy low power)
- Watchdog_t4 (Teensy 4.x watchdog)

## [0.2.0] - Phase 1 Reference Implementations

### Added
- **Morse Code Controller.mdc** - Blocking Morse code reference
- **Audio Controller Code.mdc** - Blocking audio playback reference
- **WX7V Foxhunt Controller Project Specification.md** - Complete hardware specs
- **Teensy4 Fox Hardware Reference.md** - Wiring diagrams and parts list
- **IMPLEMENTATION_PLAN.md** - Phased development roadmap
- **.cursorrules** - AI-assisted development guidelines

## [0.1.0] - Initial Repository Setup

### Added
- Project structure and repository initialization
- README.md with project overview
- LICENSE (MIT)
- .gitignore for Arduino/Teensy projects

---

## Upcoming Features

### [1.1.0] - Phase 3 (Planned)
- Deep sleep power management using Snooze library
- Duty cycle tracking and enforcement (20% maximum)
- Enhanced battery statistics logging
- Power consumption optimization

### [1.2.0] - Phase 4 (Planned)
- Randomized audio file selection
- Multiple taunt files support
- Hybrid Morse + audio transmission modes
- Battery status announcements

### [1.3.0] - Phase 4 Advanced (Planned)
- DTMF remote control (MT8870 decoder)
- VOX-activated silent mode
- Command set for remote operation

### [2.0.0] - Phase 5 (Planned)
- SD card configuration system (CONFIG.TXT)
- Comprehensive logging to SD card
- Serial command interface
- Field deployment tools
- RTC integration for timestamps

---

## Version Numbering

- **Major version (X.0.0)**: Breaking changes or major feature additions
- **Minor version (0.X.0)**: New features, backward compatible
- **Patch version (0.0.X)**: Bug fixes, documentation updates

## Links

- [Project Specification](WX7V%20Foxhunt%20Controller%20Project%20Specification.md)
- [Hardware Reference](Teensy4%20Fox%20Hardware%20Reference.md)
- [Implementation Plan](IMPLEMENTATION_PLAN.md)
- [Controller Quick Start](FoxhuntController_README.md)
