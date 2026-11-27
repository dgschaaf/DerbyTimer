/*
 * Finish Controller Hardware Functional Test
 * 
 * Test firmware validating optical sensor inputs, 7-segment display multiplexing,
 * and serial communication via 74HCT238 decoder and 74HCT244 segment drivers.
 * 
 * Serial Configuration: 9600 baud, Newline terminator
 * Arduino Board: Nano 33 BLE (nRF52840 microprocessor)
 * 
 * Available Commands:
 *   sensor_test, display1_test, display2_test, combined_test, end_test
 */

// ===== PIN DEFINITIONS =====
// Adjust pins based on your wiring (example assignments for Arduino Nano 33 BLE)

// Sensor input pins (digital, active HIGH from Schmitt trigger U3)
const int LANE1_SENSOR = A0;      // Analog pin A0, configured as digital input
const int LANE2_SENSOR = A1;      // Analog pin A1, configured as digital input

// 74HCT238 3-to-8 Decoder control pins (for digit selection)
const int DECODER_A0 = 2;         // Control line A0 (LSB)
const int DECODER_A1 = 3;         // Control line A1
const int DECODER_A2 = 4;         // Control line A2 (MSB)

// 7-segment display data lines (a, b, c, d, e, f, g, DP)
// Each pin drives one segment across both displays via 74HCT244 buffer
const int SEG_A  = 5;
const int SEG_B  = 6;
const int SEG_C  = 7;
const int SEG_D  = 8;
const int SEG_E  = 9;
const int SEG_F  = 10;
const int SEG_G  = 11;
const int SEG_DP = 12;            // Decimal point

// ===== GLOBAL VARIABLES =====
bool testActive = false;
bool sensorTestMode = false;
bool displayTestMode = false;
unsigned long lastSensorPrint = 0;
const unsigned long SENSOR_PRINT_INTERVAL = 500;  // Print sensor state every 500ms

// Segment patterns for digits 0-9 (standard 7-segment layout)
// Bits: PGFEDCBA (g is MSB, a is LSB)
const byte digitPatterns[10] = {
  0b00111111,  // 0: a,b,c,d,e,f (no g)
  0b00000110,  // 1: b,c
  0b01011011,  // 2: a,b,d,e,g
  0b01001111,  // 3: a,b,c,d,g
  0b01100110,  // 4: b,c,f,g
  0b01101101,  // 5: a,c,d,f,g
  0b01111101,  // 6: a,c,d,e,f,g
  0b00000111,  // 7: a,b,c
  0b01111111,  // 8: all segments
  0b01101111   // 9: a,b,c,d,f,g
};

// ===== SETUP =====
void setup() {
  // Initialize serial communication
  Serial.begin(9600);
  delay(1000);  // Allow serial to stabilize
  
  // Configure input/output pins
  pinMode(LANE1_SENSOR, INPUT);
  pinMode(LANE2_SENSOR, INPUT);
  
  pinMode(DECODER_A0, OUTPUT);
  pinMode(DECODER_A1, OUTPUT);
  pinMode(DECODER_A2, OUTPUT);
  
  pinMode(SEG_A, OUTPUT);
  pinMode(SEG_B, OUTPUT);
  pinMode(SEG_C, OUTPUT);
  pinMode(SEG_D, OUTPUT);
  pinMode(SEG_E, OUTPUT);
  pinMode(SEG_F, OUTPUT);
  pinMode(SEG_G, OUTPUT);
  pinMode(SEG_DP, OUTPUT);
  
  // Clear all outputs
  writeSegments(0x00);
  selectDigit(0);  // Default to digit 0
  
  // Print startup message
  Serial.println("\n==========================================");
  Serial.println("Finish Controller Functional Test Starting...");
  Serial.println("==========================================");
  Serial.println("Serial communication established at 9600 baud");
  Serial.println("\nAvailable commands:");
  Serial.println("  sensor_test    - Monitor sensor inputs continuously");
  Serial.println("  display1_test  - Test Lane 1 (left) display");
  Serial.println("  display2_test  - Test Lane 2 (right) display");
  Serial.println("  combined_test  - Run sensors and displays together");
  Serial.println("  end_test       - Exit test mode");
  Serial.println("==========================================");
  Serial.println("Ready for commands. Enter test command:");
}

