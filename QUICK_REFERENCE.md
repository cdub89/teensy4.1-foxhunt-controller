# 🦊 WX7V Foxhunt Controller - Quick Reference Card
*Print this and keep with your fox hardware!*

---

## 📋 Pre-Deployment Checklist

**Power System:**
- [ ] Battery: 16.8V (fully charged)
- [ ] Fuse: 5A inline installed
- [ ] Buck A: 8.0V output
- [ ] Buck B: 5.0V output

**Radio:**
- [ ] Frequency: __________ MHz
- [ ] CTCSS: __________
- [ ] Antenna connected
- [ ] Volume: Mid-range

**Teensy:**
- [ ] SD card inserted
- [ ] FOXID.WAV present
- [ ] Callsign configured
- [ ] Test transmission successful

---

## 🚨 LED Status Indicators

| Pattern | Meaning | Action |
|---------|---------|--------|
| **Slow heartbeat** (500ms) | Normal operation | None - system healthy |
| **Solid ON** | Transmitting | Normal - PTT keyed |
| **Rapid blink** (100ms) | Low battery warning | Replace battery soon |
| **SOS (··· --- ···)** | Critical shutdown | **POWER CYCLE REQUIRED** |

---

## 🔋 Battery Protection Thresholds

| Voltage | Status | What Happens |
|---------|--------|--------------|
| **> 14.0V** | Normal | Green - full operation |
| **13.6V - 14.0V** | Soft warning | Yellow - plays LOWBATT.WAV every 5 cycles |
| **< 12.8V** | **HARD SHUTDOWN** | Red - PTT disabled permanently, SOS LED |

**⚠️ CRITICAL:** Below 12.8V, system enters emergency mode. PTT is permanently disabled until power cycle. This protects your LiPo from damage!

---

## ⏱️ Transmission Timing

| Event | Duration | Notes |
|-------|----------|-------|
| **TX Interval** | 60 seconds | Time between transmissions |
| **PTT Pre-roll** | 1000ms | Squelch opening delay |
| **Morse TX** | 3-8 seconds | Depends on callsign length |
| **Audio TX** | Variable | Depends on WAV file |
| **PTT Tail** | 500ms | After transmission ends |

**Total TX time:** ~5-10 seconds per cycle  
**Duty cycle:** ~8-13% (safe for continuous operation)

---

## 🎵 Audio Files (SD Card Root)

| Filename | Purpose | Required? |
|----------|---------|-----------|
| `FOXID.WAV` | Callsign identification | **YES** |
| `LOWBATT.WAV` | Low battery warning | Optional |

**Audio specs:** 16-bit PCM WAV, 22.05kHz, mono/stereo, < 10 seconds

---

## 📡 Transmission Sequence

1. **IDLE** - LED heartbeat, waiting for next interval
2. **PTT_PREROLL** - PTT keys, 1s silence, LED solid
3. **TX_MORSE** or **TX_AUDIO** - Audio output, LED solid
4. **PTT_TAIL** - 500ms hold, LED solid
5. **IDLE** - Return to heartbeat

**Alternates:** Morse → Audio → Morse → Audio...

---

## 🛠️ Troubleshooting

### No PTT Keying
- Check 2N2222 transistor wiring
- Verify Pin 2 voltage (3.3V when TX)
- Inspect radio cable connection

### Distorted Audio
- Turn down 10kΩ potentiometer
- Check 1kΩ series resistor installed
- Verify 10µF capacitor polarity

### Battery Reading Wrong
- Check voltage divider: 10kΩ + 2.2kΩ
- Measure actual battery with multimeter
- Compare to Serial Monitor reading

### SD Card Not Detected
- Reformat as FAT32 (not exFAT)
- Verify card fully inserted
- Try different card brand

---

## 🔒 Safety Features

✅ **Watchdog Timer:** 30-second timeout prevents stuck PTT  
✅ **Battery Protection:** Automatic shutdown at 12.8V  
✅ **Transmission Timeout:** 30-second max per TX  
✅ **Emergency PTT Release:** Multiple fail-safes  
✅ **High Impedance Mode:** PTT disabled in emergency

---

## 📞 Emergency Procedures

### Stuck PTT (Radio Won't Unkey)
1. Immediately disconnect battery
2. Check watchdog timer status
3. Power cycle and test
4. If persists, check transistor wiring

### Critical Battery Warning (SOS LED)
1. **DO NOT continue operation**
2. Power off immediately
3. Replace battery
4. Charge depleted battery ASAP (< 24 hours)
5. Power cycle to reset system

### System Not Transmitting
1. Check Serial Monitor (115200 baud)
2. Verify state transitions
3. Check battery voltage > 13.0V
4. Test PTT circuit with multimeter
5. Verify SD card if using audio

---

## 📊 Expected Performance

| Metric | Value |
|--------|-------|
| **Battery Life** | 18-20 hours (5200mAh @ 60s interval) |
| **TX Accuracy** | ±2 seconds over 24 hours |
| **Audio Quality** | Clear, no distortion (with proper filter) |
| **PTT Response** | Instantaneous (non-blocking) |
| **Max TX Time** | 30 seconds (safety timeout) |

---

## 🔧 Pin Quick Reference

| Pin | Function | Connection |
|-----|----------|------------|
| **2** | PTT | 1kΩ → 2N2222 base |
| **12** | Audio | 1kΩ → 10µF → pot → radio mic |
| **13** | LED | Built-in status LED |
| **A9** | Battery | Voltage divider (10k/2.2k) |

---

## 📝 Field Notes

**Hunt Date:** ____________  
**Location:** ____________  
**Start Voltage:** ______V  
**End Voltage:** ______V  
**Runtime:** ______ hours  
**Transmissions:** ______ cycles  

**Issues/Observations:**
________________________________
________________________________
________________________________

---

## ⚙️ Default Configuration

```
Callsign: WX7V FOX (CHANGE THIS!)
Morse WPM: 12
Tone Frequency: 800 Hz
TX Interval: 60 seconds
Soft Warning: 13.6V
Hard Shutdown: 12.8V
Watchdog: 30 seconds
```

---

## 📚 Documentation Files

- `FoxhuntController.ino` - Main code
- `FoxhuntController_README.md` - Setup guide
- `TESTING_CHECKLIST.md` - Pre-deployment tests
- `WX7V Foxhunt Controller Project Specification.md` - Hardware specs
- `Teensy4 Fox Hardware Reference.md` - Wiring diagrams

---

## 📡 FCC Compliance (US)

✅ Station ID required every 10 minutes  
✅ Proper amateur radio license required  
✅ Operate on authorized frequencies only  
✅ No encryption or ID obscuring

---

## 🎯 Post-Hunt Checklist

- [ ] Retrieve unit safely
- [ ] Check for water intrusion
- [ ] Note final battery voltage
- [ ] Download logs from SD card
- [ ] Recharge battery (15.2V storage)
- [ ] Review performance data
- [ ] Document any issues

---

**Version:** 1.0.0  
**Controller:** FoxhuntController.ino  
**Updated:** 2026-02-10

**73 de WX7V** 📡🦊

---

*For detailed information, see FoxhuntController_README.md*  
*For hardware wiring, see Teensy4 Fox Hardware Reference.md*  
*For troubleshooting, see TESTING_CHECKLIST.md*
