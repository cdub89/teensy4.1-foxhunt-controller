/*
 * WX7V Foxhunt Controller - Audio Output Test Script
 * 
 * VERSION: 1.0
 * 
 * PURPOSE: Standalone test for audio output circuit using PWM tone generation
 * 
 * HARDWARE:
 * - Teensy 4.1
 * - Audio circuit: Pin 12 → 1kΩ → 10µF Cap → 10kΩ Pot → Radio Mic
 * - PTT circuit: Pin 2 → 1kΩ → 2N2222 Base
 * - Optional: Radio connected for real transmission test
 * 
 * CIRCUIT:
 *   Pin 12 → 1kΩ Resistor → 10µF Cap (+) → Potentiometer (left)
 *                            10µF Cap (-) → GND
 *   Potentiometer (wiper) → Radio Mic In (red wire)
 *   Potentiometer (right) → GND
 * 
 * USAGE:
 * 1. Upload to Teensy 4.1 via Arduino IDE
 * 2. Open Tools → Serial Monitor (or Ctrl+Shift+M)
 * 3. Set baud rate to 115200 in Serial Monitor dropdown
 * 4. The script will cycle through different test tones
 * 5. Adjust potentiometer for desired audio level
 * 6. Monitor with separate receiver or radio's speaker output
 * 
 * TEST SEQUENCE:
 * 1. 800Hz tone sweep (test basic audio output)
 * 2. Morse code "TEST" at different speeds
 * 3. Two-tone test (simulates DTMF-style multi-frequency)
 * 4. Frequency sweep (400Hz to 2000Hz)
 * 
 * VIEWING RESULTS:
 * - Serial Monitor shows current test and status
 * - LED blinks during audio output
 * - Listen on a separate receiver tuned to radio frequency
 * 
 * AUTHOR: WX7V
 * DATE: 2026-02-14
 * 
 * VERSION HISTORY:
 * v1.0 - Initial release with PWM tone generation and test patterns
 */

// ============================================================================
// VERSION INFORMATION
// ============================================================================
const char* VERSION = "1.0";
const char* VERSION_DATE = "2026-02-14";

// ============================================================================
// PIN DEFINITIONS
// ============================================================================
const int PTT_PIN = 2;        // Pin 2 - PTT control (via 2N2222 transistor)
const int AUDIO_PIN = 12;     // Pin 12 - PWM audio output
const int LED_PIN = 13;       // Built-in LED for status indication

// ============================================================================
// TIMING CONFIGURATION
// ============================================================================
const unsigned long PTT_PREROLL_MS = 1000;   // Baofeng squelch opening time
const unsigned long PTT_TAIL_MS = 500;       // Hold PTT after audio ends
const unsigned long TEST_INTERVAL_MS = 5000; // Pause between tests

// ============================================================================
// MORSE CODE CONFIGURATION
// ============================================================================
const int MORSE_TONE_FREQ = 800;             // Hz (standard CW tone)
const int MORSE_DOT_DURATION = 100;          // ms (12 WPM)
const int MORSE_DASH_DURATION = 300;         // ms (3x dot)
const int MORSE_ELEMENT_SPACE = 100;         // ms (between dots/dashes)
const int MORSE_LETTER_SPACE = 300;          // ms (3x dot)
const int MORSE_WORD_SPACE = 700;            // ms (7x dot)

