/*
 * WX7V Foxhunt Controller - Battery Monitor Test Script
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
 */

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
const float VOLTAGE_FULL = 16.8;          // 4.20V per cell
const float VOLTAGE_NOMINAL = 14.8;       // 3.70V per cell
const float VOLTAGE_SOFT_WARNING = 13.6;  // 3.40V per cell
const float VOLTAGE_HARD_SHUTDOWN = 12.8; // 3.20V per cell
const float VOLTAGE_CRITICAL = 12.0;      // 3.00V per cell (damage risk)

// ============================================================================
// SAMPLING CONFIGURATION
// ============================================================================
const int NUM_SAMPLES = 10;               // Number of ADC samples to average
const unsigned long SAMPLE_INTERVAL = 500; // Time between readings (ms)

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================
unsigned long lastSampleTime = 0;

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
  Serial.println("##                                    ##");
  Serial.println("########################################");
  Serial.println();
  Serial.println("Hardware Configuration:");
  Serial.println("  Pin A9: Battery voltage input");
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
  Serial.println("Battery Voltage Thresholds (4S LiPo):");
  Serial.print("  FULL:          ");
  Serial.print(VOLTAGE_FULL);
  Serial.println("V (4.20V/cell)");
  Serial.print("  NOMINAL:       ");
  Serial.print(VOLTAGE_NOMINAL);
  Serial.println("V (3.70V/cell)");
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
  Serial.println("Updates every 500ms");
  Serial.println("========================================");
  Serial.println();
  Serial.println(" ADC  | Pin A9  | Battery | Cell Avg | Status");
  Serial.println("------+---------+---------+----------+-------------------");
  
  lastSampleTime = millis();
}

// ============================================================================
// MAIN LOOP
// ============================================================================
void loop() {
  // Non-blocking delay
  if (millis() - lastSampleTime >= SAMPLE_INTERVAL) {
    lastSampleTime = millis();
    
    // Read and display battery voltage
    float batteryVoltage = readBatteryVoltage();
    displayVoltageReading(batteryVoltage);
    
    // Update LED based on battery status
    updateStatusLED(batteryVoltage);
  }
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
// DISPLAY VOLTAGE READING
// ============================================================================
void displayVoltageReading(float batteryVoltage) {
  // Calculate values
  int adcRaw = analogRead(BATTERY_PIN);
  float pinVoltage = (adcRaw / (float)ADC_MAX_VALUE) * ADC_REFERENCE_VOLTAGE;
  float cellVoltage = batteryVoltage / 4.0; // 4S battery
  
  // Format and print reading with fixed-width columns for alignment
  char buffer[80];
  
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
  if (batteryVoltage >= VOLTAGE_NOMINAL) {
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
// UPDATE STATUS LED
// ============================================================================
void updateStatusLED(float batteryVoltage) {
  static unsigned long lastBlinkTime = 0;
  static bool ledState = false;
  
  unsigned long blinkInterval;
  
  // Set blink rate based on battery status
  if (batteryVoltage >= VOLTAGE_NOMINAL) {
    blinkInterval = 1000; // Slow blink - good
  } else if (batteryVoltage >= VOLTAGE_SOFT_WARNING) {
    blinkInterval = 500;  // Medium blink - low
  } else if (batteryVoltage >= VOLTAGE_HARD_SHUTDOWN) {
    blinkInterval = 200;  // Fast blink - very low
  } else {
    blinkInterval = 100;  // Rapid blink - critical
  }
  
  // Non-blocking LED blink
  if (millis() - lastBlinkTime >= blinkInterval) {
    lastBlinkTime = millis();
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState);
  }
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
 * - Observe LED on Teensy for visual status indication:
 *     * Slow blink (1 sec): Battery GOOD
 *     * Medium blink (0.5 sec): Battery LOW
 *     * Fast blink (0.2 sec): Battery VERY LOW
 *     * Rapid blink (0.1 sec): Battery CRITICAL
 */
