# WX7V Foxhunt Controller - Hardware Reference

## Project Overview

Dual-band radio "fox" (hidden transmitter) using a **Teensy 4.1** microcontroller and **Baofeng UV-5R** radio for amateur radio foxhunt events.

**Key Specifications:**
- **Controller:** Teensy 4.1 (Cortex-M7 @ 600MHz)
- **Radio:** Baofeng UV-5R (Kenwood 2-pin interface, LOW power: 1W)
- **Power Source:** Bioenno 4.5Ah 12.8V 4S LiFePO4
- **Safety:** 5A inline fuse on main XT60 battery lead
- **Housing:** Forest green ammo can with external antenna mount

---

## Power Distribution System

**Source:** 12.8V (4S) LiFePO4 Battery via 5A Master Fuse

| Module | Input | Output | Destination |
|--------|-------|--------|-------------|
| **Buck Converter A** | 12.8V | **8.0V** | Baofeng UV-5R (via BL-5 Battery Eliminator) |
| **Buck Converter B** | 12.8V | **5.0V** | Teensy 4.1 VIN Pin |

**Buck Converter A Voltage Selection:**  
- **8.0V** - REQUIRED for LiFePO4 (12.8V input needs 4.8V dropout headroom)
- **ACTION REQUIRED:** Use a multimeter during initial setup to test voltage at the radio's power connector. Set to 8.0V for stable operation.

**⚠️ CRITICAL:** 12V output setting provides insufficient dropout margin (0.8V). Always use 8.0V output setting with LiFePO4 batteries.

**PRO-TIP:** Adjust and lock buck converter potentiometers using a multimeter *before* connecting Teensy or Radio to prevent over-voltage damage.

---

## Pinout & Wiring Map

| Component | Teensy 4.1 Pin | Notes |
|-----------|---------------|-------|
| **PTT Control** | Pin 2 | Digital Output → 1kΩ Resistor → 2N2222 Base |
| **Audio Out** | Pin 12 | PWM/MQS → 1kΩ Resistor → 10µF Cap → 10kΩ Pot → Radio Mic In |
| **System LED** | Pin 13 | Built-in LED (Heartbeat/TX indicator) |
| **Battery Monitor** | Pin A9 | Analog Input via Voltage Divider (R1=10kΩ, R2=2.0kΩ) |
| **Power In** | VIN | 5.0V from Buck Converter B |
| **Ground** | GND | Common Ground (Star Configuration) |

---

## RF Suppression & Stability (CRITICAL)

**Essential for field reliability.** These components prevent voltage spikes, RF interference, and microcontroller reboots during 1W transmission:

### Required Components

**0.1µF Ceramic Capacitor (104):**
- Solder directly across Buck Converter A output terminals (+) and (-)
- Purpose: Filters high-frequency noise and RF coupling

**Snap-On Ferrite Cores:**
- Two (2) ferrites on DC power line between Buck A and radio
- One (1) ferrite on power lines feeding Teensy 4.1
- Purpose: Prevents radio's RF output from coupling back into power rails

**Field Testing Note:** These components were discovered to be essential during actual 1W transmission testing. Without them, the Teensy may reboot or exhibit erratic behavior.

---

## Grounding Architecture (Star Ground)

**Single-Point Ground:** All ground connections must meet at ONE common point to minimize RF feedback and ground loops.

**Connection Order:**
1. Battery negative terminal → Common ground point
2. Buck Converter A GND → Common ground
3. Buck Converter B GND → Common ground
4. Teensy 4.1 GND → Common ground
5. Radio GND (white wire from pigtail) → Common ground

**Component Placement Guidelines:**
- Keep high-current paths (battery to buck converters) short with heavy gauge wire
- Route audio paths away from RF components and power lines
- Keep voltage divider wiring short to minimize noise pickup
- Consider shielded wire for battery monitor if mounted near transmitter

---

## PTT Keying Circuit

**Teensy Pin:** Digital Pin 2  
**Driver:** 2N2222 NPN Transistor  

**Signal Path:**
1. Teensy Pin 2 → 1kΩ Resistor → 2N2222 Base (middle pin)
2. 2N2222 Emitter (pin 1) → Common GND
3. 2N2222 Collector (pin 3) → Radio PTT (black wire)

**Purpose:** Provides electrical isolation and sufficient current drive for radio PTT circuit.

---

## Audio Filter & Level Control

**Teensy Pin:** Digital Pin 12 (MQS Output)

**Components:**
- 1kΩ Resistor (low-pass filter)
- 10µF Electrolytic Capacitor (DC blocking)
- 10kΩ Linear Potentiometer (volume control)

**Signal Path:**
1. Teensy Pin 12 → 1kΩ Resistor
2. 1kΩ Resistor → 10µF Capacitor (+)
3. 10µF Capacitor (-) → Potentiometer (left pin)
4. Potentiometer (center/wiper) → Radio Mic In (red wire)
5. Potentiometer (right pin) → Common GND

