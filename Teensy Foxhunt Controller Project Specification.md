This Markdown file is structured as a **Project Specification** that Cursor AI (or any LLM-based editor) can ingest to understand the hardware constraints, pinout, and your desired feature set.

Save this as fox\_controller\_project.md in your project root.

# ---

**Project: Teensy 4.1 Advanced Foxhunt Controller**

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
| **Audio Out** | Pin 12 | PWM/MQS \-\> 10uF Cap \-\> 10k Pot \-\> Radio Mic In |
| **System LED** | Pin 13 | Built-in LED (Heartbeat/TX indicator) |
| **Power In** | VIN | 5.0V from Buck Converter B |
| **Ground** | GND | Common Ground with Radio and Buck Converters |

## **4\. Required Features for Development**

* **Morse Code Engine:** Convert strings to CW (Continuous Wave) tones at a configurable WPM (Words Per Minute).  
* **Duty Cycle Management:** Configurable "On-time" and "Off-time" to prevent radio overheating.  
* **Audio Library Integration:** Utilize the Teensy Audio Library to play .wav files from the onboard SD card (e.g., voice ID or taunts).  
* **Low Power Mode:** Implementation of Snooze or deep sleep libraries to maximize battery life during long hunts.  
* **Safety Interlocks:** A "Watchdog" timer to ensure the PTT doesn't get stuck in the HIGH position if the code crashes.

## **5\. Implementation Notes for Cursor AI**

When generating code, prioritize **non-blocking** architecture using millis() rather than delay(). The Baofeng requires a **1000ms "Pre-roll"** after PTT is keyed before audio begins to ensure the squelch on receiving radios has opened.

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
- Battery voltage monitoring (shutdown < 13.2V)
- Thermal management (duty cycle limit: 20% max)
- Emergency PTT release on fault conditions

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

#### **7.5.2 Battery Monitoring**

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

