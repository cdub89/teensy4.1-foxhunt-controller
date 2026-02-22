/*
 * WX7V Foxhunt Controller - Production Version
 * 
 * VERSION: 1.3
 * 
 * PURPOSE: Production foxhunt controller with random WAV selection and battery monitoring
 * 
 * HARDWARE:
 * - Teensy 4.1 with SD card
 * - Audio Output: Pin 12 (MQS audio via hardware PWM)
 * - PTT circuit: Pin 2 → 1kΩ → 2N2222 Base
 * - Battery Monitor: Pin A9 (10kΩ + 2.0kΩ voltage divider)
 * - Status LED: Pin 13 (Morse code battery status)
 * - Radio: Baofeng UV-5R via Kenwood K connector
 * 
 * CRITICAL REQUIREMENT:
 * - Teensy must run at 150 MHz or higher for WAV playback
 * - Set in Arduino IDE: Tools → CPU Speed → 150 MHz (or higher)
 * - Lower speeds will cause WAV files to fail playback
 * 
 * AUDIO OUTPUT:
 *   Pin 12 (MQS output) → 10µF Cap (+)
 *   10µF Cap (−) → 1kΩ Resistor → Potentiometer (High Leg)
 *   Potentiometer (Wiper) → Radio Mic In (3.5mm ring)
 *   Potentiometer (Low Leg) → System Ground (2.5mm sleeve)
 * 
 * AUDIO ARCHITECTURE:
 *   AudioPlaySdWav (WAV player) ──┐
 *   AudioSynthWaveformSine (Morse)─┤──> AudioMixer4 ──> AudioOutputMQS (Pin 12)
 *   
 *   Both WAV and Morse use Audio Library to avoid pin conflicts
 *   Audio Memory: 40 blocks allocated (peak usage typically 2-5 blocks)
 * 
 * BATTERY MONITORING:
 *   Battery (+) → 10kΩ → Pin A9 → 2.0kΩ → GND
 *   Voltage Divider Ratio: 6.0 (scales 14.6V → 2.43V at pin)
 *   5 States: GOOD, LOW, VERY_LOW, SHUTDOWN, CRITICAL
 *   Debouncing: 3 consecutive readings required for state change
 * 
 * SD CARD REQUIREMENTS:
 * - Format: FAT32
 * - Audio Files: WAV format (16-bit signed PCM)
 * - Sample Rate: 44.1kHz (preferred) or 22.05kHz
 * - Channels: Mono (remove all metadata from WAV files)
 * - Filename: 8.3 format (e.g., FOXID.WAV, TEST.WAV)
 * - Random Selection: Controller automatically finds all WAV files on SD card
 * 
* TRANSMISSION SEQUENCE (5 parts):
* 1. PTT ON + 1000ms pre-roll
* 2. Morse: "V V V C" (Test/Confirmed) at 20 WPM
* 3. WAV: Play randomly-selected file
*    - If file < 30 seconds: Loop until 30 seconds elapsed
*    - If file >= 30 seconds: Play entire file to completion
* 4. Morse: "WX7V/F" (Station ID - FCC required)
* 5. PTT OFF after 500ms tail
* 
* FIELD MODE TIMING:
* - WAV Playback: Minimum 30 seconds (looped) or full file length
* - Cycle Interval: 240 seconds (4 minutes, start-to-start)
* - Approximate idle time: ~3.5 minutes between transmissions
 * 
* BATTERY SAFETY:
* - Monitors voltage every 500ms with debouncing
* - SHUTDOWN state (10.2V): Completes current transmission (FCC ID required), 
*   then disables future transmissions
* - CRITICAL state (< 10.2V): Emergency PTT off, halt system
* - LED shows battery status via Morse code patterns
 * 
 * LED MORSE CODE PATTERNS:
 * - SG  (... --. ) = STATUS GOOD
 * - OL  (--- .-..) = OVERLOAD/LOW
 * - OV  (--- ...-) = OVERLOAD/VERY LOW
 * - SOSS(SOS ...) = EMERGENCY/SHUTDOWN
 * - SOSC(SOS -.-.) = EMERGENCY/CRITICAL
 * 
 * USAGE:
 * 1. Insert SD card with WAV files into Teensy 4.1 built-in slot
 * 2. Set CPU Speed: Arduino IDE → Tools → CPU Speed → 150 MHz (or higher)
 * 3. Configure CALLSIGN_ID constant below (search for "CONFIGURATION")
 * 4. Upload to Teensy 4.1 via Arduino IDE
 * 5. Open Tools → Serial Monitor (115200 baud)
 * 6. Controller runs autonomously in field mode with random file selection
 * 
 * LOGGING:
 * - Serial Monitor: Real-time status at 115200 baud (always active)
 * - SD Card: FOXCTRL.LOG (dual output, disabled during WAV playback)
 * - Note: SD logging temporarily disabled during WAV playback to prevent card contention
 * - Includes timestamps, battery voltage, transmission status, selected WAV file
 * 
 * CRITICAL TECHNICAL NOTES:
 * - SD Card Contention: Audio Library and logging cannot access SD card simultaneously
 *   Solution: Logging temporarily disabled during WAV playback (Serial Monitor continues)
 * - Audio Memory: 40 blocks allocated (peak usage typically 2-5 blocks for mono WAV)
 * - Reset Detection: System reports reset cause at startup (Teensy 4.x SRC_SRSR register)
 * - File Validation: WAV files are pre-checked before playback to prevent crashes
 * 
 * SAFETY FEATURES:
 * - Battery voltage monitoring with debouncing
 * - Low voltage protection (LVP) with graceful shutdown
 * - PTT timeout watchdog (90s max per transmission)
 * - FCC-compliant: Always transmits ID before shutdown
 * 
 * AUTHOR: WX7V
 * DATE: 2026-02-22
 * 
 * VERSION HISTORY:
 * v1.3 - Field logging optimization + PTT rolloff fix (2026-02-22)
 *        - ADDED: Field Mode Logging flag (FIELD_MODE_LOGGING constant)
 *        - Field mode reduces SD card operations by ~75% for 24-hour deployment
 *        - Bench mode preserves detailed step-by-step logging for debugging
 *        - Field mode logs: TX count, battery, selected file, errors only
 *        - CRITICAL FIX: Moved SD card logging AFTER PTT release in Part 5
 *        - Bug: SD card open/close operations were adding ~4 seconds of delay
 *        - PTT now releases exactly 500ms after station ID completes
 *        - Field testing revealed logging delays were extending PTT time to ~5 seconds
 * v1.2 - Critical stability fixes (2026-02-20)
 *        - FIXED: SD card contention crash (logging disabled during WAV playback)
 *        - Increased audio memory from 16 to 40 blocks for stability
 *        - Added reset cause detection for Teensy 4.x (SRC_SRSR register)
 *        - Added audio memory usage monitoring for diagnostics
 *        - Added WAV file validation before playback attempts
 *        - Improved error handling and logging safety
 * v1.1 - Random WAV file selection feature
 *        - Automatic SD card scanning for all WAV files
 *        - Intelligent playback: loop if <30s, play to completion if >=30s
 *        - No manual filename configuration required
 * v1.0 - Initial production release
 *        - Integrated audio_wav_test.ino (v1.6) transmission logic
 *        - Integrated battery_monitor_test.ino (v2.1) safety monitoring
 *        - Field mode default: 30s WAV + 240s interval
 *        - Battery-aware transmission control with FCC-compliant shutdown
 */

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <TimeLib.h>  // Built-in Teensy time library

