/*
 * WX7V Foxhunt Controller - Battery Monitor Test Script
 * 
 * VERSION: 2.1
 * 
 * PURPOSE: Standalone test for battery voltage monitoring circuit
 * 
 * HARDWARE:
 * - Teensy 4.1
 * - Voltage divider: R1=10kΩ, R2=2.0kΩ
 * - Pin A9: Battery voltage input
 * - SD card in built-in slot (FAT32 formatted, optional)
 * - Optional: 100nF capacitor across R2 for noise filtering
 * 
 * CIRCUIT:
 *   Battery(+) ──┬── 10kΩ ── Pin A9 ── 2.0kΩ ── GND
 *                │                      │
 *                │                     100nF (optional)
 *                │                      │
 *                └──────────────────────┴──────
 * 
 * VOLTAGE DIVIDER RATIO: 1/6 (2.0kΩ / 12.0kΩ)
 * 
 * USAGE:
 * 1. Upload to Teensy 4.1 via Arduino IDE
 * 2. Open Tools -> Serial Monitor (or Ctrl+Shift+M)
 * 3. Set baud rate to 115200 in Serial Monitor dropdown
 * 4. Compare displayed voltage with multimeter reading
 * 5. Adjust VOLTAGE_DIVIDER_RATIO if needed for calibration
 * 
 * VIEWING RESULTS:
 * - Serial Monitor updates every 500ms with voltage readings
 * - LED blinks at different rates based on battery status
 * - All output is ASCII-safe for Arduino IDE compatibility
 * 
 * AUTHOR: WX7V
 * DATE: 2026-02-15
 * 
 * VERSION HISTORY:
 * v1.0 - Initial release with basic voltage monitoring
 * v1.1 - Added ASCII-safe status indicators for IDE compatibility
 * v1.2 - Added runtime tracking and state transition logging
 * v1.3 - Added Morse code LED patterns and reduced serial output to 5-min intervals
 * v1.4 - Fixed voltage thresholds (GOOD now 14.0V vs 14.8V for realistic LiPo monitoring)
 * v1.5 - Enhanced Morse code with multi-letter messages (SG, OL, OV, SOS)
 * v1.6 - Added SD card logging
 * v1.7 - Added date/time stamping for all log entries
 * v1.8 - SAFETY FIXES: Accurate initial state detection, improved transition logging,
 *        voltage reporting at transitions (bypasses hysteresis on first read)
 * v1.9 - CRITICAL FIX: Corrected hysteresis logic - now allows immediate transitions to
 *        worse states (safety) while preventing false recovery (stability). Previous
 *        version incorrectly prevented LOW->VERY_LOW transitions.
 * v2.0 - SIMPLIFIED: Removed hysteresis complexity entirely. Uses simple thresholds with
 *        debouncing only. Updated thresholds: GOOD=14.5V, SHUTDOWN=13.2V (conservative).
 *        Optimized for 60-second PTT transmissions in amateur radio foxhunt application.
 *        Accepts temporary state changes during PTT as accurate load readings.
 * v2.1 - FIX: Removed first-reading bypass that trusted unreliable ADC values at startup.
 *        Now uses normal debouncing from boot (1.5s delay before first accurate reading).
 *        Fixed header alignment to include timestamp column.
 */

#include <SD.h>
#include <TimeLib.h>  // Built-in Teensy time library

// ============================================================================
// VERSION INFORMATION
// ============================================================================
const char* VERSION = "2.1";
const char* VERSION_DATE = "2026-02-15";

// ============================================================================
// PIN DEFINITIONS
// ============================================================================
const int BATTERY_PIN = A9;        // Pin 23 - Analog input from voltage divider
const int LED_PIN = 13;            // Built-in LED for status indication

// ============================================================================
// VOLTAGE DIVIDER CONFIGURATION
// ============================================================================
// Theoretical ratio: (R1 + R2) / R2 = (10kΩ + 2.0kΩ) / 2.0kΩ = 6.0
// Adjust this value after calibration with a multimeter
const float VOLTAGE_DIVIDER_RATIO = 6.0;

