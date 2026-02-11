# Teensy 4.1 Foxhunt Controller - Implementation Plan

## Document Purpose
This document provides a phased approach to developing the Teensy 4.1 Foxhunt Controller, from basic proof-of-concept to advanced production-ready features.

---

## Phase 1: Foundation & Basic Functionality ✓ COMPLETED

### Objectives
- Establish basic hardware interface
- Validate PTT control and audio generation
- Prove concept with simple implementations

### Deliverables
✅ **Morse Code Controller** (`Morse Code Controller.mdc`)
- Working Morse code generation
- Standard ITU timing
- Basic PTT control (1000ms pre-roll, 500ms tail)
- Configurable WPM and callsign

✅ **Audio Controller** (`Audio Controller Code.mdc`)
- Teensy Audio Library integration
- SD card WAV playback
- MQS audio output on Pin 12
- Automatic playback completion detection

### Status: COMPLETE
Both reference implementations are functional and documented in the project specification.

---

## Phase 2: Non-Blocking Architecture Refactor

### Objectives
- Convert blocking code to state machine architecture
- Enable concurrent operations (battery monitoring, LED heartbeat)
- Prepare foundation for advanced features

### Tasks

#### Task 2.1: State Machine Framework
**Priority:** HIGH  
**Estimated Time:** 4-6 hours

**Implementation Steps:**
1. Define state enumeration:
   ```cpp
   enum FoxState {
     IDLE,
     PTT_PREROLL,
     TRANSMITTING_MORSE,
     TRANSMITTING_AUDIO,
     PTT_TAIL,
     SLEEPING
   };
   ```

2. Create state transition functions:
   - `updateStateMachine()` - called every loop iteration
   - `transitionToState(FoxState newState)` - handles state changes
   - `getCurrentStateTime()` - returns time in current state

3. Replace all `delay()` calls with `millis()` timing checks

4. Implement state-specific behaviors:
   - IDLE: Wait for next transmission time
   - PTT_PREROLL: Wait 1000ms with PTT active
   - TRANSMITTING: Handle Morse/audio output
   - PTT_TAIL: Wait 500ms before releasing PTT

**Testing:**
- Verify no blocking operations in `loop()`
- Confirm LED can blink independently during transmissions
- Measure loop iteration time (target: < 10ms)

#### Task 2.2: Non-Blocking Morse Code
**Priority:** HIGH  
**Estimated Time:** 3-4 hours

**Implementation:**
- Convert `sendMorse()` to state-based system
- Track current character, element (dit/dah), and timing
- Use `tone()` and `noTone()` with millis() tracking
- Maintain ITU Morse timing standards

**Testing:**
- Verify timing accuracy with oscilloscope
- Confirm concurrent operations possible during transmission

#### Task 2.3: Non-Blocking Audio Playback
**Priority:** MEDIUM  
**Estimated Time:** 2-3 hours

**Implementation:**
- Use `playWav1.isPlaying()` in state machine
- Remove blocking `while()` loop
- Add timeout protection (max 30 seconds per file)

**Testing:**
- Verify audio playback completes properly
- Test with various WAV file lengths

---

## Phase 3: Safety & Power Management

### Objectives
- Implement critical safety features
- Add battery monitoring
- Enable power-saving modes

### Tasks

#### Task 3.1: Watchdog Timer
**Priority:** CRITICAL  
**Estimated Time:** 2-3 hours

**Implementation:**
1. Include Watchdog_t4 library
2. Configure 30-second timeout
3. Add `WDT.feed()` calls in main loop
4. Implement emergency PTT release on timeout
5. Log watchdog resets to SD card

**Testing:**
- Intentionally hang code to verify reset
- Confirm PTT releases on watchdog trigger
- Validate recovery behavior

#### Task 3.2: Battery Voltage Monitoring
**Priority:** HIGH  
**Estimated Time:** 4-5 hours

**Hardware Setup:**
1. Build voltage divider (R1=47kΩ, R2=10kΩ)
2. Connect between battery positive and GND
3. Center tap to Pin A0
4. Add 0.1µF capacitor for noise filtering

See [Hardware Reference](Teensy4%20Fox%20Hardware%20Reference.md) for complete power distribution wiring.

