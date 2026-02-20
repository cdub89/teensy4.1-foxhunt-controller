/* * FOXHUNT CONTROLLER v3.6 - Non-Blocking Dual WAV with Fallback Test Audio
 * CPU: 150MHz | Battery: 12V LiFePO4 
 * Hardware: 10k/2k Voltage Divider on A9
 * Plays: WWVMarks.wav + WX7V_ID12.wav per cycle
 * Fallback: If WAV fails, plays diagnostic test sequence
 */

#include <Audio.h>
#include <SD.h>
#include <TimeLib.h>  // Teensy time library for logging timestamps
 
AudioPlaySdWav           playWav1;
AudioSynthWaveformSine   waveform1;      // Sine wave for morse/fallback tones
AudioMixer4              mixer1;         // Mix WAV and tone sources
AudioOutputMQS           mqs1; 
AudioConnection          patchCord1(playWav1, 0, mixer1, 0);
AudioConnection          patchCord2(playWav1, 1, mixer1, 1);
AudioConnection          patchCord3(waveform1, 0, mixer1, 2);
AudioConnection          patchCord4(mixer1, 0, mqs1, 0);
AudioConnection          patchCord5(mixer1, 0, mqs1, 1);
 
// Pin Definitions
const int PTT_PIN = 2;
const int BATT_PIN = A9;

// Timing Constants (all in milliseconds)
const unsigned long PTT_PREROLL_MS = 1000;      // Pre-roll delay for radio squelch opening
const unsigned long PTT_TAIL_MS = 500;          // Hold PTT after audio ends (prevents cut-off)
const unsigned long CYCLE_WAIT_TIME = 240000;   // 4 minutes between cycles
const unsigned long MAX_PTT_TIME = 30000;       // SAFETY: 30s max transmit time

// Battery Protection
const float LVP_CUTOFF = 10.8; 

// State Machine States
enum State {
  FIRST_BOOT_TEST,     // Run fallback test sequence on first boot
  IDLE,
  PTT_PREROLL,         // Renamed from PTT_DELAY for clarity
  PLAYING_MORSE_ID,    // Play "VVV C" morse ID before audio
  PLAYING_AUDIO_1,     // First WAV file (WWVMarks.wav)
  PLAYING_AUDIO_2,     // Second WAV file (WX7V_ID12.wav)
  PLAYING_FALLBACK,    // Fallback test audio sequence when WAV fails
  PTT_TAIL,            // New state: Hold PTT after audio before releasing
  WAIT_CYCLE
};

// Fallback Audio Test Sequence States
enum FallbackTest {
  FB_SINGLE_TONE,      // 600Hz for 2 seconds
  FB_MORSE_SLOW,       // Morse "TEST" at 12 WPM
  FB_MORSE_FAST,       // Morse "TEST" at 20 WPM
  FB_TWO_TONE,         // 600Hz/1200Hz alternating
  FB_FREQUENCY_SWEEP,  // 400Hz to 2000Hz sweep
  FB_COMPLETE          // Test sequence finished
};

// Morse Code Element States
enum MorseState {
  MORSE_IDLE,
  MORSE_TONE_ON,
  MORSE_TONE_OFF
};

// State Variables
State currentState = FIRST_BOOT_TEST;  // Start with first boot test
unsigned long stateStartTime = 0;
unsigned long pttStartTime = 0;
bool pttActive = false;
unsigned long cycleCount = 0;
bool firstBoot = true;  // Flag for first boot test sequence

// Audio Playback Tracking
bool audioStartupPhase = false;    // Track if waiting for audio to start
unsigned long audioStartTime = 0;   // When play() was called

// Fallback Audio Variables
FallbackTest currentFallbackTest = FB_SINGLE_TONE;
unsigned long fallbackTestStartTime = 0;
int fallbackSweepFreq = 400;       // Current frequency in sweep
int twoToneCount = 0;              // Counter for two-tone alternations

// Morse Code Variables (simplified for blocking implementation)
const char* morseMessage = "TEST";
unsigned long morseDotDuration = 100;  // 12 WPM default

// Morse Code Patterns (same as audio_test.ino)
const char* MORSE_T = "-";
const char* MORSE_E = ".";
const char* MORSE_S = "...";