// ============================================================================
// VERSION INFORMATION
// ============================================================================
const char* VERSION = "1.3";
const char* VERSION_DATE = "2026-02-22";

// ============================================================================
// CONFIGURATION - EDIT THESE FOR YOUR DEPLOYMENT
// ============================================================================
const char* CALLSIGN_ID = "WX7V/F";      // FCC station ID (change this!)

// Field Mode Logging: Reduces SD card operations for 24-hour field deployment
// true  = Minimal logging (TX count, battery, errors only) - recommended for field
// false = Detailed logging (step-by-step progress) - use for bench testing/debugging
const bool FIELD_MODE_LOGGING = true;

// ============================================================================
// PIN DEFINITIONS
// ============================================================================
const int PTT_PIN = 2;        // Pin 2 - PTT control (via 2N2222 transistor)
const int BATTERY_PIN = A9;   // Pin 23 - Battery voltage input
const int LED_PIN = 13;       // Built-in LED for battery status indication
// Audio Output: Pin 12 (MQS - controlled by Audio Library)

// ============================================================================
// TIMING CONFIGURATION - Field Mode
// ============================================================================
const unsigned long PLAYBACK_DURATION_MS = 30000;   // 30 seconds WAV playback (field mode)
const unsigned long CYCLE_INTERVAL_MS = 240000;     // 4 minutes between transmissions (start-to-start)

const unsigned long PTT_PREROLL_MS = 1000;   // Baofeng squelch opening time
const unsigned long PTT_TAIL_MS = 500;       // Hold PTT after audio ends
const unsigned long PTT_TIMEOUT_MS = 90000;  // SAFETY: 90s max transmit time

// ============================================================================
// MORSE CODE CONFIGURATION (20 WPM)
// ============================================================================
const int MORSE_TONE_FREQ = 800;             // Hz (standard CW tone)
const int MORSE_DOT_DURATION = 60;           // ms (20 WPM: 60ms dot)

// ============================================================================
// BATTERY MONITORING CONFIGURATION
// ============================================================================
// Voltage divider: 10kΩ + 2.0kΩ = ratio 6.0
const float VOLTAGE_DIVIDER_RATIO = 6.0;
const float ADC_REFERENCE_VOLTAGE = 3.3;     // Volts
const int ADC_RESOLUTION = 10;               // 10-bit ADC (0-1023)
const int ADC_MAX_VALUE = 1023;

// Battery thresholds (updated for field conditions)
const float VOLTAGE_GOOD = 12.0;             // Healthy operation
const float VOLTAGE_LOW = 11.0;              // Getting low - monitor closely
const float VOLTAGE_VERY_LOW = 10.5;         // Warning zone - nearing shutdown
const float VOLTAGE_SHUTDOWN = 10.2;         // Stop transmissions (FCC ID first)

// Debounce settings
const int STATE_CHANGE_DEBOUNCE_COUNT = 3;   // Require 3 consecutive readings
const int NUM_SAMPLES = 10;                  // ADC samples to average
const unsigned long BATTERY_CHECK_INTERVAL = 500;  // Check battery every 500ms

// Morse LED timing (slower for visual readability ~6 WPM)
const int MORSE_LED_DOT = 200;
const int MORSE_LED_DASH = 600;
const int MORSE_LED_ELEMENT_SPACE = 200;
const int MORSE_LED_LETTER_SPACE = 600;
const int MORSE_LED_WORD_SPACE = 1400;

// ============================================================================
// AUDIO LIBRARY OBJECTS
// ============================================================================
AudioPlaySdWav           playWav1;       // WAV file player
AudioSynthWaveformSine   toneMorse;      // Sine wave generator for Morse code
AudioMixer4              mixer1;         // Mix WAV and Morse audio
AudioOutputMQS           mqs1;           // MQS output (Pin 12)

// Audio connections - WAV and Morse through mixer to MQS
AudioConnection      patchCord1(playWav1, 0, mixer1, 0);  // WAV left -> mixer ch0
AudioConnection      patchCord2(playWav1, 1, mixer1, 1);  // WAV right -> mixer ch1
AudioConnection      patchCord3(toneMorse, 0, mixer1, 2); // Morse tone -> mixer ch2
AudioConnection      patchCord4(mixer1, 0, mqs1, 0);      // Mixer out -> MQS left
AudioConnection      patchCord5(mixer1, 0, mqs1, 1);      // Mixer out -> MQS right (mono)

// ============================================================================
// BATTERY STATE TRACKING
// ============================================================================
enum BatteryState {
  STATE_GOOD,
  STATE_LOW,
  STATE_VERY_LOW,
  STATE_SHUTDOWN,
  STATE_CRITICAL
};