**Why This Circuit:**
- **1kΩ Resistor:** Creates low-pass filter with capacitor to smooth MQS PWM output and prevent radio distortion
- **10µF Capacitor:** Blocks DC bias from Teensy, passes audio signal only
- **10kΩ Potentiometer:** Volume control (Baofeng mic inputs are very sensitive)

### Optional: Audio Isolation Transformer

If you hear hum or buzz during transmission (ground loop noise):
- Install 600:600Ω audio isolation transformer between potentiometer and radio mic input
- Physically separates Teensy ground from radio ground

---

## Battery Voltage Monitor Circuit

**Critical Safety Feature:** Provides runtime tracking and protects battery cycle life through conservative discharge thresholds

### Circuit Design

**Teensy Pin:** Analog Pin A9 (Pin 23)

**Voltage Divider:**
- **R1:** 10kΩ resistor (between Battery+ and Pin A9)
- **R2:** 2.0kΩ resistor (between Pin A9 and Ground)
- **C1:** 100nF ceramic capacitor (parallel with R2, noise filtering - optional but recommended)

**Connection Points:**
1. Battery positive terminal → 10kΩ resistor (R1)
2. 10kΩ resistor (R1) → Teensy Pin A9
3. Pin A9 → 2.0kΩ resistor (R2)
4. 2.0kΩ resistor (R2) → Common Ground
5. 100nF capacitor (C1) → Parallel across 2.0kΩ resistor

### Voltage Scaling

**Divider Ratio:** R2 / (R1 + R2) = 2.0kΩ / 12.0kΩ = 0.1667 (or 1/6)

**Formula:** \( V_{out} = V_{in} \times \frac{2.0}{10 + 2.0} = V_{in} \times \frac{1}{6} \)

**Voltage Mapping Table (LiFePO4):**

| Battery Voltage | Cell Voltage | Pin A9 Voltage | ADC Reading (10-bit) | Status |
|----------------|--------------|----------------|---------------------|---------|
| 14.6V | 3.65V | 2.43V | 750 | Fully Charged |
| 13.0V | 3.25V | 2.17V | 670 | Nominal |
| 12.4V | 3.10V | 2.07V | 639 | ⚠️ Soft Warning |
| 12.0V | 3.00V | 2.00V | 617 | ⚠️ Hard Warning |
| 11.6V | 2.90V | 1.93V | 596 | 🛑 Hard Shutdown |

**Reverse Calculation (ADC to Battery Voltage):**

\[V_{battery} = \frac{ADC_{reading} \times 3.3V}{1023} \times 6.0\]

### Safety Thresholds (LiFePO4)

**Soft Warning:** < 12.4V (3.1V per cell)
- Action: Play "LOW BATTERY" warning via WAV file or Morse code
- Frequency: Alert once every 5 transmission cycles
- Behavior: Continue normal operation but notify operators
- LED indication: Rapid pulse pattern (200ms on/off)

**Hard Shutdown:** < 11.6V (2.9V per cell) - Conservative
- Action: Immediate and permanent PTT disable
- Set PTT pin to INPUT (high-impedance) to prevent accidental keying
- Flash LED in SOS pattern (...---...) continuously
- Enter deep sleep with periodic wake for SOS LED
- No recovery without power cycle

**Note:** LiFePO4 can be safely discharged deeper than other lithium chemistries without damage, but conservative thresholds preserve battery cycle life and ensure reliable operation.

### Component Selection

- Use ±1% tolerance resistors for accurate voltage measurement
- 1/4W resistors are sufficient (power dissipation < 0.03W)
- Ceramic capacitor for noise filtering (X7R or C0G dielectric)

### Calibration Procedure

1. Assemble voltage divider circuit
2. Connect fully charged battery (14.6V for LiFePO4)
3. Measure actual voltage at Pin A9 with multimeter
4. Calculate ratio: `actual_ratio = 14.6V / measured_voltage`
5. Update `VOLTAGE_DIVIDER_RATIO` constant in code
6. Test at 13.0V and 12.4V to verify accuracy

### Wiring Notes

- Keep voltage divider wiring short to minimize noise pickup
- Route away from high-current paths (radio power, buck converters)
- Consider using shielded wire if mounted near RF transmitter
- **Double-check polarity** - Reversed polarity will destroy Teensy ADC!

---

## Baofeng/Kenwood Pigtail Color Guide

When you cut your Kenwood 2-pin cable, you'll likely see these four colors. **Always verify with a multimeter!**

- **White:** Common Ground (connects to Teensy GND)
- **Black:** PTT (connects to 2N2222 collector)
- **Red:** Mic Audio In (connects to potentiometer output)
- **Green:** Speaker Audio Out (optional: connect to Teensy for "listening" features)

---

## Physical Build & Component Placement

### Housing

**Ammo Can:** Forest green, with external antenna mount

