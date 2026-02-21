/*
 * Start Controller Hardware Functional Test
 * 
 * Test firmware validating pushbutton inputs, gate electromagnet/solenoid control,
 * and 74HC595 shift register-controlled light outputs via serial commands.
 * 
 * Serial Configuration: 9600 baud, Newline terminator
 * Arduino Board: Nano (ATmega328P microprocessor)
 * 
 * Available Commands:
 *   button_test, gate_reset_test, gate_left_test, gate_right_test,
 *   blue_left, blue_right, yellow_1, yellow_2, yellow_3, green,
 *   red_left, red_right, lights_all, end_test
 */

// ===== PIN DEFINITIONS =====

// Button inputs
#define PIN_START_BTN   A6      // Start button
#define PIN_MODE_BTN    A7      // Mode button
#define PIN_LEFT_BTN    18      // Left trigger button (digital)
#define PIN_RIGHT_BTN   19      // Right trigger button (digital)

// Gate electromagnet and solenoid outputs
#define PIN_GATE_LEFT   4       // Left gate electromagnet
#define PIN_GATE_RIGHT  7       // Right gate electromagnet
#define PIN_SOLENOID    6       // Return solenoid

// 74HC595 Shift Register control (lights)
#define PIN_SR_DATA     2       // Shift register serial data input
#define PIN_SR_CLOCK    3       // Shift register clock
#define PIN_SR_LATCH    5       // Shift register latch (storage register)

// ===== LIGHT BIT PATTERNS =====
// Shift register bit assignments for light control (Q0-Q7)

#define LIGHT_OFF     0x00  // All lights off
#define LIGHT_BL      0x02  // Blue Left (Q1)
#define LIGHT_BR      0x01  // Blue Right (Q0)
#define LIGHT_Y3      0x04  // Yellow 3 - Top (Q2)
#define LIGHT_Y2      0x08  // Yellow 2 - Middle (Q3)
#define LIGHT_Y1      0x10  // Yellow 1 - Bottom (Q4)
#define LIGHT_GREEN   0x20  // Green / GO (Q5)
#define LIGHT_RED_L   0x40  // Red Left (Q6)
#define LIGHT_RED_R   0x80  // Red Right (Q7)

// ===== TIMING CONSTANTS =====

#define SOLENOID_DURATION   3000    // Solenoid energization time (milliseconds)
#define GATE_DURATION       10000   // Gate electromagnet energization time (ms)
#define LIGHT_DURATION      5000    // Individual light test duration (ms)
#define DEBOUNCE_DELAY      20      // Button debounce delay (ms)

bool buttonTestMode = false;

struct ButtonState {
  bool current;
  bool previous;
};

ButtonState btnStart = {false, false};
ButtonState btnMode = {false, false};
ButtonState btnLeft = {false, false};
ButtonState btnRight = {false, false};

void setup() {
  Serial.begin(9600);
  delay(100);
  
  Serial.println("\nStart Controller Test - Ready");
  Serial.println("Type command (e.g., button_test, gate_reset_test, blue_left)");
  Serial.println();
  
  // Configure inputs
  pinMode(PIN_START_BTN, INPUT_PULLUP);
  pinMode(PIN_MODE_BTN, INPUT_PULLUP);
  pinMode(PIN_LEFT_BTN, INPUT_PULLUP);
  pinMode(PIN_RIGHT_BTN, INPUT_PULLUP);
  
  // Configure outputs
  pinMode(PIN_GATE_LEFT, OUTPUT);
  pinMode(PIN_GATE_RIGHT, OUTPUT);
  pinMode(PIN_SOLENOID, OUTPUT);
  digitalWrite(PIN_GATE_LEFT, LOW);
  digitalWrite(PIN_GATE_RIGHT, LOW);
  digitalWrite(PIN_SOLENOID, LOW);
  
  // Shift register pins
  pinMode(PIN_SR_DATA, OUTPUT);
  pinMode(PIN_SR_CLOCK, OUTPUT);
  pinMode(PIN_SR_LATCH, OUTPUT);
  
  allLightsOff();
}

void loop() {
  updateButtons();
  
  if (buttonTestMode) {
    printButtonStates();
  }
  
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toLowerCase();
    executeCommand(cmd);
  }
  
  delay(10);
}

// Simple debouncing
void updateButtons() {
  bool rawStart = (digitalRead(PIN_START_BTN) == LOW);
  bool rawMode = (digitalRead(PIN_MODE_BTN) == LOW);
  bool rawLeft = (digitalRead(PIN_LEFT_BTN) == LOW);
  bool rawRight = (digitalRead(PIN_RIGHT_BTN) == LOW);
  
  delay(20);  // Debounce wait
  
  if ((digitalRead(PIN_START_BTN) == LOW) == rawStart) {
    btnStart.current = rawStart;
  }
  if ((digitalRead(PIN_MODE_BTN) == LOW) == rawMode) {
    btnMode.current = rawMode;
  }
  if ((digitalRead(PIN_LEFT_BTN) == LOW) == rawLeft) {
    btnLeft.current = rawLeft;
  }
  if ((digitalRead(PIN_RIGHT_BTN) == LOW) == rawRight) {
    btnRight.current = rawRight;
  }
}

