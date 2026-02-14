# WX7V Foxhunt Controller - Software Reference

## Project Context

This document defines the software architecture, coding standards, and implementation requirements for the Teensy 4.1 Fox Controller. This is safety-critical code - a stuck PTT can drain batteries, overheat equipment, or cause interference.

**Always prioritize: Non-blocking design, safety interlocks, and defensive programming.**

---

## Hardware Pin Definitions

```cpp
// Pin Assignments - NEVER change without updating Hardware_Reference.md
const int PTT_PIN = 2;        // Digital Output -> 1kΩ -> 2N2222 Base
const int AUDIO_PIN = 12;     // PWM/MQS -> 1kΩ -> 10µF Cap -> Radio Mic
const int LED_PIN = 13;       // Built-in LED (status indicator)
const int BATTERY_PIN = A9;   // Analog Input (voltage divider)
```

---

## Architecture Requirements

### 1. NON-BLOCKING DESIGN (MANDATORY)

**Use `millis()` for timing, NEVER `delay()` in production code.**

```cpp
// BAD - Blocks entire system
void transmit() {
  digitalWrite(PTT_PIN, HIGH);
  delay(1000);  // System frozen for 1 second
  sendMorse("WX7V");
}

// GOOD - Non-blocking state machine
enum FoxState { IDLE, PTT_PREROLL, TRANSMITTING, PTT_TAIL };
FoxState currentState = IDLE;
unsigned long stateStartTime = 0;

void loop() {
  unsigned long currentTime = millis();
  
  switch (currentState) {
    case IDLE:
      if (currentTime - lastTransmitTime >= transmitInterval) {
        currentState = PTT_PREROLL;
        stateStartTime = currentTime;
        digitalWrite(PTT_PIN, HIGH);
      }
      break;
      
    case PTT_PREROLL:
      if (currentTime - stateStartTime >= 1000) {
        currentState = TRANSMITTING;
        stateStartTime = currentTime;
        startMorseTransmission();
      }
      break;
      
    // ... additional states
  }
  
  // Other tasks can run concurrently
  updateLED();
  checkBatteryVoltage();
}
```

**Requirements:**
- Keep `loop()` execution time < 10ms
- Implement state machines for complex sequences
- Allow concurrent task handling (LED blink, sensor checks, etc.)
- All timing must use `millis()` or hardware timers

---

## Baofeng UV-5R Specific Timing

**Critical for reliable operation:**

### PTT Pre-Roll: 1000ms
- **Required:** After PTT is keyed, wait 1000ms before starting audio
- **Purpose:** Allows squelch to open on receiving radios
- **Consequence:** Without pre-roll, first part of audio will be cut off

### PTT Tail: 500ms
- **Required:** After audio ends, hold PTT for 500ms before releasing
- **Purpose:** Ensures all audio is transmitted before radio un-keys
- **Consequence:** Without tail, last part of audio may be clipped

```cpp
const unsigned long PTT_PREROLL_MS = 1000;
const unsigned long PTT_TAIL_MS = 500;

// State machine must enforce these delays
```

---

## Safety Interlocks (CRITICAL)

### Watchdog Timer

**Purpose:** Prevent stuck PTT if code crashes

```cpp
#include <Watchdog_t4.h>

WDT_timings_t config;
WDT_T4<WDT1> wdt;

void setup() {
  config.trigger = 30; // 30 second timeout
  config.timeout = 35; // 35 second reset
  wdt.begin(config);
}

void loop() {
  wdt.feed(); // Reset watchdog - call frequently!
  
  // If loop() hangs for >30s, Teensy will reset
}
```

**Requirements:**
- Maximum PTT timeout: 30 seconds
- Call `wdt.feed()` at least once per second
- Emergency PTT release on timeout

### Duty Cycle Management

**Purpose:** Prevent radio overheating

```cpp
const unsigned long TRANSMIT_INTERVAL = 60000;  // 60 seconds
const unsigned long MAX_TX_DURATION = 10000;    // 10 seconds max
const float MAX_DUTY_CYCLE = 0.20;              // 20% maximum

// Calculate duty cycle
float dutyCycle = (float)txDuration / (float)TRANSMIT_INTERVAL;

if (dutyCycle > MAX_DUTY_CYCLE) {
  // SAFETY: Duty cycle too high
  logError("Duty cycle exceeded safe limit");
  increaseTransmitInterval();
}
```

**Recommended Safe Duty Cycles:**
- **8.3%** (5s TX / 60s interval) - Optimal for >24 hour operation
- **20%** (10s TX / 50s interval) - Maximum recommended
- **>20%** - Risk of radio overheating, especially in warm environments

---

## Battery Watchdog System

**Purpose:** Protect LiPo from over-discharge while providing early warning

### Voltage Monitor Configuration

```cpp
const int BATTERY_PIN = A9;
const float VOLTAGE_DIVIDER_RATIO = 6.0;  // (R1 + R2) / R2 = 12.0kΩ / 2.0kΩ
const float SOFT_WARNING_THRESHOLD = 13.6;   // 3.4V per cell
const float HARD_SHUTDOWN_THRESHOLD = 12.8;  // 3.2V per cell
const float WARNING_CLEAR_THRESHOLD = 14.0;  // Hysteresis

bool batteryWarningActive = false;
bool batteryShutdownActive = false;
int warningCounter = 0;
```

### Read Battery Voltage

```cpp
float readBatteryVoltage() {
  // Average 10 samples to reduce noise
  long sum = 0;
  for (int i = 0; i < 10; i++) {
    sum += analogRead(BATTERY_PIN);
    delay(10);
  }
  int rawValue = sum / 10;
  
  float pinVoltage = (rawValue / 1023.0) * 3.3;
  float batteryVoltage = pinVoltage * VOLTAGE_DIVIDER_RATIO;
  return batteryVoltage;
}
```