**Software Implementation:**
```cpp
const int BATTERY_PIN = A0;
const float VOLTAGE_DIVIDER_RATIO = 5.7;
const float LOW_BATTERY_THRESHOLD = 13.2;
const float CRITICAL_BATTERY = 12.8;

// Moving average for stability
float batteryReadings[10];
int readingIndex = 0;

float readBatteryVoltage() {
  int raw = analogRead(BATTERY_PIN);
  float voltage = (raw / 1023.0) * 3.3 * VOLTAGE_DIVIDER_RATIO;
  
  // Update moving average
  batteryReadings[readingIndex] = voltage;
  readingIndex = (readingIndex + 1) % 10;
  
  // Calculate average
  float sum = 0;
  for (int i = 0; i < 10; i++) sum += batteryReadings[i];
  return sum / 10.0;
}

void checkBattery() {
  float voltage = readBatteryVoltage();
  
  if (voltage < CRITICAL_BATTERY) {
    // Emergency shutdown
    digitalWrite(PTT_PIN, LOW);
    logToSD("CRITICAL: Battery shutdown at " + String(voltage) + "V");
    deepSleep(); // Never wake up
  }
  else if (voltage < LOW_BATTERY_THRESHOLD) {
    // Play low battery warning
    playLowBatteryWarning();
  }
}
```

**Testing:**
- Calibrate voltage divider with multimeter
- Test low battery warnings
- Verify critical shutdown behavior
- Validate moving average smoothing

#### Task 3.3: Duty Cycle Management
**Priority:** HIGH  
**Estimated Time:** 3-4 hours

**Implementation:**
1. Track transmission time vs. total time
2. Calculate rolling duty cycle (5-minute window)
3. Implement forced cooldown if > 20% duty cycle
4. Add temperature monitoring if sensor available

**Safety Limits:**
- Maximum single transmission: 30 seconds
- Maximum duty cycle: 20% (over 5-minute period)
- Minimum off-time: 15 seconds between transmissions

**Testing:**
- Stress test with rapid transmissions
- Verify forced cooldown engages
- Monitor radio temperature during extended operation

#### Task 3.4: Deep Sleep Mode
**Priority:** MEDIUM  
**Estimated Time:** 3-4 hours

**Implementation:**
1. Include Snooze library
2. Configure wake sources (timer, external interrupt)
3. Enter deep sleep during IDLE state (if interval > 10 seconds)
4. Calculate sleep duration to wake before next transmission

**Power Savings:**
- Active idle: ~100mA
- Deep sleep: ~5mA
- Estimated battery life improvement: 100% → 300% for 60s intervals

**Testing:**
- Measure current draw with multimeter
- Verify accurate wake timing
- Test wake-from-sleep transmission accuracy

---

## Phase 4: Advanced Features

### Objectives
- Add randomized content selection
- Implement remote control capabilities
- Create hybrid Morse/audio system

### Tasks

#### Task 4.1: Randomized Audio Selection
**Priority:** MEDIUM  
**Estimated Time:** 2-3 hours

**Implementation:**
1. Create playlist array on SD card or in code:
   ```cpp
   const char* audioFiles[] = {
     "FOXID.WAV",
     "TAUNT01.WAV",
     "TAUNT02.WAV",
     "CATCHME.WAV",
     "CLOSER.WAV",
     "NICETRY.WAV"
   };
   const int numFiles = 6;
   ```

2. True random seed from analog noise:
   ```cpp
   void setup() {
     randomSeed(analogRead(A1)); // Use floating pin for noise
   }
   ```

3. Selection algorithm:
   ```cpp
   int selectRandomAudio() {
     static int lastIndex = -1;
     int index;
     do {
       index = random(numFiles);
     } while (index == lastIndex); // Avoid immediate repeats
     lastIndex = index;
     return index;
   }
   ```

**SD Card Structure:**
```
/
├── FOXID.WAV     (Callsign identification)
├── TAUNT01.WAV   ("You're getting warmer!")
├── TAUNT02.WAV   ("Can you hear me now?")
├── CATCHME.WAV   ("Catch me if you can!")
├── CLOSER.WAV    ("You're close!")
├── NICETRY.WAV   ("Nice try!")
└── LOWBATT.WAV   (Battery warning)
```

**Testing:**
- Verify no immediate repeats
- Test with missing files (error handling)
- Confirm even distribution over 50+ cycles

#### Task 4.2: Hybrid Morse & Audio Transmission
**Priority:** MEDIUM  
**Estimated Time:** 2-3 hours

**Transmission Patterns:**

