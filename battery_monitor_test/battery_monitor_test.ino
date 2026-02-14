/*
 * WX7V Foxhunt Controller - Battery Monitor Test Script
 * 
 * VERSION: 1.5
 * 
 * PURPOSE: Standalone test for battery voltage monitoring circuit
 * 
 * HARDWARE:
 * - Teensy 4.1
 * - Voltage divider: R1=10kΩ, R2=2.0kΩ
 * - Pin A9: Battery voltage input
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
 * DATE: 2026-02-14
 * 
 * VERSION HISTORY:
 * v1.0 - Initial release with basic voltage monitoring
 * v1.1 - Added ASCII-safe status indicators for IDE compatibility
 * v1.2 - Added runtime tracking and state transition logging
 * v1.3 - Added Morse code LED patterns and reduced serial output to 5-min intervals
 * v1.4 - Fixed voltage thresholds (GOOD now 14.0V vs 14.8V for realistic LiPo monitoring)
 * v1.5 - Enhanced Morse code with multi-letter messages (SG, OL, OV, SOS)
 */

// ============================================================================
// VERSION INFORMATION
// ============================================================================
const char* VERSION = "1.8";
const char* VERSION_DATE = "2026-02-14";

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
const float VOLTAGE_GOOD = 14.0;          // 3.50V per cell (good working voltage)
const float VOLTAGE_SOFT_WARNING = 13.6;  // 3.40V per cell (low warning)
const float VOLTAGE_HARD_SHUTDOWN = 12.8; // 3.20V per cell (shutdown recommended)
const float VOLTAGE_CRITICAL = 12.0;      // 3.00V per cell (damage risk!)

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

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  while (!Serial && millis() < 3000) {
    // Wait for Serial Monitor to open (max 3 seconds)
  }
  
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
  Serial.println("V (3.50V/cell)");
  Serial.print("  SOFT WARNING:  ");
  Serial.print(VOLTAGE_SOFT_WARNING);
  Serial.println("V (3.40V/cell)");
  Serial.print("  HARD SHUTDOWN: ");
  Serial.print(VOLTAGE_HARD_SHUTDOWN);
  Serial.println("V (3.20V/cell)");
  Serial.print("  CRITICAL:      ");
  Serial.print(VOLTAGE_CRITICAL);
  Serial.println("V (3.00V/cell)");
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
  lastDisplayTime = millis();  // Initialize display timer
  
  // Initialize Morse code for GOOD state (will be updated after first reading)
  currentMorsePattern = getMorsePattern(STATE_GOOD);
  morseLastChange = millis();
  
  Serial.println(" Runtime | ADC  | Pin A9  | Battery | Cell Avg | Status");
  Serial.println("---------+------+---------+---------+----------+-------------------");
  
  // Display initial battery reading immediately
  float initialVoltage = readBatteryVoltage();
  updateBatteryState(initialVoltage);
  displayVoltageReading(initialVoltage);
  
  // Update Morse pattern based on actual initial state
  currentMorsePattern = getMorsePattern(currentState);
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
    
    // Always update LED based on battery status
    updateStatusLED(batteryVoltage);
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
  // Determine current state based on voltage
  previousState = currentState;
  
  if (batteryVoltage >= VOLTAGE_GOOD) {
    currentState = STATE_GOOD;
  } else if (batteryVoltage >= VOLTAGE_SOFT_WARNING) {
    currentState = STATE_LOW;
  } else if (batteryVoltage >= VOLTAGE_HARD_SHUTDOWN) {
    currentState = STATE_VERY_LOW;
  } else if (batteryVoltage >= VOLTAGE_CRITICAL) {
    currentState = STATE_SHUTDOWN;
  } else {
    currentState = STATE_CRITICAL;
  }
  
  // Check for state transition
  if (currentState != previousState) {
    handleStateTransition();
    
    // Update Morse pattern for new state
    currentMorsePattern = getMorsePattern(currentState);
    morseIndex = 0;
    morseElementActive = false;
  }
}