// Teensy 4.1 ADC configuration
const float ADC_REFERENCE_VOLTAGE = 3.3;  // Volts
const int ADC_RESOLUTION = 10;            // 10-bit ADC (0-1023)
const int ADC_MAX_VALUE = 1023;           // 2^10 - 1

// ============================================================================
// BATTERY THRESHOLDS (4S LiPo)
// ============================================================================
const float VOLTAGE_FULL = 16.8;          // 4.20V per cell (fully charged)
const float VOLTAGE_GOOD = 14.5;          // 3.63V per cell (healthy operation)
const float VOLTAGE_LOW = 14.0;           // 3.50V per cell (getting low)
const float VOLTAGE_VERY_LOW = 13.6;      // 3.40V per cell (replace soon)
const float VOLTAGE_SHUTDOWN = 13.2;      // 3.30V per cell (stop operation - conservative)
// Below 13.2V = CRITICAL (emergency, external BMS should disconnect)

// Debounce settings (require multiple consecutive readings to change state)
const int STATE_CHANGE_DEBOUNCE_COUNT = 3; // Require 3 consecutive readings

// ============================================================================
// SAMPLING CONFIGURATION
// ============================================================================
const int NUM_SAMPLES = 10;                     // Number of ADC samples to average
const unsigned long SAMPLE_INTERVAL = 500;      // Time between readings (ms)
const unsigned long DISPLAY_INTERVAL = 300000;  // Display update interval (5 minutes)

// ============================================================================
// MORSE CODE CONFIGURATION (Slower speed for LED readability)
// ============================================================================
// Optimized for visual LED reading (~6 WPM equivalent)
const int MORSE_DOT_DURATION = 200;             // Duration of a dot (ms) - 1 unit
const int MORSE_DASH_DURATION = 600;            // Duration of a dash (3x dot) - 3 units
const int MORSE_ELEMENT_SPACE = 200;            // Space between dots/dashes within a letter - 1 unit
const int MORSE_LETTER_SPACE = 600;             // Space between letters (3x dot) - 3 units
const int MORSE_WORD_SPACE = 1400;              // Space between words/message repeats (7x dot) - 7 units

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================
unsigned long lastSampleTime = 0;
unsigned long lastDisplayTime = 0;        // Last time we displayed output
unsigned long startTime = 0;              // System start time
unsigned long totalRunTime = 0;           // Total runtime in milliseconds

// Battery state tracking
enum BatteryState {
  STATE_GOOD,
  STATE_LOW,
  STATE_VERY_LOW,
  STATE_SHUTDOWN,
  STATE_CRITICAL
};

BatteryState currentState = STATE_GOOD;
BatteryState previousState = STATE_GOOD;
BatteryState pendingState = STATE_GOOD;     // State waiting to be confirmed
int stateDebounceCounter = 0;               // Count consecutive readings of new state

// State transition timestamps (milliseconds since start)
unsigned long timeEnteredGood = 0;
unsigned long timeEnteredLow = 0;
unsigned long timeEnteredVeryLow = 0;
unsigned long timeEnteredShutdown = 0;
unsigned long timeEnteredCritical = 0;

// Duration tracking (milliseconds)
unsigned long durationInGood = 0;
unsigned long durationInLow = 0;
unsigned long durationInVeryLow = 0;
unsigned long durationInShutdown = 0;

// Morse code LED state machine
String currentMorsePattern = "";                // Current pattern to display
unsigned int morseIndex = 0;                    // Current position in pattern
bool morseElementActive = false;                // LED on during element
unsigned long morseLastChange = 0;              // Last state change time

// SD Card logging
File logFile;
bool loggingEnabled = true;

// ============================================================================
// DUAL OUTPUT FUNCTIONS (Serial + SD Card)
// ============================================================================