### Check Battery Status (Non-Blocking)

```cpp
void checkBatteryStatus() {
  // Call this once per transmission cycle, NOT every loop iteration
  float voltage = readBatteryVoltage();
  
  // CRITICAL: Hard shutdown check (highest priority)
  if (voltage < HARD_SHUTDOWN_THRESHOLD) {
    triggerBatteryShutdown(voltage);
    return; // Never transmit again
  }
  
  // Soft warning with hysteresis
  if (!batteryWarningActive && voltage < SOFT_WARNING_THRESHOLD) {
    batteryWarningActive = true;
    warningCounter = 0;
  } else if (batteryWarningActive && voltage > WARNING_CLEAR_THRESHOLD) {
    batteryWarningActive = false;
  }
  
  // Play warning every 5 cycles (not every cycle)
  if (batteryWarningActive) {
    warningCounter++;
    if (warningCounter >= 5) {
      playLowBatteryWarning(voltage);
      warningCounter = 0;
    }
  }
}
```

### Preventing False Alarms (Hysteresis + Debouncing)

**Problem:** Electrical noise, RF interference, or transient voltage drops during SD card writes can cause false battery alarms.

**Solution:** Combine hysteresis (voltage bands) with debouncing (multiple consecutive readings).

**Complete 5-State Implementation** (from `battery_monitor_test.ino`):

```cpp
// Battery state enumeration
enum BatteryState {
  STATE_GOOD,         // >= 14.0V (3.50V/cell)
  STATE_LOW,          // >= 13.6V (3.40V/cell)
  STATE_VERY_LOW,     // >= 12.8V (3.20V/cell)
  STATE_SHUTDOWN,     // >= 12.0V (3.00V/cell)
  STATE_CRITICAL      // < 12.0V (damage risk!)
};

// Battery thresholds with hysteresis
const float VOLTAGE_GOOD = 14.0;             // 3.50V per cell
const float VOLTAGE_SOFT_WARNING = 13.6;     // 3.40V per cell
const float VOLTAGE_HARD_SHUTDOWN = 12.8;    // 3.20V per cell
const float VOLTAGE_CRITICAL = 12.0;         // 3.00V per cell
const float VOLTAGE_HYSTERESIS = 0.2;        // 0.2V band prevents bouncing

// Debounce settings
const int STATE_CHANGE_DEBOUNCE_COUNT = 3;  // Require 3 consecutive readings

BatteryState currentState = STATE_GOOD;
BatteryState pendingState = STATE_GOOD;
int stateDebounceCounter = 0;

void updateBatteryState(float batteryVoltage) {
  BatteryState measuredState;
  
  // Apply hysteresis based on current state to prevent bouncing
  if (currentState == STATE_GOOD) {
    // Need to drop below threshold minus hysteresis to trigger warning
    if (batteryVoltage >= VOLTAGE_GOOD) {
      measuredState = STATE_GOOD;
    } else if (batteryVoltage >= VOLTAGE_SOFT_WARNING - VOLTAGE_HYSTERESIS) {
      measuredState = STATE_LOW;
    } else if (batteryVoltage >= VOLTAGE_HARD_SHUTDOWN - VOLTAGE_HYSTERESIS) {
      measuredState = STATE_VERY_LOW;
    } else if (batteryVoltage >= VOLTAGE_CRITICAL - VOLTAGE_HYSTERESIS) {
      measuredState = STATE_SHUTDOWN;
    } else {
      measuredState = STATE_CRITICAL;
    }
  } else if (currentState == STATE_LOW) {
    // Need to rise above threshold + hysteresis to return to GOOD
    if (batteryVoltage >= VOLTAGE_GOOD + VOLTAGE_HYSTERESIS) {
      measuredState = STATE_GOOD;
    } else if (batteryVoltage >= VOLTAGE_SOFT_WARNING) {
      measuredState = STATE_LOW;
    } else if (batteryVoltage >= VOLTAGE_HARD_SHUTDOWN - VOLTAGE_HYSTERESIS) {
      measuredState = STATE_VERY_LOW;
    } else if (batteryVoltage >= VOLTAGE_CRITICAL - VOLTAGE_HYSTERESIS) {
      measuredState = STATE_SHUTDOWN;
    } else {
      measuredState = STATE_CRITICAL;
    }
  } else if (currentState == STATE_VERY_LOW) {
    // Add hysteresis for recovery
    if (batteryVoltage >= VOLTAGE_GOOD + VOLTAGE_HYSTERESIS) {
      measuredState = STATE_GOOD;
    } else if (batteryVoltage >= VOLTAGE_SOFT_WARNING + VOLTAGE_HYSTERESIS) {
      measuredState = STATE_LOW;
    } else if (batteryVoltage >= VOLTAGE_HARD_SHUTDOWN) {
      measuredState = STATE_VERY_LOW;
    } else if (batteryVoltage >= VOLTAGE_CRITICAL - VOLTAGE_HYSTERESIS) {
      measuredState = STATE_SHUTDOWN;
    } else {
      measuredState = STATE_CRITICAL;
    }
  } else if (currentState == STATE_SHUTDOWN) {
    // Once in shutdown, need significant recovery
    if (batteryVoltage >= VOLTAGE_GOOD + VOLTAGE_HYSTERESIS) {
      measuredState = STATE_GOOD;
    } else if (batteryVoltage >= VOLTAGE_SOFT_WARNING + VOLTAGE_HYSTERESIS) {
      measuredState = STATE_LOW;
    } else if (batteryVoltage >= VOLTAGE_HARD_SHUTDOWN + VOLTAGE_HYSTERESIS) {
      measuredState = STATE_VERY_LOW;
    } else if (batteryVoltage >= VOLTAGE_CRITICAL) {
      measuredState = STATE_SHUTDOWN;
    } else {
      measuredState = STATE_CRITICAL;
    }
  } else { // STATE_CRITICAL
    // Once critical, need significant recovery to change state
    if (batteryVoltage >= VOLTAGE_HARD_SHUTDOWN + VOLTAGE_HYSTERESIS) {
      measuredState = STATE_SHUTDOWN;
    } else {
      measuredState = STATE_CRITICAL;
    }
  }
  
  // Debounce: require multiple consecutive readings
  if (measuredState != currentState) {
    if (measuredState == pendingState) {
      // Same pending state, increment counter
      stateDebounceCounter++;
      
      if (stateDebounceCounter >= STATE_CHANGE_DEBOUNCE_COUNT) {
        // Confirmed! Change state
        previousState = currentState;
        currentState = measuredState;
        stateDebounceCounter = 0;
        
        handleStateTransition();  // Log, alert, update LED pattern, etc.
      }
    } else {
      // Different pending state, restart counter
      pendingState = measuredState;
      stateDebounceCounter = 1;
    }
  } else {
    // State matches current, reset debounce
    pendingState = currentState;
    stateDebounceCounter = 0;
  }
}
```

