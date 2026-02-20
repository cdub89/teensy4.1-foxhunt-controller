# Changelog

All notable changes to the WX7V Foxhunt Controller project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.2.0-production] - 2026-02-20

### Fixed - Critical Stability Issues
- **CRITICAL FIX:** SD card contention crash during WAV playback
  - Root cause: Audio Library and logging system accessing SD card simultaneously
  - Solution: Temporarily disable SD logging during WAV playback (Serial Monitor continues)
  - Impact: Eliminates IPP Reset crashes, system now stable through multiple transmission cycles
- Added WAV file validation before playback attempts (file size check, open test)
- Improved error handling throughout audio playback routines

### Added - Diagnostics & Monitoring
- Reset cause detection for Teensy 4.x (SRC_SRSR register)
  - Reports why Teensy rebooted at startup (Power-On, Watchdog, IPP Reset, etc.)
  - Critical for troubleshooting field issues
- Audio memory usage monitoring
  - Reports current usage during playback
  - Peak usage tracking (typically 2-5 blocks)
- Enhanced logging with SD card status messages

### Changed - Audio System
- Increased audio memory from 16 to 40 blocks for stability margin
  - Peak usage observed: 2-5 blocks (40 provides 8x safety margin)
  - Prevents potential memory exhaustion during playback
- Simplified progress logging during WAV playback (removed memory checks from loop)

### Changed - Documentation
- Updated header comments with critical technical notes
- Added SD card contention section to documentation
- Clarified audio memory allocation requirements
- Updated version history with v1.2 changes

### Testing Results
- ✅ Stable operation through multiple complete transmission cycles
- ✅ Random WAV file selection working correctly
- ✅ No reboots or crashes during normal operation
- ✅ Audio playback with looping for short files (<30s)
- ✅ Clean transmission sequence: PTT → Morse → WAV → ID → PTT Off
- ✅ Field-ready for deployment

## [1.1.0-production] - 2026-02-20

### Added - Random WAV Selection
- Automatic SD card scanning for all WAV files at startup
- Random file selection per transmission cycle
- Intelligent playback: loop if <30s, play to completion if >=30s
- Supports up to 20 WAV files on SD card
- No manual filename configuration required

### Changed - Playback Logic
- Dynamic WAV file duration detection
- Automatic looping for short files to meet 30s minimum
- Full playback for longer files (no artificial cutoff)
- Enhanced logging to show selected file per cycle

## [1.0.0-production] - 2026-02-20

### Added - Production Controller v1.0
- **controller-production.ino** - Integrated production controller combining proven components
  - Based on `audio_wav_test.ino` v1.6 (transmission logic)
  - Based on `battery_monitor_test.ino` v2.1 (battery safety)
  - Field mode default: 60s WAV playback + 240s idle interval (5-minute cycle)
  - Duration-controlled WAV playback with forced stop at time limit
  - 4-part transmission sequence: Pre-roll → Morse "V V V C" → WAV → Morse Station ID → Tail
  - FCC-compliant graceful shutdown (completes final ID before stopping)
  - Battery monitoring with 3-sample debouncing for noise immunity
  - Morse LED battery status patterns (SG, OL, OV, SOSS, SOSC)
  - Dual-output logging: Serial Monitor + SD card (FOXCTRL.LOG)
  - PTT timeout watchdog (90s safety limit)
  - Non-blocking architecture throughout

### Added - Documentation Simplification
- **Project Specification v2.0.md** - New single master reference document (103 lines)
  - Hardware specifications and pinout
  - Power architecture (direct battery-to-radio via BL-5 Eliminator)
  - Operational requirements and timing
  - References to working code implementations
- Consolidated all hardware/software specs into one authoritative document
- Working code serves as self-documenting implementation reference

### Changed - Power Architecture
- **BREAKING:** Changed from dual buck converter to single buck + direct battery feed
  - Radio: 12.0V - 13.8V direct from battery to BL-5 Eliminator
  - Teensy: 5.0V via single buck converter
  - Eliminates secondary buck converter complexity
  - Matches field-tested hardware configuration
- Updated RF suppression: 2x ferrites on radio power, 1x on Teensy power
- Single 0.1µF ceramic capacitor on buck converter output

