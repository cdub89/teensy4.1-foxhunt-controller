# FoxhuntController.ino - Quick Start Guide

## Installation & Testing

### 1. Required Libraries
Install these via Arduino Library Manager or Teensyduino:
- **Audio Library** (included with Teensyduino)
- **SD Library** (Arduino core)
- **Snooze Library** (Teensy low power)
- **Watchdog_t4** (Teensy 4.x watchdog timer)

### 2. Hardware Setup

**Pin Connections:**
- Pin 2: PTT output (via 1kΩ resistor → 2N2222 transistor)
- Pin 12: Audio output (via 1kΩ → 10µF cap → 10kΩ pot → Radio mic)
- Pin 13: Status LED (built-in)
- Pin A9: Battery voltage monitor (via 10kΩ/2.2kΩ divider)

**Audio Circuit (REQUIRED for clean audio):**
```
Pin 12 → 1kΩ resistor → 10µF capacitor (+) → 10kΩ pot (left pin)
10kΩ pot (center pin) → Radio mic input (red wire)
10kΩ pot (right pin) → Ground
```

**Battery Monitor Circuit:**
```
Battery (+) → 10kΩ (R1) → Pin A9 → 2.2kΩ (R2) → Ground
                         → 100nF cap → Ground (noise filter)
```

See: `Teensy4 Fox Hardware Reference.md` for complete wiring diagrams.

### 3. Configuration

Edit these constants in `FoxhuntController.ino`:

```cpp
// Change YOUR callsign here!
const char* CALLSIGN = "WX7V FOX";

// Adjust transmission interval (milliseconds)
const long TX_INTERVAL = 60000;  // 60 seconds

// Morse code speed
const int MORSE_WPM = 12;  // 12 words per minute

// Battery thresholds (verify with your setup)
const float SOFT_WARNING_THRESHOLD = 13.6;   // Play warning
const float HARD_SHUTDOWN_THRESHOLD = 12.8;  // Force shutdown
```

### 4. SD Card Setup

Format a microSD card as **FAT32** and copy these files to the root:
- `FOXID.WAV` - Your callsign identification (required)
- `LOWBATT.WAV` - Low battery warning (optional)

**Audio File Requirements:**
- Format: WAV (16-bit PCM)
- Sample rate: 22.05kHz or 44.1kHz
- Mono or stereo
- Filename: 8.3 format (e.g., `FOXID.WAV`, not `fox_identification.wav`)
- Duration: < 10 seconds recommended

### 5. Upload & Test

1. **Select Board:** Tools → Board → Teensy 4.1
2. **USB Type:** Serial (for debugging)
3. **CPU Speed:** 600 MHz (default)
4. **Upload** the sketch

### 6. Initial Testing

**Serial Monitor Output:**
```
============================================
WX7V Foxhunt Controller v1.0.0
Teensy 4.1 - Baofeng UV-5R
============================================

Configuration:
  Callsign: WX7V FOX
  Morse WPM: 12
  TX Interval: 60 seconds
  Battery Soft Warning: 13.60V
  Battery Hard Shutdown: 12.80V

Battery voltage: 14.83V
SD card initialized
System ready - starting transmission sequence

State: STARTUP
State: IDLE
Starting transmission sequence
State: PTT_PREROLL
PTT keyed - waiting for squelch
State: TX_MORSE
Morse transmission complete
State: PTT_TAIL
Transmission complete - returning to idle
Battery: 14.82V
```

### 7. LED Status Indicators

- **Slow heartbeat (500ms):** Normal idle
- **Rapid blink (100ms):** Low battery warning
- **Solid ON:** Transmitting
- **SOS pattern (···---···):** Critical battery shutdown

### 8. Operation Modes

The controller automatically alternates between:
1. **Morse Code:** Transmits callsign in CW at configured WPM
2. **Audio Playback:** Plays `FOXID.WAV` from SD card (if available)

If SD card is not present, it will transmit Morse code only.

### 9. Battery Protection

