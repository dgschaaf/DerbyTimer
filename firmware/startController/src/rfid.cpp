#include "rfid.h"
#include <SPI.h>
#include <MFRC522.h>

static byte RST_PIN 				= 8;
static byte CS_PIN_L 				= 10;
static byte CS_PIN_R 				= 9;
unsigned long rfidThresh			= 500;			// RFID read thresold, once ever 0.5 seconds

uint8_t leftCarID[UID_LEN]			= 0;			// left ID
uint8_t rightCarID[UID_LEN]			= 0;			// right ID

RFIDReader leftReader(CS_PIN_L, RST_PIN);
RFIDReader rightReader(CS_PIN_R, RST_PIN);

bool setupRFID() {
	pinMode(RST_PIN, OUTPUT);
	pinMode(CS_PIN_L, OUTPUT);
	pinMode(CS_PIN_R, OUTPUT);
	
	// Reset sequence
	digitalWrite(RST_PIN, LOW);
	delay(10);	// Hold reset
	digitalWrite(RST_PIN, HIGH);
	delay(50);	// Wait for RFID board boot
	
    SPI.begin();
    leftReader.init();
    rightReader.init();
	return leftReader.isValid() && rightReader.isValid();
}

RFIDResult RFIDReader::readTag(unsigned long readThreshold) {
        unsigned long currentTime = millis();
        if (currentTime - lastReadTime < readThreshold)	return RFID_SKIPPED;
        if (!reader.PICC_IsNewCardPresent()) 			return RFID_NO_TAG;
		if (!reader.PICC_ReadCardSerial()) 				return RFID_COMM_ERR;

        uidLength = reader.uid.size;
		if (uidLength > 10) 							return RFID_INVALID;	//wrong UID length
		
		if (uidLength != UID_LEN) {
			reader.PICC_HaltA();
			return RFID_INVALID;												// wrong UID length
		}
		
        for (byte i = 0; i < uidLength; i++) {
            uid[i] = reader.uid.uidByte[i];
        }
        lastReadTime = currentTime;
		reader.PICC_HaltA();
        return RFID_NEW;
}

void RFIDReader::init() {
	reader.PCD_Init();
}

bool RFIDReader::isValid() {
	byte version = reader.PCD_ReadRegister(MFRC522::VersionReg);
	return !(version == 0x00 || version == 0xFF);
}