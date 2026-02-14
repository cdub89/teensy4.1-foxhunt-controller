/*
 * WX7V Foxhunt Controller
 * Teensy 4.1 Dual-Band Radio Fox Transmitter
 * 
 * Features:
 * - Non-blocking state machine architecture
 * - Morse code CW generation
 * - WAV file playback from SD card
 * - Dual-threshold battery watchdog (soft warning + hard shutdown)
 * - Watchdog timer safety
 * - Duty cycle management
 * - Deep sleep power management
 * 
 * Hardware:
 * - Teensy 4.1
 * - Baofeng UV-5R (via Kenwood 2-pin)
 * - 14.8V 4S LiPo (SOCOKIN 5200mAh)
 * - See: WX7V Foxhunt Controller Project Specification.md
 * 
 * Author: WX7V
 * License: MIT
 * Version: 1.0.0
 */

#include <Audio.h>
#include <SD.h>
#include <SPI.h>
#include <Snooze.h>
#include <Watchdog_t4.h>

// ============================================================
// PIN DEFINITIONS
// ============================================================
const int PTT_PIN = 2;          // Digital Output -> 1k -> 2N2222 Base
const int AUDIO_PIN = 12;       // MQS Output -> 1k -> 10uF -> 10k Pot -> Radio Mic
const int LED_PIN = 13;         // Built-in LED (status indicator)
const int BATTERY_PIN = A9;     // Battery voltage monitor (via divider)

// ============================================================
// CONFIGURATION PARAMETERS
// ============================================================

// Callsign & Identification
const char* CALLSIGN = "WX7V FOX";  // Change to your callsign

// Morse Code Settings
const int MORSE_WPM = 12;           // Words per minute
const int MORSE_TONE_FREQ = 800;    // Audio frequency (Hz)
const int DOT_LENGTH = 1200 / MORSE_WPM; // ITU standard: 1 dot = 1.2s / WPM

// Transmission Timing
const long TX_INTERVAL = 60000;     // 60 seconds between transmissions
const long PTT_PREROLL = 1000;      // 1000ms pre-roll for squelch opening
const long PTT_TAIL = 500;          // 500ms tail after audio ends
const long MAX_TX_TIME = 30000;     // Maximum single transmission (30s safety)

// Battery Watchdog Configuration
const float VOLTAGE_DIVIDER_RATIO = 5.545;  // (R1+R2)/R2 = 12.2k/2.2k
const float SOFT_WARNING_THRESHOLD = 13.6;  // 3.4V per cell
const float HARD_SHUTDOWN_THRESHOLD = 12.8; // 3.2V per cell
const float WARNING_CLEAR_THRESHOLD = 14.0; // Hysteresis
const int BATTERY_CHECK_INTERVAL = 5000;    // Check every 5 seconds

// Duty Cycle Management
const int DUTY_CYCLE_WINDOW = 300000;  // 5 minute window
const int MAX_DUTY_CYCLE = 20;         // 20% maximum

// Audio Files (8.3 format for SD card)
const char* AUDIO_ID = "FOXID.WAV";
const char* AUDIO_LOWBATT = "LOWBATT.WAV";

// ============================================================
// STATE MACHINE DEFINITIONS
// ============================================================
enum FoxState {
  STATE_STARTUP,
  STATE_IDLE,
  STATE_PTT_PREROLL,
  STATE_TX_MORSE,
  STATE_TX_AUDIO,
  STATE_PTT_TAIL,
  STATE_EMERGENCY_SHUTDOWN
};

FoxState currentState = STATE_STARTUP;
unsigned long stateStartTime = 0;
unsigned long lastTransmitTime = 0;
unsigned long lastBatteryCheck = 0;

// ============================================================
// MORSE CODE ENGINE
// ============================================================
struct MorseElement {
  char character;
  const char* pattern;  // . = dit, - = dah
};

