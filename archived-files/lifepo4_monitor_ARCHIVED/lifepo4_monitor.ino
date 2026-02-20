/*
 * WX7V Foxhunt Controller - LiFePO4 Battery Monitor
 * 
 * VERSION: 1.0
 * 
 * PURPOSE: Runtime performance tracking for 4.5Ah Bioenno LiFePO4 battery
 * 
 * HARDWARE:
 * - Teensy 4.1
 * - Bioenno 4.5Ah LiFePO4 battery (12.8V nominal, 4S configuration)
 * - Voltage divider: R1=10kΩ, R2=2.0kΩ
 * - Pin A9: Battery voltage input
 * - Pin 13: Status LED (Morse code patterns)
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
 * LiFePO4 CHARACTERISTICS:
 * - Flat discharge curve (stays ~13.0-13.5V for 80% of capacity)
 * - Safe deep discharge (inherently safer than other lithium chemistries)
 * - Voltage drops rapidly in final 10-15% of capacity
 * - 2000-3000+ cycle life
 * - Temperature stable: -20°C to 60°C
 * 
 * USAGE:
 * 1. Upload to Teensy 4.1 via Arduino IDE
 * 2. Open Tools -> Serial Monitor (115200 baud)
 * 3. Monitor voltage readings and runtime statistics
 * 4. Data logged to SD card: LIFEPO4.LOG
 * 
 * DATA COLLECTION FOCUS:
 * - Voltage vs. runtime correlation
 * - Discharge curve characterization
 * - Capacity verification (4.5Ah actual vs. rated)
 * - Performance under transmit load
 * 
 * AUTHOR: WX7V
 * DATE: 2026-02-15
 */

#include <SD.h>
#include <TimeLib.h>

// ============================================================================
// VERSION INFORMATION
// ============================================================================
const char* VERSION = "1.0";
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
// Adjust after calibration with multimeter
const float VOLTAGE_DIVIDER_RATIO = 6.0;

// Teensy 4.1 ADC configuration
const float ADC_REFERENCE_VOLTAGE = 3.3;  // Volts
const int ADC_RESOLUTION = 10;            // 10-bit ADC (0-1023)
const int ADC_MAX_VALUE = 1023;           // 2^10 - 1

// ============================================================================
// LiFePO4 BATTERY THRESHOLDS (4S Configuration)
// ============================================================================
const float VOLTAGE_FULL = 14.6;          // 3.65V per cell (fully charged)
const float VOLTAGE_GOOD = 13.0;          // 3.25V per cell (nominal - stays here 80% of runtime)
const float VOLTAGE_LOW = 12.4;           // 3.10V per cell (warning - discharge accelerating)
const float VOLTAGE_VERY_LOW = 12.0;      // 3.00V per cell (nearing empty)
const float VOLTAGE_SHUTDOWN = 11.6;      // 2.90V per cell (conservative cutoff)
const float VOLTAGE_CRITICAL = 10.0;      // 2.50V per cell (absolute minimum - BMS protection)

// ============================================================================
// BATTERY STATE ENUMERATION
// ============================================================================
enum BatteryState {
  STATE_FULL,         // >= 14.6V (freshly charged)
  STATE_GOOD,         // >= 13.0V (healthy operation - majority of runtime)
  STATE_LOW,          // >= 12.4V (getting low - monitor closely)
  STATE_VERY_LOW,     // >= 12.0V (nearing empty - wrap up operations)
  STATE_SHUTDOWN,     // >= 11.6V (shutdown recommended)
  STATE_CRITICAL      // < 11.6V (emergency)
};

BatteryState currentState = STATE_GOOD;
BatteryState previousState = STATE_GOOD;

// ============================================================================
// TIMING CONFIGURATION
// ============================================================================
const unsigned long SAMPLE_INTERVAL = 500;        // Read voltage every 500ms
const unsigned long SERIAL_LOG_INTERVAL = 5000;   // Print to serial every 5 seconds
const unsigned long STATISTICS_INTERVAL = 300000; // Print statistics every 5 minutes

unsigned long lastSampleTime = 0;
unsigned long lastSerialLogTime = 0;
unsigned long lastStatisticsTime = 0;

// ============================================================================
// RUNTIME TRACKING
// ============================================================================
unsigned long startTime = 0;
unsigned long totalRuntime = 0;