// ============================================================================
// HANDLE STATE TRANSITION
// ============================================================================
void handleStateTransition() {
  unsigned long currentTime = millis() - startTime;
  
  // Print transition notice
  Serial.println();
  Serial.println("========================================");
  Serial.print("STATE TRANSITION: ");
  Serial.print(getStateName(previousState));
  Serial.print(" -> ");
  Serial.println(getStateName(currentState));
  Serial.print("Runtime: ");
  Serial.println(formatDuration(currentTime));
  Serial.println("========================================");
  
  // Record transition time and calculate duration in previous state
  switch (currentState) {
    case STATE_GOOD:
      timeEnteredGood = currentTime;
      break;
      
    case STATE_LOW:
      timeEnteredLow = currentTime;
      if (previousState == STATE_GOOD) {
        durationInGood = currentTime - timeEnteredGood;
        Serial.print("Time in GOOD state: ");
        Serial.println(formatDuration(durationInGood));
      }
      break;
      
    case STATE_VERY_LOW:
      timeEnteredVeryLow = currentTime;
      if (previousState == STATE_LOW) {
        durationInLow = timeEnteredVeryLow - timeEnteredLow;
        Serial.print("Time in LOW state: ");
        Serial.println(formatDuration(durationInLow));
      }
      break;
      
    case STATE_SHUTDOWN:
      timeEnteredShutdown = currentTime;
      if (previousState == STATE_VERY_LOW) {
        durationInVeryLow = timeEnteredShutdown - timeEnteredVeryLow;
        Serial.print("Time in VERY_LOW state: ");
        Serial.println(formatDuration(durationInVeryLow));
      }
      break;
      
    case STATE_CRITICAL:
      timeEnteredCritical = currentTime;
      if (previousState == STATE_SHUTDOWN) {
        durationInShutdown = timeEnteredCritical - timeEnteredShutdown;
        Serial.print("Time in SHUTDOWN state: ");
        Serial.println(formatDuration(durationInShutdown));
      }
      Serial.println("!!! CRITICAL: DISCONNECT BATTERY NOW !!!");
      break;
  }
  
  // Print summary statistics
  printStatistics();
  
  Serial.println("========================================");
  Serial.println();
  Serial.println(" Runtime | ADC  | Pin A9  | Battery | Cell Avg | Status");
  Serial.println("---------+------+---------+---------+----------+-------------------");
  
  // Display current reading after transition
  float batteryVoltage = readBatteryVoltage();
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
  Serial.println();
  Serial.println("Cumulative Statistics:");
  
  if (timeEnteredLow > 0) {
    Serial.print("  GOOD -> LOW transition at: ");
    Serial.println(formatDuration(timeEnteredLow));
  }
  
  if (timeEnteredVeryLow > 0) {
    Serial.print("  LOW -> VERY_LOW transition at: ");
    Serial.println(formatDuration(timeEnteredVeryLow));
  }
  
  if (timeEnteredShutdown > 0) {
    Serial.print("  VERY_LOW -> SHUTDOWN transition at: ");
    Serial.println(formatDuration(timeEnteredShutdown));
  }
  
  if (timeEnteredCritical > 0) {
    Serial.print("  SHUTDOWN -> CRITICAL transition at: ");
    Serial.println(formatDuration(timeEnteredCritical));
  }
  
  Serial.println();
  Serial.println("Total Time in Each State:");
  
  if (durationInGood > 0) {
    Serial.print("  GOOD: ");
    Serial.println(formatDuration(durationInGood));
  }
  
  if (durationInLow > 0) {
    Serial.print("  LOW: ");
    Serial.println(formatDuration(durationInLow));
  }
  
  if (durationInVeryLow > 0) {
    Serial.print("  VERY_LOW: ");
    Serial.println(formatDuration(durationInVeryLow));
  }
  
  if (durationInShutdown > 0) {
    Serial.print("  SHUTDOWN: ");
    Serial.println(formatDuration(durationInShutdown));
  }
  
  Serial.print("  Total Runtime: ");
  Serial.println(formatDuration(totalRunTime));
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
  
  // Runtime (format: HH:MM:SS)
  Serial.print(" ");
  Serial.print(formatDuration(totalRunTime));
  Serial.print(" | ");
  
  // ADC value (4 chars right-aligned)
  sprintf(buffer, "%4d", adcRaw);
  Serial.print(buffer);
  Serial.print(" | ");
  
  // Pin voltage (format: X.XXV)
  dtostrf(pinVoltage, 4, 2, buffer);
  Serial.print(buffer);
  Serial.print("V | ");
  
  // Battery voltage (format: XX.XXV)
  dtostrf(batteryVoltage, 5, 2, buffer);
  Serial.print(buffer);
  Serial.print("V | ");
  
  // Cell voltage (format: X.XXV)
  dtostrf(cellVoltage, 4, 2, buffer);
  Serial.print(buffer);
  Serial.print("V  | ");
  
  // Determine and print status (ASCII-safe for IDE compatibility)
  if (batteryVoltage >= VOLTAGE_GOOD) {
    Serial.println("[OK] GOOD");
  } else if (batteryVoltage >= VOLTAGE_SOFT_WARNING) {
    Serial.println("[WARN] LOW");
  } else if (batteryVoltage >= VOLTAGE_HARD_SHUTDOWN) {
    Serial.println("[WARN] VERY LOW");
  } else if (batteryVoltage >= VOLTAGE_CRITICAL) {
    Serial.println("[STOP] SHUTDOWN");
  } else {
    Serial.println("[CRIT] DAMAGE RISK!");
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
// UPDATE STATUS LED (DEPRECATED - NOW USING MORSE CODE)
// ============================================================================
void updateStatusLED(float batteryVoltage) {
  // This function is deprecated but kept for compatibility
  // LED control is now handled by updateMorseLED()
  // The morse code pattern is updated in updateBatteryState()
}

// ============================================================================
// CALIBRATION HELPER
// ============================================================================
/*
 * CALIBRATION PROCEDURE:
 * 
 * 1. Connect a fully charged 4S LiPo battery (16.8V)
 * 2. Measure actual battery voltage with a multimeter
 * 3. Note the "Battery" voltage displayed in Serial Monitor
 * 4. Calculate calibration factor:
 *      new_ratio = (actual_voltage / displayed_voltage) * 6.0
 * 5. Update VOLTAGE_DIVIDER_RATIO constant with new_ratio
 * 6. Re-upload and verify at multiple voltage levels
 * 
 * EXAMPLE:
 *   Multimeter reads: 16.75V
 *   Monitor displays: 16.55V
 *   Calibration: (16.75 / 16.55) * 6.0 = 6.07
 *   Update: const float VOLTAGE_DIVIDER_RATIO = 6.07;
 * 
 * NOTE: Use 1% tolerance resistors for best accuracy
 * 
 * VIEWING IN ARDUINO IDE:
 * - Open Serial Monitor: Tools -> Serial Monitor (Ctrl+Shift+M)
 * - Set baud rate to 115200 in dropdown (bottom-right)
 * - Enable "Show timestamp" for timing verification (optional)
 * - Set to "Both NL & CR" or "Newline" for proper formatting
 * - Output updates every 5 minutes (or immediately on state change)
 * - Observe LED on Teensy for continuous Morse code status indication:
 *     * SG  (... --. )      = STATUS GOOD (>= 14.0V)
 *     * OL  (--- .-..)      = OVERLOAD LOW (>= 13.6V)
 *     * OV  (--- ...-)      = OVERLOAD VERY LOW (>= 12.8V)
 *     * SOSS (... --- ... ...) = SOS SHUTDOWN (>= 12.0V)
 *     * SOSC (... --- ... -.-.) = SOS CRITICAL (< 12.0V)
 * 
 * RUNTIME TRACKING:
 * - Start time is recorded at startup
 * - Total runtime displayed in HH:MM:SS format
 * - Battery sampled every 500ms for accurate state detection
 * - Display output every 5 minutes to reduce serial clutter
 * - State transitions immediately logged with full statistics
 * - Cumulative statistics printed at each transition:
 *     * Time spent in each battery state
 *     * Transition timestamps
 *     * Total runtime
 * - Use this data to estimate battery life for foxhunt events
 * - LED provides continuous Morse code feedback between display updates
 * 
 * MORSE CODE TIMING:
 * - Speed: ~6 WPM (optimized for visual LED reading)
 * - Dot: 200ms (1 unit)
 * - Dash: 600ms (3 units)
 * - Element spacing: 200ms (1 unit - between dots/dashes within a letter)
 * - Letter spacing: 600ms (3 units - between letters in a message)
 * - Word/message spacing: 1400ms (7 units - before repeating message)
 * - Based on standard International Morse Code timing ratios
 * - Multi-letter messages provide clear status context
 */
