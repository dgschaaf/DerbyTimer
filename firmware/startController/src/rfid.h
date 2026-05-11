#ifndef RFID_H
#define RFID_H

#include <SPI.h>
#include <MFRC522.h>


bool setupRFID();

enum RFIDResult: uint8_t {
	RFID_OK,
	RFID_INVALID,
	RFID_COMM_ERR,
	RFID_NO_TAG,
	RFID_SKIPPED,
	RFID_NEW
};

struct RFIDReader {
    MFRC522 reader;
    unsigned long lastReadTime = 0;
    uint8_t uid[4];   // Buffer for UID
    byte uidLength = 0;
    RFIDReader(byte ssPin, byte rstPin)
        : reader(ssPin, rstPin) {}
    RFIDResult readTag(unsigned long readThreshold);
    void init();
    bool isValid();
};

// RFID Properties
#define UID_LEN			4

extern uint8_t leftCarID[UID_LEN];			// rx for left ID
extern uint8_t rightCarID[UID_LEN];			// rx for right ID

extern RFIDReader leftReader;
extern RFIDReader rightReader;

//extern uint8_t uidLength; - unneeded, set in the RFIDReader struct
extern unsigned long rfidThresh;

#endif