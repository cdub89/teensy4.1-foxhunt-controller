This Markdown file is structured as a **Project Specification** that Cursor AI (or any LLM-based editor) can ingest to understand the hardware constraints, pinout, and your desired feature set.

Save this as fox\_controller\_project.md in your project root.

# ---

**Project: WX7V Foxhunt Controller**

## **1\. Project Overview**

The goal is to build a robust, dual-band radio "fox" (hidden transmitter) using a **Teensy 4.1** and a **Baofeng UV-5R**. The system must manage power from a **14.8V 4S LiPo**, interface with the radio's PTT/Audio, and provide automated Morse code ID and voice announcements.

## **2\. Hardware Environment**

* **Controller:** Teensy 4.1 (Cortex-M7 @ 600MHz)  
* **Radio:** Baofeng UV-5R (Standard Kenwood 2-pin interface)  
* **Power Source:** SOCOKIN 5200mAh 14.8V 4S1P LiPo  
* **Regulators:** \* Buck Converter A: **8.0V** (Radio Power via Battery Eliminator)  
  * Buck Converter B: **5.0V** (Teensy VIN Power)  
* **Safety:** 5A Inline Fuse on the main XT60 battery lead.

## **3\. Pinout & Wiring Map**

| Component | Teensy 4.1 Pin | Notes |
| :---- | :---- | :---- |
| **PTT Control** | Pin 2 | Digital Output \-\> 1k Resistor \-\> 2N2222 Base |
| **Audio Out** | Pin 12 | PWM/MQS \-\> 1k Resistor \-\> 10uF Cap \-\> 10k Pot \-\> Radio Mic In |
| **System LED** | Pin 13 | Built-in LED (Heartbeat/TX indicator) |
| **Battery Monitor** | Pin A9 | Analog Input via Voltage Divider (R1=10kΩ, R2=2.2kΩ) |
| **Power In** | VIN | 5.0V from Buck Converter B |
| **Ground** | GND | Common Ground with Radio and Buck Converters |

**See Also:** [Hardware Reference](Teensy4%20Fox%20Hardware%20Reference.md) for complete wiring summary, pigtail color codes, and audio filtering details.

## **4\. Required Features for Development**

* **Morse Code Engine:** Convert strings to CW (Continuous Wave) tones at a configurable WPM (Words Per Minute).  
* **Duty Cycle Management:** Configurable "On-time" and "Off-time" to prevent radio overheating.  
* **Audio Library Integration:** Utilize the Teensy Audio Library to play .wav files from the onboard SD card (e.g., voice ID or taunts).  
* **Low Power Mode:** Implementation of Snooze or deep sleep libraries to maximize battery life during long hunts.  
* **Safety Interlocks:** A "Watchdog" timer to ensure the PTT doesn't get stuck in the HIGH position if the code crashes.  
* **Battery Watchdog:** Real-time voltage monitoring with soft warning alerts and hard shutdown protection for LiPo cell safety.

## **5\. Implementation Notes for Cursor AI**

When generating code, prioritize **non-blocking** architecture using millis() rather than delay(). The Baofeng requires a **1000ms "Pre-roll"** after PTT is keyed before audio begins to ensure the squelch on receiving radios has opened.

**Important:** The audio circuit includes a **1kΩ resistor** before the 10µF capacitor to create a low-pass filter that smooths MQS output and prevents radio distortion. See [Hardware Reference](Teensy4%20Fox%20Hardware%20Reference.md) for audio filtering details and optional isolation transformer information.

## **6\. Reference Code Implementations**

Two working implementations are provided as reference:

### **6.1 Morse Code Controller** (`Morse Code Controller.mdc`)

**Purpose:** Simple blocking implementation for Morse code CW identification

**Features:**
- Standard ITU Morse code timing
- Configurable WPM via `dotLen` parameter (100ms = ~12 WPM)
- Complete alphanumeric character set (A-Z, 0-9)
- 800Hz tone generation via `tone()` function
- Pin 12 for audio output, Pin 2 for PTT control

**Configuration Parameters:**
```cpp
const char* callsign = "YOURCALL FOX";  // Customize your callsign
const int toneFreq = 800;               // Audio pitch in Hz
const int dotLen = 100;                 // Dot length (100ms = 12 WPM)
const long transmitInterval = 60000;    // 60 seconds between transmissions
```

**Timing Characteristics:**
- 1000ms PTT pre-roll (squelch opening time)
- 500ms PTT tail after transmission
- ITU standard spacing: 3× dot length between letters, 7× for word spaces

**Notes:**
- This is a **blocking** implementation using `delay()` - suitable for simple dedicated fox boxes
- For production use with concurrent tasks, refactor to non-blocking state machine
- Total transmission time: ~1000ms + (Morse duration) + 500ms