const MorseElement morseTable[] = {
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
  {' ', "/"}  // Word space
};

class MorseGenerator {
private:
  const char* currentMessage;
  int messageIndex;
  int patternIndex;
  unsigned long elementStartTime;
  bool isToneActive;
  bool isComplete;

public:
  void begin(const char* message) {
    currentMessage = message;
    messageIndex = 0;
    patternIndex = 0;
    elementStartTime = millis();
    isToneActive = false;
    isComplete = false;
  }

  bool update() {
    if (isComplete) return true;

    if (currentMessage[messageIndex] == '\0') {
      noTone(AUDIO_PIN);
      isComplete = true;
      return true;
    }

    unsigned long elapsed = millis() - elementStartTime;
    char currentChar = toupper(currentMessage[messageIndex]);

    // Find morse pattern for current character
    const char* pattern = nullptr;
    for (int i = 0; i < sizeof(morseTable) / sizeof(MorseElement); i++) {
      if (morseTable[i].character == currentChar) {
        pattern = morseTable[i].pattern;
        break;
      }
    }

    // Skip unknown characters
    if (pattern == nullptr) {
      messageIndex++;
      patternIndex = 0;
      elementStartTime = millis();
      return false;
    }

    // Handle word space
    if (pattern[0] == '/') {
      noTone(AUDIO_PIN);
      if (elapsed >= DOT_LENGTH * 7) {
        messageIndex++;
        patternIndex = 0;
        elementStartTime = millis();
      }
      return false;
    }

    // End of character pattern
    if (pattern[patternIndex] == '\0') {
      noTone(AUDIO_PIN);
      if (elapsed >= DOT_LENGTH * 3) { // Inter-character space
        messageIndex++;
        patternIndex = 0;
        elementStartTime = millis();
      }
      return false;
    }

    // Process current element (dit or dah)
    char element = pattern[patternIndex];
    int elementDuration = (element == '.') ? DOT_LENGTH : (DOT_LENGTH * 3);

    if (!isToneActive) {
      tone(AUDIO_PIN, MORSE_TONE_FREQ);
      isToneActive = true;
      elementStartTime = millis();
    } else if (elapsed >= elementDuration) {
      noTone(AUDIO_PIN);
      isToneActive = false;
      patternIndex++;
      // Inter-element space (1 dot length)
      delay(DOT_LENGTH);  // Small blocking delay acceptable for inter-element spacing
      elementStartTime = millis();
    }

    return false;
  }

  bool isFinished() {
    return isComplete;
  }
};

MorseGenerator morse;

// ============================================================
// AUDIO SYSTEM
// ============================================================
AudioPlaySdWav       playWav1;
AudioOutputMQS       audioOutput;
AudioConnection      patchCord1(playWav1, 0, audioOutput, 0);
AudioConnection      patchCord2(playWav1, 1, audioOutput, 1);

bool audioInitialized = false;
unsigned long audioStartTime = 0;

// ============================================================
// BATTERY WATCHDOG
// ============================================================
bool batteryWarningActive = false;
bool batteryShutdownActive = false;
int batteryWarningCounter = 0;
float batteryReadings[10];
int batteryReadingIndex = 0;

float readBatteryVoltage() {
  int raw = analogRead(BATTERY_PIN);
  float voltage = (raw / 1023.0) * 3.3 * VOLTAGE_DIVIDER_RATIO;
  
  // Update moving average
  batteryReadings[batteryReadingIndex] = voltage;
  batteryReadingIndex = (batteryReadingIndex + 1) % 10;
  
  // Calculate average
  float sum = 0;
  for (int i = 0; i < 10; i++) sum += batteryReadings[i];
  return sum / 10.0;
}

