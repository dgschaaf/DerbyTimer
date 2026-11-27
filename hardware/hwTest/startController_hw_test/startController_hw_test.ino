/*
 * Start Controller light-path and button test (standalone sketch).
 * Proves the SC Nano runs, the 74HC595 (U4) drives each lamp switch, the
 * U3/U1 "GO and not red" logic works, and the four buttons reach the Nano.
 * Uses no race code, no FC, and no serial link. Procedure:
 * startController_hw_test.md in this folder.
 *
 * USB serial: 115200 baud. Unplug the comm cable at J5 first - D0/D1 are
 * shared between USB and the link.
 *
 * Commands (type one character, then Enter):
 *   p  pause / resume the automatic step sequence (pause holds the outputs
 *      steady so you can probe them with a meter)
 *   n  next step       b  previous step       h  help
 */

#include <avr/pgmspace.h>

// ---- Pins: same as the race firmware (lights.cpp, gates.cpp, buttons.cpp) ----
static const uint8_t kPinData   = 2;   // U4 pin 14 (DS)
static const uint8_t kPinLatch  = 3;   // U4 pin 12 (STCP)
static const uint8_t kPinClock  = 4;   // U4 pin 11 (SHCP)
static const uint8_t kPinGateR  = 14;  // A0 - held LOW, as the race firmware does at boot
static const uint8_t kPinGateL  = 15;  // A1
static const uint8_t kPinReturn = 16;  // A2
static const uint8_t kPinLeft   = 18;  // digital, external pull-up
static const uint8_t kPinRight  = 19;  // digital, external pull-up
static const uint8_t kPinStart  = A7;  // analog-only
static const uint8_t kPinMode   = A6;  // analog-only

static const int           kAnalogThreshold = 512;   // same rule as buttons.cpp: below = pressed
static const unsigned long kStepMs          = 1500;  // auto-step dwell per pattern
static const unsigned long kHeartbeatMs     = 500;   // "L" LED toggle period
static const unsigned long kButtonPollMs    = 20;

// ---- Step table ----
// One row per pattern written to U4. "Qn" is the 74HC595 output bit; the
// U4 pin is where to probe it. Firmware names (lights.h) and PCB net names
// differ for the yellows: firmware Y3 is PCB net "Yellow 1" and firmware Y1
// is PCB net "Yellow 3". Note which physical lamp lights to settle it.
struct Step {
	uint8_t pattern;
	char    label[80];
};

static const Step kSteps[] PROGMEM = {
	{ 0x00, "ALL OFF                  expect: every lamp dark, U4 pin 5 = 0 V" },
	{ 0x01, "Q0 Blue R  (U4 pin 15)   expect: no lamp fitted" },
	{ 0x02, "Q1 Blue L  (U4 pin 1)    expect: no lamp fitted" },
	{ 0x04, "Q2 fw Y3 / net Yellow 1 (U4 pin 2)  expect: one yellow, both trees" },
	{ 0x08, "Q3 Y2      (U4 pin 3)    expect: one yellow, both trees" },
	{ 0x10, "Q4 fw Y1 / net Yellow 3 (U4 pin 4)  expect: one yellow, both trees" },
	{ 0x20, "Q5 GO      (U4 pin 5)    expect: GO lamp on BOTH trees" },
	{ 0x40, "Q6 Red L   (U4 pin 6)    expect: red, left tree only" },
	{ 0x80, "Q7 Red R   (U4 pin 7)    expect: red, right tree only" },
	{ 0x60, "GO + Red L               expect: left GO OFF, right GO ON, left red ON" },
	{ 0xA0, "GO + Red R               expect: right GO OFF, left GO ON, right red ON" },
	{ 0xFF, "ALL ON                   expect: yellows + reds ON, both GO OFF (red wins)" },
};
static const uint8_t kStepCount = sizeof(kSteps) / sizeof(kSteps[0]);

// ---- Run state: owned by loop(); reset only by a reboot ----
static uint8_t       stepIndex   = 0;
static bool          paused      = false;
static unsigned long stepStart   = 0;
static unsigned long beatStart   = 0;
static bool          beatOn      = false;
static unsigned long buttonPoll  = 0;
static bool          lastPressed[4] = { false, false, false, false };  // Start, Mode, Left, Right

// Same shift sequence as updateLights() in lights.cpp; latch is left HIGH,
// so a steady ~5 V on U4 pin 12 shows this code has run.
static void writeLights(uint8_t pattern) {
	digitalWrite(kPinLatch, LOW);
	shiftOut(kPinData, kPinClock, MSBFIRST, pattern);
	digitalWrite(kPinLatch, HIGH);
}