### **6.2 Audio Controller** (`Audio Controller Code.mdc`)

**Purpose:** WAV file playback using Teensy Audio Library and SD card

**Features:**
- Teensy Audio Library integration
- SD card support (uses built-in SD card slot on Teensy 4.1)
- MQS (Medium Quality Sound) output on Pins 10 & 12
- Stereo audio support
- Automatic playback completion detection

**Hardware Requirements:**
- SD card formatted as FAT32
- Audio files in WAV format (suggested: 16-bit, 44.1kHz or 22.05kHz)
- File naming: 8.3 format recommended (e.g., `FOXID.WAV`)

**Audio Pipeline:**
```
SD Card → AudioPlaySdWav → AudioOutputMQS → Pin 12 → Radio Mic
```

**Configuration:**
```cpp
const long transmitInterval = 60000;     // 60 seconds between transmissions
AudioMemory(8);                          // 8 audio blocks allocated
```

**Timing Characteristics:**
- 1000ms PTT pre-roll (squelch opening time)
- Variable audio duration (depends on WAV file length)
- 500ms PTT tail after playback
- 25ms startup delay for Audio Library initialization

**Notes:**
- Uses `AudioOutputMQS` which outputs on Pins 10 & 12 (Pin 12 is the primary signal)
- The `while(playWav1.isPlaying())` loop is **blocking** - consider refactoring for production
- SD card initialization is **blocking** and will halt if SD card is missing
- Serial output at 9600 baud for debugging

**SD Card Files:**
- Place WAV files in the root directory of the SD card
- Filename in code: `FOXID.WAV` (customize as needed)
- Recommended audio specs: 16-bit PCM, mono or stereo, 22.05kHz or 44.1kHz sample rate
- Keep files short (< 10 seconds) to maintain reasonable duty cycle

## **7\. Future Development Recommendations**

### **7.1 Non-Blocking Architecture**

Both reference implementations use blocking code (`delay()` and `while` loops). For production use, consider:

**State Machine Approach:**
```cpp
enum FoxState { IDLE, PTT_PREROLL, TRANSMITTING, PTT_TAIL };
FoxState currentState = IDLE;
unsigned long stateStartTime = 0;
```

**Benefits:**
- Concurrent battery monitoring
- LED heartbeat during idle periods
- Responsive button inputs (if added)
- Watchdog timer compatibility

### **7.2 Hybrid Implementation**

Combine both features into a unified controller:
- Morse code ID every N transmissions
- Voice/audio files on alternating transmissions
- Random pattern selection for increased challenge

### **7.3 Power Management**

Implement `Snooze` library during idle periods:
- Deep sleep between transmissions
- Wake on timer or external interrupt
- Estimated battery life: >24 hours with 5200mAh pack

### **7.4 Safety Enhancements**

**Critical for Production:**
- Watchdog timer (max PTT timeout: 30 seconds)
- Battery voltage monitoring with dual-threshold protection
- Thermal management (duty cycle limit: 20% max)
- Emergency PTT release on fault conditions

#### **7.4.1 Battery Watchdog System**

**Purpose:** Protect LiPo battery from over-discharge while providing early warning to field operators

**Hardware Configuration:**

**Voltage Divider Circuit:**
- R1 = 10kΩ (between battery positive and Pin A9)
- R2 = 2.2kΩ (between Pin A9 and ground)
- Divider ratio: R2 / (R1 + R2) = 2.2kΩ / 12.2kΩ = 0.1803
- Input voltage range: 12.8V - 16.8V (4S LiPo)
- Output voltage range: 2.31V - 3.03V (safe for Teensy 3.3V ADC)

**Voltage Mapping Table:**

| Battery Voltage | Cell Voltage | Pin A9 Voltage | ADC Reading (10-bit) | Status |
|----------------|--------------|----------------|---------------------|---------|
| 16.8V | 4.20V | 3.03V | 936 | Fully Charged |
| 14.8V | 3.70V | 2.67V | 824 | Nominal |
| 13.6V | 3.40V | 2.45V | 757 | ⚠️ Soft Warning |
| 12.8V | 3.20V | 2.31V | 713 | 🛑 Hard Shutdown |
| 12.0V | 3.00V | 2.16V | 667 | ⚠️ Critical Damage Risk |

**Voltage Divider Formula:**
\[V_{out} = V_{in} \times \frac{R2}{R1 + R2}\]

**Reverse Calculation (ADC to Battery Voltage):**
\[V_{battery} = \frac{ADC_{reading} \times 3.3V}{1023} \times \frac{R1 + R2}{R2}\]
\[V_{battery} = \frac{ADC_{reading} \times 3.3V}{1023} \times 5.545\]

**Implementation Requirements:**