**Component Layout:**
- Keep high-current paths short and use heavy gauge wire
- Route audio paths away from RF components and power lines
- Secure all connections with heat shrink or electrical tape
- Label wires for future troubleshooting

### Wire Management

- Double-check polarity on all connections - reversed polarity damages components
- Use color-coded wire where possible (red=positive, black=ground)
- Dress wires neatly to prevent chafing and shorts
- Leave service loops for future modifications

---

## Pre-Flight Hardware Checklist

Before field deployment, verify all critical systems:

- [ ] Buck Converter A voltage verified with multimeter (radio powered on, no transmission)
- [ ] Buck Converter B outputs exactly 5.0V ±0.1V
- [ ] All ferrites installed and secured (2 on radio power, 1 on Teensy power)
- [ ] 0.1µF ceramic capacitor soldered across Buck A output
- [ ] Battery voltage monitor reads correctly (compare A9 reading to multimeter)
- [ ] Battery monitor calibration complete (tested at 16.8V, 14.8V, 13.6V)
- [ ] PTT circuit tested (transistor switches cleanly)
- [ ] Audio level adjusted (clear but not distorted on receive radio)
- [ ] Star ground verified with continuity tester
- [ ] 5A fuse installed in main battery lead
- [ ] Radio set to LOW power (1W) via menu
- [ ] Antenna securely mounted and connected
- [ ] Ammo can closes properly with all components inside
- [ ] SD card installed with audio files (if using WAV playback)

---

## Current Consumption & Battery Life

**Radio Power Setting:** Must be set to **LOW (1W)** for optimal battery life and reduced RF interference. Set via radio menu: **Menu → 2 (TXP) → LOW**

| Mode | Current Draw | Notes |
|------|-------------|-------|
| **Idle (CPU @ 24MHz)** | ~20-30mA | Teensy 4.1 underclocked |
| **Idle (CPU @ 600MHz)** | ~100mA | Default speed (not recommended) |
| **Transmitting (Morse)** | ~40-50mA | Teensy + audio @ 24MHz |
| **Transmitting (Audio)** | ~60-80mA | Audio library @ 24MHz |
| **Deep Sleep** | ~5mA | With Snooze library |
| **Radio TX (1W LOW)** | ~350-400mA | From Buck A (8-12V supply) |
| **Radio TX (4W HIGH)** | ~1.5A | NOT RECOMMENDED |

**Battery Life Estimate (LiFePO4 4.5Ah):**
- 60-second interval, 5-second transmission: **~18-20 hours**
- 30-second interval, 10-second transmission: ~10-12 hours

**Power Savings:**
- CPU underclocking: 70-80% reduction (100mA → 20-30mA)
- Radio LOW power: 75% reduction (1.5A → 0.35-0.4A)

---

## Arduino IDE Configuration

### Tools Menu Settings:
- **Board:** "Teensy 4.1"
- **USB Type:** "Serial"
- **CPU Speed:** "24 MHz" (for battery life optimization)
- **Optimize:** "Smallest Code" or "Faster" (depending on requirements)
- **Port:** Select from "Teensy Ports" section (NOT "Serial Ports")

### Compilation Notes:
- All `.ino` files must be in their own folder with matching name
- Teensy-specific libraries are available through Teensyduino
- Use "Verify" button to check compilation before uploading
- Use "Upload" button to compile and flash to Teensy 4.1

### Memory Usage Reporting:
After compilation, Arduino IDE displays Teensy 4.1 memory usage:
```
FLASH: code:XXXXX, data:XXXX, headers:8320   free for files:8101892
 RAM1: variables:XXXX, code:XXXXX, padding:XXXXX   free for local variables:486688
 RAM2: variables:XXXXX  free for malloc/new:511872
```
- **FLASH:** Total ~8MB available for code and files (SD card storage uses remaining space)
- **RAM1:** 512KB total (DTCM + OCRAM) for variables and code
- **RAM2:** 524KB total (OCRAM2) for dynamic memory allocation

---

## Troubleshooting Guide

### Teensy Reboots During Transmission

**Cause:** RF coupling into power rails  
**Fix:** Verify all RF suppression components installed (ferrites, ceramic cap)

### Distorted Audio

**Cause:** Audio level too high or missing low-pass filter  
**Fix:** Adjust potentiometer or verify 1kΩ resistor is installed

### Ground Loop Hum

**Cause:** Multiple ground paths creating loop  
**Fix:** Verify star ground configuration, consider audio isolation transformer

### Battery Monitor Inaccurate

**Cause:** Wrong divider ratio or high-impedance wiring  
**Fix:** Re-calibrate with known voltage, shorten wiring, verify resistor values

### Radio Won't Key

**Cause:** 2N2222 transistor wired incorrectly or damaged  
**Fix:** Verify pinout (E-B-C), test with multimeter, check 1kΩ base resistor

---

**Hardware specification complete. See Software_Reference.md for programming requirements and implementation details.**