// Test Timing Constants (from audio_test.ino)
const int TEST_TONE_FREQ = 600;
const int TEST_TONE_DURATION = 2000;
const int MORSE_TONE_FREQ = 600;
const int SWEEP_START_FREQ = 400;
const int SWEEP_END_FREQ = 2000;
const int SWEEP_STEP = 100;
const int SWEEP_STEP_DURATION = 200;

// WAV File Names
const char* WAV_FILE_1 = "WWVMARKS.WAV";   // First file: WWV time marks
//const char* WAV_FILE_1 = "Skylab02.WAV";   // First file: WWV time marks
//const char* WAV_FILE_1 = "Apollo8a.WAV";   // First file: WWV time marks/
//const char* WAV_FILE_1 = "WWVBBL01.WAV";   // First file: WWV time marks
//const char* WAV_FILE_1 = "JFKmoon1.WAV";   // First file: WWV time marks

const char* WAV_FILE_2 = "WX7V_ID2.WAV";   // Second file: Station ID

// SD Card Logging
File logFile;
bool loggingEnabled = false;
const char* LOG_FILE_NAME = "CONTRLR.LOG";  // 8.3 filename format for SD card

// Function Prototypes
void logPrint(const char* str);
void logPrint(const String& str);
void logPrint(int val);
void logPrint(float val, int decimals);
void logPrintln(const char* str);
void logPrintln(const String& str);
void logPrintln(int val);
void logPrintln();
void printTimestampToLog();
void printDateTimeToFile(File &file);
time_t getTeensy3Time();
void runFallbackAudioSequence();
void sendMorseMessage(const char* message);
void sendDit(int dotDuration);
void sendDah(int dashDuration, int dotDuration);
String getMorsePattern(char c);
float getBatteryVoltage();

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000); // Wait up to 3s for Serial
  
  pinMode(PTT_PIN, OUTPUT);
  digitalWrite(PTT_PIN, LOW);
  
  AudioMemory(20);
  
  // Configure audio mixer: WAV on channels 0-1, Waveform on channel 2
  mixer1.gain(0, 0.5);  // WAV left channel at 50%
  mixer1.gain(1, 0.5);  // WAV right channel at 50%
  mixer1.gain(2, 0.8);  // Morse/tone at 80% (matches audio_wav_test.ino)
  mixer1.gain(3, 0.0);  // Unused
  
  // Configure waveform generator for fallback audio and morse
  waveform1.frequency(600);
  waveform1.amplitude(0.0);  // Start silent
  
  // Initialize SD card for WAV playback and logging
  if (!(SD.begin(BUILTIN_SDCARD))) {
    Serial.println("ERROR: SD card initialization failed!");
    loggingEnabled = false;
  } else {
    Serial.println("SD card initialized successfully");
    loggingEnabled = true;
    
    // Write startup header to log file
    logFile = SD.open(LOG_FILE_NAME, FILE_WRITE);
    if (logFile) {
      logFile.println();
      logFile.println("========================================");
      logFile.println("WX7V Foxhunt Controller v3.6 - STARTUP");
      logFile.print("Time: ");
      printDateTimeToFile(logFile);
      logFile.println();
      logFile.println("========================================");
      logFile.close();
    }
  }
  
  // Set up Teensy RTC time source
  setSyncProvider(getTeensy3Time);
  
  Serial.println("WX7V Foxhunt Controller v3.6 - Non-Blocking Dual WAV + Fallback");
  Serial.print("Audio Files: ");
  Serial.print(WAV_FILE_1);
  Serial.print(" + ");
  Serial.println(WAV_FILE_2);
  Serial.println("Fallback: Diagnostic test sequence if WAV fails");
  if (loggingEnabled) {
    Serial.print("Logging to: ");
    Serial.println(LOG_FILE_NAME);
  }
  Serial.println("Starting first boot test sequence...");
  currentState = FIRST_BOOT_TEST;
  stateStartTime = millis();
}

