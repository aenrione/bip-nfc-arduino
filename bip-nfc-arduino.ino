#include <SPI.h>
#include <MFRC522.h>

#define RST_PIN         9
#define SS_PIN          10

MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance

MFRC522::MIFARE_Key key;  // Declare the key structure

// Define the custom key from BIP.java
byte key_33[6] = { 0x64, 0xE3, 0xC1, 0x03, 0x94, 0xC2 };  

void setup() {
    Serial.begin(9600);  // Initialize serial communications with the PC
    while (!Serial);     // Do nothing if no serial port is opened
    SPI.begin();         // Init SPI bus
    mfrc522.PCD_Init();  // Init MFRC522
    delay(4);            // Optional delay
    mfrc522.PCD_DumpVersionToSerial();  // Show details of PCD - MFRC522 Card Reader details
    Serial.println(F("Scan PICC to see UID, SAK, type, and data blocks..."));

    // Initialize the key with the custom key values
    for (byte i = 0; i < 6; i++) {
        key.keyByte[i] = key_33[i];
    }
}

// Converts a little-endian byte array to a numeric value
long leToNumeric(byte* buffer, int size) {
    long value = 0;
    for (int i = 0; i < size; i++) {
        value += ((long) buffer[i] & 0xFFL) << (8 * i);
    }
    return value;
}

// Converts a little-endian byte array to a numeric string
String leToNumericString(byte* buffer, int size) {
    return String(leToNumeric(buffer, size));
}

// Formats the numeric value as currency
String formatMoneda(long valor) {
    char formatted[20];
    sprintf(formatted, "$%ld", valor);  // Removing comma formatting for simplicity
    return String(formatted);
}

// Example usage within your loop or where you process the block data
void processBlockData(byte* buffer) {
    // Assuming the saldo is stored in the first 4 bytes of block 34
    long saldo = leToNumeric(buffer, 4);
    String saldoStr = formatMoneda(saldo);
    Serial.print(F("Saldo: "));
    Serial.println(saldoStr);
}


void loop() {
    // Reset the loop if no new card present on the sensor/reader
    if (!mfrc522.PICC_IsNewCardPresent()) {
        return;
    }

    // Select one of the cards
    if (!mfrc522.PICC_ReadCardSerial()) {
        return;
    }

    Serial.print(F("Card UID: "));
    for (byte i = 0; i < mfrc522.uid.size; i++) {
        Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
        Serial.print(mfrc522.uid.uidByte[i], HEX);
    }
    Serial.println();

    // Authenticate and read specific blocks as needed
    byte block = 34;  // Try a different block number
    MFRC522::StatusCode status;

    // Authenticate using the custom key
    status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_B, block, &key, &(mfrc522.uid));
    if (status != MFRC522::STATUS_OK) {
        Serial.print(F("PCD_Authenticate() failed: "));
        Serial.println(mfrc522.GetStatusCodeName(status));
        return;
    }

    // Read the block
    byte buffer[18];
    byte size = sizeof(buffer);
    status = mfrc522.MIFARE_Read(block, buffer, &size);
    if (status != MFRC522::STATUS_OK) {
        Serial.print(F("MIFARE_Read() failed: "));
        Serial.println(mfrc522.GetStatusCodeName(status));
        return;
    }

    // Print block data
    Serial.print(F("Data in block "));
    Serial.print(block);
    Serial.println(F(":"));
    for (byte i = 0; i < 16; i++) {
        Serial.print(buffer[i] < 0x10 ? " 0" : " ");
        Serial.print(buffer[i], HEX);
    }
    Serial.println();

    processBlockData(buffer);

    // Halt PICC
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
}