void logPrint(const char* str) {
  Serial.print(str);
  if (loggingEnabled) {
    logFile = SD.open("BAT.LOG", FILE_WRITE);
    if (logFile) {
      logFile.print(str);
      logFile.close();
    }
  }
}

void logPrint(const String& str) {
  Serial.print(str);
  if (loggingEnabled) {
    logFile = SD.open("BAT.LOG", FILE_WRITE);
    if (logFile) {
      logFile.print(str);
      logFile.close();
    }
  }
}

void logPrint(int val) {
  Serial.print(val);
  if (loggingEnabled) {
    logFile = SD.open("BAT.LOG", FILE_WRITE);
    if (logFile) {
      logFile.print(val);
      logFile.close();
    }
  }
}

void logPrint(float val, int decimals = 2) {
  Serial.print(val, decimals);
  if (loggingEnabled) {
    logFile = SD.open("BAT.LOG", FILE_WRITE);
    if (logFile) {
      logFile.print(val, decimals);
      logFile.close();
    }
  }
}

void logPrintln(const char* str) {
  Serial.println(str);
  if (loggingEnabled) {
    logFile = SD.open("BAT.LOG", FILE_WRITE);
    if (logFile) {
      logFile.println(str);
      logFile.close();
    }
  }
}

void logPrintln(const String& str) {
  Serial.println(str);
  if (loggingEnabled) {
    logFile = SD.open("BAT.LOG", FILE_WRITE);
    if (logFile) {
      logFile.println(str);
      logFile.close();
    }
  }
}

