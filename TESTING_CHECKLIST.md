# FoxhuntController.ino - Testing & Validation Checklist

## Pre-Upload Testing

### 1. Code Review
- [ ] Callsign configured correctly (line 35)
- [ ] TX_INTERVAL appropriate for your hunt (line 46)
- [ ] Morse WPM comfortable speed (line 41)
- [ ] Battery thresholds match your hardware (lines 56-57)
- [ ] Audio files named correctly (lines 66-67)
- [ ] All required libraries installed

### 2. Hardware Verification

**Power System:**
- [ ] Battery fully charged (16.8V measured with multimeter)
- [ ] Buck A output set to 8.0V (radio power)
- [ ] Buck B output set to 5.0V (Teensy VIN)
- [ ] 5A fuse installed in XT60 line
- [ ] All power connections secure

**PTT Circuit:**
- [ ] 2N2222 transistor installed correctly (flat side reference)
- [ ] 1kΩ resistor between Pin 2 and transistor base
- [ ] Collector to radio PTT (tip of 2.5mm plug)
- [ ] Emitter to ground
- [ ] Measure Pin 2 voltage: 0V idle, 3.3V when keying

**Audio Circuit (CRITICAL):**
- [ ] 1kΩ resistor installed between Pin 12 and capacitor
- [ ] 10µF capacitor polarity correct (+ toward resistor)
- [ ] 10kΩ potentiometer wired: left=cap, center=radio mic, right=ground
- [ ] Connection to radio mic input (ring of 2.5mm plug, red wire)
- [ ] Test with external speaker first

**Battery Monitor Circuit:**
- [ ] 10kΩ resistor (R1) between Battery+ and Pin A9
- [ ] 2.2kΩ resistor (R2) between Pin A9 and Ground
- [ ] 100nF capacitor parallel with R2 (noise filter)
- [ ] Measure voltage at Pin A9 with multimeter:
  - 16.8V battery → ~3.03V at A9
  - 14.8V battery → ~2.67V at A9
  - 13.6V battery → ~2.45V at A9

**SD Card:**
- [ ] Card formatted as FAT32 (not exFAT)
- [ ] FOXID.WAV present (8.3 filename format)
- [ ] LOWBATT.WAV present (optional)
- [ ] Audio files: 16-bit PCM WAV, 22.05kHz or 44.1kHz
- [ ] Card fully inserted into Teensy 4.1 slot

**Radio Setup:**
- [ ] Frequency set correctly
- [ ] CTCSS tone configured (if required)
- [ ] Squelch adjusted
- [ ] Antenna connected
- [ ] Battery eliminator cable connected (8V supply)

## Upload Process

### 1. Arduino IDE Configuration
- [ ] Board: Tools → Board → Teensy 4.1
- [ ] USB Type: Serial (for debugging)
- [ ] CPU Speed: 600 MHz
- [ ] Port: Correct COM port selected

### 2. Compilation
- [ ] Sketch compiles without errors
- [ ] Check compiler warnings (address if any)
- [ ] Verify memory usage reasonable (< 80% of flash/RAM)

### 3. Upload
- [ ] Upload successful
- [ ] Teensy reboot indicator blinks
- [ ] Wait for upload complete message

## Initial Power-On Testing

### 1. Serial Monitor Check (115200 baud)
Expected output:
```
============================================
WX7V Foxhunt Controller v1.0.0
Teensy 4.1 - Baofeng UV-5R
============================================

Watchdog timer configured (30s timeout)
Configuration:
  Callsign: [YOUR CALLSIGN]
  Morse WPM: 12
  TX Interval: 60 seconds
  Battery Soft Warning: 13.60V
  Battery Hard Shutdown: 12.80V

SD card initialized
Battery voltage: XX.XXV
System ready - starting transmission sequence

State: STARTUP
State: IDLE
```

**Checks:**
- [ ] Boot message appears
- [ ] SD card initialized message (or warning if missing)
- [ ] Battery voltage reading reasonable (13.0V - 16.8V)
- [ ] Watchdog configured successfully
- [ ] State transitions to IDLE

### 2. LED Behavior
- [ ] Built-in LED (Pin 13) shows slow heartbeat (500ms on/off)
- [ ] Pattern is regular and consistent
- [ ] No stuck LED (always on or always off)

### 3. Battery Voltage Calibration
**With multimeter:**
1. [ ] Measure actual battery voltage at terminals
2. [ ] Compare to Serial Monitor reading
3. [ ] Difference should be < 0.2V
4. [ ] If > 0.2V difference, adjust VOLTAGE_DIVIDER_RATIO constant

**Formula:** `actual_ratio = measured_battery_voltage / measured_pin_A9_voltage`

## First Transmission Test