BatteryState currentBatteryState = STATE_GOOD;
BatteryState previousBatteryState = STATE_GOOD;
BatteryState pendingBatteryState = STATE_GOOD;
int batteryDebounceCounter = 0;

bool transmissionsDisabled = false;      // Set true when battery too low
bool currentTransmissionAllowed = true;  // Allow current TX to complete

// ============================================================================
// WAV FILE MANAGEMENT
// ============================================================================
const int MAX_WAV_FILES = 20;        // Maximum number of WAV files to track
String wavFiles[MAX_WAV_FILES];      // Array of WAV filenames
int wavFileCount = 0;                // Actual number of WAV files found

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================
unsigned long lastTransmissionTime = 0;
unsigned long lastBatteryCheckTime = 0;
unsigned long pttStartTime = 0;
unsigned long playbackStartTime = 0;
bool isPlaying = false;
bool pttActive = false;
unsigned long transmissionCount = 0;

// SD Card logging
File logFile;
bool loggingEnabled = true;
const char* LOG_FILENAME = "FOXCTRL.LOG";

// Morse LED state machine
String currentMorsePattern = "";
unsigned int morseIndex = 0;
bool morseElementActive = false;
unsigned long morseLastChange = 0;

// ============================================================================
// FUNCTION PROTOTYPES
// ============================================================================
void scanSDCardForWavFiles();
String selectRandomWavFile();
float readBatteryVoltage();
void updateBatteryState(float voltage);
void handleBatteryStateTransition();
String getBatteryStateName(BatteryState state);
String getMorseBatteryPattern(BatteryState state);
void updateMorseLED();
void runTransmissionCycle();
void sendMorseMessage(const char* message);
void sendDit(int dotDuration);
void sendDah(int dashDuration, int dotDuration);
String getMorsePattern(char c);
void logPrint(const char* str);
void logPrint(const String& str);
void logPrint(int val);
void logPrint(float val, int decimals);
void logPrintln(const char* str);
void logPrintln(const String& str);
void logPrintln();
void printTimestamp();
time_t getTeensy3Time();

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {
    // Wait for Serial Monitor to open (max 3 seconds)
  }
  
  Serial.println("\n\n========================================");
  Serial.println("WX7V Foxhunt Controller - Production");
  Serial.print("Version: ");
  Serial.println(VERSION);
  Serial.print("Date: ");
  Serial.println(VERSION_DATE);
  Serial.println("========================================\n");
  
  // Detect and report reset cause (Teensy 4.x uses SRC_SRSR register)
  uint32_t resetCause = SRC_SRSR;
  Serial.print("Reset Cause: 0x");
  Serial.print(resetCause, HEX);
  Serial.print(" - ");
  
  if (resetCause & 0x0001) Serial.println("IPP Reset");
  else if (resetCause & 0x0004) Serial.println("CSU Reset");
  else if (resetCause & 0x0008) Serial.println("IPP User Reset");
  else if (resetCause & 0x0010) Serial.println("WDOG1 Reset (Watchdog)");
  else if (resetCause & 0x0020) Serial.println("JTAG Reset");
  else if (resetCause & 0x0040) Serial.println("JTAG Software Reset");
  else if (resetCause & 0x0080) Serial.println("WDOG3 Reset (Watchdog 3)");
  else if (resetCause & 0x0100) Serial.println("TEMPSENSE Reset (Overheating)");
  else if (resetCause & 0x0200) Serial.println("CSU Alarm Reset");
  else if (resetCause & 0x10000) Serial.println("WDOG4 Reset (Watchdog 4)");
  else Serial.println("Power-On Reset or Unknown");
  Serial.println();
  
  // Initialize pin modes
  pinMode(PTT_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BATTERY_PIN, INPUT);
  digitalWrite(PTT_PIN, LOW);   // PTT off
  digitalWrite(LED_PIN, LOW);   // LED off
  
  // Set ADC resolution
  analogReadResolution(ADC_RESOLUTION);
  
  Serial.println("Pin Configuration:");
  Serial.println("  PTT:     Pin 2 (OUTPUT)");
  Serial.println("  Battery: Pin A9 (INPUT)");
  Serial.println("  LED:     Pin 13 (OUTPUT - Battery status)");
  Serial.println("  Audio:   Pin 12 (MQS output via Audio Library)");
  Serial.println();
  
  // Initialize Audio Library
  AudioMemory(40);  // Allocate 40 audio memory blocks (increased for WAV playback stability)
  Serial.println("Audio Library initialized (40 blocks)");
  
  // Configure mixer gains
  mixer1.gain(0, 0.5);  // WAV left channel at 50% volume
  mixer1.gain(1, 0.5);  // WAV right channel at 50% volume
  mixer1.gain(2, 0.8);  // Morse tone at 80% volume
  mixer1.gain(3, 0.0);  // Channel 3 unused (muted)
  Serial.println("Mixer gains configured");
  
  // Configure Morse tone generator
  toneMorse.frequency(MORSE_TONE_FREQ);
  toneMorse.amplitude(0.0);  // Start muted
  Serial.print("Morse tone: ");
  Serial.print(MORSE_TONE_FREQ);
  Serial.println(" Hz at 20 WPM");
  Serial.println();
  
  // Set up Teensy RTC
  setSyncProvider(getTeensy3Time);
  
  // Initialize SD card
  Serial.print("Initializing SD card...");
  if (!SD.begin(BUILTIN_SDCARD)) {
    Serial.println(" FAILED!");
    Serial.println("ERROR: Cannot access SD card. Please check:");
    Serial.println("  1. SD card is inserted in Teensy 4.1 slot");
    Serial.println("  2. SD card is FAT32 formatted");
    Serial.println("  3. Card is not write-protected");
    loggingEnabled = false;
    while (1) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      delay(100);  // Fast blink = SD error
    }
  }
  Serial.println(" OK");
  
  // Scan SD card for WAV files
  Serial.println();
  Serial.print("Scanning SD card for WAV files...");
  scanSDCardForWavFiles();
  
  if (wavFileCount == 0) {
    Serial.println(" NONE FOUND!");
    Serial.println("ERROR: No WAV files found on SD card.");
    Serial.println("Please add at least one WAV file (16-bit PCM, 44.1kHz or 22.05kHz)");
    loggingEnabled = false;
    while (1) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      delay(300);  // Medium blink = no WAV files
    }
  }
  
  Serial.print(" Found ");
  Serial.print(wavFileCount);
  Serial.println(" file(s):");
  for (int i = 0; i < wavFileCount; i++) {
    Serial.print("  ");
    Serial.print(i + 1);
    Serial.print(". ");
    Serial.println(wavFiles[i]);
  }
  Serial.println();
  
  // Seed random number generator for file selection
  randomSeed(analogRead(BATTERY_PIN) + millis());
  Serial.println("Random number generator seeded for file selection");
  
  // Check for WAV file (legacy code removed - now dynamic)
  Serial.print("WAV playback mode: Random selection from ");
  Serial.print(wavFileCount);
  Serial.println(" available files");
  
  // Open log file and write header
  if (loggingEnabled) {
    logFile = SD.open(LOG_FILENAME, FILE_WRITE);
    if (logFile) {
      logFile.println();
      logFile.println("========================================");
      logFile.print("WX7V Foxhunt Controller v");
      logFile.print(VERSION);
      logFile.println(" - STARTUP");
      logFile.print("Time: ");
      logFile.print(year());
      logFile.print("-");
      logFile.print(month());
      logFile.print("-");
      logFile.print(day());
      logFile.print(" ");
      logFile.print(hour());
      logFile.print(":");
      logFile.print(minute());
      logFile.print(":");
      logFile.println(second());
      logFile.println("========================================");
      logFile.close();
      Serial.print("Logging to: ");
      Serial.println(LOG_FILENAME);
    } else {
      Serial.println("WARNING: Could not open log file");
      loggingEnabled = false;
    }
  }
  
  // Display configuration
  Serial.println("\n========================================");
  Serial.println("CONFIGURATION");
  Serial.println("========================================");
  Serial.print("WAV Files: ");
  Serial.print(wavFileCount);
  Serial.println(" files (random selection)");
  Serial.print("Station ID: ");
  Serial.println(CALLSIGN_ID);
  Serial.print("Minimum Playback: ");
  Serial.print(PLAYBACK_DURATION_MS / 1000);
  Serial.println(" seconds (loop if shorter)");
  Serial.print("Cycle Interval: ");
  Serial.print(CYCLE_INTERVAL_MS / 1000);
  Serial.println(" seconds");
  Serial.print("Logging Mode: ");
  Serial.println(FIELD_MODE_LOGGING ? "FIELD (Minimal)" : "BENCH (Detailed)");
  Serial.println();
  
  Serial.println("TRANSMISSION SEQUENCE:");
  Serial.println("  1. PTT ON + 1000ms pre-roll");
  Serial.println("  2. Morse: 'V V V C' (Test/Confirmed)");
  Serial.println("  3. WAV: Play random file (loop if <30s, or play to completion)");
  Serial.print("  4. Morse: '");
  Serial.print(CALLSIGN_ID);
  Serial.println("' (Station ID)");
  Serial.println("  5. PTT OFF after 500ms tail");
  Serial.println();
  
  Serial.println("BATTERY MONITORING:");
  Serial.print("  GOOD:       >= ");
  Serial.print(VOLTAGE_GOOD);
  Serial.println("V");
  Serial.print("  LOW:        >= ");
  Serial.print(VOLTAGE_LOW);
  Serial.println("V");
  Serial.print("  VERY_LOW:   >= ");
  Serial.print(VOLTAGE_VERY_LOW);
  Serial.println("V");
  Serial.print("  SHUTDOWN:   >= ");
  Serial.print(VOLTAGE_SHUTDOWN);
  Serial.println("V");
  Serial.print("  CRITICAL:   < ");
  Serial.print(VOLTAGE_SHUTDOWN);
  Serial.println("V");
  Serial.println("  Debouncing: 3 consecutive readings");
  Serial.println();
  
  // Read initial battery voltage
  float initialVoltage = readBatteryVoltage();
  Serial.print("Initial Battery Voltage: ");
  Serial.print(initialVoltage, 2);
  Serial.println("V");
  
  // Initialize Morse LED pattern
  currentMorsePattern = getMorseBatteryPattern(currentBatteryState);
  morseLastChange = millis();
  
  Serial.println("\n========================================");
  Serial.println("System Ready - Starting in 10 seconds...");
  Serial.println("========================================\n");
  
  // Wait before first transmission
  delay(10000);
  lastTransmissionTime = millis() - CYCLE_INTERVAL_MS;  // Trigger first transmission
  lastBatteryCheckTime = millis();
}