**Option A: Alternating**
```
TX1: Morse ID
TX2: Random Audio
TX3: Morse ID
TX4: Random Audio
...
```

**Option B: Audio with Morse Trailer**
```
Each TX: Random Audio → 500ms pause → Morse ID
```

**Option C: Random Selection**
```cpp
void selectTransmissionType() {
  int choice = random(100);
  if (choice < 30) {
    transmitMorseOnly();
  } else if (choice < 90) {
    transmitAudioOnly();
  } else {
    transmitAudioThenMorse(); // 10% - full ID
  }
}
```

**Testing:**
- Verify proper PTT control during transitions
- Test audio/Morse timing coordination
- Confirm FCC ID requirements met (every 10 minutes)

#### Task 4.3: Battery Status Announcement
**Priority:** LOW  
**Estimated Time:** 2 hours

**Implementation:**
1. Create voltage-to-speech mapping:
   ```cpp
   void announceBatteryStatus() {
     float voltage = readBatteryVoltage();
     
     if (voltage > 15.5) playWav1.play("BATT100.WAV");
     else if (voltage > 14.8) playWav1.play("BATT75.WAV");
     else if (voltage > 14.0) playWav1.play("BATT50.WAV");
     else if (voltage > 13.2) playWav1.play("BATT25.WAV");
     else playWav1.play("LOWBATT.WAV");
   }
   ```

2. Trigger options:
   - Periodic (every 10 transmissions)
   - On low battery detection
   - On DTMF command (if remote control implemented)

**Audio Files Needed:**
- `BATT100.WAV` - "Battery full"
- `BATT75.WAV` - "Battery three quarters"
- `BATT50.WAV` - "Battery half"
- `BATT25.WAV` - "Battery low"
- `LOWBATT.WAV` - "Low battery warning"

**Testing:**
- Test at various voltage levels
- Verify accurate reporting
- Test announcement timing in transmission sequence

#### Task 4.4: DTMF Remote Control
**Priority:** LOW (Advanced)  
**Estimated Time:** 8-10 hours (including hardware)

**Hardware Setup:**
1. **MT8870 DTMF Decoder Module**
   - Purchase pre-made module (~$5) or build from IC
   - Connect to radio speaker/audio output (before speaker)
   - Wire digital outputs to Teensy

**Pin Connections:**
```cpp
const int DTMF_D0 = 3;
const int DTMF_D1 = 4;
const int DTMF_D2 = 5;
const int DTMF_D3 = 6;
const int DTMF_STD = 7;  // Valid tone detected (strobe)
```

**Software Implementation:**
```cpp
char readDTMF() {
  if (digitalRead(DTMF_STD) == HIGH) {
    int value = 0;
    value |= digitalRead(DTMF_D0);
    value |= digitalRead(DTMF_D1) << 1;
    value |= digitalRead(DTMF_D2) << 2;
    value |= digitalRead(DTMF_D3) << 3;
    
    const char dtmfChars[] = {'D','1','2','3','4','5','6','7','8','9','0','*','#','A','B','C'};
    return dtmfChars[value];
  }
  return '\0';
}

void processDTMFCommand(String command) {
  if (command == "*123#") {
    // Immediate transmission
    forceTransmission();
  }
  else if (command == "*456#") {
    // Enable continuous mode
    continuousMode = true;
  }
  else if (command == "*789#") {
    // Battery status
    queueBatteryAnnouncement();
  }
  else if (command == "*000#") {
    // Disable fox (stealth)
    foxEnabled = false;
  }
}
```

**Command Set:**
- `*123#` - Force immediate transmission
- `*456#` - Enable continuous mode (every 30s)
- `*555#` - Enable normal mode (60s intervals)
- `*666#` - Enable sparse mode (5 minute intervals)
- `*789#` - Battery status announcement
- `*000#` - Disable fox (stealth mode)
- `*999#` - Re-enable fox from stealth

**Testing:**
- Test each command individually
- Verify command buffering and timeout
- Test invalid command rejection
- Confirm mode transitions work correctly

#### Task 4.5: VOX-Activated Mode
**Priority:** LOW  
**Estimated Time:** 4-6 hours

**Hardware Options:**

**Option A: Radio Squelch Output**
- Many radios have a squelch detect pin
- Goes HIGH when signal detected
- Connect to digital input (with current limiting)