// ============================================================================
// TEST CONFIGURATION
// ============================================================================
const int TEST_TONE_FREQ = 800;              // Hz
const int TEST_TONE_DURATION = 2000;         // ms
const int SWEEP_START_FREQ = 400;            // Hz
const int SWEEP_END_FREQ = 2000;             // Hz
const int SWEEP_STEP = 100;                  // Hz
const int SWEEP_STEP_DURATION = 200;         // ms per frequency

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================
int currentTest = 0;
unsigned long lastTestTime = 0;

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
  pinMode(PTT_PIN, OUTPUT);
  pinMode(AUDIO_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  
  // Ensure PTT is off at startup
  digitalWrite(PTT_PIN, LOW);
  digitalWrite(AUDIO_PIN, LOW);
  
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
  Serial.println("##   WX7V AUDIO OUTPUT TEST           ##");
  Serial.print("##   Version ");
  Serial.print(VERSION);
  Serial.print(" - ");
  Serial.print(VERSION_DATE);
  Serial.println("          ##");
  Serial.println("##                                    ##");
  Serial.println("########################################");
  Serial.println();
  Serial.println("Hardware Configuration:");
  Serial.println("  Pin 2:  PTT Control (via 2N2222)");
  Serial.println("  Pin 12: Audio Output (PWM)");
  Serial.println("  Pin 13: LED Status Indicator");
  Serial.println();
  Serial.println("Audio Circuit:");
  Serial.println("  Pin 12 -> 1kOhm -> 10uF Cap -> 10kOhm Pot -> Radio Mic");
  Serial.println();
  Serial.println("Test Sequence:");
  Serial.println("  1. Single tone test (800Hz)");
  Serial.println("  2. Morse code 'TEST' (slow)");
  Serial.println("  3. Morse code 'TEST' (fast)");
  Serial.println("  4. Two-tone test");
  Serial.println("  5. Frequency sweep (400-2000Hz)");
  Serial.println();
  Serial.println("========================================");
  Serial.println("Starting continuous test cycle...");
  Serial.println("Tests run every 5 seconds");
  Serial.println("========================================");
  Serial.println();
  
  lastTestTime = millis();
}

// ============================================================================
// MAIN LOOP
// ============================================================================
void loop() {
  unsigned long currentTime = millis();
  
  // Run test sequence with delays between tests
  if (currentTime - lastTestTime >= TEST_INTERVAL_MS) {
    lastTestTime = currentTime;
    
    // Execute current test
    switch (currentTest) {
      case 0:
        runSingleToneTest();
        break;
      case 1:
        runMorseTestSlow();
        break;
      case 2:
        runMorseTestFast();
        break;
      case 3:
        runTwoToneTest();
        break;
      case 4:
        runFrequencySweep();
        break;
      default:
        currentTest = 0;
        return;
    }
    
    // Advance to next test
    currentTest++;
    if (currentTest > 4) {
      currentTest = 0;
      Serial.println();
      Serial.println("=== Test cycle complete, restarting ===");
      Serial.println();
    }
  }
}

// ============================================================================
// TEST 1: SINGLE TONE (800Hz for 2 seconds)
// ============================================================================
void runSingleToneTest() {
  Serial.println("[TEST 1] Single Tone: 800Hz for 2 seconds");
  Serial.print("  PTT ON... ");
  
  // Key radio
  digitalWrite(PTT_PIN, HIGH);
  digitalWrite(LED_PIN, HIGH);
  delay(PTT_PREROLL_MS);
  
  Serial.println("Transmitting...");
  
  // Generate tone
  tone(AUDIO_PIN, TEST_TONE_FREQ);
  delay(TEST_TONE_DURATION);
  noTone(AUDIO_PIN);
  
  // Un-key radio
  Serial.print("  PTT OFF");
  delay(PTT_TAIL_MS);
  digitalWrite(PTT_PIN, LOW);
  digitalWrite(LED_PIN, LOW);
  
  Serial.println(" [DONE]");
  Serial.println();
}

// ============================================================================
// TEST 2: MORSE CODE "TEST" (Slow - 12 WPM)
// ============================================================================
void runMorseTestSlow() {
  Serial.println("[TEST 2] Morse Code: 'TEST' at 12 WPM");
  Serial.print("  PTT ON... ");
  
  // Key radio
  digitalWrite(PTT_PIN, HIGH);
  digitalWrite(LED_PIN, HIGH);
  delay(PTT_PREROLL_MS);
  
  Serial.println("Transmitting...");
  
  // Send "TEST" in Morse code
  sendMorseMessage("TEST", MORSE_DOT_DURATION);
  
  // Un-key radio
  Serial.print("  PTT OFF");
  delay(PTT_TAIL_MS);
  digitalWrite(PTT_PIN, LOW);
  digitalWrite(LED_PIN, LOW);
  
  Serial.println(" [DONE]");
  Serial.println();
}

