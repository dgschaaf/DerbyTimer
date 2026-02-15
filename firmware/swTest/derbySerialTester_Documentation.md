# DerbyTimer UART Protocol Test Document

## 1. Test Strategy Overview
### Overview and Background
The Derby Timer project uses a shared library which serves as the serial communication protocol between the two Arduino nodes. This testing serves as integration and protocol checkout.  

The current design uses one Arduino Nano ATmega328 with a custom shield called "startController."  The other node is an Arduino Nano 33 BLE nRF52840 with a custom shield called "finishController."  
### Approach
The test involves one node connected to a terminal connection in the Arduino IDE on a PC.  Because of the 5V logic level, the startController is chosen.  The user may select the desired test through the terminal and view results.  

The tests generally consist of a message being sent to a responder and listening for an ACK.  For messages with payloads, the responder will echo the received payload for confirmation of transmission.  The user has an option of executing all messages sequentially or each message individually.  The script keeps statistics of test results.

The user has the option of running advanced tests such as timing stress, error handling, etc.

## 2. Physcial Configuration
### Wiring Configuration
```
┌─────────────────────┐     ┌─────────────────────┐     ┌─────────────────────┐
│   PC Terminal       │     │   startController   │     │   finishController  │
│   (Arduino IDE)     │     │   (Arduino Nano)    │     │(Arduino Nano 33 BLE)│
│                     │     │                     │     │                     │
│       USB+ ─────────┼─────┼──→ Pin 5 (RX SW Ser)│     |                     │
│       USB- ←────────┼─────┼─── Pin 6 (TX SW Ser)│     |                     |
│       GND ──────────┼─────┼─── GND ─────────────│─────|─── GND              |
│       USB +5V (N/C) │     │    Pin 0 (RX) ←─────|─────|─── Pin 1 (TX)       │
│  USB ←→ PC Monitor  │     │    Pin 1 (TX) ──────│─────|──→ Pin 0 (RX)       |
│                     │     │                     │     |                     |
└─────────────────────┘     └─────────────────────┘     └─────────────────────┘
```
### Important Notes
1. **Level Compatibility**: Both Nano AVR (5V logic) and Nano 33 BLE (3.3V logic) are involved. The finishController PCB contains a logic level shifter for the hardware serial.  Nano 33 BLE pins are **NOT 5V tolerant**.
2. **Common Ground**: Always connect GND between devices.
3. **USB Power**: Do not connect +5V from the PC to the startController
4. **startController PC Connection**: The PC Terminal is connected via a USB TTL converter attached directly to the startController pins.  Be cautious connecting to avoid shorts and ESD.

## 3. Test Files Description
### File 1: derbySerialTester.ino
**Purpose**:	Main test controller that exercises the test
**Features**:
	- Tests all 10+ message types
	- Validates ACK/NACK responses
	- Tracks pass/fail statistics
	- Includes advanced testing
	- Communicates with responder
	- Communicates with user on PC Terminal
**Upload to**:	startController as main file

### File 2: derbySerialResponder.ino
**Purpose**:	Simple responder that ACKs all valid messages
**Features**: 
	- Receives messages and provides ACK
	- Echos back payload for verification
**Upload to**: finishController as main file

### File 3: serialComm.cpp
**Purpose**:	Software communication module under test
**Features**:
	- Defines communication functions
**Upload to**:	startController as supporting file

### File 4: serialComm.h
**Purpose**:	Header for software communication module under test
**Features**:
	- Provides external variables
	- Declares communication functions
**Upload to**:	startController as supporting file

## 4. Test Execution Instructions
### Prerequisites
1. Arduinio IDE installed
2. startController and finishController with respective Arduinos
3. USB to TTL converter
4. Twisted serial cable

### Phase 1: Setup
1. Upload Responder firmware finishController
2. Upload Tester firmware to startController
3. Wire connections between boards
4. Wire connections to PC
5. Open Arduino IDE Serial Monitor

### Phase 2: Basic Tests
1. Verify basic communicaiton by running 'r' to reset responder
2. Run desired test, either all (a) or by selecting a number
3. Check results with 'Pass' or 'Fail'

### Phase 3: Reviewing Results
1. Print results with 'p'
2. Review results.  Check for failed tests, timeouts, slow responses.
3. Clear stats with 'c'

### Phase 4: Advanced Tests
1. Timing stress test - TBD
2. Error handling test - TBD
3. Sequence test (full race) - TBD

## 5. Test Coverage Details
TBD

## 6. Interpreting Results
### Common Issues and Solutions

| Symptom | Likely Cause | Solution |
|---------|--------------|----------|
| All tests timeout | Wiring reversed | Swap TX/RX connections |
| All tests timeout | Baud mismatch | Verify both at 115200 |
| Intermittent timeouts | Noise/interference | Add 100Ω series resistors |
| Slow response times | DUT loop blocking | Check DUT `loop()` for delays |
| NACK instead of ACK | Message parsing error | Check payload lengths |
| Wrong ACK ID | State machine issue | Review `resetTxState()` calls |

### Response Time Guidelines

| Category | Typical | Acceptable | Concerning |
|----------|---------|------------|------------|
| ACK response | 100-500 us | <1 ms | >5 ms |
| State transition | 1-5 ms | <10 ms | >50 ms |
| Race start to ACK | <1 ms | <2 ms | >5 ms |

## 7. Protocol Issues Found During Code Review
TBD

## 8. Additional Test Scenarios
TBD

## 9. Files Summary
TBD

## 10. Quick Reference Commands
```
a - Run all tests
1-9 - Individual message tests
t - Timing stress test
e - Error handling test
s - Full race sequence
r - Reset DUT
p - Print stats
c - Clear stats
h - Help
```