// Time tracking for each state
unsigned long timeEnteredFull = 0;
unsigned long timeEnteredGood = 0;
unsigned long timeEnteredLow = 0;
unsigned long timeEnteredVeryLow = 0;
unsigned long timeEnteredShutdown = 0;
unsigned long timeEnteredCritical = 0;

// Duration tracking (cumulative)
unsigned long durationInFull = 0;
unsigned long durationInGood = 0;
unsigned long durationInLow = 0;
unsigned long durationInVeryLow = 0;
unsigned long durationInShutdown = 0;
unsigned long durationInCritical = 0;

// ============================================================================
// VOLTAGE STATISTICS
// ============================================================================
float voltageMin = 99.0;
float voltageMax = 0.0;
float voltageSum = 0.0;
unsigned long voltageReadings = 0;

// ============================================================================
// SD CARD LOGGING
// ============================================================================
File logFile;
bool loggingEnabled = false;
const char* LOG_FILENAME = "LIFEPO4.LOG";

// ============================================================================
// MORSE CODE LED PATTERNS
// ============================================================================
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

// ============================================================================
// SETUP FUNCTION
// ============================================================================
void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  while (!Serial && millis() < 3000);  // Wait up to 3 seconds for Serial Monitor
  
  // Initialize LED pin
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  // Initialize ADC
  analogReadResolution(ADC_RESOLUTION);
  
  // Print startup banner
  Serial.println();
  Serial.println("========================================");
  Serial.println("WX7V LiFePO4 Battery Monitor");
  Serial.print("Version: ");
  Serial.print(VERSION);
  Serial.print(" (");
  Serial.print(VERSION_DATE);
  Serial.println(")");
  Serial.println("========================================");
  Serial.println();
  
  // Initialize RTC
  setSyncProvider(getTeensy3Time);
  if (timeStatus() != timeSet) {
    Serial.println("WARNING: RTC not set - timestamps will be incorrect");
  } else {
    Serial.print("RTC synchronized: ");
    printDateTime();
    Serial.println();
  }
  Serial.println();
  
  // Initialize SD card
  Serial.print("Initializing SD card... ");
  if (SD.begin(BUILTIN_SDCARD)) {
    loggingEnabled = true;
    Serial.println("SUCCESS");
    
    // Write startup message to log file
    logFile = SD.open(LOG_FILENAME, FILE_WRITE);
    if (logFile) {
      logFile.println();
      logFile.println("========================================");
      logFile.print("SYSTEM STARTUP: ");
      printDateTimeToFile(logFile);
      logFile.println();
      logFile.print("Version: ");
      logFile.print(VERSION);
      logFile.print(" (");
      logFile.print(VERSION_DATE);
      logFile.println(")");
      logFile.println("Battery: Bioenno 4.5Ah LiFePO4 (4S)");
      logFile.println("========================================");
      logFile.close();
    }
  } else {
    loggingEnabled = false;
    Serial.println("FAILED (continuing without logging)");
  }
  Serial.println();
  
  // Read initial battery voltage
  float initialVoltage = readBatteryVoltage();
  currentState = determineState(initialVoltage);
  previousState = currentState;
  
  // Initialize state tracking
  startTime = millis();
  recordStateEntry(currentState);
  
  // Log initial status
  logPrint("Initial battery voltage: ");
  logPrint(initialVoltage, 2);
  logPrint("V (");
  logPrint(getStateString(currentState));
  logPrintln(")");
  logPrintln();
  
  // Print column headers
  printTableHeader();
  
  // Initialize last sample time
  lastSampleTime = millis();
  lastSerialLogTime = millis();
  lastStatisticsTime = millis();
}

// ============================================================================
// MAIN LOOP
// ============================================================================
void loop() {
  unsigned long currentTime = millis();
  
  // Sample voltage at regular intervals
  if (currentTime - lastSampleTime >= SAMPLE_INTERVAL) {
    lastSampleTime = currentTime;
    
    // Read battery voltage
    float batteryVoltage = readBatteryVoltage();
    
    // Update statistics
    updateStatistics(batteryVoltage);
    
    // Check for state changes
    BatteryState newState = determineState(batteryVoltage);
    if (newState != currentState) {
      handleStateTransition(newState, batteryVoltage);
    }
  }
  
  // Log to serial at slower interval
  if (currentTime - lastSerialLogTime >= SERIAL_LOG_INTERVAL) {
    lastSerialLogTime = currentTime;
    float voltage = readBatteryVoltage();
    logVoltageReading(voltage);
  }
  
  // Print statistics periodically
  if (currentTime - lastStatisticsTime >= STATISTICS_INTERVAL) {
    lastStatisticsTime = currentTime;
    printStatistics();
  }
  
  // Update total runtime
  totalRuntime = millis() - startTime;
  
  // Update Morse code LED pattern (non-blocking)
  updateMorseLED();
}