**Option B: Audio Envelope Detector**
- Simple circuit: Diode → Capacitor → Comparator
- Detects presence of audio signal
- More universal (works with any radio)

See [Hardware Reference](Teensy4%20Fox%20Hardware%20Reference.md) for Baofeng pigtail pinout and color codes.

**Circuit:**
```
Radio Speaker Out → 1N4148 Diode → 10µF Cap → 10kΩ → GND
                                      ↓
                              LM393 Comparator → Teensy Pin 8
```

**Software Implementation:**
```cpp
const int VOX_PIN = 8;
const long VOX_COOLDOWN = 300000; // 5 minutes
unsigned long lastVoxActivation = 0;
bool foxActive = false;

void loop() {
  updateStateMachine();
  
  // Check for VOX trigger
  if (digitalRead(VOX_PIN) == HIGH && 
      (millis() - lastVoxActivation > VOX_COOLDOWN)) {
    
    delay(2000); // Wait for transmission to end
    
    if (digitalRead(VOX_PIN) == LOW) {
      // Transmission ended, activate fox
      foxActive = true;
      lastVoxActivation = millis();
    }
  }
  
  if (foxActive && currentState == IDLE) {
    // Start transmission sequence
    transitionToState(PTT_PREROLL);
    foxActive = false; // One-shot
  }
}
```

**Operating Modes:**
- **Silent Mode:** No transmissions until VOX triggered
- **Cooldown Mode:** 5-minute quiet period after each activation
- **Persistent Mode:** Continuous transmissions after first VOX trigger

**Testing:**
- Test VOX sensitivity (adjust threshold)
- Verify cooldown timer
- Test false trigger rejection (short signals)
- Confirm proper debouncing

---

## Phase 5: Polish & Production

### Objectives
- Add configuration interface
- Implement logging and diagnostics
- Create field deployment procedures

### Tasks

#### Task 5.1: Configuration System
**Priority:** MEDIUM  
**Estimated Time:** 4-5 hours

**Implementation:**
1. Store configuration on SD card (`CONFIG.TXT`)
2. Parse settings at startup
3. Provide serial interface for field changes

**Configuration File Format:**
```ini
# Teensy Fox Configuration
CALLSIGN=W1ABC FOX
MORSE_WPM=12
TONE_FREQ=800
TX_INTERVAL=60
DUTY_CYCLE_MAX=20
LOW_BATT_THRESHOLD=13.2
MODE=HYBRID
DTMF_ENABLED=1
VOX_ENABLED=0
RANDOM_TAUNTS=1
```

**Serial Commands:**
```
> config list              # Show all settings
> config set TX_INTERVAL 30
> config save
> config reload
> status                   # Show current state
```

**Testing:**
- Test config file parsing
- Verify invalid entry handling
- Test serial command interface
- Confirm settings persist after reboot

#### Task 5.2: Logging System
**Priority:** MEDIUM  
**Estimated Time:** 3-4 hours

**Implementation:**
1. Create log file on SD card with timestamp
2. Log significant events:
   - Boot/restart events
   - Each transmission (time, type, duration)
   - Battery voltage readings
   - Errors and warnings
   - DTMF commands received
   - Watchdog resets

**Log Format:**
```
2026-02-10 14:32:15 | BOOT    | System initialized
2026-02-10 14:32:16 | BATT    | Voltage: 14.8V
2026-02-10 14:33:00 | TX      | Morse: W1ABC FOX (4.2s)
2026-02-10 14:34:00 | TX      | Audio: TAUNT01.WAV (3.8s)
2026-02-10 14:35:00 | WARNING | Battery: 13.1V
2026-02-10 14:35:05 | TX      | Audio: LOWBATT.WAV (2.1s)
```

**Features:**
- Circular buffer (auto-delete old logs)
- Real-time clock integration (if RTC available)
- Serial log streaming for debugging
- Log download via serial interface

**Testing:**
- Verify log rotation
- Test log integrity after power loss
- Confirm timestamp accuracy

#### Task 5.3: LED Status Indicators
**Priority:** LOW  
**Estimated Time:** 2 hours

**LED Patterns:**
- **Idle:** Slow heartbeat (1 second cycle)
- **Transmitting:** Solid ON
- **Low Battery:** Rapid blink (200ms cycle)
- **Error:** SOS pattern (···---···)
- **DTMF Command:** Triple flash