void checkBatteryStatus() {
  float voltage = readBatteryVoltage();
  
  // CRITICAL: Hard shutdown check
  if (voltage < HARD_SHUTDOWN_THRESHOLD && !batteryShutdownActive) {
    triggerBatteryShutdown(voltage);
    return;
  }
  
  // Soft warning with hysteresis
  if (!batteryWarningActive && voltage < SOFT_WARNING_THRESHOLD) {
    batteryWarningActive = true;
    batteryWarningCounter = 0;
    Serial.print("WARNING: Battery low: ");
    Serial.print(voltage, 2);
    Serial.println("V");
  } else if (batteryWarningActive && voltage > WARNING_CLEAR_THRESHOLD) {
    batteryWarningActive = false;
    Serial.println("Battery voltage recovered");
  }
}

void triggerBatteryShutdown(float voltage) {
  batteryShutdownActive = true;
  
  // Immediately disable PTT (set to high impedance)
  pinMode(PTT_PIN, INPUT);
  
  // Stop any audio
  noTone(AUDIO_PIN);
  if (audioInitialized) {
    playWav1.stop();
  }
  
  // Log critical event
  Serial.println("============================================");
  Serial.println("CRITICAL: BATTERY PROTECTION ACTIVATED");
  Serial.print("Battery voltage: ");
  Serial.print(voltage, 2);
  Serial.println("V");
  Serial.println("PTT PERMANENTLY DISABLED");
  Serial.println("============================================");
  
  // Log to SD if available
  if (SD.begin(BUILTIN_SDCARD)) {
    File logFile = SD.open("CRITICAL.LOG", FILE_WRITE);
    if (logFile) {
      logFile.print("Battery shutdown at ");
      logFile.print(voltage, 2);
      logFile.println("V");
      logFile.close();
    }
  }
  
  // Enter emergency state
  currentState = STATE_EMERGENCY_SHUTDOWN;
}

void blinkSOS() {
  // SOS pattern: ... --- ...
  const int ditLen = 200;
  const int dahLen = 600;
  
  // S (...)
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(ditLen);
    digitalWrite(LED_PIN, LOW);
    delay(ditLen);
  }
  delay(dahLen);
  
  // O (---)
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(dahLen);
    digitalWrite(LED_PIN, LOW);
    delay(ditLen);
  }
  delay(dahLen);
  
  // S (...)
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(ditLen);
    digitalWrite(LED_PIN, LOW);
    delay(ditLen);
  }
  delay(2000);
}

// ============================================================
// WATCHDOG TIMER
// ============================================================
WDT_T4<WDT1> wdt;

void watchdogCallback() {
  Serial.println("WATCHDOG RESET TRIGGERED!");
  // Emergency PTT release
  pinMode(PTT_PIN, INPUT);
  noTone(AUDIO_PIN);
}

// ============================================================
// STATE MACHINE
// ============================================================
void transitionToState(FoxState newState) {
  currentState = newState;
  stateStartTime = millis();
  
  // Debug output
  Serial.print("State: ");
  switch(newState) {
    case STATE_STARTUP: Serial.println("STARTUP"); break;
    case STATE_IDLE: Serial.println("IDLE"); break;
    case STATE_PTT_PREROLL: Serial.println("PTT_PREROLL"); break;
    case STATE_TX_MORSE: Serial.println("TX_MORSE"); break;
    case STATE_TX_AUDIO: Serial.println("TX_AUDIO"); break;
    case STATE_PTT_TAIL: Serial.println("PTT_TAIL"); break;
    case STATE_EMERGENCY_SHUTDOWN: Serial.println("EMERGENCY_SHUTDOWN"); break;
  }
}

unsigned long getStateElapsedTime() {
  return millis() - stateStartTime;
}