// ============================================================================
// MAIN LOOP
// ============================================================================
void loop() {
  unsigned long currentTime = millis();
  
  // ========== BATTERY MONITORING (Non-blocking, every 500ms) ==========
  if (currentTime - lastBatteryCheckTime >= BATTERY_CHECK_INTERVAL) {
    lastBatteryCheckTime = currentTime;
    
    float batteryVoltage = readBatteryVoltage();
    updateBatteryState(batteryVoltage);
    
    // CRITICAL: Emergency shutdown on CRITICAL voltage
    if (currentBatteryState == STATE_CRITICAL) {
      logPrintln();
      logPrintln("========================================");
      logPrintln("CRITICAL: Battery voltage critically low!");
      logPrint("Voltage: ");
      logPrint(batteryVoltage, 2);
      logPrintln("V");
      logPrintln("EMERGENCY: Forcing PTT off immediately");
      logPrintln("========================================");
      
      digitalWrite(PTT_PIN, LOW);
      pttActive = false;
      
      while(1) {
        // Halt system - flash LED rapidly
        digitalWrite(LED_PIN, HIGH);
        delay(100);
        digitalWrite(LED_PIN, LOW);
        delay(100);
      }
    }
  }
  
  // ========== PTT TIMEOUT WATCHDOG ==========
  if (pttActive && (currentTime - pttStartTime > PTT_TIMEOUT_MS)) {
    logPrintln();
    logPrintln("SAFETY: PTT timeout exceeded - forcing PTT off");
    digitalWrite(PTT_PIN, LOW);
    pttActive = false;
    
    // Reset for next cycle
    lastTransmissionTime = millis();
  }
  
  // ========== TRANSMISSION CYCLE TIMING (Non-blocking) ==========
  if (!transmissionsDisabled && currentTime - lastTransmissionTime >= CYCLE_INTERVAL_MS) {
    lastTransmissionTime = currentTime;
    runTransmissionCycle();
  }
  
  // ========== MORSE LED UPDATE (Non-blocking) ==========
  updateMorseLED();
  
  // Loop runs continuously with no blocking delays
}