// ============================================================================
// TEST 3: MORSE CODE "TEST" (Fast - 20 WPM)
// ============================================================================
void runMorseTestFast() {
  Serial.println("[TEST 3] Morse Code: 'TEST' at 20 WPM");
  Serial.print("  PTT ON... ");
  
  // Key radio
  digitalWrite(PTT_PIN, HIGH);
  digitalWrite(LED_PIN, HIGH);
  delay(PTT_PREROLL_MS);
  
  Serial.println("Transmitting...");
  
  // Send "TEST" in Morse code at faster speed
  sendMorseMessage("TEST", 60);  // 60ms dot = ~20 WPM
  
  // Un-key radio
  Serial.print("  PTT OFF");
  delay(PTT_TAIL_MS);
  digitalWrite(PTT_PIN, LOW);
  digitalWrite(LED_PIN, LOW);
  
  Serial.println(" [DONE]");
  Serial.println();
}

// ============================================================================
// TEST 4: TWO-TONE TEST (Alternating tones)
// ============================================================================
void runTwoToneTest() {
  Serial.println("[TEST 4] Two-Tone Test: 800Hz / 1200Hz alternating");
  Serial.print("  PTT ON... ");
  
  // Key radio
  digitalWrite(PTT_PIN, HIGH);
  digitalWrite(LED_PIN, HIGH);
  delay(PTT_PREROLL_MS);
  
  Serial.println("Transmitting...");
  
  // Alternate between two tones
  for (int i = 0; i < 4; i++) {
    tone(AUDIO_PIN, 800);
    delay(300);
    tone(AUDIO_PIN, 1200);
    delay(300);
  }
  noTone(AUDIO_PIN);
  
  // Un-key radio
  Serial.print("  PTT OFF");
  delay(PTT_TAIL_MS);
  digitalWrite(PTT_PIN, LOW);
  digitalWrite(LED_PIN, LOW);
  
  Serial.println(" [DONE]");
  Serial.println();
}

// ============================================================================
// TEST 5: FREQUENCY SWEEP (400Hz to 2000Hz)
// ============================================================================
void runFrequencySweep() {
  Serial.println("[TEST 5] Frequency Sweep: 400Hz to 2000Hz");
  Serial.print("  PTT ON... ");
  
  // Key radio
  digitalWrite(PTT_PIN, HIGH);
  digitalWrite(LED_PIN, HIGH);
  delay(PTT_PREROLL_MS);
  
  Serial.println("Transmitting...");
  
  // Sweep through frequencies
  for (int freq = SWEEP_START_FREQ; freq <= SWEEP_END_FREQ; freq += SWEEP_STEP) {
    Serial.print("    Frequency: ");
    Serial.print(freq);
    Serial.println("Hz");
    tone(AUDIO_PIN, freq);
    delay(SWEEP_STEP_DURATION);
  }
  noTone(AUDIO_PIN);
  
  // Un-key radio
  Serial.print("  PTT OFF");
  delay(PTT_TAIL_MS);
  digitalWrite(PTT_PIN, LOW);
  digitalWrite(LED_PIN, LOW);
  
  Serial.println(" [DONE]");
  Serial.println();
}

// ============================================================================
// SEND MORSE CODE MESSAGE
// ============================================================================
void sendMorseMessage(const char* message, int dotDuration) {
  for (int i = 0; message[i] != '\0'; i++) {
    char c = toupper(message[i]);
    
    // Handle space
    if (c == ' ') {
      delay(dotDuration * 7);
      continue;
    }
    
    // Get Morse pattern
    String pattern = getMorsePattern(c);
    
    // Send pattern
    for (unsigned int j = 0; j < pattern.length(); j++) {
      if (pattern[j] == '.') {
        sendDit(dotDuration);
      } else if (pattern[j] == '-') {
        sendDah(dotDuration);
      }
    }
    
    // Letter space
    delay(dotDuration * 3);
  }
}