void loop() {
  // Battery voltage monitoring at start of every loop iteration
  float batteryVoltage = getBatteryVoltage();
  
  // SAFETY: Battery watchdog - checked every loop
  if (batteryVoltage < LVP_CUTOFF) {
    logPrint("CRITICAL: Low voltage detected: ");
    logPrint(batteryVoltage, 2);
    logPrintln("V - Shutting down PTT");
    digitalWrite(PTT_PIN, LOW);
    pttActive = false;
    while(1); // Halt system
  }

  // SAFETY: PTT timeout watchdog
  if (pttActive && (millis() - pttStartTime > MAX_PTT_TIME)) {
    logPrintln("SAFETY: PTT timeout exceeded - forcing PTT off");
    digitalWrite(PTT_PIN, LOW);
    pttActive = false;
    currentState = WAIT_CYCLE;
    stateStartTime = millis();
  }

  // State Machine
  switch (currentState) {
    case FIRST_BOOT_TEST:
      // Run fallback test sequence on first boot (without PTT)
      if (firstBoot) {
        logPrintln();
        logPrintln("========================================");
        logPrintln("FIRST BOOT: Running diagnostic test audio");
        logPrintln("========================================");
        
        // Key PTT for startup test
        digitalWrite(PTT_PIN, HIGH);
        pttActive = true;
        pttStartTime = millis();
        logPrintln("PTT: ON (First Boot Test)");
        
        // Wait for PTT preroll
        delay(PTT_PREROLL_MS);
        
        // Initialize fallback test
        currentFallbackTest = FB_SINGLE_TONE;
        fallbackTestStartTime = millis();
        firstBoot = false;  // Clear flag after first test
        
        currentState = PLAYING_FALLBACK;
        stateStartTime = millis();
      } else {
        // Should never reach here, but safety fallback
        currentState = IDLE;
      }
      break;

    case IDLE:
      // Ready to start new transmission cycle
      logPrintln();
      logPrintln("========================================");
      logPrint("Starting Cycle #");
      logPrintln(cycleCount + 1);
      logPrint("Battery Voltage: ");
      logPrint(batteryVoltage, 2);
      logPrintln("V");
      printTimestampToLog();
      logPrintln("========================================");
      
      // Key PTT
      digitalWrite(PTT_PIN, HIGH);
      pttActive = true;
      pttStartTime = millis();
      logPrintln("PTT: ON");
      
      cycleCount++;
      currentState = PTT_PREROLL;
      stateStartTime = millis();
      break;

    case PTT_PREROLL:
      // Wait for PTT pre-roll delay before starting morse ID
      if (millis() - stateStartTime >= PTT_PREROLL_MS) {
        logPrint("Pre-roll complete (");
        logPrint((int)PTT_PREROLL_MS);
        logPrintln("ms)");
        logPrintln("Starting morse ID: VVV C");
        
        // Set morse speed for ID
        morseDotDuration = 100;  // 12 WPM
        
        currentState = PLAYING_MORSE_ID;
        stateStartTime = millis();
      }
      break;

    case PLAYING_MORSE_ID:
      // Play "VVV C" morse ID before audio (blocking morse is OK - only a few seconds)
      logPrintln("Sending morse ID: V V V C");
      sendMorseMessage("V V V C");  // Spaces = word spacing (7 dit)
      logPrintln("Morse ID complete");
      
      // Now start first WAV file
      logPrint("Starting first audio file: ");
      logPrintln(WAV_FILE_1);

      playWav1.play(WAV_FILE_1);
      
      audioStartupPhase = true;      // Enter startup monitoring phase
      audioStartTime = millis();     // Track when we called play()
      
      currentState = PLAYING_AUDIO_1;
      stateStartTime = millis();
      break;

    case PLAYING_AUDIO_1:
      // Two-phase audio monitoring for FIRST file: startup verification, then completion monitoring
      if (audioStartupPhase) {
        // PHASE 1: Wait for audio stream to actually start
        if (playWav1.isPlaying()) {
          // Audio has started successfully!
          logPrintln("Audio stream 1 started successfully");
          audioStartupPhase = false;  // Move to monitoring phase
        } else if (millis() - audioStartTime > 500) {
          // Timeout: Audio didn't start within 500ms
          logPrint("WARNING: ");
          logPrint(WAV_FILE_1);
          logPrintln(" did not start within 500ms");
          logPrintln("FALLBACK: Starting diagnostic test audio sequence");
          
          // Enter fallback audio mode
          audioStartupPhase = false;
          currentFallbackTest = FB_SINGLE_TONE;
          fallbackTestStartTime = millis();
          currentState = PLAYING_FALLBACK;
          stateStartTime = millis();
        }
      } else {
        // PHASE 2: Monitor for playback completion
        if (!playWav1.isPlaying()) {
          unsigned long playbackDuration = millis() - audioStartTime;
          logPrint("First audio complete (duration: ");
          logPrint((int)playbackDuration);
          logPrintln("ms)");
          
          // Start second file immediately
          logPrint("Starting second audio file: ");
          logPrintln(WAV_FILE_2);
          
          playWav1.play(WAV_FILE_2);
          audioStartupPhase = true;      // Reset to startup phase for second file
          audioStartTime = millis();     // Track when we called play()
          
          currentState = PLAYING_AUDIO_2;
          stateStartTime = millis();
        }
      }
      break;

    case PLAYING_AUDIO_2:
      // Two-phase audio monitoring for SECOND file: startup verification, then completion monitoring
      if (audioStartupPhase) {
        // PHASE 1: Wait for audio stream to actually start
        if (playWav1.isPlaying()) {
          // Audio has started successfully!
          logPrintln("Audio stream 2 started successfully");
          audioStartupPhase = false;  // Move to monitoring phase
        } else if (millis() - audioStartTime > 500) {
          // Timeout: Audio didn't start within 500ms
          logPrint("WARNING: ");
          logPrint(WAV_FILE_2);
          logPrintln(" did not start within 500ms");
          logPrintln("FALLBACK: Starting diagnostic test audio sequence");
          
          // Enter fallback audio mode
          audioStartupPhase = false;
          currentFallbackTest = FB_SINGLE_TONE;
          fallbackTestStartTime = millis();
          currentState = PLAYING_FALLBACK;
          stateStartTime = millis();
        }
      } else {
        // PHASE 2: Monitor for playback completion
        if (!playWav1.isPlaying()) {
          unsigned long playbackDuration = millis() - audioStartTime;
          logPrint("Second audio complete (duration: ");
          logPrint((int)playbackDuration);
          logPrintln("ms)");
          
          currentState = PTT_TAIL;
          stateStartTime = millis();
        }
      }
      break;

    case PLAYING_FALLBACK:
      // Run diagnostic test audio sequence when WAV files fail
      runFallbackAudioSequence();
      break;

    case PTT_TAIL:
      {
        // Hold PTT after audio ends to prevent cut-off
        if (millis() - stateStartTime >= PTT_TAIL_MS) {
          logPrint("Tail delay complete (");
          logPrint((int)PTT_TAIL_MS);
          logPrintln("ms)");
          
          // Release PTT
          digitalWrite(PTT_PIN, LOW);
          pttActive = false;
          logPrintln("PTT: OFF");
          
          unsigned long transmitDuration = millis() - pttStartTime;
          logPrint("Total transmit time: ");
          logPrint((int)transmitDuration);
          logPrintln("ms");
          
          currentState = WAIT_CYCLE;
          stateStartTime = millis();
        }
      }
      break;

    case WAIT_CYCLE:
      // Wait for cycle interval before next transmission
      if (millis() - stateStartTime >= CYCLE_WAIT_TIME) {
        logPrint("Cycle #");
        logPrint((int)cycleCount);
        logPrintln(" COMPLETE");
        logPrint("Battery Voltage: ");
        logPrint(batteryVoltage, 2);
        logPrintln("V");
        
        currentState = IDLE;
      }
      break;
  }
  
  // Loop executes quickly, allowing concurrent tasks
  // No blocking delays - system remains responsive
}

