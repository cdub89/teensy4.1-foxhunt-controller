# LiFePO4 Battery Monitor

## Overview

Simplified battery monitoring script designed specifically for **Bioenno 4.5Ah LiFePO4** batteries (4S configuration). Unlike the complex LiPo monitoring script, this version focuses on **runtime performance tracking** and data collection rather than aggressive safety interlocks.

**Key Philosophy:** LiFePO4 batteries are inherently safe and can be deeply discharged without damage. This script prioritizes data collection over protection.

---

## Hardware Requirements

- **Teensy 4.1** microcontroller
- **Bioenno 4.5Ah LiFePO4 battery** (12.8V nominal, 4S)
- **Voltage divider circuit:**
  - R1: 10kΩ resistor (battery+ to Pin A9)
  - R2: 2.0kΩ resistor (Pin A9 to GND)
  - Optional: 100nF ceramic capacitor (parallel with R2 for noise filtering)
- **SD card** (FAT32 formatted, optional but recommended)
- **Built-in LED** (Pin 13 for Morse code status)

### Wiring Diagram

```
Battery(+) ──┬── 10kΩ ── Pin A9 (Teensy) ── 2.0kΩ ── GND
             │                                │
             │                              100nF (optional)
             │                                │
             └────────────────────────────────┴──────
```

**Voltage Divider Ratio:** 1/6 (scales 14.6V → 2.43V at Pin A9)

---

## LiFePO4 vs LiPo Differences

| Characteristic | LiPo (4S) | LiFePO4 (4S) |
|---------------|-----------|--------------|
| **Nominal Voltage** | 14.8V (3.7V/cell) | **12.8V (3.2V/cell)** |
| **Fully Charged** | 16.8V (4.2V/cell) | **14.6V (3.65V/cell)** |
| **Safe Minimum** | 12.8V (3.2V/cell) | **12.0V (3.0V/cell)** |
| **Discharge Curve** | Gradual slope | **Flat plateau** |
| **Safety** | Fire/explosion risk | **Extremely safe** |
| **Deep Discharge Damage** | ✅ Yes - permanent | **❌ No - tolerant** |
| **Cycle Life** | 300-500 cycles | **2000-3000+ cycles** |

**Critical Difference:** LiFePO4 maintains ~13.0-13.5V for 80% of discharge, then drops rapidly in the final 10-15%. Voltage provides less warning than LiPo.

---

## Voltage Thresholds

```cpp
STATE_FULL:      >= 14.6V  (3.65V/cell) - Freshly charged
STATE_GOOD:      >= 13.0V  (3.25V/cell) - Healthy (80% of runtime here!)
STATE_LOW:       >= 12.4V  (3.10V/cell) - Getting low, monitor closely
STATE_VERY_LOW:  >= 12.0V  (3.00V/cell) - Nearing empty
STATE_SHUTDOWN:  >= 11.6V  (2.90V/cell) - Shutdown recommended
STATE_CRITICAL:  < 11.6V   (2.50V/cell) - Emergency (BMS should disconnect)
```

---

## Features

### ✅ Simple Logic
- No complex hysteresis or debouncing (LiFePO4 is stable)
- Threshold-based state detection
- No time-based capacity estimates
- Graceful degradation (continues without SD card if needed)

### 📊 Runtime Performance Tracking
- Voltage readings every 500ms
- Serial output every 5 seconds
- Statistics summary every 5 minutes
- Tracks time spent in each battery state
- Min/max/average voltage statistics

### 💾 Comprehensive Logging
- All output mirrored to SD card (`LIFEPO4.LOG`)
- Timestamped entries (requires RTC set)
- State transition logs with durations
- Periodic statistics snapshots

### 🔦 Morse Code Status LED
Visual battery status indication via built-in LED (Pin 13):

| State | Pattern | Morse |
|-------|---------|-------|
| **FULL** | `... ..-` | SF (Status Full) |
| **GOOD** | `... --.` | SG (Status Good) |
| **LOW** | `--- .-..` | OL (Oh Low) |
| **VERY_LOW** | `--- ...-` | OV (Oh Very low) |
| **SHUTDOWN** | `... --- ... ...` | SOSS (SOS Shutdown) |
| **CRITICAL** | `... --- ... -.-.` | SOSC (SOS Critical) |

Speed: ~6 WPM (200ms dot duration) for visual clarity