**How it works:**
- **Hysteresis:** Creates a "no-man's land" between states. At 14.0V threshold, must drop to 13.8V to trigger LOW, but must rise to 14.2V to return to GOOD
- **Debouncing:** Requires 3 consecutive matching readings (1.5 seconds at 500ms sampling) before accepting state change
- **Result:** Single noise spike won't trigger alarm; real battery discharge still detected within ~1.5 seconds

**Example scenario:**
```
Battery at 14.1V (GOOD)
  → Noise spike: 13.9V (1/3)
  → Next reading: 14.0V → Counter resets, stays GOOD ✓

Battery actually discharging
  → 13.7V, 13.7V, 13.6V (3/3) → Changes to LOW
  → Must reach 14.2V to return to GOOD (prevents bouncing)
```

### Hard Shutdown Implementation

```cpp
void triggerBatteryShutdown(float voltage) {
  batteryShutdownActive = true;
  
  // Immediately disable PTT - use INPUT mode for high-impedance
  pinMode(PTT_PIN, INPUT);
  
  // Disable all audio output
  AudioMemory(0);
  
  // Log critical event
  Serial.print("CRITICAL: Battery shutdown at ");
  Serial.print(voltage, 2);
  Serial.println("V");
  
  // Log to SD card if available
  logCriticalEvent(voltage);
  
  // Enter emergency mode with SOS LED pattern
  while (true) {
    blinkSOS();  // ...---...
    Snooze.sleep(60000); // Wake every 60s to continue SOS
  }
  // System will never exit this loop - power cycle required
}
```

### SOS LED Pattern

```cpp
void blinkSOS() {
  // S (...)
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(200);
    digitalWrite(LED_PIN, LOW);
    delay(200);
  }
  delay(400);
  
  // O (---)
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(600);
    digitalWrite(LED_PIN, LOW);
    delay(200);
  }
  delay(400);
  
  // S (...)
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(200);
    digitalWrite(LED_PIN, LOW);
    delay(200);
  }
  delay(2000); // Long pause between SOS repetitions
}
```

**Safety Notes:**
- ADC readings must be averaged (10 samples minimum) to reduce noise
- Test with actual battery under load (radio transmitting) for accurate readings
- Over-discharge below 3.0V/cell (12.0V total) permanently damages LiPo cells
- Never bypass hard shutdown threshold

---

## LED Morse Code Battery Status Indication

### Overview

Visual feedback via LED flashing Morse code patterns provides instant battery status without requiring serial connection. Enhances field deployment by giving clear warnings visible from a distance.

**Implementation:** Non-blocking state machine that continuously repeats contextual Morse messages.

### Morse Code Patterns by Battery State

| Battery State | LED Pattern | Message |
|--------------|-------------|---------|
| GOOD | `... --.` | "SG" - **S**tatus **G**ood |
| LOW | `--- .-..` | "OL" - **O**h **L**ow |
| VERY_LOW | `--- ...-` | "OV" - **O**h **V**ery low |
| SHUTDOWN | `... --- ... ...` | "SOSS" - **SOS** **S**hutdown |
| CRITICAL | `... --- ... -.-.` | "SOSC" - **SOS** **C**ritical |

**Design rationale:**
- **Status (S)** prefix for normal operation (GOOD)
- **Oh (O)** prefix for warning states (LOW, VERY_LOW)
- **SOS** prefix for emergency states (SHUTDOWN, CRITICAL)

### Complete Non-Blocking Implementation

See `battery_monitor_test/battery_monitor_test.ino` (lines 511-629) for full working example.