// ============================================================================
// BATTERY VOLTAGE READING
// ============================================================================
float readBatteryVoltage() {
  // Average 10 samples to reduce noise
  long sum = 0;
  for (int i = 0; i < 10; i++) {
    sum += analogRead(BATTERY_PIN);
    delay(5);  // Small delay between samples
  }
  int rawValue = sum / 10;
  
  // Convert ADC reading to voltage
  float pinVoltage = (rawValue / (float)ADC_MAX_VALUE) * ADC_REFERENCE_VOLTAGE;
  float batteryVoltage = pinVoltage * VOLTAGE_DIVIDER_RATIO;
  
  return batteryVoltage;
}

// ============================================================================
// BATTERY STATE DETERMINATION
// ============================================================================
BatteryState determineState(float voltage) {
  // Simple threshold-based state determination
  // No hysteresis or debouncing needed - LiFePO4 is very stable
  
  if (voltage >= VOLTAGE_FULL) {
    return STATE_FULL;
  } else if (voltage >= VOLTAGE_GOOD) {
    return STATE_GOOD;
  } else if (voltage >= VOLTAGE_LOW) {
    return STATE_LOW;
  } else if (voltage >= VOLTAGE_VERY_LOW) {
    return STATE_VERY_LOW;
  } else if (voltage >= VOLTAGE_SHUTDOWN) {
    return STATE_SHUTDOWN;
  } else {
    return STATE_CRITICAL;
  }
}

// ============================================================================
// STATE MANAGEMENT
// ============================================================================
void handleStateTransition(BatteryState newState, float voltage) {
  unsigned long now = millis();
  
  // Calculate duration in previous state
  unsigned long duration = calculateStateDuration(previousState, now);
  
  // Record transition
  previousState = currentState;
  currentState = newState;
  
  // Log the transition
  logPrintln();
  logPrintln("========================================");
  printTimestamp();
  logPrint(" STATE TRANSITION");
  logPrintln();
  logPrintln("========================================");
  
  logPrint("Previous state: ");
  logPrint(getStateString(previousState));
  logPrint(" (");
  printDuration(duration);
  logPrintln(")");
  
  logPrint("New state: ");
  logPrint(getStateString(currentState));
  logPrint(" at ");
  logPrint(voltage, 2);
  logPrintln("V");
  
  logPrint("Total runtime: ");
  printDuration(totalRuntime);
  logPrintln();
  logPrintln("========================================");
  logPrintln();
  
  // Record entry time for new state
  recordStateEntry(currentState);
  
  // Update table header after state change
  printTableHeader();
}