**Soft Warning Threshold (13.6V / 3.4V per cell):**
- Trigger condition: Battery voltage < 13.6V
- Action: Play "LOW BATTERY" warning via WAV file or Morse code
- Frequency: Alert once every 5 transmission cycles (not every cycle)
- Behavior: Continue normal operation but notify operators
- LED indication: Change blink pattern to rapid pulse (200ms on/off)
- Hysteresis: Once triggered, require 14.0V to clear warning

**Hard Shutdown Threshold (12.8V / 3.2V per cell):**
- Trigger condition: Battery voltage < 12.8V
- Action: Immediate and permanent PTT disable
- Behavior: 
  * Set PTT pin to INPUT (high-impedance) to prevent accidental keying
  * Disable all audio output
  * Flash LED in SOS pattern (... --- ...) continuously
  * Log shutdown event to SD card (if available)
  * Enter deep sleep mode with periodic wake (every 60 seconds) to continue SOS LED
- No recovery without power cycle: Prevent repeated discharge attempts
- Serial log: "CRITICAL: Battery protection triggered at XX.XVdc"

**Monitoring Implementation:**
```cpp
const int BATTERY_PIN = A9;
const float VOLTAGE_DIVIDER_RATIO = 5.545;  // (R1 + R2) / R2 = 12.2k / 2.2k
const float SOFT_WARNING_THRESHOLD = 13.6;   // 3.4V per cell
const float HARD_SHUTDOWN_THRESHOLD = 12.8;  // 3.2V per cell
const float WARNING_CLEAR_THRESHOLD = 14.0;  // Hysteresis

bool batteryWarningActive = false;
bool batteryShutdownActive = false;
int warningCounter = 0;

float readBatteryVoltage() {
  int rawValue = analogRead(BATTERY_PIN);
  float pinVoltage = (rawValue / 1023.0) * 3.3;
  float batteryVoltage = pinVoltage * VOLTAGE_DIVIDER_RATIO;
  return batteryVoltage;
}

void checkBatteryStatus() {
  float voltage = readBatteryVoltage();
  
  // CRITICAL: Hard shutdown check
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
  
  // Play warning every 5 cycles
  if (batteryWarningActive) {
    warningCounter++;
    if (warningCounter >= 5) {
      playLowBatteryWarning(voltage);
      warningCounter = 0;
    }
  }
}

void triggerBatteryShutdown(float voltage) {
  batteryShutdownActive = true;
  
  // Immediately disable PTT
  pinMode(PTT_PIN, INPUT);  // High impedance
  
  // Log critical event
  Serial.print("CRITICAL: Battery shutdown at ");
  Serial.print(voltage, 2);
  Serial.println("V");
  
  // Log to SD card if available
  logCriticalEvent(voltage);
  
  // Enter emergency mode with SOS LED
  while (true) {
    blinkSOS();
    Snooze.sleep(60000); // Wake every 60s to continue SOS
  }
}
```

**Calibration Procedure:**
1. Connect fully charged 4S LiPo (16.8V)
2. Read ADC value from Pin A9
3. Calculate actual divider ratio: `ratio = 16.8V / pinVoltage`
4. Update `VOLTAGE_DIVIDER_RATIO` constant with measured value
5. Verify accuracy at 14.8V (nominal) and 13.6V (warning) using load test
6. Document actual ratio in code comments

**Safety Notes:**
- Never bypass hard shutdown threshold
- Voltage divider resistors should be ±1% tolerance for accuracy
- Consider 100nF ceramic capacitor across R2 to filter noise
- ADC readings should be averaged (10 samples) to reduce noise
- Test with actual battery under load (radio transmitting) for accurate readings
- Over-discharge below 3.0V/cell (12.0V) can permanently damage LiPo cells

### **7.5 Advanced Features (Future Development)**

#### **7.5.1 Randomized Taunts**

**Purpose:** Increase hunt difficulty and entertainment by varying audio content

**Implementation:**
- Store multiple WAV files on SD card (`TAUNT01.WAV`, `TAUNT02.WAV`, etc.)
- Use `random()` or `randomSeed(analogRead(A0))` for true randomization
- Create a playlist array and randomly select indices
- Mix Morse code ID with random audio files

**Example Structure:**
```cpp
const char* audioFiles[] = {
  "FOXID.WAV",
  "TAUNT01.WAV",
  "TAUNT02.WAV",
  "CATCHME.WAV",
  "CLOSER.WAV"
};
int numFiles = 5;
int randomIndex = random(numFiles);
```

**Benefits:**
- Prevents hunters from timing transmissions by content
- Adds entertainment value
- Can include tactical hints or misdirection

#### **7.5.2 Battery Monitoring (Legacy Reference)**