```cpp
// International Morse Code timing (~6 WPM for visual clarity)
const int MORSE_DOT_DURATION = 200;       // 200ms
const int MORSE_DASH_DURATION = 600;      // 3x dot
const int MORSE_ELEMENT_SPACE = 200;      // Same as dot
const int MORSE_LETTER_SPACE = 600;       // 3x dot
const int MORSE_WORD_SPACE = 1400;        // 7x dot (between repeats)

// Morse state machine
String currentMorsePattern = "";
unsigned int morseIndex = 0;
unsigned long morseElementStartTime = 0;
bool morseLedOn = false;

// Get Morse pattern based on battery state
String getMorsePattern(BatteryState state) {
  switch (state) {
    case STATE_GOOD:
      return "... --. ";        // SG = Status Good
    case STATE_LOW:
      return "--- .-.. ";       // OL = Oh Low
    case STATE_VERY_LOW:
      return "--- ...- ";       // OV = Oh Very low
    case STATE_SHUTDOWN:
      return "... --- ... ... ";  // SOSS = SOS Shutdown
    case STATE_CRITICAL:
      return "... --- ... -.-. ";  // SOSC = SOS Critical
    default:
      return "...- ";           // V = unknown
  }
}

// Non-blocking Morse code LED flasher
void updateMorseLED() {
  unsigned long currentTime = millis();
  
  // Initialize pattern if empty
  if (currentMorsePattern.length() == 0) {
    currentMorsePattern = getMorsePattern(currentState);
    morseIndex = 0;
    morseElementStartTime = currentTime;
    morseLedOn = false;
  }
  
  // Finished pattern? Reset and add inter-word space
  if (morseIndex >= currentMorsePattern.length()) {
    if (!morseLedOn && (currentTime - morseElementStartTime >= MORSE_WORD_SPACE)) {
      // Time to restart pattern
      currentMorsePattern = getMorsePattern(currentState);  // Refresh in case state changed
      morseIndex = 0;
      morseElementStartTime = currentTime;
    }
    return;  // Wait for word space to finish
  }
  
  char currentChar = currentMorsePattern[morseIndex];
  
  if (morseLedOn) {
    // LED is currently ON - check if it's time to turn OFF
    unsigned long onDuration = (currentChar == '.') ? MORSE_DOT_DURATION : MORSE_DASH_DURATION;
    
    if (currentTime - morseElementStartTime >= onDuration) {
      digitalWrite(LED_PIN, LOW);
      morseLedOn = false;
      morseElementStartTime = currentTime;
      morseIndex++;  // Move to next element AFTER turning off
    }
  } else {
    // LED is currently OFF - check if it's time to turn ON next element
    unsigned long offDuration;
    
    if (currentChar == ' ') {
      // Space between letters
      offDuration = MORSE_LETTER_SPACE;
      morseIndex++;  // Skip the space character
      morseElementStartTime = currentTime;
      return;
    } else {
      // Space between dots/dashes within a letter
      offDuration = MORSE_ELEMENT_SPACE;
    }
    
    if (currentTime - morseElementStartTime >= offDuration) {
      // Time to turn on LED for next element
      if (morseIndex < currentMorsePattern.length()) {
        currentChar = currentMorsePattern[morseIndex];
        
        if (currentChar == '.' || currentChar == '-') {
          digitalWrite(LED_PIN, HIGH);
          morseLedOn = true;
          morseElementStartTime = currentTime;
        }
      }
    }
  }
}
```

### Usage in Main Loop

```cpp
void loop() {
  // Read battery voltage
  float batteryVoltage = readBatteryVoltage();
  
  // Update battery state (handles hysteresis & debouncing)
  updateBatteryState(batteryVoltage);
  
  // Update LED Morse pattern continuously (non-blocking)
  updateMorseLED();
  
  // ... rest of application code ...
}
```

### Timing Considerations

**Standard Morse Timing (per ITU):**
- 1 unit = dot duration
- Dash = 3 units
- Space between elements = 1 unit
- Space between letters = 3 units
- Space between words = 7 units

**Speed settings:**
- **6 WPM** (200ms dot): Optimal for LED visibility in field
- **12 WPM** (100ms dot): Faster, still readable
- **20 WPM** (60ms dot): Too fast for visual LED reading

**Formula:** `WPM = 1200 / dot_duration_ms`

### State Change Handling

The Morse pattern automatically updates when battery state changes:

```cpp
void handleStateTransition() {
  previousState = currentState;
  
  // Log state change with timestamp
  printTimestamp();
  logPrint(" Battery state changed: ");
  logPrint(getStateString(previousState));
  logPrint(" → ");
  logPrintln(getStateString(currentState));
  
  // Morse pattern will update on next updateMorseLED() call
  // No need to manually reset - pattern fetched fresh each cycle
}
```

### Benefits

1. **Visual Status:** Instantly see battery health without serial connection
2. **Field Deployment:** Radio operators can see LED from a distance
3. **No Additional Hardware:** Uses existing status LED
4. **Non-blocking:** Doesn't interfere with other real-time tasks
5. **Contextual Messaging:** Pattern immediately indicates severity (S, O, SOS)

---

## Runtime Tracking & State Transition Analytics

### Overview

Track total runtime and time spent in each battery state for performance analysis and battery health monitoring. Essential for:
- Field testing and validation
- Battery capacity estimation
- Identifying premature discharge issues
- Long-term performance trending

### Implementation

```cpp
// Runtime tracking variables
unsigned long startTime = 0;
unsigned long totalRunTime = 0;

// State tracking
BatteryState currentState = STATE_GOOD;
BatteryState previousState = STATE_GOOD;

// Time tracking for each state
unsigned long timeEnteredGood = 0;
unsigned long timeEnteredLow = 0;
unsigned long timeEnteredVeryLow = 0;
unsigned long timeEnteredShutdown = 0;
unsigned long timeEnteredCritical = 0;

// Duration tracking (cumulative)
unsigned long durationInGood = 0;
unsigned long durationInLow = 0;
unsigned long durationInVeryLow = 0;
unsigned long durationInShutdown = 0;

void setup() {
  startTime = millis();
  timeEnteredGood = startTime;  // Assume GOOD at startup
}

void loop() {
  // Update total runtime
  totalRunTime = millis() - startTime;
  
  // ... battery monitoring code ...
}
```

### State Transition Handler