// ============================================================================
// MORSE CODE: DIT (dot)
// ============================================================================
void sendDit(int dotDuration) {
  tone(AUDIO_PIN, MORSE_TONE_FREQ);
  delay(dotDuration);
  noTone(AUDIO_PIN);
  delay(dotDuration);  // Element space
}

// ============================================================================
// MORSE CODE: DAH (dash)
// ============================================================================
void sendDah(int dotDuration) {
  tone(AUDIO_PIN, MORSE_TONE_FREQ);
  delay(dotDuration * 3);
  noTone(AUDIO_PIN);
  delay(dotDuration);  // Element space
}

// ============================================================================
// GET MORSE PATTERN FOR CHARACTER
// ============================================================================
String getMorsePattern(char c) {
  switch (c) {
    // Letters
    case 'A': return ".-";
    case 'B': return "-...";
    case 'C': return "-.-.";
    case 'D': return "-..";
    case 'E': return ".";
    case 'F': return "..-.";
    case 'G': return "--.";
    case 'H': return "....";
    case 'I': return "..";
    case 'J': return ".---";
    case 'K': return "-.-";
    case 'L': return ".-..";
    case 'M': return "--";
    case 'N': return "-.";
    case 'O': return "---";
    case 'P': return ".--.";
    case 'Q': return "--.-";
    case 'R': return ".-.";
    case 'S': return "...";
    case 'T': return "-";
    case 'U': return "..-";
    case 'V': return "...-";
    case 'W': return ".--";
    case 'X': return "-..-";
    case 'Y': return "-.--";
    case 'Z': return "--..";
    
    // Numbers
    case '0': return "-----";
    case '1': return ".----";
    case '2': return "..---";
    case '3': return "...--";
    case '4': return "....-";
    case '5': return ".....";
    case '6': return "-....";
    case '7': return "--...";
    case '8': return "---..";
    case '9': return "----.";
    
    // Punctuation
    case '/': return "-..-.";
    case '?': return "..--..";
    case '.': return ".-.-.-";
    case ',': return "--..--";
    
    // Unknown character
    default: return "";
  }
}

// ============================================================================
// CALIBRATION & TESTING NOTES
// ============================================================================
/*
 * AUDIO LEVEL ADJUSTMENT:
 * 
 * 1. Connect radio with audio cable and PTT circuit
 * 2. Set radio to LOW power (1W) via Menu → 2 (TXP) → LOW
 * 3. Tune a separate receiver to the radio's frequency
 * 4. Run this test script
 * 5. Adjust 10kΩ potentiometer for clear audio (not distorted)
 * 6. Optimal level: Strong signal without clipping/distortion
 * 
 * CIRCUIT VERIFICATION:
 * 
 * Without radio connected:
 * - Measure voltage at Pin 12 with multimeter (should show ~1.65V DC)
 * - During tone output, voltage will fluctuate (PWM signal)
 * - LED should blink during transmission
 * - PTT pin should go HIGH (3.3V) during tests
 * 
 * With radio connected:
 * - PTT LED on radio should light during tests
 * - Audio should be clear and undistorted
 * - Morse code should be recognizable
 * - Frequency sweep should sound smooth
 * 
 * TROUBLESHOOTING:
 * 
 * No audio heard:
 * - Check potentiometer wiring
 * - Verify 10µF capacitor polarity
 * - Confirm radio is in LOW power mode
 * - Check radio frequency matches receiver
 * 
 * Distorted audio:
 * - Reduce potentiometer level
 * - Check 1kΩ resistor is installed
 * - Verify capacitor value (10µF, not 100µF)
 * 
 * Weak audio:
 * - Increase potentiometer level
 * - Check all connections
 * - Verify Pin 12 is correct (not Pin 11)
 * 
 * PTT not working:
 * - Verify 2N2222 transistor pinout (E-B-C)
 * - Check 1kΩ base resistor
 * - Measure collector voltage (should go to ~0V when PTT active)
 */
