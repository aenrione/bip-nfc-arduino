#include <SPI.h>
#include <MFRC522.h>

#define RST_PIN         9
#define SS_PIN          10


#define BLOCK_SIZE        16
#define BLOCKS_PER_SECTOR 4
#define TOTAL_BLOCKS      64

MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance
MFRC522::MIFARE_Key key;  // Declare the key structure

// data var to store blocks
byte data[TOTAL_BLOCKS][BLOCK_SIZE];

typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} BipTimestamp;

typedef struct {
    BipTimestamp timestamp;
    uint16_t amount;
} BipTransaction;

static const uint64_t bip_keys_a[] = {
    0x3a42f33af429,
    0x6338a371c0ed,
    0xf124c2578ad0,
    0x32ac3b90ac13,
    0x4ad1e273eaf1,
    0xe2c42591368a,
    0x2a3c347a1200,
    0x16f3d5ab1139,
    0x937a4fff3011,
    0x35c3d2caee88,
    0x693143f10368,
    0xa3f97428dd01,
    0x63f17a449af0,
    0xc4652c54261c,
    0xd49e2826664f,
    0x3df14c8000a1,
};

static const uint64_t bip_keys_b[] = {
    0x1fc235ac1309,
    0x243f160918d1,
    0x9afc42372af1,
    0x682d401abb09,
    0x067db45454a9,
    0x15fc4c7613fe,
    0x68d30288910a,
    0xf59a36a2546d,
    0x64e3c10394c2,
    0xb736412614af,
    0x324f5df65310,
    0x643fb6de2217,
    0x82f435dedf01,
    0x0263de1278f3,
    0x51284c3686a6,
    0x6a470d54127c,
};

struct BipData {
    uint32_t card_id;
    uint16_t balance;
    uint16_t flags;
    BipTimestamp trip_time_window;
    BipTransaction top_ups[3];
    BipTransaction charges[3];
};

void setup() {
    Serial.begin(9600);  // Initialize serial communications with the PC
    while (!Serial);     // Do nothing if no serial port is opened
    SPI.begin();         // Init SPI bus
    mfrc522.PCD_Init();  // Init MFRC522
    
    Serial.println(F("Scan PICC to see UID, SAK, type, and data blocks..."));
}


void loop() {
    if (!mfrc522.PICC_IsNewCardPresent()) {
        return;
    }

    // Select one of the cards
    if (!mfrc522.PICC_ReadCardSerial()) {
        return;
    }

    // Clear the data buffer
    for (int i = 0; i < TOTAL_BLOCKS; i++) {
        for (int j = 0; j < BLOCK_SIZE; j++) {
            data[i][j] = 0;
        }
    }

    // Authenticate and read all blocks
    byte buffer[BLOCK_SIZE + 2];  // Each block is 16 bytes + 2 bytes CRC
    byte size = sizeof(buffer);
    MFRC522::StatusCode status;

    for (byte block = 0; block < TOTAL_BLOCKS; block++) {  // Assuming 64 blocks for a typical MIFARE card
        // Determine which key to use
        MFRC522::MIFARE_Key key_a, key_b;
        byte sector = block / BLOCKS_PER_SECTOR;
        for (byte j = 0; j < 6; j++) {
            key_a.keyByte[j] = (bip_keys_a[sector] >> (8 * (5 - j))) & 0xFF;
            key_b.keyByte[j] = (bip_keys_b[sector] >> (8 * (5 - j))) & 0xFF;
        }

        // Authenticate using the determined key
        status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &key_a, &(mfrc522.uid));

        if (status != MFRC522::STATUS_OK) {
            status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_B, block, &key_b, &(mfrc522.uid));
        }

        if (status != MFRC522::STATUS_OK) {
            Serial.println();
            Serial.print(F("PCD_Authenticate() failed for block "));
            Serial.print(block);
            Serial.print(F(": "));
            Serial.println(mfrc522.GetStatusCodeName(status));
            continue;
        }

        // Read the block
        status = mfrc522.MIFARE_Read(block, buffer, &size);
        if (status != MFRC522::STATUS_OK) {
            Serial.print(F("MIFARE_Read() failed for block "));
            Serial.print(block);
            Serial.print(F(": "));
            Serial.println(mfrc522.GetStatusCodeName(status));
            continue;
        }

        // Store block data
        for (byte i = 0; i < BLOCK_SIZE; i++) {
            data[block][i] = buffer[i];
        }
    }

    // Parse and process the block data
    BipData bip_data;
    bip_parse(data, &bip_data);
    bip_print(&bip_data);
    // print_bip_dump(data); // Uncomment to print the raw block data

    // Halt PICC
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    delay(1000);
}