```cpp
void handleStateTransition() {
  unsigned long now = millis();
  
  // Calculate duration in previous state
  unsigned long duration;
  
  switch (previousState) {
    case STATE_GOOD:
      duration = now - timeEnteredGood;
      durationInGood += duration;
      break;
    case STATE_LOW:
      duration = now - timeEnteredLow;
      durationInLow += duration;
      break;
    case STATE_VERY_LOW:
      duration = now - timeEnteredVeryLow;
      durationInVeryLow += duration;
      break;
    case STATE_SHUTDOWN:
      duration = now - timeEnteredShutdown;
      durationInShutdown += duration;
      break;
    case STATE_CRITICAL:
      // Critical state - no cumulative tracking needed
      break;
  }
  
  // Log the transition with timestamp
  printTimestamp();
  logPrint(" State transition: ");
  logPrint(getStateString(previousState));
  logPrint(" → ");
  logPrint(getStateString(currentState));
  logPrint(" (");
  printDuration(duration);
  logPrintln(")");
  
  // Record entry time for new state
  switch (currentState) {
    case STATE_GOOD:
      timeEnteredGood = now;
      break;
    case STATE_LOW:
      timeEnteredLow = now;
      break;
    case STATE_VERY_LOW:
      timeEnteredVeryLow = now;
      break;
    case STATE_SHUTDOWN:
      timeEnteredShutdown = now;
      break;
    case STATE_CRITICAL:
      timeEnteredCritical = now;
      break;
  }
}
```

### Duration Formatting

```cpp
void printDuration(unsigned long milliseconds) {
  unsigned long seconds = milliseconds / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  
  seconds = seconds % 60;
  minutes = minutes % 60;
  
  char buffer[20];
  sprintf(buffer, "%02luh:%02lum:%02lus", hours, minutes, seconds);
  logPrint(buffer);
}
```

### Example Output

```
[2026-02-14 14:00:00] SYSTEM STARTUP: Feb 14, 2026 14:00:00
[2026-02-14 14:00:00] Initial battery: 14.82V (GOOD)

Runtime | ADC | Pin A9 | Battery | Cell Avg | Status
---------+------+---------+---------+----------+-------------------
00h:00m:00s | 770 | 2.48V | 14.82V | 3.71V | [OK] GOOD

[2026-02-14 15:23:15] State transition: GOOD → LOW (01h:23m:15s)

Runtime | ADC | Pin A9 | Battery | Cell Avg | Status
---------+------+---------+---------+----------+-------------------
01h:23m:15s | 696 | 2.24V | 13.48V | 3.37V | [WARN] LOW

[2026-02-14 17:45:02] State transition: LOW → VERY_LOW (02h:21m:47s)
```

### Summary Statistics

Periodically log cumulative statistics:

```cpp
void printStatistics() {
  printTimestamp();
  logPrintln(" === Runtime Statistics ===");
  
  logPrint("Total runtime: ");
  printDuration(totalRunTime);
  logPrintln();
  
  logPrint("Time in GOOD: ");
  printDuration(durationInGood);
  logPrintln();
  
  logPrint("Time in LOW: ");
  printDuration(durationInLow);
  logPrintln();
  
  logPrint("Time in VERY_LOW: ");
  printDuration(durationInVeryLow);
  logPrintln();
  
  logPrint("Time in SHUTDOWN: ");
  printDuration(durationInShutdown);
  logPrintln();
}

// Call every 6 hours or before shutdown
```

### Benefits

1. **Performance Validation:** Verify battery meets expected runtime
2. **Capacity Estimation:** Calculate actual mAh delivered based on discharge curve
3. **Health Monitoring:** Compare discharge patterns over multiple cycles
4. **Field Data:** Real-world performance under RF transmission load
5. **Predictive Maintenance:** Identify batteries that discharge prematurely

---

## Power Management Configuration

### CPU Clock Speed

**Setting:** Teensy 4.1 must be underclocked to **24MHz** for battery life

**Arduino IDE:** Tools → CPU Speed → 24 MHz

**Benefits:**
- Reduces current consumption from ~100mA to ~20-30mA during idle
- Still provides ample processing power for audio and timing tasks
- Does NOT affect Audio Library functionality
- Extends battery life to >24 hours with 5200mAh pack

**Important:** This setting is configured in the Arduino IDE, not in code.

### Deep Sleep (Optional - Future Development)

```cpp
#include <Snooze.h>

SnoozeTimer timer;
SnoozeBlock config(timer);

void enterDeepSleep(unsigned long milliseconds) {
  timer.setTimer(milliseconds);
  Snooze.sleep(config);
  // Current draw: ~5mA during sleep
}

// In main loop - sleep during idle periods
if (currentState == IDLE && timeUntilNextTX > 10000) {
  enterDeepSleep(timeUntilNextTX - 5000); // Wake 5s early
}
```

---

## Morse Code Engine

### Standard ITU Morse Timing

```cpp
const int TONE_FREQ = 800;      // Audio pitch in Hz
const int DOT_LENGTH = 100;     // Dot length in ms (100ms = ~12 WPM)
const int DASH_LENGTH = DOT_LENGTH * 3;
const int INTER_ELEMENT_GAP = DOT_LENGTH;
const int INTER_LETTER_GAP = DOT_LENGTH * 3;
const int INTER_WORD_GAP = DOT_LENGTH * 7;

// WPM Calculation: WPM = 1200 / DOT_LENGTH
// 100ms dot = 12 WPM
// 80ms dot = 15 WPM
// 60ms dot = 20 WPM
```

### Morse Character Map

```cpp
struct MorseChar {
  char letter;
  const char* code;
};

const MorseChar morseTable[] = {
  {'A', ".-"},    {'B', "-..."},  {'C', "-.-."},  {'D', "-.."},
  {'E', "."},     {'F', "..-."},  {'G', "--."},   {'H', "...."},
  {'I', ".."},    {'J', ".---"},  {'K', "-.-"},   {'L', ".-.."},
  {'M', "--"},    {'N', "-."},    {'O', "---"},   {'P', ".--."},
  {'Q', "--.-"},  {'R', ".-."},   {'S', "..."},   {'T', "-"},
  {'U', "..-"},   {'V', "...-"},  {'W', ".--"},   {'X', "-..-"},
  {'Y', "-.--"},  {'Z', "--.."},
  {'0', "-----"}, {'1', ".----"}, {'2', "..---"}, {'3', "...--"},
  {'4', "....-"}, {'5', "....."}, {'6', "-...."}, {'7', "--..."},
  {'8', "---.."}, {'9', "----."},
  {'/', "-..-."}  // Slash for callsign separators
};
```