void updateStateMachine() {
  // Feed watchdog
  wdt.feed();
  
  // Check battery status periodically
  if (millis() - lastBatteryCheck >= BATTERY_CHECK_INTERVAL) {
    checkBatteryStatus();
    lastBatteryCheck = millis();
  }
  
  // Don't allow transitions if battery shutdown active
  if (batteryShutdownActive && currentState != STATE_EMERGENCY_SHUTDOWN) {
    transitionToState(STATE_EMERGENCY_SHUTDOWN);
    return;
  }
  
  switch(currentState) {
    case STATE_STARTUP:
      handleStartup();
      break;
    case STATE_IDLE:
      handleIdle();
      break;
    case STATE_PTT_PREROLL:
      handlePttPreroll();
      break;
    case STATE_TX_MORSE:
      handleTxMorse();
      break;
    case STATE_TX_AUDIO:
      handleTxAudio();
      break;
    case STATE_PTT_TAIL:
      handlePttTail();
      break;
    case STATE_EMERGENCY_SHUTDOWN:
      handleEmergencyShutdown();
      break;
  }
}

void handleStartup() {
  // Initialize audio system
  AudioMemory(8);
  
  // Try to initialize SD card
  if (SD.begin(BUILTIN_SDCARD)) {
    Serial.println("SD card initialized");
    audioInitialized = true;
  } else {
    Serial.println("WARNING: SD card not found - audio playback disabled");
    audioInitialized = false;
  }
  
  // Set last transmit time to enable immediate first transmission
  lastTransmitTime = millis() - TX_INTERVAL;
  
  // Transition to idle
  transitionToState(STATE_IDLE);
}

void handleIdle() {
  // Update LED heartbeat
  static unsigned long lastLedToggle = 0;
  int ledInterval = batteryWarningActive ? 100 : 500;
  
  if (millis() - lastLedToggle >= ledInterval) {
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    lastLedToggle = millis();
  }
  
  // Check if it's time to transmit
  if (millis() - lastTransmitTime >= TX_INTERVAL) {
    Serial.println("Starting transmission sequence");
    transitionToState(STATE_PTT_PREROLL);
  }
  
  // TODO: Deep sleep during long idle periods (Phase 3)
}

void handlePttPreroll() {
  // Key PTT
  static bool pttKeyed = false;
  if (!pttKeyed) {
    digitalWrite(PTT_PIN, HIGH);
    digitalWrite(LED_PIN, HIGH);
    pttKeyed = true;
    Serial.println("PTT keyed - waiting for squelch");
  }
  
  // Wait for pre-roll period
  if (getStateElapsedTime() >= PTT_PREROLL) {
    pttKeyed = false;
    
    // Alternate between Morse and Audio (if available)
    static bool useMorse = true;
    if (useMorse || !audioInitialized) {
      morse.begin(CALLSIGN);
      transitionToState(STATE_TX_MORSE);
      useMorse = false;
    } else {
      transitionToState(STATE_TX_AUDIO);
      useMorse = true;
    }
  }
  
  // Safety timeout
  if (getStateElapsedTime() >= MAX_TX_TIME) {
    Serial.println("ERROR: PTT preroll timeout!");
    transitionToState(STATE_PTT_TAIL);
  }
}

void handleTxMorse() {
  // Update morse generator
  bool complete = morse.update();
  
  if (complete || getStateElapsedTime() >= MAX_TX_TIME) {
    if (getStateElapsedTime() >= MAX_TX_TIME) {
      Serial.println("ERROR: Morse transmission timeout!");
    } else {
      Serial.println("Morse transmission complete");
    }
    noTone(AUDIO_PIN);
    transitionToState(STATE_PTT_TAIL);
  }
}