// ============================================================================
// FALLBACK AUDIO SEQUENCE - Diagnostic test tones when WAV fails
// ============================================================================
void runFallbackAudioSequence() {
  unsigned long currentTime = millis();
  
  switch (currentFallbackTest) {
    case FB_SINGLE_TONE:
      // Test 1: 600Hz tone for 2 seconds
      if (currentTime - fallbackTestStartTime == 0) {
        logPrintln("[FALLBACK TEST 1] Single Tone: 600Hz for 2 seconds");
        waveform1.frequency(TEST_TONE_FREQ);
        waveform1.amplitude(0.3);
      }
      
      if (currentTime - fallbackTestStartTime >= TEST_TONE_DURATION) {
        waveform1.amplitude(0);  // Silence
        logPrintln("  Test 1 complete");
        currentFallbackTest = FB_MORSE_SLOW;
        fallbackTestStartTime = currentTime;
      }
      break;
      
    case FB_MORSE_SLOW:
      // Test 2: Morse "TEST" at 12 WPM (100ms dot)
      if (currentTime - fallbackTestStartTime == 0) {
        logPrintln("[FALLBACK TEST 2] Morse Code: 'TEST' at 12 WPM");
        morseDotDuration = 100;  // 12 WPM
        sendMorseMessage("TEST");
        logPrintln("  Test 2 complete");
      }
      
      // Move to next test immediately (morse is blocking)
      currentFallbackTest = FB_MORSE_FAST;
      fallbackTestStartTime = millis();
      break;
      
    case FB_MORSE_FAST:
      // Test 3: Morse "TEST" at 20 WPM (60ms dot)
      if (currentTime - fallbackTestStartTime == 0) {
        logPrintln("[FALLBACK TEST 3] Morse Code: 'TEST' at 20 WPM");
        morseDotDuration = 60;  // 20 WPM
        sendMorseMessage("TEST");
        logPrintln("  Test 3 complete");
      }
      
      // Move to next test immediately (morse is blocking)
      currentFallbackTest = FB_TWO_TONE;
      fallbackTestStartTime = millis();
      twoToneCount = 0;
      break;
      
    case FB_TWO_TONE:
      // Test 4: Two-tone test (600Hz/1200Hz alternating)
      if (twoToneCount == 0 && (currentTime - fallbackTestStartTime) < 50) {
        logPrintln("[FALLBACK TEST 4] Two-Tone Test: 600Hz / 1200Hz alternating");
      }
      
      {
        unsigned long elapsed = currentTime - fallbackTestStartTime;
        int segment = (elapsed / 300) % 2;  // Switch every 300ms
        int cycle = elapsed / 600;          // Complete cycle every 600ms
        
        if (cycle < 4) {  // 4 complete cycles = 8 tones
          if (segment == 0) {
            waveform1.frequency(600);
            waveform1.amplitude(0.3);
          } else {
            waveform1.frequency(1200);
            waveform1.amplitude(0.3);
          }
        } else {
          waveform1.amplitude(0);  // Silence
          logPrintln("  Test 4 complete");
          currentFallbackTest = FB_FREQUENCY_SWEEP;
          fallbackTestStartTime = currentTime;
          fallbackSweepFreq = SWEEP_START_FREQ;
        }
      }
      break;
      
    case FB_FREQUENCY_SWEEP:
      // Test 5: Frequency sweep 400Hz to 2000Hz
      if (fallbackSweepFreq == SWEEP_START_FREQ && (currentTime - fallbackTestStartTime) < 50) {
        logPrintln("[FALLBACK TEST 5] Frequency Sweep: 400Hz to 2000Hz");
      }
      
      {
        unsigned long elapsed = currentTime - fallbackTestStartTime;
        int stepNumber = elapsed / SWEEP_STEP_DURATION;
        int targetFreq = SWEEP_START_FREQ + (stepNumber * SWEEP_STEP);
        
        if (targetFreq <= SWEEP_END_FREQ) {
          if (targetFreq != fallbackSweepFreq) {
            fallbackSweepFreq = targetFreq;
            waveform1.frequency(fallbackSweepFreq);
            waveform1.amplitude(0.3);
            logPrint("    Frequency: ");
            logPrint(fallbackSweepFreq);
            logPrintln("Hz");
          }
        } else {
          waveform1.amplitude(0);  // Silence
          logPrintln("  Test 5 complete");
          logPrintln("FALLBACK: All diagnostic tests complete");
          currentFallbackTest = FB_COMPLETE;
          
          // If this was first boot test, go to IDLE, otherwise go to PTT_TAIL
          if (cycleCount == 0 && pttActive) {
            // First boot test complete
            logPrintln("First boot test complete - releasing PTT");
            delay(PTT_TAIL_MS);
            digitalWrite(PTT_PIN, LOW);
            pttActive = false;
            logPrintln("PTT: OFF");
            logPrintln("Entering normal operation mode");
            currentState = IDLE;
          } else {
            // Regular fallback during normal operation
            currentState = PTT_TAIL;
          }
          stateStartTime = currentTime;
        }
      }
      break;
      
    case FB_COMPLETE:
      // Should not reach here (transition to PTT_TAIL happens in FB_FREQUENCY_SWEEP)
      currentState = PTT_TAIL;
      stateStartTime = currentTime;
      break;
  }
}