### Morse Transmission Function (Blocking Example)

```cpp
void sendMorse(const char* text) {
  for (int i = 0; text[i] != '\0'; i++) {
    char c = toupper(text[i]);
    
    if (c == ' ') {
      delay(INTER_WORD_GAP);
      continue;
    }
    
    // Find character in morse table
    const char* code = findMorseCode(c);
    if (code == nullptr) continue;
    
    // Send dots and dashes
    for (int j = 0; code[j] != '\0'; j++) {
      if (code[j] == '.') {
        tone(AUDIO_PIN, TONE_FREQ);
        delay(DOT_LENGTH);
        noTone(AUDIO_PIN);
      } else if (code[j] == '-') {
        tone(AUDIO_PIN, TONE_FREQ);
        delay(DASH_LENGTH);
        noTone(AUDIO_PIN);
      }
      delay(INTER_ELEMENT_GAP);
    }
    delay(INTER_LETTER_GAP);
  }
}

const char* findMorseCode(char c) {
  for (int i = 0; i < sizeof(morseTable) / sizeof(MorseChar); i++) {
    if (morseTable[i].letter == c) {
      return morseTable[i].code;
    }
  }
  return nullptr;
}
```

**Note:** This is a blocking implementation. For production, refactor to non-blocking state machine.

---

## Required Libraries

Prefer these libraries for Teensy 4.1:

- **Audio Library** (native Teensy) - Audio playback and synthesis
- **Snooze Library** - Power management
- **TimeLib** (built-in Teensy) - RTC functions and date/time management
- **SD Library** (built-in) - File access and logging
- **Watchdog_t4** - Safety interlocks

**Installation:** Arduino IDE → Tools → Manage Libraries → Search for library name

---

## Real-Time Clock (RTC) Integration

### TimeLib Setup

The Teensy 4.1 has a built-in battery-backed RTC for accurate timekeeping.

```cpp
#include <TimeLib.h>

// Get time from Teensy's hardware RTC
time_t getTeensy3Time() {
  return Teensy3Clock.get();
}

void setup() {
  Serial.begin(115200);
  
  // Sync TimeLib with hardware RTC
  setSyncProvider(getTeensy3Time);
  
  // Check if time is set
  if (timeStatus() != timeSet) {
    Serial.println("Unable to sync with RTC");
  } else {
    Serial.println("RTC synchronized");
  }
}
```

### Setting the RTC

**Method 1: Arduino IDE (Recommended)**
1. Connect Teensy 4.1
2. Go to `Tools → Set Time`
3. Click OK to set RTC to computer's current time
4. RTC maintains time even after power off (with backup battery)

**Method 2: Programmatically**
```cpp
void setup() {
  // Set specific date/time: Feb 14, 2026 at 2:30 PM
  setTime(14, 30, 0, 14, 2, 2026);  // hour, min, sec, day, month, year
  Teensy3Clock.set(now());           // Write to hardware RTC
}
```

### Timestamp Formatting

```cpp
// ISO 8601 format: [YYYY-MM-DD HH:MM:SS]
void printTimestamp() {
  char timestamp[30];
  sprintf(timestamp, "[%04d-%02d-%02d %02d:%02d:%02d]", 
          year(), month(), day(), hour(), minute(), second());
  Serial.print(timestamp);
}

// Human-readable format: Feb 14, 2026 14:30:00
void printDateTime() {
  const char* monthNames[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
  };
  
  Serial.print(monthNames[month() - 1]);
  Serial.print(" ");
  Serial.print(day());
  Serial.print(", ");
  Serial.print(year());
  Serial.print(" ");
  
  if (hour() < 10) Serial.print("0");
  Serial.print(hour());
  Serial.print(":");
  
  if (minute() < 10) Serial.print("0");
  Serial.print(minute());
  Serial.print(":");
  
  if (second() < 10) Serial.print("0");
  Serial.print(second());
}
```

### Example Usage

```cpp
void logEvent(const char* event) {
  printTimestamp();
  Serial.print(" ");
  Serial.println(event);
}

// Output: [2026-02-14 14:30:05] Battery LOW warning
```

---

## Audio Library Integration (WAV Playback)

### Audio Pipeline Setup

```cpp
#include <Audio.h>
#include <SD.h>

// Audio objects
AudioPlaySdWav       playWav1;
AudioOutputMQS       audioOutput;

// Audio connections
AudioConnection      patchCord1(playWav1, 0, audioOutput, 0);  // Left channel
AudioConnection      patchCord2(playWav1, 1, audioOutput, 1);  // Right channel

void setup() {
  // Allocate audio memory (8 blocks = ~183ms buffer)
  AudioMemory(8);
  
  // Initialize SD card (built-in slot on Teensy 4.1)
  if (!SD.begin(BUILTIN_SDCARD)) {
    Serial.println("ERROR: SD card initialization failed");
    // Handle error - flash LED, log to serial, etc.
  }
}
```

### WAV File Playback

```cpp
void playAudioFile(const char* filename) {
  if (!playWav1.play(filename)) {
    Serial.print("ERROR: Failed to play ");
    Serial.println(filename);
    return;
  }
  
  // Non-blocking check
  while (playWav1.isPlaying()) {
    delay(10);  // Small delay to prevent tight loop
    // Can check other conditions here (battery, watchdog, etc.)
  }
}

// In main transmission sequence:
digitalWrite(PTT_PIN, HIGH);
delay(PTT_PREROLL_MS);  // 1000ms squelch opening time
playAudioFile("FOXID.WAV");
delay(PTT_TAIL_MS);     // 500ms tail
digitalWrite(PTT_PIN, LOW);
```