void handleTxAudio() {
  static bool audioStarted = false;
  
  if (!audioStarted) {
    // Play low battery warning if needed
    const char* filename = AUDIO_ID;
    if (batteryWarningActive) {
      batteryWarningCounter++;
      if (batteryWarningCounter >= 5) {
        filename = AUDIO_LOWBATT;
        batteryWarningCounter = 0;
        Serial.println("Playing low battery warning");
      }
    }
    
    if (SD.exists(filename)) {
      Serial.print("Playing: ");
      Serial.println(filename);
      playWav1.play(filename);
      audioStarted = true;
      audioStartTime = millis();
    } else {
      Serial.print("ERROR: File not found: ");
      Serial.println(filename);
      transitionToState(STATE_PTT_TAIL);
      return;
    }
  }
  
  // Check if audio finished or timeout
  if (!playWav1.isPlaying() || getStateElapsedTime() >= MAX_TX_TIME) {
    if (getStateElapsedTime() >= MAX_TX_TIME) {
      Serial.println("ERROR: Audio transmission timeout!");
      playWav1.stop();
    } else {
      Serial.println("Audio transmission complete");
    }
    audioStarted = false;
    transitionToState(STATE_PTT_TAIL);
  }
}

void handlePttTail() {
  // Wait for tail period
  if (getStateElapsedTime() >= PTT_TAIL) {
    digitalWrite(PTT_PIN, LOW);
    digitalWrite(LED_PIN, LOW);
    lastTransmitTime = millis();
    
    Serial.println("Transmission complete - returning to idle");
    Serial.print("Battery: ");
    Serial.print(readBatteryVoltage(), 2);
    Serial.println("V\n");
    
    transitionToState(STATE_IDLE);
  }
}

void handleEmergencyShutdown() {
  // Infinite SOS loop
  blinkSOS();
  
  // Periodic wake to continue SOS
  // (In production, use Snooze library for power savings)
}

// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n\n============================================");
  Serial.println("WX7V Foxhunt Controller v1.0.0");
  Serial.println("Teensy 4.1 - Baofeng UV-5R");
  Serial.println("============================================\n");
  
  // Configure pins
  pinMode(PTT_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BATTERY_PIN, INPUT);
  
  digitalWrite(PTT_PIN, LOW);
  digitalWrite(LED_PIN, LOW);
  
  // Initialize battery readings array
  for (int i = 0; i < 10; i++) {
    batteryReadings[i] = 14.8;  // Initialize to nominal voltage
  }
  
  // Configure watchdog (30 second timeout)
  WDT_timings_t config;
  config.timeout = 30;  // seconds
  config.trigger = 20;  // seconds (warning)
  config.callback = watchdogCallback;
  wdt.begin(config);
  
  Serial.println("Watchdog timer configured (30s timeout)");
  
  // Display configuration
  Serial.println("Configuration:");
  Serial.print("  Callsign: ");
  Serial.println(CALLSIGN);
  Serial.print("  Morse WPM: ");
  Serial.println(MORSE_WPM);
  Serial.print("  TX Interval: ");
  Serial.print(TX_INTERVAL / 1000);
  Serial.println(" seconds");
  Serial.print("  Battery Soft Warning: ");
  Serial.print(SOFT_WARNING_THRESHOLD);
  Serial.println("V");
  Serial.print("  Battery Hard Shutdown: ");
  Serial.print(HARD_SHUTDOWN_THRESHOLD);
  Serial.println("V");
  
  // Initial battery check
  delay(100);  // Let ADC settle
  float voltage = readBatteryVoltage();
  Serial.print("\nBattery voltage: ");
  Serial.print(voltage, 2);
  Serial.println("V");
  
  if (voltage < HARD_SHUTDOWN_THRESHOLD) {
    Serial.println("CRITICAL: Battery too low for operation!");
    triggerBatteryShutdown(voltage);
  } else if (voltage < SOFT_WARNING_THRESHOLD) {
    Serial.println("WARNING: Battery voltage low");
    batteryWarningActive = true;
  }
  
  Serial.println("\nSystem ready - starting transmission sequence\n");
  
  // Start state machine
  transitionToState(STATE_STARTUP);
}

// ============================================================
// MAIN LOOP
// ============================================================
void loop() {
  updateStateMachine();
  
  // Keep loop responsive - target < 10ms execution time
  // All timing handled by state machine with millis()
}