// ============================================================================
// TRANSMISSION CYCLE
// ============================================================================
void runTransmissionCycle() {
  transmissionCount++;
  
  // ALWAYS log transmission header (critical for field tracking)
  logPrintln();
  logPrintln("========================================");
  logPrint("TRANSMISSION CYCLE #");
  logPrintln((int)transmissionCount);
  printTimestamp();
  logPrintln();
  
  // ALWAYS log battery voltage (critical safety monitoring)
  float batteryVoltage = readBatteryVoltage();
  logPrint("Battery Voltage: ");
  logPrint(batteryVoltage, 2);
  logPrint("V (");
  logPrint(getBatteryStateName(currentBatteryState));
  logPrintln(")");
  
  // Check if battery is in SHUTDOWN state
  if (currentBatteryState == STATE_SHUTDOWN) {
    logPrintln("WARNING: Battery at SHUTDOWN threshold");
    logPrintln("This will be the FINAL transmission (FCC ID required)");
    transmissionsDisabled = true;  // Disable future transmissions
    currentTransmissionAllowed = true;  // Allow this final TX
  }
  
  logPrintln("========================================");
  
  // ============== PART 1: PTT ON + PRE-ROLL ==============
  if (!FIELD_MODE_LOGGING) {
    logPrintln("\n[1/5] PTT ON + Pre-roll");
  }
  digitalWrite(PTT_PIN, HIGH);
  pttActive = true;
  pttStartTime = millis();
  if (!FIELD_MODE_LOGGING) {
    logPrint("  PTT keyed, waiting ");
    logPrint((int)PTT_PREROLL_MS);
    logPrintln(" ms for radio squelch...");
  }
  delay(PTT_PREROLL_MS);
  if (!FIELD_MODE_LOGGING) {
    logPrintln("  Radio ready!");
  }
  
  // ============== PART 2: MORSE "V V V C" ==============
  if (!FIELD_MODE_LOGGING) {
    logPrintln("\n[2/5] Morse: 'V V V C' (Test/Confirmed)");
  }
  sendMorseMessage("V V V C");
  delay(500);  // Brief pause after Morse
  
  // ============== PART 3: PLAY WAV FILE ==============
  // Select random WAV file for this transmission
  String selectedFile = selectRandomWavFile();
  
  // ALWAYS log selected file (important for field tracking)
  logPrint("Selected WAV: ");
  logPrintln(selectedFile);
  
  // Pre-flight check: Verify file exists and can be opened
  File testFile = SD.open(selectedFile.c_str());
  if (!testFile) {
    logPrintln("ERROR: Cannot open selected WAV file!");
    logPrintln("Skipping WAV playback...");
  } else {
    unsigned long fileSize = testFile.size();
    testFile.close();
    if (!FIELD_MODE_LOGGING) {
      logPrint("  File size: ");
      logPrint((int)fileSize);
      logPrintln(" bytes");
    }
    
    // CRITICAL: Disable SD logging during WAV playback to prevent SD card contention
    bool loggingWasEnabled = loggingEnabled;
    loggingEnabled = false;  // Temporarily disable SD card logging
    if (!FIELD_MODE_LOGGING) {
      Serial.println("  [SD logging disabled during WAV playback to prevent conflicts]");
    }
    
    // Start playback with retry logic
    bool playbackSuccess = false;
    const int MAX_PLAY_ATTEMPTS = 3;
    
    for (int attempt = 1; attempt <= MAX_PLAY_ATTEMPTS; attempt++) {
      if (playWav1.play(selectedFile.c_str())) {
        playbackSuccess = true;
        break;
      }
      
      if (attempt < MAX_PLAY_ATTEMPTS) {
        if (!FIELD_MODE_LOGGING) {
          logPrint("  Retry loading file (attempt ");
          logPrint(attempt);
          logPrintln(")");
        }
        delay(300);
      }
    }
    
    if (playbackSuccess) {
      if (!FIELD_MODE_LOGGING) {
        logPrintln("  WAV playback initiated, waiting for audio stream...");
      }
      
      // Wait for playback to start (moved before memory checks for safety)
      unsigned long startWaitTime = millis();
      bool playbackStarted = false;
      while (millis() - startWaitTime < 500) {
        if (playWav1.isPlaying()) {
          playbackStarted = true;
          if (!FIELD_MODE_LOGGING) {
            logPrintln("  Audio stream started successfully!");
          }
          break;
        }
        delay(10);
      }
      
      // Report audio memory usage after stream starts
      if (playbackStarted) {
        delay(100);  // Brief delay to let audio stabilize
        if (!FIELD_MODE_LOGGING) {
          logPrint("  Audio Memory in use: ");
          logPrintln(AudioMemoryUsage());
        }
      }
    
    if (!playbackStarted) {
      logPrintln("  WARNING: Audio stream did not start within 500ms");
      logPrintln("  Check: CPU speed set to 150 MHz or higher?");
    } else {
      // Intelligent playback: loop if <30s, or play to completion if >=30s
      playbackStartTime = millis();
      isPlaying = true;
      bool loopMode = false;  // Will determine dynamically
      unsigned long fileDuration = 0;  // Track individual file duration
      unsigned long lastProgressUpdate = 0;
      
      // Monitor playback and handle looping/completion
      while (isPlaying) {
        unsigned long elapsed = millis() - playbackStartTime;
        
        // Print progress every 10 seconds (only if not in field mode)
        if (!FIELD_MODE_LOGGING && (elapsed - lastProgressUpdate >= 10000)) {
          lastProgressUpdate = elapsed;
          logPrint("  Playing: ");
          logPrint((int)(elapsed / 1000));
          logPrintln(" seconds");
        }
        
        // Check if file stopped playing
        if (!playWav1.isPlaying()) {
          unsigned long filePlayTime = elapsed;
          
          // If file finished before 30 seconds, loop it
          if (filePlayTime < PLAYBACK_DURATION_MS) {
            if (!loopMode) {
              if (!FIELD_MODE_LOGGING) {
                logPrint("  File duration: ~");
                logPrint((int)(filePlayTime / 1000));
                logPrintln(" seconds (looping until 30s minimum)");
              }
              loopMode = true;
              fileDuration = filePlayTime;  // Save file duration for loop detection
            } else {
              // Check if we've reached minimum duration - if so, stop naturally
              if (elapsed >= PLAYBACK_DURATION_MS) {
                if (!FIELD_MODE_LOGGING) {
                  logPrint("  Minimum duration reached (");
                  logPrint((int)(elapsed / 1000));
                  logPrintln("s), file loop complete");
                }
                isPlaying = false;
                break;
              }
              if (!FIELD_MODE_LOGGING) {
                logPrintln("  Looping file...");
              }
            }
            
            delay(50);  // Brief pause before restart
            
            // Restart playback
            if (playWav1.play(selectedFile.c_str())) {
              // Continue monitoring
            } else {
              logPrintln("  ERROR: Could not restart WAV playback!");
              isPlaying = false;
              break;
            }
          } else {
            // File was >= 30s, play to completion
            if (!FIELD_MODE_LOGGING) {
              logPrint("  File playback complete: ");
              logPrint((int)(filePlayTime / 1000));
              logPrintln(" seconds (played to completion)");
            }
            isPlaying = false;
            break;
          }
        }
        
        delay(100);  // Small delay for responsiveness
      }
      
      if (!FIELD_MODE_LOGGING) {
        logPrintln("  WAV playback complete");
      }
      
      // Final audio memory report (safe check)
      if (!FIELD_MODE_LOGGING) {
        delay(50);
        logPrint("  Peak Audio Memory: ");
        logPrint(AudioMemoryUsageMax());
        logPrintln(" blocks");
      }
    }
    
    } else {
      logPrintln("ERROR: Could not start WAV playback after 3 attempts!");
    }
    
    // Re-enable SD logging after WAV playback completes
    loggingEnabled = loggingWasEnabled;
    if (loggingEnabled && !FIELD_MODE_LOGGING) {
      Serial.println("  [SD logging re-enabled]");
    }
  }
  
  delay(500);  // Brief pause after WAV
  
  // ============== PART 4: MORSE STATION ID (FCC REQUIRED) ==============
  if (!FIELD_MODE_LOGGING) {
    logPrintln("\n[4/5] Morse Station ID (FCC Required)");
  }
  sendMorseMessage(CALLSIGN_ID);
  
  // ============== PART 5: PTT OFF + TAIL ==============
  // CRITICAL: Release PTT immediately after tail delay (no logging delays!)
  delay(PTT_TAIL_MS);
  digitalWrite(PTT_PIN, LOW);
  pttActive = false;
  
  // Log PTT release AFTER it's complete (prevents SD card delays from extending PTT time)
  if (!FIELD_MODE_LOGGING) {
    logPrintln("\n[5/5] PTT OFF");
    logPrint("  Tail delay: ");
    logPrint((int)PTT_TAIL_MS);
    logPrintln(" ms");
    logPrintln("  PTT un-keyed");
  }
  
  unsigned long totalTxTime = millis() - pttStartTime;
  if (!FIELD_MODE_LOGGING) {
    logPrint("  Total TX time: ");
    logPrint((int)(totalTxTime / 1000));
    logPrintln(" seconds");
  }
  
  logPrintln("\n========================================");
  logPrintln("TRANSMISSION COMPLETE");
  
  // Check if this was the final transmission
  if (transmissionsDisabled) {
    logPrintln("========================================");
    logPrintln("SHUTDOWN: Battery too low for further TX");
    logPrintln("Final FCC-required ID transmitted");
    logPrintln("System entering low-power monitoring mode");
    logPrintln("========================================");
    
    // Enter low-power mode - just monitor battery and blink LED
    while(1) {
      float voltage = readBatteryVoltage();
      updateBatteryState(voltage);
      updateMorseLED();
      delay(100);
    }
  }
  
  if (!FIELD_MODE_LOGGING) {
    logPrint("Next transmission in ");
    logPrint((int)(CYCLE_INTERVAL_MS / 1000));
    logPrintln(" seconds");
  }
  logPrintln("========================================\n");
}