### SD Card File Requirements

**Format:** FAT32  
**Audio Files:** WAV format (16-bit PCM recommended)  
**Sample Rate:** 22.05kHz or 44.1kHz  
**Channels:** Mono or Stereo  
**Filename:** 8.3 format recommended (e.g., `FOXID.WAV`, `TAUNT01.WAV`)  
**Location:** Root directory of SD card  
**Duration:** Keep files short (< 10 seconds) for reasonable duty cycle

**MQS Output Pins:**
- Pin 10: MQS output (secondary)
- Pin 12: MQS output (primary - used for audio circuit)

---

## Recommended Code Structure

### State Machine Approach

```cpp
enum FoxState {
  IDLE,           // Waiting for next transmission
  PTT_PREROLL,    // PTT keyed, waiting 1000ms
  TRANSMITTING,   // Sending audio/morse
  PTT_TAIL,       // Audio done, waiting 500ms
  BATTERY_CHECK   // Checking voltage between cycles
};

FoxState currentState = IDLE;
unsigned long stateStartTime = 0;
unsigned long lastTransmitTime = 0;

const unsigned long TRANSMIT_INTERVAL = 60000;  // 60 seconds

void loop() {
  unsigned long currentTime = millis();
  
  // SAFETY: Always feed watchdog
  wdt.feed();
  
  switch (currentState) {
    case IDLE:
      // Update LED heartbeat
      updateHeartbeatLED(currentTime);
      
      // Check if time to transmit
      if (currentTime - lastTransmitTime >= TRANSMIT_INTERVAL) {
        currentState = BATTERY_CHECK;
      }
      break;
      
    case BATTERY_CHECK:
      checkBatteryStatus();
      
      // If shutdown was triggered, we never get here
      currentState = PTT_PREROLL;
      stateStartTime = currentTime;
      digitalWrite(PTT_PIN, HIGH);
      digitalWrite(LED_PIN, HIGH);  // TX indicator
      break;
      
    case PTT_PREROLL:
      if (currentTime - stateStartTime >= PTT_PREROLL_MS) {
        currentState = TRANSMITTING;
        stateStartTime = currentTime;
        startTransmission();
      }
      break;
      
    case TRANSMITTING:
      // Check if transmission complete
      if (isTransmissionComplete()) {
        currentState = PTT_TAIL;
        stateStartTime = currentTime;
      }
      break;
      
    case PTT_TAIL:
      if (currentTime - stateStartTime >= PTT_TAIL_MS) {
        digitalWrite(PTT_PIN, LOW);
        digitalWrite(LED_PIN, LOW);
        lastTransmitTime = currentTime;
        currentState = IDLE;
      }
      break;
  }
}
```

### Benefits of State Machine

- Concurrent battery monitoring
- LED heartbeat during idle periods
- Responsive to conditions (button inputs, temperature, etc.)
- Watchdog timer compatibility
- Easy to add new states (e.g., low power mode)

---

## FCC Part 97 Requirements

**Station Identification:**
- Must broadcast callsign every 10 minutes minimum
- Can use CW (Morse code) or voice
- Must be in English or ITU phonetics

**Implementation:**
```cpp
const unsigned long FCC_ID_INTERVAL = 600000;  // 10 minutes
unsigned long lastIdTime = 0;

if (currentTime - lastIdTime >= FCC_ID_INTERVAL) {
  sendMorse("WX7V FOX");  // Replace with your callsign
  lastIdTime = currentTime;
}
```

---

## Coding Standards

### Naming Conventions

```cpp
// Constants: UPPER_CASE_WITH_UNDERSCORES
const int PTT_PIN = 2;
const long TRANSMIT_INTERVAL = 60000;

// Variables: camelCase
unsigned long lastTransmitTime = 0;
bool isPttActive = false;

// Functions: camelCase with descriptive names
void keyRadio();
void sendMorseCode(const char* text);
unsigned long calculateDutyCycle();
```

### Comments

```cpp
// Document WHY, not WHAT (code should be self-documenting)

// SAFETY: Force PTT off after timeout to prevent stuck key
if (pttDuration > MAX_PTT_TIME) {
  emergencyPttRelease();
}

// Baofeng requires 1000ms pre-roll for squelch opening
delay(PTT_PREROLL_MS);

// FCC Part 97.119(a) - Station ID required every 10 minutes
if (idRequired) {
  sendStationID();
}
```

### Error Handling

```cpp
// Check for valid states before operations
if (isPttActive && (millis() - pttStartTime > MAX_PTT_TIME)) {
  // SAFETY: Force PTT off after timeout
  emergencyPttRelease();
  logError("PTT timeout exceeded");
}

// Validate sensor readings
float voltage = readBatteryVoltage();
if (voltage < 10.0 || voltage > 17.0) {
  // Reading out of range - sensor error
  logError("Battery voltage sensor malfunction");
  useDefaultBehavior();
}
```

---

## Reference Code Implementations

Two working examples are available for reference:

### 1. Morse Code Controller

**File:** `Morse Code Controller.mdc`

**Features:**
- Simple blocking implementation for CW identification
- Complete alphanumeric character set (A-Z, 0-9)
- 800Hz tone generation via `tone()` function
- Pin 12 for audio output, Pin 2 for PTT control

**Configuration:**
```cpp
const char* callsign = "WX7V FOX";  // Customize your callsign
const int toneFreq = 800;           // Audio pitch in Hz
const int dotLen = 100;             // Dot length (100ms = 12 WPM)
const long transmitInterval = 60000; // 60 seconds between TX
```

**Notes:**
- Uses `delay()` - suitable for simple dedicated fox boxes
- For production with concurrent tasks, refactor to non-blocking state machine