void printButtonStates() {
  Serial.print("Start: ");
  Serial.print(btnStart.current ? "HIGH" : "LOW");
  Serial.print("  Mode: ");
  Serial.print(btnMode.current ? "HIGH" : "LOW");
  Serial.print("  Right: ");
  Serial.print(btnRight.current ? "HIGH" : "LOW");
  Serial.print("  Left: ");
  Serial.println(btnLeft.current ? "HIGH" : "LOW");
  delay(100);
}

// Shift register control
void shiftRegisterUpdate(byte pattern) {
  digitalWrite(PIN_SR_LATCH, LOW);
  
  for (int i = 7; i >= 0; i--) {
    byte bit = (pattern >> i) & 0x01;
    digitalWrite(PIN_SR_DATA, bit);
    digitalWrite(PIN_SR_CLOCK, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_SR_CLOCK, LOW);
    delayMicroseconds(10);
  }
  
  digitalWrite(PIN_SR_LATCH, HIGH);
  delayMicroseconds(20);
  digitalWrite(PIN_SR_LATCH, LOW);
}

void allLightsOff() {
  shiftRegisterUpdate(LIGHT_OFF);
}

// Gate control
void energizeGateLeft(unsigned long duration) {
  Serial.println(">> Left gate ON");
  digitalWrite(PIN_GATE_LEFT, HIGH);
  delay(duration);
  digitalWrite(PIN_GATE_LEFT, LOW);
  Serial.println("<< Left gate OFF");
}

void energizeGateRight(unsigned long duration) {
  Serial.println(">> Right gate ON");
  digitalWrite(PIN_GATE_RIGHT, HIGH);
  delay(duration);
  digitalWrite(PIN_GATE_RIGHT, LOW);
  Serial.println("<< Right gate OFF");
}

void energizeSolenoid(unsigned long duration) {
  Serial.println(">> Solenoid ON");
  digitalWrite(PIN_SOLENOID, HIGH);
  delay(duration);
  digitalWrite(PIN_SOLENOID, LOW);
  Serial.println("<< Solenoid OFF");
}

// Light tests
void lightTest(byte pattern, const char* name) {
  Serial.print("Light: ");
  Serial.println(name);
  shiftRegisterUpdate(pattern);
  delay(LIGHT_TIME);
  allLightsOff();
  Serial.println("Done");
}

// Command handler
void executeCommand(String cmd) {
  if (cmd == "button_test") {
    buttonTestMode = true;
    Serial.println("Button test mode - press buttons or type 'end_test' to exit");
    
  } else if (cmd == "end_test") {
    buttonTestMode = false;
    Serial.println("Test mode exited");
    
  } else if (cmd == "gate_reset_test") {
    energizeSolenoid(SOLENOID_TIME);
    
  } else if (cmd == "gate_left_test") {
    energizeGateLeft(GATE_TIME);
    
  } else if (cmd == "gate_right_test") {
    energizeGateRight(GATE_TIME);
    
  } else if (cmd == "blue_left") {
    lightTest(LIGHT_BL, "Blue Left");
    
  } else if (cmd == "blue_right") {
    lightTest(LIGHT_BR, "Blue Right");
    
  } else if (cmd == "yellow_3") {
    lightTest(LIGHT_Y3, "Yellow 3");
    
  } else if (cmd == "yellow_2") {
    lightTest(LIGHT_Y2, "Yellow 2");
    
  } else if (cmd == "yellow_1") {
    lightTest(LIGHT_Y1, "Yellow 1");
    
  } else if (cmd == "green") {
    lightTest(LIGHT_GO, "Green");
    
  } else if (cmd == "red_left") {
    lightTest((LIGHT_FL | LIGHT_GO), "Red Left");
    
  } else if (cmd == "red_right") {
    lightTest((LIGHT_GO | LIGHT_FR), "Red Right");
    
  } else if (cmd == "lights_all") {
    Serial.println("Light: Countdown");
    shiftRegisterUpdate(LIGHT_BL | LIGHT_BR | LIGHT_Y3 | LIGHT_Y2 | LIGHT_Y1 | LIGHT_GO);
    delay(2000);
    Serial.println("Light: Fault");
    shiftRegisterUpdate(LIGHT_BL | LIGHT_BR | LIGHT_Y3 | LIGHT_Y2 | LIGHT_Y1 | LIGHT_FL | LIGHT_FR);
    delay(2000);
    allLightsOff();
    Serial.println("Done");
    
  } else if (cmd.length() > 0) {
    Serial.print("Unknown: ");
    Serial.println(cmd);
  }
}