// ===== MAIN LOOP =====
void loop() {
  // Handle serial input
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();  // Remove whitespace
    command.toLowerCase();  // Normalize to lowercase
    
    if (command == "sensor_test") {
      startSensorTest();
    } 
    else if (command == "display1_test") {
      testDisplay1();
    } 
    else if (command == "display2_test") {
      testDisplay2();
    } 
    else if (command == "combined_test") {
      startCombinedTest();
    } 
    else if (command == "end_test") {
      stopTest();
    } 
    else if (command.length() > 0) {
      Serial.println("Unknown command. Type: sensor_test, display1_test, display2_test, combined_test, or end_test");
    }
  }
  
  // Handle sensor test mode: print states periodically
  if (sensorTestMode) {
    unsigned long now = millis();
    if (now - lastSensorPrint >= SENSOR_PRINT_INTERVAL) {
      int lane1 = digitalRead(LANE1_SENSOR);
      int lane2 = digitalRead(LANE2_SENSOR);
      
      Serial.print("Sensor readings: Lane 1=");
      Serial.print(lane1 ? "HIGH" : "LOW");
      Serial.print(" Lane 2=");
      Serial.println(lane2 ? "HIGH" : "LOW");
      
      lastSensorPrint = now;
    }
  }
}

// ===== TEST FUNCTIONS =====

/**
 * Start continuous sensor monitoring
 * Prints HIGH/LOW state every 500ms for each lane
 */
void startSensorTest() {
  Serial.println("\n--- SENSOR TEST STARTED ---");
  Serial.println("Blocking a sensor changes its output from HIGH to LOW");
  Serial.println("Type 'end_test' to stop\n");
  
  sensorTestMode = true;
  testActive = true;
  displayTestMode = false;
  lastSensorPrint = 0;
}

/**
 * Test display 1 (Lane 1 / Left display)
 * Cycles through digit patterns 0-9 on all 5 positions
 */
void testDisplay1() {
  Serial.println("\n--- LANE 1 DISPLAY TEST STARTED ---");
  Serial.println("Testing left (Lane 1) display: all 5 digit positions");
  Serial.println("Each digit will cycle through 0-9 pattern\n");
  
  displayTestMode = true;
  testActive = true;
  sensorTestMode = false;
  
  // Test all 5 digit positions (0-4 for Lane 1)
  for (int position = 0; position < 5; position++) {
    Serial.print("Testing digit position ");
    Serial.println(position);
    
    // Display each digit pattern 0-9
    for (int digit = 0; digit < 10; digit++) {
      selectDigit(position);
      writeSegments(digitPatterns[digit]);
      Serial.print("  Pattern ");
      Serial.println(digit);
      delay(300);  // 300ms per pattern to allow visual observation
    }
  }
  
  // Test decimal point
  Serial.println("\nTesting decimal point:");
  selectDigit(0);
  for (int i = 0; i < 6; i++) {
    writeSegments(0x00);
    digitalWrite(SEG_DP, HIGH);  // DP on
    Serial.println("  Decimal point ON");
    delay(300);
    
    digitalWrite(SEG_DP, LOW);   // DP off
    Serial.println("  Decimal point OFF");
    delay(300);
  }
  
  // Turn off all segments
  writeSegments(0x00);
  digitalWrite(SEG_DP, LOW);
  
  Serial.println("\n--- LANE 1 DISPLAY TEST COMPLETE ---\n");
  displayTestMode = false;
  testActive = false;
}

/**
 * Test display 2 (Lane 2 / Right display)
 * Same pattern as display 1 test
 */
void testDisplay2() {
  Serial.println("\n--- LANE 2 DISPLAY TEST STARTED ---");
  Serial.println("Testing right (Lane 2) display: all 5 digit positions");
  Serial.println("Each digit will cycle through 0-9 pattern\n");
  
  displayTestMode = true;
  testActive = true;
  sensorTestMode = false;
  
  // Test all 5 digit positions (5-9 for Lane 2)
  for (int position = 5; position < 10; position++) {
    Serial.print("Testing digit position ");
    Serial.println(position);
    
    // Display each digit pattern 0-9
    for (int digit = 0; digit < 10; digit++) {
      selectDigit(position);
      writeSegments(digitPatterns[digit]);
      Serial.print("  Pattern ");
      Serial.println(digit);
      delay(300);
    }
  }
  
  // Test decimal point
  Serial.println("\nTesting decimal point:");
  selectDigit(5);
  for (int i = 0; i < 6; i++) {
    writeSegments(0x00);
    digitalWrite(SEG_DP, HIGH);
    Serial.println("  Decimal point ON");
    delay(300);
    
    digitalWrite(SEG_DP, LOW);
    Serial.println("  Decimal point OFF");
    delay(300);
  }
  
  // Turn off all segments
  writeSegments(0x00);
  digitalWrite(SEG_DP, LOW);
  
  Serial.println("\n--- LANE 2 DISPLAY TEST COMPLETE ---\n");
  displayTestMode = false;
  testActive = false;
}