### 2. Audio Controller

**File:** `Audio Controller Code.mdc`

**Features:**
- WAV file playback using Teensy Audio Library
- SD card support (built-in slot on Teensy 4.1)
- MQS output on Pins 10 & 12
- Automatic playback completion detection

**Configuration:**
```cpp
const long transmitInterval = 60000;  // 60 seconds between TX
AudioMemory(8);                       // 8 audio blocks allocated
```

**Notes:**
- `while(playWav1.isPlaying())` loop is blocking - refactor for production
- SD card initialization is blocking - will halt if SD card missing

---

## Future Development Ideas

### Hybrid Implementation

Combine Morse and audio for unified controller:
- Morse code ID every N transmissions
- Voice/audio files on alternating transmissions
- Random pattern selection for increased challenge

```cpp
int transmissionCounter = 0;

void selectTransmissionType() {
  transmissionCounter++;
  
  if (transmissionCounter % 5 == 0) {
    // Every 5th transmission: Morse ID (FCC requirement)
    sendMorse("WX7V FOX");
  } else {
    // Other transmissions: Random audio file
    playRandomTaunt();
  }
}
```

### Randomized Taunts

```cpp
const char* audioFiles[] = {
  "FOXID.WAV",
  "TAUNT01.WAV",
  "TAUNT02.WAV",
  "CATCHME.WAV",
  "CLOSER.WAV"
};
const int numFiles = 5;

void playRandomTaunt() {
  int randomIndex = random(numFiles);
  playAudioFile(audioFiles[randomIndex]);
}

void setup() {
  randomSeed(analogRead(A0));  // Seed from floating pin noise
}
```

### Remote Control via DTMF (Advanced)

**Hardware Required:** MT8870 DTMF Decoder IC

```cpp
const int DTMF_D0 = 3;
const int DTMF_D1 = 4;
const int DTMF_D2 = 5;
const int DTMF_D3 = 6;
const int DTMF_STD = 7;  // Valid tone detected

// Commands:
// *123# - Trigger immediate transmission
// *456# - Enable continuous mode
// *789# - Request battery status
// *000# - Disable fox (stealth mode)
```

---

## Required Features

### 1. Morse Code Engine
- Configurable WPM (Words Per Minute)
- Standard ITU Morse timing
- Clean tone generation (800Hz default, minimize harmonics)
- Non-blocking implementation using state machine

### 2. Audio Integration
- Teensy Audio Library for .wav playback
- SD card support for voice files (FAT32, 16-bit PCM WAV)
- Proper audio filtering (1kΩ resistor + 10µF cap + 10kΩ pot)
- Graceful degradation if SD card missing/files unavailable

### 3. Battery Watchdog System
- Dual-threshold protection (13.6V soft warning, 12.8V hard shutdown)
- Voltage divider monitoring on Pin A9 (10kΩ + 2.0kΩ)
- Hysteresis to prevent warning spam
- Permanent PTT disable on hard shutdown
- SOS LED pattern when in shutdown mode

### 4. Safety Interlocks
- Watchdog timer (30s timeout) to prevent stuck PTT
- Duty cycle management to prevent radio overheating
- Emergency PTT release mechanisms
- Non-blocking architecture for responsive monitoring

---

## Testing Checklist

Before marking any feature complete:

- [ ] Non-blocking behavior verified (no `delay()` calls in critical paths)
- [ ] PTT timing verified (1000ms pre-roll, proper tail)
- [ ] Watchdog timer tested (intentional hang recovery)
- [ ] Duty cycle limits enforced
- [ ] Low-battery condition handled (soft warning + hard shutdown)
- [ ] Battery false-alarm prevention tested (hysteresis + debouncing working)
- [ ] Audio quality checked (clean tones, no distortion)
- [ ] Power consumption measured
- [ ] FCC ID requirement met (every 10 minutes)
- [ ] SD card logging functional (writes on state changes, survives SD card removal)
- [ ] SD card handling graceful (continues without SD if not needed)

---

## Debugging & Serial Output

```cpp
void setup() {
  Serial.begin(9600);
  while (!Serial && millis() < 3000); // Wait up to 3s for serial
  
  Serial.println("WX7V Fox Controller v1.0");
  Serial.println("Initializing...");
}

void loop() {
  // Log important events
  if (currentState == PTT_PREROLL) {
    Serial.println("PTT keyed - pre-roll started");
  }
  
  if (batteryWarningActive) {
    Serial.print("Battery voltage: ");
    Serial.print(readBatteryVoltage(), 2);
    Serial.println("V (LOW)");
  }
}
```

**Note:** Serial output adds ~5-10mA current draw. Disable in production for maximum battery life.

---

## Performance Characteristics

### Duty Cycle Examples

**Example 1: 60-second interval, 5-second transmission (Recommended)**
- Duty Cycle: (5s / 60s) × 100% = **8.3%**
- Safe for continuous operation
- Battery life: **>24 hours** with 5200mAh @ 24MHz + 1W
- Thermal stress: Minimal

**Example 2: 30-second interval, 10-second transmission**
- Duty Cycle: (10s / 30s) × 100% = **33.3%**
- ⚠️ May cause radio overheating in warm environments
- Recommended maximum: **20% duty cycle**
- Battery life: ~12-15 hours

---

## Summary

This software reference provides all the requirements and examples needed to implement a safe, reliable fox controller. Key priorities:

1. **Safety First:** Watchdog timer, battery protection, duty cycle limits
2. **Non-Blocking Design:** State machines, `millis()` timing, responsive loop
3. **Radio Timing:** 1000ms pre-roll, 500ms tail for Baofeng compatibility
4. **FCC Compliance:** Station ID every 10 minutes
5. **Power Efficiency:** 24MHz CPU, deep sleep during idle

**See Hardware_Reference.md for physical wiring and component details.**