// ============================================================================
// WAV FILE MANAGEMENT FUNCTIONS
// ============================================================================

// Scan SD card root directory for all WAV files
void scanSDCardForWavFiles() {
  wavFileCount = 0;
  
  File root = SD.open("/");
  if (!root) {
    Serial.println(" ERROR: Cannot open SD card root directory");
    return;
  }
  
  while (wavFileCount < MAX_WAV_FILES) {
    File entry = root.openNextFile();
    if (!entry) {
      break;  // No more files
    }
    
    // Skip directories
    if (entry.isDirectory()) {
      entry.close();
      continue;
    }
    
    // Get filename and check if it's a WAV file
    String filename = String(entry.name());
    entry.close();
    
    // Check if it's a WAV file (case-insensitive)
    String filenameUpper = filename;
    filenameUpper.toUpperCase();
    if (filenameUpper.endsWith(".WAV")) {
      wavFiles[wavFileCount] = filename;
      wavFileCount++;
    }
  }
  
  root.close();
}

// Select a random WAV file from the available list
String selectRandomWavFile() {
  if (wavFileCount == 0) {
    return "";  // No files available
  }
  
  if (wavFileCount == 1) {
    return wavFiles[0];  // Only one file
  }
  
  // Random selection
  int randomIndex = random(0, wavFileCount);
  return wavFiles[randomIndex];
}