// ============================================================================
// MORSE CODE GENERATOR - Simple blocking version (from audio_wav_test.ino)
// ============================================================================
void sendMorseMessage(const char* message) {
  // Ensure morse frequency is always set correctly (in case fallback changed it)
  waveform1.frequency(MORSE_TONE_FREQ);
  
  int dotDuration = morseDotDuration;
  int dashDuration = dotDuration * 3;
  int wordSpace = dotDuration * 7;
  int charSpace = dotDuration * 3;
  
  for (int i = 0; message[i] != '\0'; i++) {
    char c = toupper(message[i]);
    
    // Handle space between words (standard morse - matches audio_wav_test.ino)
    if (c == ' ') {
      delay(wordSpace);
      continue;
    }
    
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
      
      // Space between characters (letter space)
      delay(charSpace);
    }
  }
}

// Send a Morse code dit (dot) 
void sendDit(int dotDuration) {
  waveform1.amplitude(1.0);  // Turn on tone at full amplitude
  delay(dotDuration);
  waveform1.amplitude(0.0);  // Turn off tone
  delay(dotDuration);  // Element space
}

// Send a Morse code dah (dash)
void sendDah(int dashDuration, int dotDuration) {
  waveform1.amplitude(1.0);  // Turn on tone at full amplitude
  delay(dashDuration);
  waveform1.amplitude(0.0);  // Turn off tone
  delay(dotDuration);  // Element space
}

