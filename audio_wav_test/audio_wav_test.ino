/*
 * WX7V Foxhunt Controller - WAV File Playback Test
 * 
 * VERSION: 1.6
 * 
 * PURPOSE: Test 4-part transmission sequence with Morse and WAV playback
 * 
 * HARDWARE:
 * - Teensy 4.1 with SD card
 * - Audio Output: Pin 12 (MQS audio via hardware PWM)
 * - PTT circuit: Pin 2 → 1kΩ → 2N2222 Base
 * - Optional: Radio connected for real transmission test
 * 
 * CRITICAL REQUIREMENT:
 * - Teensy must run at 150 MHz or higher for WAV playback
 * - Set in Arduino IDE: Tools → CPU Speed → 150 MHz (or higher)
 * - Lower speeds will cause WAV files to fail playback
 * 
 * AUDIO OUTPUT:
 *   Pin 10 (MQS output) → 10µF Cap (+)  10µF Cap (-) → 1kΩ Resistor  → GND
 *   Pin 12 (MQS output) → 10µF Cap (+)  10µF Cap (-) → 1kΩ Resistor → Potentiometer (left) → GND
 *   Potentiometer (wiper) → Radio Mic In (red wire)
 *   Potentiometer (right) → GND
 * 
 * AUDIO ARCHITECTURE:
 *   AudioPlaySdWav (WAV player) ──┐
 *   AudioSynthWaveformSine (Morse)─┤──> AudioMixer4 ──> AudioOutputMQS (Pin 12)
 *   
 *   Both WAV and Morse use Audio Library to avoid pin conflicts
 * 
 * SD CARD REQUIREMENTS:
 * - Format: FAT32
 * - Audio Files: WAV format (16-bit signed PCM)
 * - Sample Rate: 44.1kHz (preferred) or 22.05kHz
 * - Channels: Mono
 * - Remove all Meta data from the WAV files can cause playback issues.
 * - Filename: 8.3 format (e.g., FOXID.WAV, TEST.WAV)
 * 
 * TRANSMISSION SEQUENCE (4 parts):
 * 1. PTT ON + 1000ms pre-roll
 * 2. Morse: "V V V C" (Test/Confirmed) at 20 WPM using AudioSynthWaveformSine
 * 3. WAV: Play audio file for 15 seconds using AudioPlaySdWav
 * 4. Morse: "WX7V/F" (Station ID) at 20 WPM using AudioSynthWaveformSine
 * 5. PTT OFF after 500ms tail
 * 
 * USAGE:
 * 1. Insert SD card with WAV files into Teensy 4.1 built-in slot
 * 2. Set CPU Speed: Arduino IDE → Tools → CPU Speed → 150 MHz (or higher)
 * 3. Upload to Teensy 4.1 via Arduino IDE
 * 4. Open Tools → Serial Monitor (or Ctrl+Shift+M)
 * 5. Set baud rate to 115200 in Serial Monitor dropdown
 * 6. Script will run transmission test every 2 minutes (bench mode)
 * 
 * TEST MODES:
 * BENCH TEST: 15 sec WAV + 2 min interval (for testing)
 * FIELD MODE:  60 sec WAV + 4 min interval (for deployment)
 * Switch modes by editing timing constants in code (lines 110-117)
 * 
 * VIEWING RESULTS:
 * - Serial Monitor shows test progress and status
 * - SD card logging to WAVTEST.LOG
 * - LED blinks during Morse code transmission
 * - Listen on a separate receiver tuned to radio frequency
 * 
 * AUTHOR: WX7V
 * DATE: 2026-02-16
 * 
 * VERSION HISTORY:
 * v1.0 - Initial release with WAV playback, file listing, 15-second clip support
 * v1.1 - Added three-component test: WAV + Morse ID "WX7V/F" at 20 WPM
 * v1.2 - Fixed pin conflict: Use Audio Library for both WAV and Morse (no tone())
 * v1.3 - Fixed audio routing: Proper mono output to both MQS channels
 * v1.4 - Fixed MQS connection bug, added configurable bench/field timing modes
 * v1.5 - Simplified architecture, added 4-part sequence: V V V C + WAV + ID
 * v1.6 - Fixed mixer gains, added 150MHz CPU requirement for WAV playback
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
const char* VERSION = "1.6";
const char* VERSION_DATE = "2026-02-16";

// ============================================================================
// CONFIGURATION
// ============================================================================
//const char* WAV_FILENAME = "WWVBBL01.wav";  // WAV file to play

//const char* WAV_FILENAME = "JFKMoon1.wav";  // WAV file to play

const char* WAV_FILENAME = "WWVMarks.wav";  // WAV file to play


// ============================================================================
// PIN DEFINITIONS
// ============================================================================
const int PTT_PIN = 2;        // Pin 2 - PTT control (via 2N2222 transistor)
const int LED_PIN = 13;       // Built-in LED for status indication
// Audio Output: Pin 12 (MQS - controlled by Audio Library)

// ============================================================================
// TIMING CONFIGURATION - Field vs Bench Test
// ============================================================================
// BENCH TEST MODE (current setting - for testing):
const unsigned long PLAYBACK_DURATION_MS = 15000;   // 15 seconds WAV playback
const unsigned long TEST_INTERVAL_MS = 120000;      // 2 minutes between tests

// FIELD DEPLOYMENT MODE (comment out bench mode above, uncomment below):
// const unsigned long PLAYBACK_DURATION_MS = 60000;   // 60 seconds WAV playback  
// const unsigned long TEST_INTERVAL_MS = 240000;      // 4 minutes between tests

const unsigned long PTT_PREROLL_MS = 1000;   // Baofeng squelch opening time
const unsigned long PTT_TAIL_MS = 500;       // Hold PTT after audio ends

// ============================================================================
// MORSE CODE CONFIGURATION (20 WPM)
// ============================================================================
const int MORSE_TONE_FREQ = 800;             // Hz (standard CW tone)
const int MORSE_DOT_DURATION = 60;           // ms (20 WPM: 60ms dot)

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
// GLOBAL VARIABLES
// ============================================================================
unsigned long lastTestTime = 0;
unsigned long playbackStartTime = 0;
bool isPlaying = false;

// SD Card logging
File logFile;
bool loggingEnabled = true;

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
  Serial.println("WX7V Foxhunt Controller - WAV Test");
  Serial.print("Version: ");
  Serial.println(VERSION);
  Serial.print("Date: ");
  Serial.println(VERSION_DATE);
  Serial.println("========================================\n");
  
  // Initialize pin modes
  pinMode(PTT_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(PTT_PIN, LOW);   // PTT off
  digitalWrite(LED_PIN, LOW);   // LED off
  
  Serial.println("Pin Configuration:");
  Serial.println("  PTT:   Pin 2 (OUTPUT)");
  Serial.println("  LED:   Pin 13 (OUTPUT)");
  Serial.println("  Audio: Pin 12 (MQS output via Audio Library)");
  Serial.println();
  
  // Initialize Audio Library
  AudioMemory(16);  // Allocate 16 audio memory blocks for mixer + WAV
  Serial.println("Audio Library initialized (16 blocks)");
  
  // CRITICAL: Configure mixer gains (channels default to 0.0 = muted!)
  mixer1.gain(0, 0.5);  // WAV left channel at 50% volume
  mixer1.gain(1, 0.5);  // WAV right channel at 50% volume
  mixer1.gain(2, 0.8);  // Morse tone at 80% volume
  mixer1.gain(3, 0.0);  // Channel 3 unused (muted)
  Serial.println("Mixer gains configured:");
  Serial.println("  Ch0 (WAV L): 0.5");
  Serial.println("  Ch1 (WAV R): 0.5");
  Serial.println("  Ch2 (Morse): 0.8");
  Serial.println("  Ch3 (unused): 0.0");
  
  // Configure Morse tone generator (AudioSynthWaveformSine is always sine wave)
  toneMorse.frequency(MORSE_TONE_FREQ);
  toneMorse.amplitude(0.0);  // Start muted
  Serial.print("Morse tone generator: ");
  Serial.print(MORSE_TONE_FREQ);
  Serial.println(" Hz");
  Serial.println();
  
  // Initialize SD card
  Serial.print("Initializing SD card...");
  if (!SD.begin(BUILTIN_SDCARD)) {
    Serial.println(" FAILED!");
    Serial.println("ERROR: Cannot access SD card. Please check:");
    Serial.println("  1. SD card is inserted in Teensy 4.1 slot");
    Serial.println("  2. SD card is FAT32 formatted");
    Serial.println("  3. Card is not write-protected");
    while (1) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      delay(100);  // Fast blink = SD error
    }
  }
  Serial.println(" OK");
  
  // Check for WAV file
  Serial.print("Checking for WAV file: ");
  Serial.print(WAV_FILENAME);
  Serial.print("...");
  if (!SD.exists(WAV_FILENAME)) {
    Serial.println(" NOT FOUND!");
    Serial.print("ERROR: ");
    Serial.print(WAV_FILENAME);
    Serial.println(" not found on SD card.");
    while (1) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      delay(500);  // Slow blink = file not found
    }
  }
  Serial.println(" Found!");
  
  // List all WAV files on SD card
  listWavFiles();
  
  // Open log file
  logFile = SD.open("WAVTEST.LOG", FILE_WRITE);
  if (logFile) {
    Serial.println("\nLogging to: WAVTEST.LOG");
    logFile.println("\n========================================");
    logFile.print("WX7V WAV Test Log - ");
    logFile.println(__DATE__);
    logFile.println("========================================");
    logFile.flush();
  } else {
    Serial.println("\nWARNING: Could not open log file");
    loggingEnabled = false;
  }
  
  // Display test configuration
  Serial.println("\n========================================");
  Serial.println("TEST CONFIGURATION");
  Serial.println("========================================");
  Serial.print("WAV Playback Duration: ");
  Serial.print(PLAYBACK_DURATION_MS / 1000);
  Serial.println(" seconds");
  Serial.print("Test Interval: ");
  Serial.print(TEST_INTERVAL_MS / 1000);
  Serial.println(" seconds");
  Serial.print("Morse Speed: 20 WPM (dot = ");
  Serial.print(MORSE_DOT_DURATION);
  Serial.println(" ms)");
  Serial.print("Morse Tone: ");
  Serial.print(MORSE_TONE_FREQ);
  Serial.println(" Hz");
  Serial.println();
  
  Serial.println("TRANSMISSION SEQUENCE:");
  Serial.println("  1. PTT ON + 1000ms pre-roll");
  Serial.println("  2. Morse: 'V V V C' (Test/Confirmed)");
  Serial.println("  3. WAV: Play audio file for 15 seconds");
  Serial.println("  4. Morse: 'WX7V/F' (Station ID)");
  Serial.println("  5. PTT OFF after 500ms tail");
  Serial.println();
  
  Serial.println("========================================");
  Serial.println("Ready! First test in 10 seconds...");
  Serial.println("========================================\n");
  
  // Wait before first test
  delay(10000);
  lastTestTime = millis() - TEST_INTERVAL_MS;  // Trigger first test immediately after 10s
}

// ============================================================================
// MAIN LOOP
// ============================================================================
void loop() {
  unsigned long currentTime = millis();
  
  // Check if it's time to run a test
  if (currentTime - lastTestTime >= TEST_INTERVAL_MS) {
    lastTestTime = currentTime;
    runTransmissionTest();
  }
  
  // Blink LED when idle (heartbeat)
  static unsigned long lastHeartbeat = 0;
  if (currentTime - lastHeartbeat >= 1000) {
    lastHeartbeat = currentTime;
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }
}

// ============================================================================
// TRANSMISSION TEST SEQUENCE
// ============================================================================
void runTransmissionTest() {
  Serial.println("\n========================================");
  Serial.println("STARTING TRANSMISSION TEST");
  Serial.println("========================================");
  
  if (loggingEnabled && logFile) {
    logFile.println("\n--- Test Started ---");
    logFile.print("Time: ");
    logFile.println(millis());
    logFile.flush();
  }
  
  // ============== PART 1: PTT ON + PRE-ROLL ==============
  Serial.println("\n[1/5] PTT ON + Pre-roll");
  digitalWrite(PTT_PIN, HIGH);
  Serial.print("  PTT keyed, waiting ");
  Serial.print(PTT_PREROLL_MS);
  Serial.println(" ms for radio squelch...");
  delay(PTT_PREROLL_MS);
  Serial.println("  Radio ready!");
  
  // ============== PART 2: MORSE "V V V C" ==============
  Serial.println("\n[2/5] Morse: 'V V V C' (Test/Confirmed)");
  sendMorseMessage("V V V C");
  delay(500);  // Brief pause after Morse
  
  // ============== PART 3: PLAY WAV FILE ==============
  Serial.println("\n[3/5] Playing WAV file");
  Serial.print("  File: ");
  Serial.println(WAV_FILENAME);
  Serial.print("  Duration: ");
  Serial.print(PLAYBACK_DURATION_MS / 1000);
  Serial.println(" seconds");
  
  // Start playback
  if (playWav1.play(WAV_FILENAME)) {
    Serial.println("  WAV playback initiated, waiting for audio stream to start...");
    if (loggingEnabled && logFile) {
      logFile.print("Playing: ");
      logFile.println(WAV_FILENAME);
      logFile.flush();
    }
    
    // CRITICAL: Wait for playback to actually start (Audio Library needs time to initialize)
    unsigned long startWaitTime = millis();
    bool playbackStarted = false;
    while (millis() - startWaitTime < 500) {  // Wait up to 500ms for playback to start
      if (playWav1.isPlaying()) {
        playbackStarted = true;
        Serial.println("  Audio stream started successfully!");
        break;
      }
      delay(10);
    }
    
    if (!playbackStarted) {
      Serial.println("  WARNING: Audio stream did not start within 500ms");
      Serial.println("  Check: CPU speed set to 150 MHz or higher?");
      if (loggingEnabled && logFile) {
        logFile.println("WARNING: WAV playback did not start");
        logFile.flush();
      }
    } else {
      // Track playback time
      playbackStartTime = millis();
      isPlaying = true;
      
      // Monitor playback duration
      while (isPlaying) {
        unsigned long elapsed = millis() - playbackStartTime;
        
        // Check if we've played long enough
        if (elapsed >= PLAYBACK_DURATION_MS) {
          Serial.println("  Target duration reached, stopping playback");
          playWav1.stop();
          isPlaying = false;
          break;
        }
        
        // Check if file stopped naturally
        if (!playWav1.isPlaying()) {
          Serial.println("  WAV file finished naturally");
          isPlaying = false;
          break;
        }
        
        // Print progress every second
        static unsigned long lastProgress = 0;
        if (elapsed - lastProgress >= 1000) {
          lastProgress = elapsed;
          Serial.print("  Playing: ");
          Serial.print(elapsed / 1000);
          Serial.print(" / ");
          Serial.print(PLAYBACK_DURATION_MS / 1000);
          Serial.println(" seconds");
        }
        
        delay(100);  // Small delay for loop responsiveness
      }
      
      Serial.println("  WAV playback complete");
    }
    
  } else {
    Serial.println("  ERROR: Could not start WAV playback!");
    if (loggingEnabled && logFile) {
      logFile.println("ERROR: WAV playback failed");
      logFile.flush();
    }
  }
  
  delay(500);  // Brief pause after WAV
  
  // ============== PART 4: MORSE STATION ID ==============
  Serial.println("\n[4/5] Morse: 'WX7V/F' (Station ID)");
  sendMorseMessage("WX7V/F");
  
  // ============== PART 5: PTT OFF + TAIL ==============
  Serial.println("\n[5/5] PTT OFF");
  Serial.print("  Tail delay: ");
  Serial.print(PTT_TAIL_MS);
  Serial.println(" ms");
  delay(PTT_TAIL_MS);
  digitalWrite(PTT_PIN, LOW);
  Serial.println("  PTT un-keyed");
  
  if (loggingEnabled && logFile) {
    logFile.println("Test completed successfully");
    logFile.flush();
  }
  
  Serial.println("\n========================================");
  Serial.println("TRANSMISSION TEST COMPLETE");
  Serial.print("Next test in ");
  Serial.print(TEST_INTERVAL_MS / 1000);
  Serial.println(" seconds");
  Serial.println("========================================\n");
}

// ============================================================================
// MORSE CODE FUNCTIONS (Using Audio Library)
// ============================================================================

// Send a complete Morse code message
void sendMorseMessage(const char* message) {
  Serial.print("  Sending: '");
  Serial.print(message);
  Serial.println("'");
  
  int dotDuration = MORSE_DOT_DURATION;
  int dashDuration = dotDuration * 3;
  int wordSpace = dotDuration * 7;
  int charSpace = dotDuration * 3;
  
  for (int i = 0; message[i] != '\0'; i++) {
    char c = toupper(message[i]);
    
    if (c == ' ') {
      // Space between words
      Serial.print(" ");
      delay(wordSpace);
    } else {
      // Get Morse pattern for character
      String pattern = getMorsePattern(c);
      
      if (pattern.length() > 0) {
        Serial.print(c);
        Serial.print("(");
        Serial.print(pattern);
        Serial.print(") ");
        
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
  
  Serial.println();  // New line after message complete
  Serial.println("  Morse transmission complete");
}

// Send a Morse code dit (dot) using Audio Library
void sendDit(int dotDuration) {
  toneMorse.amplitude(1.0);  // Turn on tone at full amplitude
  digitalWrite(LED_PIN, HIGH);
  delay(dotDuration);
  toneMorse.amplitude(0.0);  // Turn off tone
  digitalWrite(LED_PIN, LOW);
  delay(dotDuration);  // Element space
}

// Send a Morse code dah (dash) using Audio Library
void sendDah(int dashDuration, int dotDuration) {
  toneMorse.amplitude(1.0);  // Turn on tone at full amplitude
  digitalWrite(LED_PIN, HIGH);
  delay(dashDuration);
  toneMorse.amplitude(0.0);  // Turn off tone
  digitalWrite(LED_PIN, LOW);
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
// SD CARD UTILITY FUNCTIONS
// ============================================================================

void listWavFiles() {
  Serial.println("\n========================================");
  Serial.println("WAV FILES ON SD CARD");
  Serial.println("========================================");
  
  File root = SD.open("/");
  if (!root) {
    Serial.println("ERROR: Cannot open root directory");
    return;
  }
  
  int fileCount = 0;
  while (true) {
    File entry = root.openNextFile();
    if (!entry) {
      break;  // No more files
    }
    
    String filename = entry.name();
    filename.toUpperCase();
    
    if (filename.endsWith(".WAV")) {
      fileCount++;
      Serial.print("  ");
      Serial.print(fileCount);
      Serial.print(". ");
      Serial.print(entry.name());
      Serial.print(" (");
      Serial.print(entry.size());
      Serial.println(" bytes)");
    }
    
    entry.close();
  }
  
  root.close();
  
  if (fileCount == 0) {
    Serial.println("  No WAV files found!");
  } else {
    Serial.print("\nTotal: ");
    Serial.print(fileCount);
    Serial.println(" WAV file(s)");
  }
  Serial.println("========================================");
}