// ============================================================================
// BATTERY VOLTAGE READING
// ============================================================================
float readBatteryVoltage() {
  // Average multiple samples for noise reduction
  unsigned long adcSum = 0;
  
  for (int i = 0; i < NUM_SAMPLES; i++) {
    adcSum += analogRead(BATTERY_PIN);
    delayMicroseconds(100);
  }
  
  float adcAverage = (float)adcSum / NUM_SAMPLES;
  float pinVoltage = (adcAverage / ADC_MAX_VALUE) * ADC_REFERENCE_VOLTAGE;
  float batteryVoltage = pinVoltage * VOLTAGE_DIVIDER_RATIO;
  
  return batteryVoltage;
}

// ============================================================================
// BATTERY STATE UPDATE WITH DEBOUNCING
// ============================================================================
void updateBatteryState(float voltage) {
  // Determine state from voltage thresholds
  BatteryState measuredState;
  
  if (voltage >= VOLTAGE_GOOD) {
    measuredState = STATE_GOOD;
  } else if (voltage >= VOLTAGE_LOW) {
    measuredState = STATE_LOW;
  } else if (voltage >= VOLTAGE_VERY_LOW) {
    measuredState = STATE_VERY_LOW;
  } else if (voltage >= VOLTAGE_SHUTDOWN) {
    measuredState = STATE_SHUTDOWN;
  } else {
    measuredState = STATE_CRITICAL;
  }
  
  // Debounce state changes
  if (measuredState != currentBatteryState) {
    if (measuredState == pendingBatteryState) {
      batteryDebounceCounter++;
      
      if (batteryDebounceCounter >= STATE_CHANGE_DEBOUNCE_COUNT) {
        // Confirmed state change
        previousBatteryState = currentBatteryState;
        currentBatteryState = measuredState;
        batteryDebounceCounter = 0;
        
        handleBatteryStateTransition();
        
        // Update Morse LED pattern
        currentMorsePattern = getMorseBatteryPattern(currentBatteryState);
        morseIndex = 0;
        morseElementActive = false;
      }
    } else {
      // Different pending state, restart counter
      pendingBatteryState = measuredState;
      batteryDebounceCounter = 1;
    }
  } else {
    // State matches current, reset debounce
    pendingBatteryState = currentBatteryState;
    batteryDebounceCounter = 0;
  }
}

// ============================================================================
// BATTERY STATE TRANSITION HANDLER
// ============================================================================
void handleBatteryStateTransition() {
  float voltage = readBatteryVoltage();
  
  logPrintln();
  logPrintln("========================================");
  logPrintln("BATTERY STATE TRANSITION");
  printTimestamp();
  logPrintln();
  logPrint("  From: ");
  logPrintln(getBatteryStateName(previousBatteryState));
  logPrint("  To:   ");
  logPrintln(getBatteryStateName(currentBatteryState));
  logPrint("  Voltage: ");
  logPrint(voltage, 2);
  logPrintln("V");
  logPrintln("========================================");
}

// ============================================================================
// GET BATTERY STATE NAME
// ============================================================================
String getBatteryStateName(BatteryState state) {
  switch (state) {
    case STATE_GOOD:      return "GOOD";
    case STATE_LOW:       return "LOW";
    case STATE_VERY_LOW:  return "VERY_LOW";
    case STATE_SHUTDOWN:  return "SHUTDOWN";
    case STATE_CRITICAL:  return "CRITICAL";
    default:              return "UNKNOWN";
  }
}

// ============================================================================
// GET MORSE PATTERN FOR BATTERY STATE
// ============================================================================
String getMorseBatteryPattern(BatteryState state) {
  switch (state) {
    case STATE_GOOD:
      return "... --.";          // S G = STATUS GOOD
    case STATE_LOW:
      return "--- .-..";          // O L = OVERLOAD LOW
    case STATE_VERY_LOW:
      return "--- ...-";          // O V = OVERLOAD VERY LOW
    case STATE_SHUTDOWN:
      return "... --- ... ...";   // SOS S = EMERGENCY SHUTDOWN
    case STATE_CRITICAL:
      return "... --- ... -.-.";  // SOS C = EMERGENCY CRITICAL
    default:
      return "........";           // Error indication
  }
}

// ============================================================================
// UPDATE MORSE LED (NON-BLOCKING)
// ============================================================================
void updateMorseLED() {
  unsigned long currentTime = millis();
  
  // Check if pattern is complete
  if (morseIndex >= currentMorsePattern.length()) {
    // Pattern complete, wait for word space, then restart
    if (!morseElementActive && (currentTime - morseLastChange >= MORSE_LED_WORD_SPACE)) {
      morseIndex = 0;
      morseElementActive = false;
      morseLastChange = currentTime;
    }
    return;
  }
  
  // Get current character
  char element = currentMorsePattern[morseIndex];
  
  // Handle space character (letter separator)
  if (element == ' ') {
    if (!morseElementActive && (currentTime - morseLastChange >= MORSE_LED_LETTER_SPACE)) {
      morseIndex++;
      morseLastChange = currentTime;
    }
    return;
  }
  
  // Handle dot or dash
  if (morseElementActive) {
    // LED is ON - check if duration complete
    unsigned long elementDuration = (element == '.') ? MORSE_LED_DOT : MORSE_LED_DASH;
    
    if (currentTime - morseLastChange >= elementDuration) {
      digitalWrite(LED_PIN, LOW);
      morseElementActive = false;
      morseLastChange = currentTime;
      morseIndex++;
    }
  } else {
    // LED is OFF - check if we should start next element
    bool isFirstElement = (morseIndex == 0);
    bool afterSpace = (morseIndex > 0 && currentMorsePattern[morseIndex - 1] == ' ');
    
    if (isFirstElement || afterSpace || (currentTime - morseLastChange >= MORSE_LED_ELEMENT_SPACE)) {
      digitalWrite(LED_PIN, HIGH);
      morseElementActive = true;
      morseLastChange = currentTime;
    }
  }
}