// ============================================================================
// GET MORSE PATTERN FOR CHARACTER (from audio_wav_test.ino)
// ============================================================================
String getMorsePattern(char c) {
  switch (toupper(c)) {
    case 'T': return "-";
    case 'E': return ".";
    case 'S': return "...";
    case 'V': return "...-";
    case 'C': return "-.-.";
    // Add more characters as needed
    default: return "";
  }
}

// ============================================================================
// TEENSY RTC TIME FUNCTIONS
// ============================================================================
time_t getTeensy3Time() {
  return Teensy3Clock.get();
}

void printTimestampToLog() {
  char timestamp[30];
  sprintf(timestamp, "[%04d-%02d-%02d %02d:%02d:%02d] ", 
          year(), month(), day(), hour(), minute(), second());
  logPrint(timestamp);
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

// ============================================================================
// DUAL-OUTPUT LOGGING FUNCTIONS (Serial + SD Card)
// ============================================================================
void logPrint(const char* str) {
  Serial.print(str);
  if (loggingEnabled) {
    logFile = SD.open(LOG_FILE_NAME, FILE_WRITE);
    if (logFile) {
      logFile.print(str);
      logFile.close();
    }
  }
}

void logPrint(const String& str) {
  Serial.print(str);
  if (loggingEnabled) {
    logFile = SD.open(LOG_FILE_NAME, FILE_WRITE);
    if (logFile) {
      logFile.print(str);
      logFile.close();
    }
  }
}

void logPrint(int val) {
  Serial.print(val);
  if (loggingEnabled) {
    logFile = SD.open(LOG_FILE_NAME, FILE_WRITE);
    if (logFile) {
      logFile.print(val);
      logFile.close();
    }
  }
}

void logPrint(float val, int decimals) {
  Serial.print(val, decimals);
  if (loggingEnabled) {
    logFile = SD.open(LOG_FILE_NAME, FILE_WRITE);
    if (logFile) {
      logFile.print(val, decimals);
      logFile.close();
    }
  }
}

void logPrintln(const char* str) {
  Serial.println(str);
  if (loggingEnabled) {
    logFile = SD.open(LOG_FILE_NAME, FILE_WRITE);
    if (logFile) {
      logFile.println(str);
      logFile.close();
    }
  }
}

void logPrintln(const String& str) {
  Serial.println(str);
  if (loggingEnabled) {
    logFile = SD.open(LOG_FILE_NAME, FILE_WRITE);
    if (logFile) {
      logFile.println(str);
      logFile.close();
    }
  }
}

void logPrintln(int val) {
  Serial.println(val);
  if (loggingEnabled) {
    logFile = SD.open(LOG_FILE_NAME, FILE_WRITE);
    if (logFile) {
      logFile.println(val);
      logFile.close();
    }
  }
}

void logPrintln() {
  Serial.println();
  if (loggingEnabled) {
    logFile = SD.open(LOG_FILE_NAME, FILE_WRITE);
    if (logFile) {
      logFile.println();
      logFile.close();
    }
  }
}

float getBatteryVoltage() {
  int raw = analogRead(BATT_PIN);
  float vPin = (raw * 3.3) / 1023.0;
  // Multiplier for 10k & 2k divider: (10000 + 2000) / 2000 = 6.0
  return vPin * 6.0; 
}