#include <SPI.h>
#include <MFRC522.h>

// Adjust these pins if needed
#define RST_PIN   9
#define SS_PIN    10

MFRC522 reader(SS_PIN, RST_PIN);

void setup() {
  Serial.begin(9600);
  SPI.begin();          
  reader.PCD_Init();    // initialize MFRC522
  //Serial.println(F("RFID reader ready"));
}

void loop() {
  reader.PCD_SoftPowerDown();
  delay(100);
  reader.PCD_Init();
  // If a new card is present AND we can read its serial:
  if ( reader.PICC_IsNewCardPresent() && reader.PICC_ReadCardSerial() ) {
    // Dump UID
    Serial.print(F("Card UID:"));
    for (byte i = 0; i < reader.uid.size; i++) {
      if (reader.uid.uidByte[i] < 0x10) Serial.print('0');
      Serial.print(reader.uid.uidByte[i], HEX);
      Serial.print(' ');
    }
    Serial.println();

    // Halt this PICC and stop encryption on PCD
    reader.PICC_HaltA();

    // Small pause to debounce the reading
    delay(900);
    //delay(200);
  }
  // no need for a long delay here – loop will spin and check again ~as fast as it can
}