// ============================================================================
// MORSE CODE FUNCTIONS
// ============================================================================

// Send a complete Morse code message
void sendMorseMessage(const char* message) {
  if (!FIELD_MODE_LOGGING) {
    logPrint("  Sending: '");
    logPrint(message);
    logPrintln("'");
  }
  
  int dotDuration = MORSE_DOT_DURATION;
  int dashDuration = dotDuration * 3;
  int wordSpace = dotDuration * 7;
  int charSpace = dotDuration * 3;
  
  for (int i = 0; message[i] != '\0'; i++) {
    char c = toupper(message[i]);
    
    if (c == ' ') {
      // Space between words
      delay(wordSpace);
    } else {
      // Get Morse pattern for character
      String pattern = getMorsePattern(c);
      
      if (pattern.length() > 0) {
        // Send each dit or dah
        for (unsigned int j = 0; j < pattern.length(); j++) {
          if (pattern[j] == '.') {
            sendDit(dotDuration);
          } else if (pattern[j] == '-') {
            sendDah(dashDuration, dotDuration);
          }
        }
        
        // Space between characters
        delay(charSpace);
      }
    }
  }
  
  if (!FIELD_MODE_LOGGING) {
    logPrintln("  Morse transmission complete");
  }
}

// Send a Morse code dit (dot)
void sendDit(int dotDuration) {
  toneMorse.amplitude(1.0);  // Turn on tone
  delay(dotDuration);
  toneMorse.amplitude(0.0);  // Turn off tone
  delay(dotDuration);  // Element space
}

// Send a Morse code dah (dash)
void sendDah(int dashDuration, int dotDuration) {
  toneMorse.amplitude(1.0);  // Turn on tone
  delay(dashDuration);
  toneMorse.amplitude(0.0);  // Turn off tone
  delay(dotDuration);  // Element space
}

// Get Morse code pattern for a character
String getMorsePattern(char c) {
  c = toupper(c);
  
  // Letters
  if (c == 'A') return ".-";
  if (c == 'B') return "-...";
  if (c == 'C') return "-.-.";
  if (c == 'D') return "-..";
  if (c == 'E') return ".";
  if (c == 'F') return "..-.";
  if (c == 'G') return "--.";
  if (c == 'H') return "....";
  if (c == 'I') return "..";
  if (c == 'J') return ".---";
  if (c == 'K') return "-.-";
  if (c == 'L') return ".-..";
  if (c == 'M') return "--";
  if (c == 'N') return "-.";
  if (c == 'O') return "---";
  if (c == 'P') return ".--.";
  if (c == 'Q') return "--.-";
  if (c == 'R') return ".-.";
  if (c == 'S') return "...";
  if (c == 'T') return "-";
  if (c == 'U') return "..-";
  if (c == 'V') return "...-";
  if (c == 'W') return ".--";
  if (c == 'X') return "-..-";
  if (c == 'Y') return "-.--";
  if (c == 'Z') return "--..";
  
  // Numbers
  if (c == '0') return "-----";
  if (c == '1') return ".----";
  if (c == '2') return "..---";
  if (c == '3') return "...--";
  if (c == '4') return "....-";
  if (c == '5') return ".....";
  if (c == '6') return "-....";
  if (c == '7') return "--...";
  if (c == '8') return "---..";
  if (c == '9') return "----.";
  
  // Punctuation
  if (c == '/') return "-..-.";
  if (c == '.') return ".-.-.-";
  if (c == ',') return "--..--";
  if (c == '?') return "..--..";
  
  return "";  // Unknown character
}

// ============================================================================
// DUAL OUTPUT LOGGING FUNCTIONS
// ============================================================================

void logPrint(const char* str) {
  Serial.print(str);
  if (loggingEnabled) {
    logFile = SD.open(LOG_FILENAME, FILE_WRITE);
    if (logFile) {
      logFile.print(str);
      logFile.close();
    }
  }
}

void logPrint(const String& str) {
  Serial.print(str);
  if (loggingEnabled) {
    logFile = SD.open(LOG_FILENAME, FILE_WRITE);
    if (logFile) {
      logFile.print(str);
      logFile.close();
    }
  }
}

void logPrint(int val) {
  Serial.print(val);
  if (loggingEnabled) {
    logFile = SD.open(LOG_FILENAME, FILE_WRITE);
    if (logFile) {
      logFile.print(val);
      logFile.close();
    }
  }
}

void logPrint(float val, int decimals) {
  Serial.print(val, decimals);
  if (loggingEnabled) {
    logFile = SD.open(LOG_FILENAME, FILE_WRITE);
    if (logFile) {
      logFile.print(val, decimals);
      logFile.close();
    }
  }
}

void logPrintln(const char* str) {
  Serial.println(str);
  if (loggingEnabled) {
    logFile = SD.open(LOG_FILENAME, FILE_WRITE);
    if (logFile) {
      logFile.println(str);
      logFile.close();
    }
  }
}

void logPrintln(const String& str) {
  Serial.println(str);
  if (loggingEnabled) {
    logFile = SD.open(LOG_FILENAME, FILE_WRITE);
    if (logFile) {
      logFile.println(str);
      logFile.close();
    }
  }
}

void logPrintln() {
  Serial.println();
  if (loggingEnabled) {
    logFile = SD.open(LOG_FILENAME, FILE_WRITE);
    if (logFile) {
      logFile.println();
      logFile.close();
    }
  }
}

// ============================================================================
// TIMESTAMP FUNCTIONS
// ============================================================================

time_t getTeensy3Time() {
  return Teensy3Clock.get();
}

void printTimestamp() {
  char timestamp[30];
  sprintf(timestamp, "Time: [%04d-%02d-%02d %02d:%02d:%02d]", 
          year(), month(), day(), hour(), minute(), second());
  logPrintln(timestamp);
}