static void printStep() {
	Step s;
	memcpy_P(&s, &kSteps[stepIndex], sizeof(Step));
	Serial.print(F("[t="));
	Serial.print(millis() / 1000);
	Serial.print(F("s] step "));
	Serial.print(stepIndex);
	Serial.print(F(" 0x"));
	if (s.pattern < 0x10) Serial.print('0');
	Serial.print(s.pattern, HEX);
	Serial.print(F("  "));
	Serial.println(s.label);
}

static void applyStep() {
	Step s;
	memcpy_P(&s, &kSteps[stepIndex], sizeof(Step));
	writeLights(s.pattern);
	printStep();
	stepStart = millis();
}

static void printHelp() {
	Serial.println(F("Commands: p = pause/resume, n = next, b = back, h = help"));
}

// Read all four buttons with the race firmware's rules; print on any change.
static void pollButtons() {
	int  rawStart = analogRead(kPinStart);
	int  rawMode  = analogRead(kPinMode);
	bool now[4] = {
		rawStart < kAnalogThreshold,
		rawMode  < kAnalogThreshold,
		digitalRead(kPinLeft)  == LOW,
		digitalRead(kPinRight) == LOW,
	};
	static const char* const kNames[4] = { "Start (A7)", "Mode (A6)", "Left (D18)", "Right (D19)" };
	for (uint8_t i = 0; i < 4; i++) {
		if (now[i] == lastPressed[i]) continue;
		lastPressed[i] = now[i];
		Serial.print(F("BUTTON "));
		Serial.print(kNames[i]);
		Serial.print(now[i] ? F(" PRESSED") : F(" released"));
		if (i == 0) { Serial.print(F("  raw=")); Serial.print(rawStart); }
		if (i == 1) { Serial.print(F("  raw=")); Serial.print(rawMode); }
		Serial.println();
	}
}

static void handleCommand(char c) {
	switch (c) {
		case 'p':
			paused = !paused;
			Serial.println(paused ? F("PAUSED - outputs held") : F("RUNNING"));
			if (!paused) stepStart = millis();
			break;
		case 'n':
			stepIndex = (stepIndex + 1) % kStepCount;
			applyStep();
			break;
		case 'b':
			stepIndex = (stepIndex + kStepCount - 1) % kStepCount;
			applyStep();
			break;
		case 'h':
			printHelp();
			break;
		default:
			break;  // ignore Enter, spaces, and unknown keys
	}
}

void setup() {
	// Sample Mode first, the same way the race firmware does at power-up.
	int bootMode = analogRead(kPinMode);

	pinMode(kPinData, OUTPUT);
	pinMode(kPinClock, OUTPUT);
	pinMode(kPinLatch, OUTPUT);
	writeLights(0x00);

	pinMode(kPinGateL, OUTPUT);   digitalWrite(kPinGateL, LOW);
	pinMode(kPinGateR, OUTPUT);   digitalWrite(kPinGateR, LOW);
	pinMode(kPinReturn, OUTPUT);  digitalWrite(kPinReturn, LOW);

	pinMode(kPinLeft, INPUT);
	pinMode(kPinRight, INPUT);
	pinMode(LED_BUILTIN, OUTPUT);

	Serial.begin(115200);
	Serial.println();
	Serial.println(F("=== SC light-path test ==="));
	Serial.print(F("Built "));
	Serial.print(F(__DATE__));
	Serial.print(' ');
	Serial.println(F(__TIME__));
	Serial.print(F("Mode at boot: raw="));
	Serial.print(bootMode);
	Serial.println(bootMode < kAnalogThreshold
		? F(" -> PRESSED (race firmware would enter RACE_TEST)")
		: F(" -> released"));
	printHelp();

	applyStep();
	beatStart = millis();
}

void loop() {
	unsigned long now = millis();

	if (now - beatStart >= kHeartbeatMs) {
		beatStart = now;
		beatOn = !beatOn;
		digitalWrite(LED_BUILTIN, beatOn ? HIGH : LOW);
	}

	if (now - buttonPoll >= kButtonPollMs) {
		buttonPoll = now;
		pollButtons();
	}

	while (Serial.available() > 0) handleCommand((char)Serial.read());

	if (!paused && now - stepStart >= kStepMs) {
		stepIndex = (stepIndex + 1) % kStepCount;
		applyStep();
	}
}
