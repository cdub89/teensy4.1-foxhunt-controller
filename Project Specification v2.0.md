# Master Wiring & Logic Specification v2.0: LiFePO4 Fox

## Project Overview

Dual-band radio "fox" (hidden transmitter) using a **Teensy 4.1** microcontroller and **Baofeng UV-5R** radio for amateur radio foxhunt events.   - **Housing:** Forest green plastic ammo can that can.  Option external antenna mount


## 1. Power & Runtime Overview

**Battery:** Bioenno 12V 4.5Ah LiFePO4  
**Cycle:** 60s Transmit / 240s Idle (5-minute total)

| Feature | Specification |
|---------|--------------|
| Radio Power | 12.0V - 13.8V DC (Direct from battery to BL-5) |
| Teensy Power | 5.0V DC (Via Buck Converter) |
| Avg. Current | ~164mA |
| Est. Runtime | ~24.5 Hours (Continuous) |

### RF & Stability Requirements

- **Ferrites:** 2x snap-on ferrites on the radio power lead; 1x on the Teensy power lead
- **Filtering:** 0.1µF ceramic capacitor across the Buck Converter output
- **Eliminator:** The BL-5 Eliminator handles the step-down to the radio; no secondary buck is needed for the radio rail

## 2. Controller Pinout & Interface

### A. PTT Keying Circuit

- **Teensy Pin:** Digital Pin 2
- **Driver:** 2N2222 NPN Transistor
- **Radio Connector:** Kenwood K Type (3.5mm + 2.5mm plugs)
- **Path:** 
  - Pin 2 → 1kΩ Resistor → Base (2N2222)
  - Emitter → 2.5mm Sleeve (System Ground)
  - Collector → 3.5mm Sleeve (PTT Sense)

### B. Audio Filter & Level Control

- **Teensy Pin:** Digital Pin 12 (MQS Output)
- **Components:** 10µF Electrolytic Capacitor, 1kΩ Resistor, 10kΩ Potentiometer
- **Radio Connector:** Kenwood K Type (3.5mm + 2.5mm plugs)
- **Signal Path:**
  1. Teensy Pin 12 → 10µF Cap (+)
  2. 10µF Cap (−) → 1kΩ Resistor → Potentiometer (High Leg)
  3. Potentiometer (Wiper) → 3.5mm Ring (Mic+)
  4. Potentiometer (Low Leg) → 2.5mm Sleeve (System Ground)

### C. Battery Voltage Monitor

- **Teensy Pin:** Analog Pin A9 (Pin 23)
- **Voltage Divider:** R_High = 10kΩ, R_Low = 2.0kΩ
- **Path:** Battery (+) → 10kΩ → (Junction to A9) → 2.0kΩ → GND

## 3. Operational Logic & Efficiency

**Always prioritize: Non-blocking code design, safety interlocks, and defensive programming.**

- **Radio Power:** Must be set to LOW (1W)
- **CPU Clock:** Teensy 4.1 should be underclocked to 150MHz (Tools > CPU Speed) to maximize battery life.   
**CRITICAL:** For WAV file playback using the Teensy Audio Library with MQS output, the Teensy **MUST** run at **150 MHz or higher**. Set this in Arduino IDE: `Tools → CPU Speed → 150 MHz` (or higher). Lower speeds will cause WAV playback to fail due to insufficient SD card read/decode performance.
- **LVP (Low Voltage Protection):** Software must stop transmission when battery voltage drops below 10.8V
- **Grounding:** Use a single-point "Star Ground" at the battery negative terminal/bus to prevent RF ground loops
- **Non Blocking Code:** Teensy 4.1 controller logic should always use non-blocking code.  Avoid the use of delay when possible.   **Use `millis()` for timing, NEVER `delay()` in production code.**

---

## 4. Implementation Examples

Working, tested reference code is available in the repository. These scripts are self-documenting with extensive inline comments and serve as the primary implementation reference:

### Audio & Transmission Control
**File:** `audio_wav_test/audio_wav_test.ino`

Complete implementation of:
- WAV file playback via Teensy Audio Library
- Morse code generation using AudioSynthWaveformSine
- 4-part transmission sequence (Pre-roll → Morse → WAV → ID → Tail)
- PTT timing (1000ms pre-roll, 500ms tail for Baofeng UV-5R)
- SD card file management
- MQS audio output configuration

### Battery Monitoring & Safety
**File:** `battery_monitor_test/battery_monitor_test.ino`

Complete implementation of:
- Voltage divider reading (10kΩ + 2.0kΩ) on Pin A9
- 5-state battery management (GOOD, LOW, VERY_LOW, SHUTDOWN, CRITICAL)
- Debouncing for noise immunity (3 consecutive readings)
- Runtime tracking and state transition analytics
- LED Morse code status indication (SG, OL, OV, SOS patterns)
- SD card dual-output logging (mirrors Serial Monitor)
- RTC timestamp integration

### Usage
Both scripts include:
- Version history and changelog
- Detailed configuration constants
- Hardware wiring diagrams in comments
- Step-by-step usage instructions
- Calibration procedures

Refer to these scripts as the authoritative implementation reference when building the final integrated controller.