### 1. Pre-Transmission (State: PTT_PREROLL)
Serial Monitor shows:
```
Starting transmission sequence
State: PTT_PREROLL
PTT keyed - waiting for squelch
```

**Verify:**
- [ ] Radio PTT LED lights up
- [ ] Status LED goes solid ON
- [ ] Timing: exactly 1000ms delay before audio
- [ ] No audio during pre-roll (squelch opening time)

### 2. Morse Transmission (State: TX_MORSE)
Serial Monitor shows:
```
State: TX_MORSE
Morse transmission complete
```

**Verify with receive radio:**
- [ ] Clean CW tone (no distortion or harmonics)
- [ ] Correct frequency (800Hz default)
- [ ] Proper Morse timing (dits/dahs distinguishable)
- [ ] Complete callsign received
- [ ] No "choppy" or "gated" audio

**Timing Check:**
- [ ] Dot length matches WPM setting (100ms @ 12 WPM)
- [ ] Dah is 3× dot length
- [ ] Letter spacing is 3× dot length
- [ ] Word spacing is 7× dot length

### 3. Audio Transmission (State: TX_AUDIO)
Serial Monitor shows:
```
State: TX_AUDIO
Playing: FOXID.WAV
Audio transmission complete
```

**Verify with receive radio:**
- [ ] Audio plays completely
- [ ] No distortion or clipping
- [ ] Volume appropriate (adjust pot if needed)
- [ ] No "buzzing" or "hum" in background
- [ ] File plays to completion

**If audio quality poor:**
- [ ] Reduce potentiometer volume
- [ ] Check 1kΩ series resistor installed
- [ ] Verify 10µF capacitor polarity
- [ ] Test without radio (external speaker on audio pin)

### 4. PTT Tail (State: PTT_TAIL)
Serial Monitor shows:
```
State: PTT_TAIL
Transmission complete - returning to idle
Battery: XX.XXV
```

**Verify:**
- [ ] 500ms delay after audio stops
- [ ] PTT remains keyed during tail
- [ ] Then PTT releases cleanly
- [ ] Radio PTT LED goes off
- [ ] Status LED returns to heartbeat

### 5. Return to Idle
Serial Monitor shows:
```
State: IDLE
```

**Verify:**
- [ ] Status LED returns to slow heartbeat
- [ ] Timing accurate for next transmission
- [ ] No error messages
- [ ] Battery voltage logged

## Extended Operation Testing

### 1. Multiple Transmission Cycles (10+ cycles)
- [ ] Alternates between Morse and Audio correctly
- [ ] Timing remains consistent (±2 seconds)
- [ ] No transmission failures
- [ ] No watchdog resets
- [ ] Battery voltage gradually decreasing (normal)
- [ ] No SD card errors

### 2. Audio Quality Over Time
- [ ] Volume remains consistent
- [ ] No degradation in audio quality
- [ ] PTT timing stable

### 3. System Stability
- [ ] No unexpected reboots
- [ ] No stuck PTT conditions
- [ ] No state machine hangs
- [ ] LED patterns remain correct

## Battery Protection Testing

### ⚠️ WARNING: Test with variable power supply, NOT with actual LiPo
**Discharging LiPo below 12.8V can cause permanent damage!**

### 1. Soft Warning Test (13.6V threshold)
**Setup:** Use adjustable power supply set to 13.5V

Expected Serial Monitor output:
```
WARNING: Battery low: 13.5V
```

**Verify:**
- [ ] Warning message appears
- [ ] LED changes to rapid blink (100ms on/off)
- [ ] Continues normal operation
- [ ] Plays LOWBATT.WAV every 5 cycles (if file present)

**Recovery test:**
- [ ] Increase voltage to 14.1V
- [ ] Warning clears (hysteresis at 14.0V)
- [ ] LED returns to normal heartbeat
- [ ] Serial shows "Battery voltage recovered"

### 2. Hard Shutdown Test (12.8V threshold)
**Setup:** Use adjustable power supply set to 12.7V

Expected Serial Monitor output:
```
============================================
CRITICAL: BATTERY PROTECTION ACTIVATED
Battery voltage: 12.7V
PTT PERMANENTLY DISABLED
============================================
State: EMERGENCY_SHUTDOWN
```

**Verify:**
- [ ] PTT immediately releases
- [ ] Pin 2 set to INPUT mode (high impedance)
- [ ] Audio stops immediately
- [ ] LED begins SOS pattern (··· --- ···)
- [ ] System does NOT recover automatically
- [ ] Requires power cycle to reset

**SOS LED Pattern:**
- [ ] 3 short blinks (dit dit dit)
- [ ] 3 long blinks (dah dah dah)
- [ ] 3 short blinks (dit dit dit)
- [ ] 2 second pause
- [ ] Pattern repeats indefinitely