void recordStateEntry(BatteryState state) {
  unsigned long now = millis();
  
  switch (state) {
    case STATE_FULL:
      timeEnteredFull = now;
      break;
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

unsigned long calculateStateDuration(BatteryState state, unsigned long now) {
  unsigned long duration = 0;
  
  switch (state) {
    case STATE_FULL:
      duration = now - timeEnteredFull;
      durationInFull += duration;
      break;
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
      duration = now - timeEnteredCritical;
      durationInCritical += duration;
      break;
  }
  
  return duration;
}

// ============================================================================
// STATISTICS TRACKING
// ============================================================================
void updateStatistics(float voltage) {
  voltageSum += voltage;
  voltageReadings++;
  
  if (voltage < voltageMin) {
    voltageMin = voltage;
  }
  
  if (voltage > voltageMax) {
    voltageMax = voltage;
  }
}

void printStatistics() {
  logPrintln();
  logPrintln("========================================");
  printTimestamp();
  logPrint(" RUNTIME STATISTICS");
  logPrintln();
  logPrintln("========================================");
  
  // Total runtime
  logPrint("Total runtime: ");
  printDuration(totalRuntime);
  logPrintln();
  logPrintln();
  
  // Voltage statistics
  float voltageAvg = (voltageReadings > 0) ? (voltageSum / voltageReadings) : 0.0;
  logPrint("Voltage - Min: ");
  logPrint(voltageMin, 2);
  logPrint("V  Max: ");
  logPrint(voltageMax, 2);
  logPrint("V  Avg: ");
  logPrint(voltageAvg, 2);
  logPrintln("V");
  logPrintln();
  
  // Time in each state
  logPrintln("Time in each state:");
  if (durationInFull > 0) {
    logPrint("  FULL:      ");
    printDuration(durationInFull);
    logPrintln();
  }
  if (durationInGood > 0) {
    logPrint("  GOOD:      ");
    printDuration(durationInGood);
    logPrintln();
  }
  if (durationInLow > 0) {
    logPrint("  LOW:       ");
    printDuration(durationInLow);
    logPrintln();
  }
  if (durationInVeryLow > 0) {
    logPrint("  VERY_LOW:  ");
    printDuration(durationInVeryLow);
    logPrintln();
  }
  if (durationInShutdown > 0) {
    logPrint("  SHUTDOWN:  ");
    printDuration(durationInShutdown);
    logPrintln();
  }
  if (durationInCritical > 0) {
    logPrint("  CRITICAL:  ");
    printDuration(durationInCritical);
    logPrintln();
  }
  
  logPrintln("========================================");
  logPrintln();
  
  // Print table header for continued logging
  printTableHeader();
}

// ============================================================================
// LOGGING FUNCTIONS
// ============================================================================
void printTableHeader() {
  logPrintln("Timestamp           | Runtime    | ADC  | Pin A9 | Battery | V/Cell | Status");
  logPrintln("--------------------+------------+------+--------+---------+--------+------------------");
}

void logVoltageReading(float batteryVoltage) {
  // Read raw ADC value
  int rawAdc = analogRead(BATTERY_PIN);
  float pinVoltage = (rawAdc / (float)ADC_MAX_VALUE) * ADC_REFERENCE_VOLTAGE;
  float cellVoltage = batteryVoltage / 4.0;  // 4S configuration
  
  // Print timestamp
  printTimestamp();
  logPrint(" | ");
  
  // Print runtime
  printDuration(totalRuntime);
  logPrint(" | ");
  
  // Print ADC reading
  char buf[5];
  sprintf(buf, "%4d", rawAdc);
  logPrint(buf);
  logPrint(" | ");
  
  // Print pin voltage
  logPrint(pinVoltage, 2);
  logPrint("V | ");
  
  // Print battery voltage
  logPrint(batteryVoltage, 2);
  logPrint("V | ");
  
  // Print cell voltage
  logPrint(cellVoltage, 2);
  logPrint("V | ");
  
  // Print status
  logPrint("[");
  logPrint(getStateString(currentState));
  logPrint("]");
  logPrintln();
}

// ============================================================================
// MORSE CODE LED PATTERNS
// ============================================================================
String getMorsePattern(BatteryState state) {
  switch (state) {
    case STATE_FULL:
      return "... ..-. ";         // SF = Status Full
    case STATE_GOOD:
      return "... --. ";           // SG = Status Good
    case STATE_LOW:
      return "--- .-.. ";          // OL = Oh Low
    case STATE_VERY_LOW:
      return "--- ...- ";          // OV = Oh Very low
    case STATE_SHUTDOWN:
      return "... --- ... ... ";   // SOSS = SOS Shutdown
    case STATE_CRITICAL:
      return "... --- ... -.-. ";  // SOSC = SOS Critical
    default:
      return "...- ";              // V = unknown
  }
}

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

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================
const char* getStateString(BatteryState state) {
  switch (state) {
    case STATE_FULL:      return "FULL";
    case STATE_GOOD:      return "GOOD";
    case STATE_LOW:       return "LOW";
    case STATE_VERY_LOW:  return "VERY_LOW";
    case STATE_SHUTDOWN:  return "SHUTDOWN";
    case STATE_CRITICAL:  return "CRITICAL";
    default:              return "UNKNOWN";
  }
}

void printDuration(unsigned long milliseconds) {
  unsigned long seconds = milliseconds / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  
  seconds = seconds % 60;
  minutes = minutes % 60;
  
  char buffer[16];
  sprintf(buffer, "%02luh:%02lum:%02lus", hours, minutes, seconds);
  logPrint(buffer);
}

void printTimestamp() {
  char timestamp[20];
  sprintf(timestamp, "%04d-%02d-%02d %02d:%02d:%02d", 
          year(), month(), day(), hour(), minute(), second());
  logPrint(timestamp);
}

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

time_t getTeensy3Time() {
  return Teensy3Clock.get();
}

// ============================================================================
// DUAL-OUTPUT LOGGING FUNCTIONS (Serial + SD Card)
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