**Dual-Threshold Watchdog:**
- **13.6V (3.4V/cell):** Soft warning - plays `LOWBATT.WAV` every 5 cycles
- **12.8V (3.2V/cell):** Hard shutdown - PTT permanently disabled, SOS LED

**Critical:** The system will enter emergency mode if battery drops below 12.8V and require a power cycle to reset.

### 10. Safety Features

✅ **Watchdog Timer:** 30-second timeout prevents stuck PTT  
✅ **Battery Protection:** Dual-threshold prevents LiPo damage  
✅ **Transmission Timeout:** 30-second maximum per transmission  
✅ **Non-blocking Architecture:** Responsive to all safety checks  
✅ **Hysteresis:** Prevents battery warning spam

## Troubleshooting

### No Audio Output
- Check 1kΩ resistor + 10µF capacitor installation
- Verify 10kΩ pot is not turned fully down
- Confirm Pin 12 wiring to radio mic input
- Test with a speaker connected to audio circuit

### PTT Not Keying
- Verify 2N2222 transistor wiring (Pin 2 → 1kΩ → base)
- Check transistor orientation (flat side reference)
- Measure voltage at Pin 2 (should be 3.3V when transmitting)
- Test PTT circuit with multimeter

### SD Card Not Detected
- Ensure card is formatted as FAT32 (not exFAT)
- Verify card is fully inserted into Teensy 4.1 slot
- Try different SD card (some cards are not compatible)
- Check Serial Monitor for "SD card initialized" message

### Battery Reading Incorrect
- Verify voltage divider values (10kΩ and 2.2kΩ)
- Measure actual battery voltage with multimeter
- Adjust `VOLTAGE_DIVIDER_RATIO` constant if needed
- Formula: `ratio = battery_voltage / pin_A9_voltage`

### Morse Code Not Playing
- Check `CALLSIGN` constant is set correctly
- Verify tone frequency (800Hz default)
- Test with external speaker on Pin 12
- Check Serial Monitor for state transitions

## Field Deployment Checklist

**Before deploying in the field:**
- [ ] Battery fully charged (16.8V)
- [ ] SD card inserted with `FOXID.WAV`
- [ ] Callsign configured correctly
- [ ] Tested on receive radio (good audio quality)
- [ ] PTT timing verified (1s pre-roll working)
- [ ] Battery voltage reading accurate
- [ ] Radio frequency/CTCSS set correctly
- [ ] Antenna connected
- [ ] Enclosure weatherproofed
- [ ] Audio level adjusted (not distorted)

## Performance Characteristics

**Current Consumption:**
- Idle: ~100mA (Teensy active)
- Transmitting: ~150-180mA (Teensy + audio)
- Radio TX: ~1.5A (from separate 8V rail)

**Battery Life Estimate:**
- 5200mAh battery @ 60s intervals, 5s TX: ~18-20 hours

**Transmission Timing:**
- Interval: 60 seconds (configurable)
- PTT pre-roll: 1000ms (Baofeng squelch opening)
- Transmission: 3-8 seconds typical
- PTT tail: 500ms
- Duty cycle: ~8-13% (safe for continuous operation)

## Next Steps

See `IMPLEMENTATION_PLAN.md` for advanced features:
- Deep sleep power management (Phase 3)
- Randomized audio selection (Phase 4)
- DTMF remote control (Phase 4)
- Configuration system (Phase 5)
- Comprehensive logging (Phase 5)

## Support

**Documentation:**
- `WX7V Foxhunt Controller Project Specification.md` - Complete hardware specs
- `Teensy4 Fox Hardware Reference.md` - Wiring diagrams and parts list
- `IMPLEMENTATION_PLAN.md` - Phased development roadmap

**Debugging:**
- Open Serial Monitor at 115200 baud for detailed status
- Check state transitions for proper sequencing
- Monitor battery voltage readings
- Verify LED patterns match expected behavior

---

**License:** MIT  
**Version:** 1.0.0  
**Author:** WX7V  
**Last Updated:** 2026-02-10