**Implementation:**
```cpp
void updateLED() {
  static unsigned long lastUpdate = 0;
  static bool ledState = false;
  
  if (millis() - lastUpdate > getLEDInterval()) {
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState);
    lastUpdate = millis();
  }
}

int getLEDInterval() {
  if (batteryVoltage < LOW_BATTERY_THRESHOLD) return 100;
  if (currentState == TRANSMITTING) return 0; // Always on
  return 500; // Heartbeat
}
```

**Testing:**
- Verify patterns for each state
- Test transitions between states
- Confirm visibility in daylight

#### Task 5.4: Field Deployment Checklist
**Priority:** HIGH  
**Estimated Time:** 2-3 hours (documentation)

**Pre-Deployment Checklist:**
```markdown
Hardware:
[ ] Battery fully charged (16.8V)
[ ] SD card inserted with audio files
[ ] All connections secure (XT60, radio cable)
[ ] Antenna connected to radio
[ ] Radio set to correct frequency/CTCSS
[ ] Radio volume adjusted (test with speaker)
[ ] Enclosure weatherproofed
[ ] Audio filter components installed (1kΩ resistor + 10µF cap)

See [Hardware Reference](Teensy4%20Fox%20Hardware%20Reference.md) for complete parts checklist and wiring verification.

Software:
[ ] Callsign configured correctly
[ ] Transmission interval appropriate for hunt
[ ] Duty cycle limits set
[ ] Low battery threshold configured
[ ] SD card has free space for logs
[ ] Clock set to correct time (if using RTC)

Testing:
[ ] Test transmission received on second radio
[ ] Verify PTT timing (1s pre-roll, 0.5s tail)
[ ] Check audio levels (not distorted)
[ ] Confirm battery voltage reading accurate
[ ] Test watchdog recovery (optional)
[ ] Verify LED indicators working

Post-Hunt:
[ ] Retrieve unit safely
[ ] Download logs from SD card
[ ] Check battery voltage
[ ] Inspect for water intrusion
[ ] Review transmission log for issues
```

**Documentation:**
- Create operator's manual
- Document troubleshooting procedures
- Include circuit diagrams
- Provide BOM (Bill of Materials)
- Reference [Hardware Reference](Teensy4%20Fox%20Hardware%20Reference.md) for wiring details

---

## Phase 6: Testing & Validation

### Field Testing Requirements

#### Test 1: Basic Operation
**Duration:** 2 hours  
**Objective:** Validate core functionality

**Procedure:**
1. Deploy fox in controlled environment
2. Monitor for 10 transmission cycles
3. Verify timing accuracy
4. Check audio quality on receive radio
5. Confirm PTT timing correct

**Success Criteria:**
- [ ] All transmissions occur on schedule (±2 seconds)
- [ ] Audio is clear and intelligible
- [ ] No stuck PTT conditions
- [ ] Battery voltage stable

#### Test 2: Battery Life
**Duration:** 24 hours  
**Objective:** Validate power management and battery life

**Procedure:**
1. Start with fully charged battery (16.8V)
2. Configure for normal operations (60s intervals, 5s TX)
3. Monitor voltage every hour
4. Record transmission count

**Success Criteria:**
- [ ] Operates for >18 hours
- [ ] Low battery warning at correct threshold
- [ ] Automatic shutdown prevents over-discharge
- [ ] Deep sleep mode reduces idle current to <10mA

#### Test 3: Environmental Stress
**Duration:** 4 hours  
**Objective:** Test in field conditions

**Conditions:**
- Temperature range: 0°C to 40°C
- Humidity: Up to 80%
- Vibration/movement

**Success Criteria:**
- [ ] No false triggers or lockups
- [ ] Audio quality maintained
- [ ] No connection failures
- [ ] Enclosure remains weatherproof

#### Test 4: Extended Operation
**Duration:** 48 hours  
**Objective:** Validate long-term stability

**Procedure:**
1. Deploy in realistic hunt scenario
2. Monitor remotely (if possible)
3. Check logs after completion

**Success Criteria:**
- [ ] No watchdog resets
- [ ] No SD card errors
- [ ] Transmission timing drift < 5 seconds
- [ ] All features operate correctly

---

## Implementation Timeline

### Aggressive Schedule (Full-time development)
- **Phase 2:** 2-3 days
- **Phase 3:** 3-4 days
- **Phase 4:** 5-7 days
- **Phase 5:** 2-3 days
- **Phase 6:** 3-4 days
- **Total:** 15-21 days