---

## Installation

### Arduino IDE Setup

1. **Install Teensyduino** (if not already installed)
   - Download from: https://www.pjrc.com/teensy/td_download.html
   - Follow installation instructions

2. **Configure Arduino IDE:**
   - **Board:** Tools → Board → Teensy 4.1
   - **USB Type:** Tools → USB Type → Serial
   - **CPU Speed:** Tools → CPU Speed → **24 MHz** (battery optimization)
   - **Optimize:** Tools → Optimize → Smallest Code
   - **Port:** Select Teensy port (NOT "Serial Ports")

3. **Set RTC (Important for timestamps):**
   - Connect Teensy 4.1
   - Go to Tools → Set Time
   - Click OK to sync RTC with computer time

4. **Upload Script:**
   - Open `lifepo4_monitor.ino`
   - Click "Upload" button (or Ctrl+U)
   - Wait for "Reboot OK" message

---

## Usage

### Serial Monitor

1. Open Arduino IDE Serial Monitor (Tools → Serial Monitor or Ctrl+Shift+M)
2. Set baud rate to **115200**
3. Monitor real-time voltage readings and statistics

### Example Output

```
========================================
WX7V LiFePO4 Battery Monitor
Version: 1.0 (2026-02-15)
========================================

RTC synchronized: Feb 15, 2026 14:30:00

Initializing SD card... SUCCESS

Initial battery voltage: 14.52V (FULL)

Timestamp           | Runtime    | ADC  | Pin A9 | Battery | V/Cell | Status
--------------------+------------+------+--------+---------+--------+------------------
2026-02-15 14:30:05 | 00h:00m:05s | 748 | 2.41V | 14.48V | 3.62V | [FULL]
2026-02-15 14:30:10 | 00h:00m:10s | 745 | 2.40V | 14.42V | 3.61V | [FULL]
2026-02-15 14:30:15 | 00h:00m:15s | 742 | 2.39V | 14.36V | 3.59V | [GOOD]

========================================
2026-02-15 14:30:15 STATE TRANSITION
========================================
Previous state: FULL (00h:00m:10s)
New state: GOOD at 14.36V
Total runtime: 00h:00m:15s
========================================

[... continued logging ...]
```

### Statistics Output (Every 5 Minutes)

```
========================================
2026-02-15 14:35:00 RUNTIME STATISTICS
========================================
Total runtime: 00h:05m:00s

Voltage - Min: 14.18V  Max: 14.52V  Avg: 14.35V

Time in each state:
  FULL:      00h:00m:10s
  GOOD:      00h:04m:50s

========================================
```

---

## SD Card Logging

All Serial Monitor output is automatically mirrored to `LIFEPO4.LOG` on the SD card.

### SD Card Requirements
- **Format:** FAT32
- **Card type:** microSD (Class 10 recommended)
- **Location:** Built-in SD card slot on Teensy 4.1 (underside of board)

### Reading Log Files
1. Remove SD card from Teensy
2. Insert into computer
3. Open `LIFEPO4.LOG` with any text editor
4. Log format matches Serial Monitor output exactly

---

## Calibration

The default voltage divider ratio is **6.0** (theoretical). For best accuracy:

1. Connect fully charged LiFePO4 battery (14.6V)
2. Measure actual battery voltage with multimeter
3. Read displayed voltage in Serial Monitor
4. Calculate correction:
   ```
   actual_ratio = multimeter_reading / displayed_reading
   ```
5. Update `VOLTAGE_DIVIDER_RATIO` constant in code:
   ```cpp
   const float VOLTAGE_DIVIDER_RATIO = 6.0;  // Adjust this value
   ```
6. Re-upload script and verify accuracy

**Example:**
- Multimeter reads: 14.60V
- Serial Monitor shows: 14.48V
- New ratio: 6.0 × (14.60 / 14.48) = 6.05

---

## Data Collection Goals

This script is optimized for characterizing LiFePO4 runtime performance:

### 1. **Discharge Curve Analysis**
Monitor how voltage changes over time under load. Expect:
- Flat plateau at ~13.0-13.5V for 80% of capacity
- Rapid drop in final 10-15%

### 2. **Capacity Verification**
Compare actual runtime to rated 4.5Ah capacity:
```
Theoretical runtime = 4.5Ah / average_current
With 400mA TX average (8.3% duty cycle): ~11 hours
With 30mA idle only: ~150 hours
```