**CRITICAL SAFETY CHECK:**
- [ ] Verify PTT pin is HIGH IMPEDANCE (measure with multimeter)
- [ ] Radio PTT must NOT be keyed
- [ ] This prevents battery drain and radio damage

### 3. Critical Event Logging
- [ ] Check for CRITICAL.LOG file on SD card
- [ ] File contains shutdown voltage reading
- [ ] Timestamp present (if RTC available)

## Watchdog Timer Testing (Optional but Recommended)

### ⚠️ This test intentionally crashes the system

**Method 1: Comment out wdt.feed() call**
1. [ ] Comment out `wdt.feed();` in `updateStateMachine()`
2. [ ] Upload modified code
3. [ ] System should reset after 30 seconds
4. [ ] Serial Monitor shows "WATCHDOG RESET TRIGGERED!"
5. [ ] PTT releases automatically
6. [ ] System reboots normally

**Method 2: Add infinite loop (more aggressive)**
1. [ ] Add `while(1);` in STATE_IDLE case
2. [ ] Upload modified code
3. [ ] System hangs, then resets after 30 seconds
4. [ ] Verify clean reboot

**After testing:**
- [ ] Restore original code
- [ ] Re-upload
- [ ] Verify normal operation

## Field Readiness Checklist

### 1. Documentation
- [ ] Read FoxhuntController_README.md thoroughly
- [ ] Understand all LED patterns
- [ ] Know battery thresholds and warnings
- [ ] Familiar with emergency procedures

### 2. Backup Plan
- [ ] Have backup battery (fully charged)
- [ ] Spare SD card with audio files
- [ ] Printed wiring diagram
- [ ] Contact info for tech support

### 3. Enclosure & Weather Protection
- [ ] All components secured in weather-resistant case
- [ ] Cable strain relief adequate
- [ ] Antenna connector weather-sealed
- [ ] Battery secured (no movement)
- [ ] SD card retention verified (won't pop out)

### 4. Final Checks
- [ ] Complete transmission cycle successful
- [ ] Receive radio confirms good audio
- [ ] Battery voltage above 15.0V (for long operation)
- [ ] All connections verified tight
- [ ] Volume set appropriately (not distorted)
- [ ] Callsign correct and legally compliant

### 5. Operating Parameters Documented
Record these for reference during hunt:
- Callsign: ________________
- Frequency: ________________
- CTCSS: ________________
- TX Interval: ________________ seconds
- Battery Start Voltage: ________________V
- Soft Warning Threshold: 13.6V
- Hard Shutdown Threshold: 12.8V
- Expected Battery Life: ~18-20 hours

## Troubleshooting Reference

### Problem: No PTT keying
**Check:**
- Pin 2 voltage (should be 3.3V when transmitting)
- 2N2222 wiring and orientation
- Radio cable connection
- Measure transistor collector voltage

### Problem: Distorted audio
**Fix:**
- Reduce potentiometer volume
- Verify 1kΩ series resistor present
- Check capacitor polarity
- Lower audio file sample rate

### Problem: Battery reading way off
**Fix:**
- Verify voltage divider resistor values
- Check A9 pin voltage with multimeter
- Recalculate VOLTAGE_DIVIDER_RATIO
- Inspect for loose connections

### Problem: SD card not detected
**Fix:**
- Reformat as FAT32 (not exFAT)
- Try different SD card brand
- Verify card is fully inserted
- Check for bent pins in Teensy socket

### Problem: System reboots randomly
**Check:**
- Power supply voltage stable
- No loose connections
- Check Serial Monitor for watchdog messages
- Verify buck converter output ripple

## Post-Hunt Procedure

### 1. Retrieval
- [ ] Power off before moving
- [ ] Check for water intrusion
- [ ] Inspect all connections

### 2. Data Collection
- [ ] Download CRITICAL.LOG from SD card (if present)
- [ ] Review Serial Monitor log
- [ ] Note final battery voltage
- [ ] Record any anomalies

### 3. Battery Management
- [ ] Check battery voltage immediately
- [ ] If below 13.0V, recharge within 24 hours
- [ ] Storage charge: 15.2V (3.8V per cell)
- [ ] Never store fully charged or depleted

### 4. System Review
- [ ] Any missed transmissions?
- [ ] Audio quality consistent?
- [ ] Battery life as expected?
- [ ] Any error messages?

---

## Sign-Off

**Tester:** ________________  
**Date:** ________________  
**Callsign:** ________________  

**All tests passed:** [ ] YES [ ] NO

**Notes/Issues:**
________________________________
________________________________
________________________________

**Ready for field deployment:** [ ] YES [ ] NO

---

**Version:** 1.0.0  
**Last Updated:** 2026-02-10  
**Controller:** FoxhuntController.ino
