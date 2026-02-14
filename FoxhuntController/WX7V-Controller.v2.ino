/*
 * PROJECT: Teensy 4.1 Foxhunt Controller (Ammo Can Build)
 * CONFIG: 4S LiPo, Dual Buck (12V/5V), 1W Transmit, 10k/2k Divider
 * CLOCK: Set to 24MHz in Tools > CPU Speed
 */

// --- PIN DEFINITIONS ---
const int PTT_PIN   = 2;     // 2N2222 Transistor Base
const int AUDIO_PIN = 12;    // MQS Audio Output
const int BATT_PIN  = A9;    // Voltage Divider Junction (Pin 23)

// --- CONFIGURATION ---
const char* foxCallsign = "K6XXX FOX"; // <--- CHANGE TO YOUR CALLSIGN
const int dotLen   = 100;    // Speed (~12 WPM)
const int toneFreq = 800;    // Standard foxhunt pitch (Hz)
const float LOW_BATT_CUTOFF = 13.2; // 3.3V per cell for 4S LiPo

// --- VOLTAGE DIVIDER MATH ---
const float R1 = 10000.0; // 10k
const float R2 = 2000.0;  // 2k
const float vRef = 3.3;   // Teensy 3.3V Logic

void setup() {
  pinMode(PTT_PIN, OUTPUT);
  digitalWrite(PTT_PIN, LOW); // Start with Transmitter OFF
  
  Serial.begin(9600);
  // No special setup for MQS Pin 12; tone() handles it.
}

float getBatteryVoltage() {
  int raw = analogRead(BATT_PIN);
  float vPin = (raw * vRef) / 1023.0;
  // Multiplier for 10k/2k is 6.0
  return vPin * ((R1 + R2) / R2);
}

void playTone(int duration) {
  tone(AUDIO_PIN, toneFreq);
  delay(duration);
  noTone(AUDIO_PIN);
  delay(dotLen); // Space between dits and dahs
}

void dot()  { playTone(dotLen); }
void dash() { playTone(dotLen * 3); }

void sendMorse(const char* str) {
  for (int i = 0; str[i] != '\0'; i++) {
    char c = toupper(str[i]);
    if (c == ' ') { delay(dotLen * 4); continue; }
    
    // Morse Lookup (Common Characters)
    switch (c) {
      case 'A': dot(); dash(); break;
      case 'B': dash(); dot(); dot(); dot(); break;
      case 'C': dash(); dot(); dash(); dot(); break;
      case 'D': dash(); dot(); dot(); break;
      case 'E': dot(); break;
      case 'F': dot(); dot(); dash(); dot(); break;
      case 'G': dash(); dash(); dot(); break;
      case 'H': dot(); dot(); dot(); dot(); break;
      case 'I': dot(); dot(); break;
      case 'J': dot(); dash(); dash(); dash(); break;
      case 'K': dash(); dot(); dash(); break;
      case 'L': dot(); dash(); dot(); dot(); break;
      case 'M': dash(); dash(); break;
      case 'N': dash(); dot(); break;
      case 'O': dash(); dash(); dash(); break;
      case 'P': dot(); dash(); dash(); dot(); break;
      case 'Q': dash(); dash(); dot(); dash(); break;
      case 'R': dot(); dash(); dot(); break;
      case 'S': dot(); dot(); dot(); break;
      case 'T': dash(); break;
      case 'U': dot(); dot(); dash(); break;
      case 'V': dot(); dot(); dot(); dash(); break;
      case 'W': dot(); dash(); dash(); break;
      case 'X': dash(); dot(); dot(); dash(); break;
      case 'Y': dash(); dot(); dash(); dash(); break;
      case 'Z': dash(); dash(); dot(); dot(); break;
      case '1': dot(); dash(); dash(); dash(); dash(); break;
      case '2': dot(); dot(); dash(); dash(); dash(); break;
      case '3': dot(); dot(); dot(); dash(); dash(); break;
      case '4': dot(); dot(); dot(); dot(); dash(); break;
      case '5': dot(); dot(); dot(); dot(); dot(); break;
      case '6': dash(); dot(); dot(); dot(); dot(); break;
      case '7': dash(); dash(); dot(); dot(); dot(); break;
      case '8': dash(); dash(); dash(); dot(); dot(); break;
      case '9': dash(); dash(); dash(); dash(); dot(); break;
      case '0': dash(); dash(); dash(); dash(); dash(); break;
      case '/': dash(); dot(); dot(); dash(); dot(); break;
    }
    delay(dotLen * 2); // Space between letters
  }
}

void loop() {
  float v = getBatteryVoltage();
  
  // 1. SAFETY: Check for Low Battery
  if (v < LOW_BATT_CUTOFF && v > 5.0) { // Check v > 5 to avoid false trigger if unplugged
    digitalWrite(PTT_PIN, LOW);
    Serial.printf("CRITICAL: Battery %.2fV. Stopping TX.\n", v);
    while(1) { delay(1000); } // Stop everything to save battery
  }

  // 2. TRANSMIT CYCLE
  Serial.printf("Battery: %.2fV. Transmitting...\n", v);
  
  digitalWrite(PTT_PIN, HIGH); 
  delay(1000); // Radio wake-up time
  
  sendMorse(foxCallsign);
  
  delay(1000); // Trailing silence
  digitalWrite(PTT_PIN, LOW);

  // 3. STANDBY (150 seconds)
  Serial.println("Standby for 150s.");
  delay(150000); 
}