### 3. **State Duration Tracking**
Statistics show how long battery stays in each state:
- **GOOD state** should dominate (80%+ of runtime)
- **LOW → VERY_LOW transition** indicates rapid discharge (warning!)

### 4. **Voltage Under Load**
Compare voltage readings:
- During idle periods
- During radio transmission (PTT active)
- Voltage sag indicates internal resistance

---

## Differences from LiPo Monitor

| Feature | LiPo Monitor | LiFePO4 Monitor |
|---------|--------------|-----------------|
| **State Machine** | Complex hysteresis + debouncing | Simple threshold-based |
| **Safety Focus** | Aggressive (prevent damage) | Relaxed (no damage risk) |
| **Thresholds** | 5 states (14.5V → 13.2V) | 6 states (14.6V → 11.6V) |
| **Primary Goal** | Battery protection | Data collection |
| **Time-based Tracking** | Optional | Not implemented |
| **Logging Interval** | Configurable | Fixed (5 seconds) |

---

## Integration with Fox Controller

This script is standalone for testing. To integrate with main fox controller:

1. **Copy voltage reading function:**
   ```cpp
   float readBatteryVoltage() { ... }
   ```

2. **Copy state determination:**
   ```cpp
   BatteryState determineState(float voltage) { ... }
   ```

3. **Add to main loop:**
   ```cpp
   void loop() {
     // Existing fox logic...
     
     // Check battery periodically
     if (currentTime - lastBatteryCheck >= 5000) {
       float voltage = readBatteryVoltage();
       BatteryState state = determineState(voltage);
       
       if (state == STATE_SHUTDOWN || state == STATE_CRITICAL) {
         // Optional: Disable transmissions
         // LiFePO4 is safe, so not critical
       }
       
       lastBatteryCheck = currentTime;
     }
   }
   ```

---

## Troubleshooting

### ❌ "SD card failed"
- **Cause:** SD card not inserted, wrong format, or hardware issue
- **Fix:** Format card as FAT32, re-insert, or continue without logging

### ❌ "RTC not set"
- **Cause:** Teensy RTC never initialized
- **Fix:** Arduino IDE → Tools → Set Time (requires connection to computer)

### ❌ Voltage readings seem off
- **Cause:** Incorrect divider ratio or wiring
- **Fix:** Verify with multimeter, recalibrate `VOLTAGE_DIVIDER_RATIO`

### ❌ LED not blinking
- **Cause:** LED pattern timing issue (rare)
- **Fix:** Check serial output - if working, ignore LED (not critical)

### ❌ Voltage fluctuates wildly
- **Cause:** Missing 100nF noise filter capacitor
- **Fix:** Add capacitor across R2 (parallel with 2.0kΩ resistor)

---

## Technical Specifications

| Parameter | Value |
|-----------|-------|
| **ADC Resolution** | 10-bit (0-1023) |
| **ADC Reference** | 3.3V |
| **Sample Rate** | 500ms (2 Hz) |
| **Serial Baud Rate** | 115200 |
| **Log Filename** | `LIFEPO4.LOG` |
| **Current Draw** | ~30-40mA @ 24MHz CPU |
| **Memory Usage** | <10% FLASH, <5% RAM |

---

## Safety Notes

⚠️ **While LiFePO4 is very safe:**
- Do NOT short circuit terminals
- Monitor first discharge to verify BMS cutoff voltage
- External BMS should disconnect at ~10.0V (2.5V/cell)
- Never force charge above 14.6V (3.65V/cell)

✅ **Unlike LiPo, LiFePO4:**
- Will NOT catch fire if punctured
- Can be stored at any charge level
- Tolerates deep discharge without permanent damage
- Safe to use in enclosed spaces (ammo can)

---

## Version History

- **v1.0** (2026-02-15) - Initial release
  - Simple threshold-based monitoring
  - Runtime statistics tracking
  - SD card logging with timestamps
  - Morse code LED status patterns
  - Optimized for LiFePO4 discharge characteristics

---

## Credits

**WX7V Amateur Radio**
- Part of the Foxhunt Controller project
- Companion to Hardware_Reference.md and Software_Reference.md

---

## License

Open source for amateur radio use. Credit appreciated but not required.
