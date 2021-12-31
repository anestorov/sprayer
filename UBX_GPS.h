//#include <SoftwareSerial.h>

// Connect the GPS RX/TX to arduino pins 3 and 5
//SoftwareSerial serial = SoftwareSerial(3,5);

const unsigned char UBX_HEADER[]        = { 0xB5, 0x62 };
const unsigned char NAV_POSLLH_HEADER[] = { 0x01, 0x02 };
const unsigned char NAV_STATUS_HEADER[] = { 0x01, 0x03 };
const unsigned char NAV_VELNED_HEADER[] = { 0x01, 0x12 };

enum _ubxMsgType {
  MT_NONE,
  MT_NAV_POSLLH,
  MT_NAV_STATUS,
  MT_NAV_VELNED
};

struct NAV_VELNED {
  unsigned char cls;
  unsigned char id;
  unsigned short len;
  unsigned long iTOW;
  long velN;
  long velE;
  long velD;
  unsigned long speed;
  unsigned long gSpeed;
  long heading;
  unsigned long sAcc;
  unsigned long cAcc;
};

struct NAV_POSLLH {
  unsigned char cls;
  unsigned char id;
  unsigned short len;
  unsigned long iTOW;
  long lon;
  long lat;
  long height;
  long hMSL;
  unsigned long hAcc;
  unsigned long vAcc;
};

struct NAV_STATUS {
  unsigned char cls;
  unsigned char id;
  unsigned short len;
  unsigned long iTOW;
  unsigned char gpsFix;
  char flags;
  char fixStat;
  char flags2;
  unsigned long ttff;
  unsigned long msss;
};

union UBXMessage {
  NAV_POSLLH navPosllh;
  NAV_STATUS navStatus;
  NAV_VELNED navVelned;
};

UBXMessage ubxMessage;

// The last two bytes of the message is a checksum value, used to confirm that the received payload is valid.
// The procedure used to calculate this is given as pseudo-code in the uBlox manual.
void calcChecksum(unsigned char* CK, int msgSize) {
  memset(CK, 0, 2);
  for (int i = 0; i < msgSize; i++) {
    CK[0] += ((unsigned char*)(&ubxMessage))[i];
    CK[1] += CK[0];
  }
}


// Compares the first two bytes of the ubxMessage struct with a specific message header.
// Returns true if the two bytes match.
boolean compareMsgHeader(const unsigned char* msgHeader) {
  unsigned char* ptr = (unsigned char*)(&ubxMessage);
  return ptr[0] == msgHeader[0] && ptr[1] == msgHeader[1];
}


// Reads in bytes from the GPS module and checks to see if a valid message has been constructed.
// Returns the type of the message found if successful, or MT_NONE if no message was found.
// After a successful return the contents of the ubxMessage union will be valid, for the 
// message type that was found. Note that further calls to this function can invalidate the
// message content, so you must use the obtained values before calling this function again.
int processGPS() {
  static int fpos = 0;
  static unsigned char checksum[2];
  
  static byte currentMsgType = MT_NONE;
  static int payloadSize = sizeof(UBXMessage);

  while ( Serial.available() ) {
    
    byte c = Serial.read();    
    //Serial.write(c);
    
    if ( fpos < 2 ) {
      // For the first two bytes we are simply looking for a match with the UBX header bytes (0xB5,0x62)
      if ( c == UBX_HEADER[fpos] )
        fpos++;
      else
        fpos = 0; // Reset to beginning state.
    }
    else {
      // If we come here then fpos >= 2, which means we have found a match with the UBX_HEADER
      // and we are now reading in the bytes that make up the payload.
      
      // Place the incoming byte into the ubxMessage struct. The position is fpos-2 because
      // the struct does not include the initial two-byte header (UBX_HEADER).
      if ( (fpos-2) < payloadSize )
        ((unsigned char*)(&ubxMessage))[fpos-2] = c;

      fpos++;
      
      if ( fpos == 4 ) {
        // We have just received the second byte of the message type header, 
        // so now we can check to see what kind of message it is.
        if ( compareMsgHeader(NAV_POSLLH_HEADER) ) {
          currentMsgType = MT_NAV_POSLLH;
          payloadSize = sizeof(NAV_POSLLH);
        }
        else if ( compareMsgHeader(NAV_STATUS_HEADER) ) {
          currentMsgType = MT_NAV_STATUS;
          payloadSize = sizeof(NAV_STATUS);
        }
        else if ( compareMsgHeader(NAV_VELNED_HEADER) ) {
          currentMsgType = MT_NAV_VELNED;
          payloadSize = sizeof(NAV_VELNED);
        }
        else {
          // unknown message type, bail
          fpos = 0;
          continue;
        }
      }

      if ( fpos == (payloadSize+2) ) {
        // All payload bytes have now been received, so we can calculate the 
        // expected checksum value to compare with the next two incoming bytes.
        calcChecksum(checksum, payloadSize);
      }
      else if ( fpos == (payloadSize+3) ) {
        // First byte after the payload, ie. first byte of the checksum.
        // Does it match the first byte of the checksum we calculated?
        if ( c != checksum[0] ) {
          // Checksum doesn't match, reset to beginning state and try again.
          fpos = 0; 
        }
      }
      else if ( fpos == (payloadSize+4) ) {
        // Second byte after the payload, ie. second byte of the checksum.
        // Does it match the second byte of the checksum we calculated?
        fpos = 0; // We will reset the state regardless of whether the checksum matches.
        if ( c == checksum[1] ) {
          // Checksum matches, we have a valid message.
          return currentMsgType; 
        }
      }
      else if ( fpos > (payloadSize+4) ) {
        // We have now read more bytes than both the expected payload and checksum 
        // together, so something went wrong. Reset to beginning state and try again.
        fpos = 0;
      }
    }
  }
  return MT_NONE;
}