/**
 * Combined test: displays a simple timing demo with sensor interaction
 * Useful for verifying displays and sensors work together
 */
void startCombinedTest() {
  Serial.println("\n--- COMBINED SENSOR & DISPLAY TEST STARTED ---");
  Serial.println("Sensors and displays running simultaneously");
  Serial.println("Press sensors to trigger display changes (simulated timing)");
  Serial.println("Type 'end_test' to stop\n");
  
  sensorTestMode = true;
  displayTestMode = true;
  testActive = true;
  lastSensorPrint = 0;
  
  unsigned long testStart = millis();
  unsigned long testDuration = 60000;  // 60 second test
  
  while (testActive && (millis() - testStart) < testDuration) {
    // Read sensors
    int lane1 = digitalRead(LANE1_SENSOR);
    int lane2 = digitalRead(LANE2_SENSOR);
    
    // Print sensor status periodically (every 500ms)
    unsigned long now = millis();
    if (now - lastSensorPrint >= SENSOR_PRINT_INTERVAL) {
      Serial.print("Sensor readings: Lane 1=");
      Serial.print(lane1 ? "HIGH" : "LOW");
      Serial.print(" | Lane 2=");
      Serial.println(lane2 ? "HIGH" : "LOW");
      lastSensorPrint = now;
    }
    
    // Update display based on sensor state
    // Lane 1 sensor LOW -> display digit 3 on position 0
    // Lane 2 sensor LOW -> display digit 5 on position 5
    if (lane1 == LOW) {
      selectDigit(0);
      writeSegments(digitPatterns[3]);
    } else {
      selectDigit(0);
      writeSegments(digitPatterns[0]);
    }
    
    if (lane2 == LOW) {
      selectDigit(5);
      writeSegments(digitPatterns[5]);
    } else {
      selectDigit(5);
      writeSegments(digitPatterns[0]);
    }
    
    // Check for user input to end test early
    if (Serial.available() > 0) {
      String input = Serial.readStringUntil('\n');
      input.trim();
      input.toLowerCase();
      
      if (input == "end_test") {
        testActive = false;
        break;
      }
    }
    
    delay(50);  // Small delay to prevent hogging CPU
  }
  
  // Clean up
  writeSegments(0x00);
  digitalWrite(SEG_DP, LOW);
  Serial.println("\n--- COMBINED TEST COMPLETE ---");
  Serial.println("Test duration exceeded or stopped by user\n");
  
  sensorTestMode = false;
  displayTestMode = false;
  testActive = false;
}

/**
 * Stop current test and return to command prompt
 */
void stopTest() {
  testActive = false;
  sensorTestMode = false;
  displayTestMode = false;
  
  // Clear display
  writeSegments(0x00);
  digitalWrite(SEG_DP, LOW);
  
  Serial.println("\n--- TEST STOPPED ---");
  Serial.println("Ready for commands. Enter test command:");
}

// ===== HARDWARE CONTROL FUNCTIONS =====

/**
 * Select a digit position using 74HCT238 3-to-8 decoder
 * Positions 0-4: Lane 1 display
 * Positions 5-9: Lane 2 display
 */
void selectDigit(int position) {
  // Extract bits from position (position is 0-9, but decoder is 0-7)
  // In a real system, you might have additional control logic
  int digit = position % 8;  // Wrap to 0-7 for decoder
  
  // Set address lines A0, A1, A2
  digitalWrite(DECODER_A0, (digit >> 0) & 1);  // A0 = bit 0
  digitalWrite(DECODER_A1, (digit >> 1) & 1);  // A1 = bit 1
  digitalWrite(DECODER_A2, (digit >> 2) & 1);  // A2 = bit 2
}

/**
 * Write segment data to active digit
 * Bit layout (MSB to LSB): DP, G, F, E, D, C, B, A
 * Each bit represents one segment
 */
void writeSegments(byte segmentPattern) {
  // Extract individual bits and write to pins
  digitalWrite(SEG_A, (segmentPattern >> 0) & 1);
  digitalWrite(SEG_B, (segmentPattern >> 1) & 1);
  digitalWrite(SEG_C, (segmentPattern >> 2) & 1);
  digitalWrite(SEG_D, (segmentPattern >> 3) & 1);
  digitalWrite(SEG_E, (segmentPattern >> 4) & 1);
  digitalWrite(SEG_F, (segmentPattern >> 5) & 1);
  digitalWrite(SEG_G, (segmentPattern >> 6) & 1);
  // Note: SEG_DP handled separately for flexibility
}

// ===== END OF SKETCH =====