### Changed - Audio Architecture
- Fixed audio wiring diagram throughout documentation
- Correct signal path: Pin 12 → 10µF Cap → 1kΩ Resistor → Potentiometer → Radio Mic
- Audio Library mixer for WAV + Morse (no pin conflicts)
- Mono output to both MQS channels for balanced audio

### Changed - Battery Monitoring
- Simplified from hysteresis to threshold-based with debouncing
- 5 states: GOOD (≥13.0V), LOW (≥12.4V), VERY_LOW (≥12.0V), SHUTDOWN (≥11.6V), CRITICAL (<11.6V)
- Debouncing prevents false alarms during PTT voltage sag
- Conservative thresholds preserve LiFePO4 cycle life

### Archived - Legacy Documentation
- `Hardware_Reference.md` → `Hardware_Reference_ARCHIVED.md` (outdated dual-buck design, wrong CPU speed)
- `Software_Reference.md` → `Software_Reference_ARCHIVED.md` (1,773 lines superseded by working code)
- Total: 2,118 lines of theory replaced by 103 lines of spec + working code

### Archived - Proof-of-Concept Code
- `Morse Code Controller.mdc` → `Morse Code Controller_ARCHIVED.mdc` (blocking tone() approach)
- `Audio Controller Code.mdc` → `Audio Controller Code_ARCHIVED.mdc` (basic WAV skeleton)
- `audio_test/` → `audio_test_ARCHIVED/` (deprecated tone() method with pin conflicts)
- `lifepo4_monitor/` → `lifepo4_monitor_ARCHIVED/` (experimental characterization tool)
- `controller/controller-v3/` → `controller/controller-v3_ARCHIVED/` (superseded by production version)

### Technical Implementation - Production Controller
- Audio Library: AudioPlaySdWav + AudioSynthWaveformSine + AudioMixer4 + AudioOutputMQS
- Battery: 10kΩ + 2.0kΩ voltage divider (ratio 6.0) on Pin A9
- Morse code: 20 WPM using Audio Library sine wave generator
- WAV requirements: 16-bit PCM, 44.1kHz/22.05kHz, mono, metadata removed
- CPU speed requirement: 150 MHz minimum (critical for WAV playback)
- User configuration: WAV_FILENAME and CALLSIGN_ID constants

### Safety Features - Production Controller
- Battery voltage monitoring every 500ms with debouncing
- SHUTDOWN state: Completes current transmission (FCC ID required), then disables future TX
- CRITICAL state: Emergency PTT off, halt system
- PTT timeout watchdog: 90-second maximum per transmission
- Graceful degradation: System continues with battery warnings displayed

### Documentation Structure - Final
```
Active Documentation:
  - Project Specification v2.0.md (master reference)
  - controller-production.ino (production code)
  - audio_wav_test.ino (reference implementation)
  - battery_monitor_test.ino (reference implementation)
Archived:
  - 6 historical files/folders (2,267 lines superseded)
```

---

## Historical Releases

### Changed - LiFePO4 Battery Standardization
- **BREAKING:** Removed LiPo battery support - project now standardized on LiFePO4 batteries only
- Updated voltage thresholds throughout documentation:
  - Soft warning: 13.6V → 12.4V (3.1V per cell)
  - Hard shutdown: 12.8V → 11.6V (2.9V per cell conservative)
  - Nominal voltage: 14.8V → 13.0V
  - Fully charged: 16.8V → 14.6V
- Removed LiPo over-discharge damage warnings (not applicable to LiFePO4)
- Updated `README.md`, `Hardware_Reference.md`, and `Software_Reference.md`
- Buck Converter A now specified as 8.0V only (required for LiFePO4 headroom)
- Battery life estimates updated for 4.5Ah LiFePO4 capacity (~18-20 hours)
- Emphasized LiFePO4 safety advantages: no damage from deep discharge

**Rationale:** LiFePO4 provides superior safety characteristics for field deployment:
- No fire/explosion risk from over-discharge
- Stable voltage platform for consistent operation
- Longer cycle life (2000+ cycles vs 300-500 for LiPo)
- Better temperature performance

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
- **LiFePO4 4S battery** (Bioenno 4.5Ah recommended)
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