void logPrintln() {
  Serial.println();
  if (loggingEnabled) {
    logFile = SD.open("BAT.LOG", FILE_WRITE);
    if (logFile) {
      logFile.println();
      logFile.close();
    }
  }
}

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {
    // Wait for Serial Monitor to open (max 3 seconds)
  }
  
  // Set up RTC - try to use Teensy's hardware RTC
  setSyncProvider(getTeensy3Time);
  
  // Configure pins
  pinMode(LED_PIN, OUTPUT);
  pinMode(BATTERY_PIN, INPUT);
  
  // Set ADC resolution (Teensy 4.1 supports up to 12-bit)
  analogReadResolution(ADC_RESOLUTION);
  
  // Initial LED blink to indicate startup
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
    delay(100);
  }
  
  // Print header with clear visibility in Serial Monitor
  Serial.println("\n");
  Serial.println("########################################");
  Serial.println("##                                    ##");
  Serial.println("##   WX7V BATTERY MONITOR TEST        ##");
  Serial.println("##   (with Morse Code LED)            ##");
  Serial.print("##   Version ");
  Serial.print(VERSION);
  Serial.print(" - ");
  Serial.print(VERSION_DATE);
  Serial.println("          ##");
  Serial.println("##                                    ##");
  Serial.println("########################################");
  Serial.println();
  
  // Initialize SD card logging
  Serial.print("Initializing SD card...");
  if (SD.begin(BUILTIN_SDCARD)) {
    loggingEnabled = false;
    Serial.println(" OK");
  } else {
    loggingEnabled = false;
    Serial.println(" FAILED (continuing without logging)");
  }
  
  // Print startup timestamp to both Serial and SD card
  Serial.println();
  Serial.println("========================================");
  Serial.print("SYSTEM STARTUP: ");
  printDateTime();
  Serial.println();
  Serial.println("========================================");
  Serial.println();
  
  // Write startup message to SD card log
  if (loggingEnabled) {
    logFile = SD.open("BAT.LOG", FILE_WRITE);
    if (logFile) {
      logFile.println();
      logFile.println("========================================");
      logFile.print("SYSTEM STARTUP: ");
      printDateTimeToFile(logFile);
      logFile.println();
      logFile.println("========================================");
      logFile.println();
      logFile.close();
    }
  }
  
  Serial.println("Hardware Configuration:");
  Serial.println("  Pin A9: Battery voltage input");
  Serial.println("  Pin 13: LED status (Morse code)");
  Serial.println("  Voltage Divider: 10kOhm + 2.0kOhm");
  Serial.print("  Divider Ratio: ");
  Serial.println(VOLTAGE_DIVIDER_RATIO, 2);
  Serial.print("  ADC Resolution: ");
  Serial.print(ADC_RESOLUTION);
  Serial.println(" bits");
  Serial.print("  Reference: ");
  Serial.print(ADC_REFERENCE_VOLTAGE);
  Serial.println("V");
  Serial.println();
  Serial.println("Morse Code LED Patterns:");
  Serial.println("  SG  (... --. ) = STATUS GOOD");
  Serial.println("  OL  (--- .-..) = OVERLOAD/LOW");
  Serial.println("  OV  (--- ...-) = OVERLOAD/VERY LOW");
  Serial.println("  SOSS(SOS ...) = EMERGENCY/SHUTDOWN");
  Serial.println("  SOSC(SOS -.-.) = EMERGENCY/CRITICAL");
  Serial.println();
  Serial.println("Battery Voltage Thresholds (4S LiPo):");
  Serial.print("  FULL:          ");
  Serial.print(VOLTAGE_FULL);
  Serial.println("V (4.20V/cell)");
  Serial.print("  GOOD:          ");
  Serial.print(VOLTAGE_GOOD);
  Serial.println("V (3.63V/cell)");
  Serial.print("  LOW:           ");
  Serial.print(VOLTAGE_LOW);
  Serial.println("V (3.50V/cell)");
  Serial.print("  VERY LOW:      ");
  Serial.print(VOLTAGE_VERY_LOW);
  Serial.println("V (3.40V/cell)");
  Serial.print("  SHUTDOWN:      ");
  Serial.print(VOLTAGE_SHUTDOWN);
  Serial.println("V (3.30V/cell - conservative)");
  Serial.println();
  Serial.println("Note: No hysteresis - simple thresholds with debouncing");
  Serial.println("      PTT voltage sag may cause temporary state changes");
  Serial.println();
  Serial.println("========================================");
  Serial.println("Starting continuous monitoring...");
  Serial.println("Sampling every 500ms");
  Serial.println("Display updates every 5 minutes");
  Serial.println("(Immediate display on state change)");
  Serial.println("Tracking runtime and state transitions");
  Serial.println("========================================");
  Serial.println();
  
  // Initialize timing
  startTime = millis();
  timeEnteredGood = 0;  // Start in GOOD state at time 0
  lastSampleTime = millis();
  lastDisplayTime = millis() - DISPLAY_INTERVAL - 1000;  // Force first reading to display
  
  // Initialize Morse code for GOOD state (will be updated after first reading)
  currentMorsePattern = getMorsePattern(STATE_GOOD);
  morseLastChange = millis();
  
  Serial.println("     Timestamp        | Runtime | ADC  | Pin A9  | Battery | Cell Avg | Status");
  Serial.println("----------------------+---------+------+---------+---------+----------+-------------------");
  
  // Note: First voltage reading will appear after debouncing (1.5 seconds)
  // This allows ADC to settle and provides accurate initial state
}

// ============================================================================
// MAIN LOOP
// ============================================================================
void loop() {
  // Update total runtime
  totalRunTime = millis() - startTime;
  
  // Non-blocking delay
  if (millis() - lastSampleTime >= SAMPLE_INTERVAL) {
    lastSampleTime = millis();
    
    // Read battery voltage
    float batteryVoltage = readBatteryVoltage();
    
    // Update state and check for transitions
    BatteryState oldState = currentState;
    updateBatteryState(batteryVoltage);
    
    // Determine if we should display output
    bool stateChanged = (currentState != oldState);
    bool timeToDisplay = (millis() - lastDisplayTime >= DISPLAY_INTERVAL);
    
    // Display if state changed OR time interval elapsed
    if (stateChanged || timeToDisplay) {
      if (timeToDisplay && !stateChanged) {
        // Periodic update (no state change)
        displayVoltageReading(batteryVoltage);
        lastDisplayTime = millis();
      }
      // Note: State change display is handled in handleStateTransition()
    }
  }
  
  // Update Morse code LED (runs independently, non-blocking)
  updateMorseLED();
}