### Leisurely Schedule (Hobby pace, evenings/weekends)
- **Phase 2:** 2-3 weeks
- **Phase 3:** 3-4 weeks
- **Phase 4:** 4-6 weeks
- **Phase 5:** 1-2 weeks
- **Phase 6:** 1-2 weeks
- **Total:** 11-17 weeks (~3-4 months)

---

## Dependencies & Prerequisites

### Hardware Components

**Phase 2-3 (Required):**
- Teensy 4.1 (already owned)
- Baofeng UV-5R (already owned)
- SD card (microSD, 8GB+, FAT32 formatted)
- 2N2222 transistor (already in use)
- Resistors: 1kΩ, 47kΩ, 10kΩ
- Capacitors: 10µF, 0.1µF
- LiPo battery (already owned)

**Phase 4 (Optional - DTMF):**
- MT8870 DTMF decoder module ($5)
- Jumper wires

**Phase 4 (Optional - VOX):**
- LM393 comparator IC ($1)
- 1N4148 diode
- Additional resistors/capacitors

**Phase 5 (Optional):**
- DS3231 RTC module for accurate timestamps ($3)

### Software Libraries

**Required:**
- Arduino IDE or PlatformIO
- Teensy Teensyduino add-on
- Audio Library (included with Teensyduino)
- SD Library (Arduino core)

**Optional:**
- Snooze Library (Teensy low power)
- Watchdog_t4 (Teensy 4.x watchdog)
- TimeLib (RTC support)

### Skills Required

**Basic (Phase 1-2):**
- C/C++ programming
- Arduino IDE familiarity
- Basic electronics knowledge

**Intermediate (Phase 3-4):**
- State machine design
- Analog circuit design (voltage divider)
- Audio processing concepts

**Advanced (Phase 4-5):**
- DTMF decoding
- Digital signal processing
- System integration

---

## Risk Assessment

| Risk | Probability | Impact | Mitigation |
|------|------------|--------|------------|
| **Stuck PTT** | Medium | Critical | Watchdog timer, max TX timeout |
| **Battery Over-discharge** | Low | High | Voltage monitoring, auto-shutdown |
| **SD Card Failure** | Low | Medium | Error handling, operate without SD |
| **Radio Overheating** | Medium | Medium | Duty cycle limits, thermal monitoring |
| **Audio Quality Issues** | Medium | Low | Volume pot, proper coupling circuit |
| **False DTMF Triggers** | Low | Low | Command validation, checksum |

---

## Success Metrics

### Minimum Viable Product (MVP)
- [ ] Transmits on schedule (±5 seconds accuracy)
- [ ] PTT timing correct (1000ms pre-roll)
- [ ] Audio is intelligible
- [ ] Operates for 12+ hours on battery
- [ ] Safe failure modes (no stuck PTT)

### Production Ready
- [ ] All MVP criteria met
- [ ] Battery life 18+ hours
- [ ] Watchdog timer functional
- [ ] Low battery warning/shutdown
- [ ] Duty cycle enforcement
- [ ] Logging operational
- [ ] Field tested 48+ hours

### Advanced Features
- [ ] Randomized audio working
- [ ] Battery status announcements
- [ ] DTMF or VOX control functional
- [ ] Configuration system complete
- [ ] Documentation published

---

## Next Steps

1. **Review this plan** - Adjust priorities based on your needs
2. **Order missing hardware** - Especially voltage divider components
3. **Begin Phase 2** - Start with state machine refactor
4. **Set up development environment** - Ensure all libraries installed
5. **Create Git repository** - Track progress and changes
6. **Schedule field test** - Coordinate with local radio club

---

## Notes & Considerations

### FCC Compliance (US)
- Station identification required every 10 minutes
- Morse code ID satisfies requirement
- No encryption or obscuring of ID
- Proper license required for frequency

### Safety
- Never exceed 20% duty cycle in warm weather
- Monitor radio temperature during development
- Use proper fuse protection (5A inline fuse)
- Weatherproof enclosure for outdoor deployment

### Best Practices
- Always test in controlled environment first
- Keep detailed notes during field testing
- Have backup radio for receive testing
- Bring spare batteries to events
- Document all configuration changes

---

**Document Version:** 1.0  
**Last Updated:** 2026-02-10  
**Status:** Phase 1 Complete, Phase 2 Ready to Begin