**Note:** This section is superseded by the comprehensive Battery Watchdog System in section 7.4.1. The information below is preserved for historical reference only.

**Purpose:** Real-time voltage monitoring with audio alerts for low battery conditions

**Hardware Requirements:**
- Analog input pin (recommend **Pin A0** or **Pin A9**)
- Voltage divider to scale 14.8V → 3.3V range
- Resistor values: R1 = 47kΩ, R2 = 10kΩ (scales ~16.8V to 2.95V)

**Voltage Divider Calculation:**
```
Vout = Vin × (R2 / (R1 + R2))
14.8V × (10kΩ / 57kΩ) = 2.60V
```

**Implementation:**
```cpp
const int BATTERY_PIN = A0;
const float VOLTAGE_DIVIDER_RATIO = 5.7;  // (R1 + R2) / R2
const float LOW_BATTERY_THRESHOLD = 13.2; // 3.3V per cell
const float CRITICAL_BATTERY = 12.8;      // 3.2V per cell (shutdown)

float readBatteryVoltage() {
  int rawValue = analogRead(BATTERY_PIN);
  float voltage = (rawValue / 1023.0) * 3.3 * VOLTAGE_DIVIDER_RATIO;
  return voltage;
}
```

**Features:**
- Periodic voltage checks during idle periods
- Play "LOW BATTERY" WAV file when < 13.2V
- Automatic shutdown at < 12.8V (prevent cell damage)
- Serial logging of voltage readings for post-hunt analysis
- LED blink pattern changes on low battery

**Safety Considerations:**
- Never discharge LiPo below 3.0V per cell (12.0V for 4S)
- Implement hysteresis to prevent alert spam
- Log battery state to SD card for diagnostics

#### **7.5.3 Remote Control via DTMF/VOX**

**Purpose:** Allow fox to respond to specific radio commands (DTMF tones) or operate in silent mode until "called"

**Option A: DTMF Decoder Integration**

**Hardware:**
- MT8870 DTMF Decoder IC
- Connect to audio output of radio (before speaker)
- Digital outputs to Teensy pins (D0-D3 for digit, StD for valid tone)

**Pin Assignments:**
```cpp
const int DTMF_D0 = 3;
const int DTMF_D1 = 4;
const int DTMF_D2 = 5;
const int DTMF_D3 = 6;
const int DTMF_STD = 7;  // Valid tone detected
```

**Commands:**
- `*123#` - Trigger immediate transmission
- `*456#` - Enable continuous mode
- `*789#` - Request battery status
- `*000#` - Disable fox (stealth mode)

**Option B: VOX (Voice-Activated) Mode**

**Hardware:**
- Use radio's existing VOX circuitry (if available)
- Monitor squelch/audio detect pin from radio
- Alternative: Simple envelope detector on speaker output

**Implementation:**
```cpp
const int VOX_DETECT_PIN = 8;
bool foxEnabled = false;

void loop() {
  if (digitalRead(VOX_DETECT_PIN) == HIGH) {
    // Radio detected incoming signal
    delay(500); // Wait for transmission to end
    foxEnabled = true; // Enable fox for next cycle
  }
  
  if (foxEnabled) {
    transmitFox();
    foxEnabled = false; // Return to silent mode
  }
}
```

**Features:**
- Silent operation until activated by hunters
- Requires hunters to "wake up" the fox with a transmission
- Increases difficulty - no predictable timing
- Can implement cooldown period (5-10 minutes) after activation

**Use Cases:**
- Advanced hunts where fox location changes
- Multi-fox events with selective activation
- Training scenarios with controlled transmission timing
- Battery conservation for extended events (>24 hours)

## **8\. Performance Characteristics**

### **8.1 Current Consumption (Estimated)**

| Mode | Current Draw | Notes |
|------|-------------|-------|
| **Idle (CPU active)** | ~100mA | Teensy 4.1 at 600MHz |
| **Transmitting (Morse)** | ~150mA | Teensy + low-level audio |
| **Transmitting (Audio)** | ~180mA | Audio library overhead |
| **Deep Sleep** | ~5mA | With Snooze library |
| **Radio TX (8W)** | ~1.5A | From Buck A (8V supply) |

**Note:** Radio current comes from Buck Converter A (8V rail), not the Teensy 5V rail

### **8.2 Duty Cycle Analysis**

**Example: 60-second interval, 5-second transmission**
- Duty Cycle: (5s / 60s) × 100% = **8.3%**
- Safe for continuous operation
- Battery life estimate: ~18-20 hours with 5200mAh pack

**Example: 30-second interval, 10-second transmission**
- Duty Cycle: (10s / 30s) × 100% = **33.3%**
- ⚠️ May cause radio overheating in warm environments
- Recommended maximum: 20% duty cycle

### ---