// ============================================================================
// READ BATTERY VOLTAGE
// ============================================================================
float readBatteryVoltage() {
  // Take multiple samples and average for noise reduction
  unsigned long adcSum = 0;
  
  for (int i = 0; i < NUM_SAMPLES; i++) {
    adcSum += analogRead(BATTERY_PIN);
    delayMicroseconds(100); // Small delay between samples
  }
  
  float adcAverage = (float)adcSum / NUM_SAMPLES;
  
  // Convert ADC reading to Pin A9 voltage
  float pinVoltage = (adcAverage / ADC_MAX_VALUE) * ADC_REFERENCE_VOLTAGE;
  
  // Scale up to battery voltage using divider ratio
  float batteryVoltage = pinVoltage * VOLTAGE_DIVIDER_RATIO;
  
  return batteryVoltage;
}

// ============================================================================
// UPDATE BATTERY STATE AND TRACK TRANSITIONS
// ============================================================================
void updateBatteryState(float batteryVoltage) {
  // Determine what state the voltage indicates based on simple thresholds
  BatteryState measuredState;
  
  // Simple threshold logic - no hysteresis
  // Debouncing (3 consecutive samples) filters noise and brief transients
  // Note: 60-second PTT events will trigger state changes (voltage sag is real)
  
  if (batteryVoltage >= VOLTAGE_GOOD) {
    // Battery healthy: >= 14.5V (3.63V/cell)
    measuredState = STATE_GOOD;
    
  } else if (batteryVoltage >= VOLTAGE_LOW) {
    // Battery getting low: 14.0V - 14.5V (3.50V - 3.63V/cell)
    measuredState = STATE_LOW;
    
  } else if (batteryVoltage >= VOLTAGE_VERY_LOW) {
    // Battery very low: 13.6V - 14.0V (3.40V - 3.50V/cell)
    measuredState = STATE_VERY_LOW;
    
  } else if (batteryVoltage >= VOLTAGE_SHUTDOWN) {
    // Battery at shutdown threshold: 13.2V - 13.6V (3.30V - 3.40V/cell)
    measuredState = STATE_SHUTDOWN;
    
  } else {
    // Battery critically low: < 13.2V (< 3.30V/cell)
    // External BMS should disconnect before reaching here
    measuredState = STATE_CRITICAL;
  }
  
  // Debounce state changes: require multiple consecutive readings
  if (measuredState != currentState) {
    // New state detected
    if (measuredState == pendingState) {
      // Same pending state as before, increment counter
      stateDebounceCounter++;
      
      if (stateDebounceCounter >= STATE_CHANGE_DEBOUNCE_COUNT) {
        // Confirmed! Change state
        previousState = currentState;
        currentState = measuredState;
        stateDebounceCounter = 0;
        
        handleStateTransition();
        
        // Update Morse pattern for new state
        currentMorsePattern = getMorsePattern(currentState);
        morseIndex = 0;
        morseElementActive = false;
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

// ============================================================================
// HANDLE STATE TRANSITION
// ============================================================================
void handleStateTransition() {
  unsigned long currentTime = millis() - startTime;
  
  // Read current voltage for logging
  float batteryVoltage = readBatteryVoltage();
  
  // Print transition notice with voltage
  logPrintln();
  logPrintln("========================================");
  logPrint("STATE TRANSITION: ");
  logPrint(getStateName(previousState));
  logPrint(" -> ");
  logPrint(getStateName(currentState));
  logPrint(" at ");
  logPrint(batteryVoltage, 2);
  logPrintln("V");
  logPrint("Runtime: ");
  logPrintln(formatDuration(currentTime));
  logPrintln("========================================");
  
  // Record transition time and calculate duration in previous state
  switch (currentState) {
    case STATE_GOOD:
      timeEnteredGood = currentTime;
      break;
      
    case STATE_LOW:
      timeEnteredLow = currentTime;
      if (previousState == STATE_GOOD) {
        durationInGood = currentTime - timeEnteredGood;
        logPrint("Time in GOOD state: ");
        logPrintln(formatDuration(durationInGood));
      }
      break;
      
    case STATE_VERY_LOW:
      timeEnteredVeryLow = currentTime;
      if (previousState == STATE_LOW) {
        durationInLow = timeEnteredVeryLow - timeEnteredLow;
        logPrint("Time in LOW state: ");
        logPrintln(formatDuration(durationInLow));
      }
      break;
      
    case STATE_SHUTDOWN:
      timeEnteredShutdown = currentTime;
      if (previousState == STATE_VERY_LOW) {
        durationInVeryLow = timeEnteredShutdown - timeEnteredVeryLow;
        logPrint("Time in VERY_LOW state: ");
        logPrintln(formatDuration(durationInVeryLow));
      }
      break;
      
    case STATE_CRITICAL:
      timeEnteredCritical = currentTime;
      if (previousState == STATE_SHUTDOWN) {
        durationInShutdown = timeEnteredCritical - timeEnteredShutdown;
        logPrint("Time in SHUTDOWN state: ");
        logPrintln(formatDuration(durationInShutdown));
      }
      logPrintln("!!! CRITICAL: DISCONNECT BATTERY NOW !!!");
      break;
  }
  
  // Print summary statistics
  printStatistics();
  
  logPrintln("========================================");
  logPrintln();
  logPrintln("     Timestamp        | Runtime       | ADC  | Pin A9  | Battery | Cell Avg | Status");
  logPrintln("----------------------+---------------+------+---------+---------+----------+-------------------");
  
  // Display current reading after transition (use already-read batteryVoltage)
  displayVoltageReading(batteryVoltage);
  
  // Reset display timer so we don't immediately print again
  lastDisplayTime = millis();
}

// ============================================================================
// GET STATE NAME
// ============================================================================
String getStateName(BatteryState state) {
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
// FORMAT DURATION
// ============================================================================
String formatDuration(unsigned long milliseconds) {
  unsigned long seconds = milliseconds / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  
  seconds = seconds % 60;
  minutes = minutes % 60;
  
  char buffer[20];
  sprintf(buffer, "%02luh:%02lum:%02lus", hours, minutes, seconds);
  return String(buffer);
}

// ============================================================================
// PRINT STATISTICS
// ============================================================================
void printStatistics() {
  logPrintln();
  logPrintln("Cumulative Statistics:");
  
  // Print actual transition that occurred
  logPrint("  Actual transition: ");
  logPrint(getStateName(previousState));
  logPrint(" -> ");
  logPrintln(getStateName(currentState));
  
  logPrintln();
  logPrintln("State Entry Times:");
  
  if (timeEnteredGood >= 0) {
    logPrint("  Entered GOOD at: ");
    logPrintln(formatDuration(timeEnteredGood));
  }
  
  if (timeEnteredLow > 0) {
    logPrint("  Entered LOW at: ");
    logPrintln(formatDuration(timeEnteredLow));
  }
  
  if (timeEnteredVeryLow > 0) {
    logPrint("  Entered VERY_LOW at: ");
    logPrintln(formatDuration(timeEnteredVeryLow));
  }
  
  if (timeEnteredShutdown > 0) {
    logPrint("  Entered SHUTDOWN at: ");
    logPrintln(formatDuration(timeEnteredShutdown));
  }
  
  if (timeEnteredCritical > 0) {
    logPrint("  Entered CRITICAL at: ");
    logPrintln(formatDuration(timeEnteredCritical));
  }
  
  logPrintln();
  logPrintln("Time Spent in Previous State:");
  
  // Calculate actual time spent in previous state
  unsigned long previousStateDuration = 0;
  unsigned long currentTimeStamp = millis() - startTime;
  
  if (previousState == STATE_GOOD && timeEnteredGood >= 0) {
    previousStateDuration = currentTimeStamp - timeEnteredGood;
    logPrint("  GOOD: ");
    logPrintln(formatDuration(previousStateDuration));
  } else if (previousState == STATE_LOW && timeEnteredLow > 0) {
    previousStateDuration = currentTimeStamp - timeEnteredLow;
    logPrint("  LOW: ");
    logPrintln(formatDuration(previousStateDuration));
  } else if (previousState == STATE_VERY_LOW && timeEnteredVeryLow > 0) {
    previousStateDuration = currentTimeStamp - timeEnteredVeryLow;
    logPrint("  VERY_LOW: ");
    logPrintln(formatDuration(previousStateDuration));
  } else if (previousState == STATE_SHUTDOWN && timeEnteredShutdown > 0) {
    previousStateDuration = currentTimeStamp - timeEnteredShutdown;
    logPrint("  SHUTDOWN: ");
    logPrintln(formatDuration(previousStateDuration));
  }
  
  logPrint("  Total Runtime: ");
  logPrintln(formatDuration(currentTimeStamp));
}

// ============================================================================
// DISPLAY VOLTAGE READING
// ============================================================================
void displayVoltageReading(float batteryVoltage) {
  // Calculate values
  int adcRaw = analogRead(BATTERY_PIN);
  float pinVoltage = (adcRaw / (float)ADC_MAX_VALUE) * ADC_REFERENCE_VOLTAGE;
  float cellVoltage = batteryVoltage / 4.0; // 4S battery
  
  // Format and print reading with fixed-width columns for alignment
  char buffer[80];
  
  // Timestamp prefix
  printTimestamp();
  
  // Runtime (format: HH:MM:SS)
  logPrint(" ");
  logPrint(formatDuration(totalRunTime));
  logPrint(" | ");
  
  // ADC value (4 chars right-aligned)
  sprintf(buffer, "%4d", adcRaw);
  logPrint(buffer);
  logPrint(" | ");
  
  // Pin voltage (format: X.XXV)
  dtostrf(pinVoltage, 4, 2, buffer);
  logPrint(buffer);
  logPrint("V | ");
  
  // Battery voltage (format: XX.XXV)
  dtostrf(batteryVoltage, 5, 2, buffer);
  logPrint(buffer);
  logPrint("V | ");
  
  // Cell voltage (format: X.XXV)
  dtostrf(cellVoltage, 4, 2, buffer);
  logPrint(buffer);
  logPrint("V  | ");
  
  // Print status based on current state (matches LED Morse code)
  switch (currentState) {
    case STATE_GOOD:
      logPrintln("[OK] GOOD");
      break;
    case STATE_LOW:
      logPrintln("[WARN] LOW");
      break;
    case STATE_VERY_LOW:
      logPrintln("[WARN] VERY LOW");
      break;
    case STATE_SHUTDOWN:
      logPrintln("[STOP] SHUTDOWN");
      break;
    case STATE_CRITICAL:
      logPrintln("[CRIT] DAMAGE RISK!");
      break;
    default:
      logPrintln("[???] UNKNOWN");
      break;
  }
}

// ============================================================================
// GET MORSE PATTERN FOR BATTERY STATE
// ============================================================================
String getMorsePattern(BatteryState state) {
  // Multi-letter Morse code patterns with status prefixes
  // Use space character to separate letters (handled specially in LED update)
  //
  // GOOD states: "S" (Status) + letter
  //   SG: ... --.     = STATUS GOOD
  //
  // Warning states: "O" (Overload/Oh-no) + letter  
  //   OL: --- .-..    = OVERLOAD LOW
  //   OV: --- ...-    = OVERLOAD VERY LOW
  //
  // Emergency states: "SOS" + letter
  //   SOS S: ... --- ... ...     = SOS SHUTDOWN
  //   SOS C: ... --- ... -.-.    = SOS CRITICAL
  
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
      return "........";           // Error indication (8 dots)
  }
}

// ============================================================================
// UPDATE MORSE CODE LED (NON-BLOCKING)
// ============================================================================
void updateMorseLED() {
  unsigned long currentTime = millis();
  
  // Check if pattern is complete
  if (morseIndex >= currentMorsePattern.length()) {
    // Pattern complete, wait for word space, then restart
    if (!morseElementActive && (currentTime - morseLastChange >= MORSE_WORD_SPACE)) {
      morseIndex = 0;  // Restart pattern
      morseElementActive = false;
      morseLastChange = currentTime;
    }
    return;
  }
  
  // Get current character (dot, dash, or space)
  char element = currentMorsePattern[morseIndex];
  
  // Handle space character (letter separator)
  if (element == ' ') {
    if (!morseElementActive && (currentTime - morseLastChange >= MORSE_LETTER_SPACE)) {
      // Letter space complete, move to next character
      morseIndex++;
      morseLastChange = currentTime;
    }
    return;
  }
  
  // Handle dot or dash
  if (morseElementActive) {
    // LED is currently ON - check if element duration is complete
    unsigned long elementDuration = (element == '.') ? MORSE_DOT_DURATION : MORSE_DASH_DURATION;
    
    if (currentTime - morseLastChange >= elementDuration) {
      // Turn LED OFF and move to next element
      digitalWrite(LED_PIN, LOW);
      morseElementActive = false;
      morseLastChange = currentTime;
      morseIndex++;  // Move to next element after turning OFF
    }
  } else {
    // LED is currently OFF - check if we should start next element
    
    // For the first element (index 0) or right after a space, start immediately
    bool isFirstElement = (morseIndex == 0);
    bool afterSpace = (morseIndex > 0 && currentMorsePattern[morseIndex - 1] == ' ');
    
    if (isFirstElement || afterSpace || (currentTime - morseLastChange >= MORSE_ELEMENT_SPACE)) {
      // Turn LED ON for current element
      digitalWrite(LED_PIN, HIGH);
      morseElementActive = true;
      morseLastChange = currentTime;
    }
  }
}

// ============================================================================
// GET TEENSY RTC TIME
// ============================================================================
time_t getTeensy3Time() {
  return Teensy3Clock.get();
}

// ============================================================================
// PRINT TIMESTAMP (Date and Time)
// ============================================================================
void printTimestamp() {
  char timestamp[30];
  sprintf(timestamp, "[%04d-%02d-%02d %02d:%02d:%02d]", 
          year(), month(), day(), hour(), minute(), second());
  logPrint(timestamp);  // Use logPrint for dual output
}

// ============================================================================
// PRINT DATE AND TIME (Full format) - For Serial Monitor
// ============================================================================
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

// ============================================================================
// PRINT DATE AND TIME TO FILE (Full format) - For SD Card
// ============================================================================
void printDateTimeToFile(File &file) {
  const char* monthNames[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
  };
  
  file.print(monthNames[month() - 1]);
  file.print(" ");
  file.print(day());
  file.print(", ");
  file.print(year());
  file.print(" ");
  
  if (hour() < 10) file.print("0");
  file.print(hour());
  file.print(":");
  
  if (minute() < 10) file.print("0");
  file.print(minute());
  file.print(":");
  
  if (second() < 10) file.print("0");
  file.print(second());
}

// ============================================================================
// VOLTAGE CALIBRATION
// ============================================================================
/*
 * To calibrate voltage readings:
 * 
 * 1. Measure actual battery voltage with multimeter
 * 2. Compare with "Battery" voltage in Serial Monitor
 * 3. Calculate: new_ratio = (actual_voltage / displayed_voltage) * 6.0
 * 4. Update VOLTAGE_DIVIDER_RATIO constant (line 70) and re-upload
 * 
 * Example: If multimeter reads 16.75V but monitor shows 16.55V:
 *   new_ratio = (16.75 / 16.55) * 6.0 = 6.07
 * 
 * Note: Use 1% tolerance resistors for best accuracy
 */
