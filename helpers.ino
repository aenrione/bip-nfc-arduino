long leToNumeric(byte* buffer, int size) {
    long value = 0;
    for (int i = 0; i < size; i++) {
        value += ((long) buffer[i] & 0xFFL) << (8 * i);
    }
    return value;
}

void parse_bip_timestamp(const byte* block, BipTimestamp* timestamp) {
    timestamp->day = (((block[1] << 8) + block[0]) >> 6) & 0x1f;
    timestamp->month = (((block[1] << 8) + block[0]) >> 11) & 0xf;
    timestamp->year = 2000 + ((((block[2] << 8) + block[1]) >> 7) & 0x1f);
    timestamp->hour = (((block[3] << 8) + block[2]) >> 4) & 0x1f;
    timestamp->minute = (((block[3] << 8) + block[2]) >> 9) & 0x3f;
    timestamp->second = (((block[4] << 8) + block[3]) >> 7) & 0x3f;
}

static bool is_bip_block_empty(const byte* block) {
    for(size_t i = 0; i < BLOCK_SIZE - 1; i++) { // Assuming block size is 16 bytes, last byte is the CRC
        if(block[i] != 0) {
            return false;
        }
    }
    return true;
}
static int compare_bip_timestamp(const BipTimestamp* t1, const BipTimestamp* t2) {
    if(t1->year != t2->year) {
        return t1->year - t2->year;
    }
    if(t1->month != t2->month) {
        return t1->month - t2->month;
    }
    if(t1->day != t2->day) {
        return t1->day - t2->day;
    }
    if(t1->hour != t2->hour) {
        return t1->hour - t2->hour;
    }
    if(t1->minute != t2->minute) {
        return t1->minute - t2->minute;
    }
    if(t1->second != t2->second) {
        return t1->second - t2->second;
    }
    return 0;
}

// Parses and prints the BIP transaction timestamp
void printBipTimestamp(BipTimestamp* timestamp) {
    Serial.print(timestamp->year);
    Serial.print("-");
    if (timestamp->month < 10) Serial.print("0");
    Serial.print(timestamp->month);
    Serial.print("-");
    if (timestamp->day < 10) Serial.print("0");
    Serial.print(timestamp->day);
    Serial.print(" ");
    if (timestamp->hour < 10) Serial.print("0");
    Serial.print(timestamp->hour);
    Serial.print(":");
    if (timestamp->minute < 10) Serial.print("0");
    Serial.print(timestamp->minute);
    Serial.print(":");
    if (timestamp->second < 10) Serial.print("0");
    Serial.print(timestamp->second);
}

// Function to print the parsed bip data
void bip_print(const BipData* bip_data) {
    Serial.print("Card ID: ");
    Serial.println(bip_data->card_id);
    
    Serial.print("Balance: $");
    Serial.println(bip_data->balance);
    
    Serial.print("Flags: ");
    Serial.println(bip_data->flags);
    
    Serial.print("Trip Time Window Ends: ");
    printBipTimestamp(&bip_data->trip_time_window);
    Serial.println();
    
    Serial.println("#Last Recharges (Top-ups)");
    for(size_t i = 0; i < 3; i++) {
        const BipTransaction* top_up = &bip_data->top_ups[i];
        if(compare_bip_timestamp(&top_up->timestamp, &bip_data->trip_time_window) > 0) {
            continue;
        }
        Serial.print("\n+$");
        Serial.print(top_up->amount);
        Serial.print("\n  @");
        printBipTimestamp(&top_up->timestamp);
    }
    Serial.println();
    
    Serial.println("\n#Last Charges (Trips)");
    for(size_t i = 0; i < 3; i++) {
        const BipTransaction* charge = &bip_data->charges[i];
        if(compare_bip_timestamp(&charge->timestamp, &bip_data->trip_time_window) > 0) {
            continue;
        }
        Serial.print("\n-$");
        Serial.print(charge->amount);
        Serial.print("\n  @");
        printBipTimestamp(&charge->timestamp);
    }
}

void bip_parse(byte data[64][16], BipData* bip_data) {
    bip_data->card_id = leToNumeric(&data[1][4], 4);
    bip_data->balance = leToNumeric(&data[34][0], 2);
    bip_data->flags = leToNumeric(&data[34][2], 2);
    
    // Parse trip time window
    parse_bip_timestamp(data[21], &bip_data->trip_time_window);
    
    // Last 3 top-ups: sector 10, ring-buffer of 3 blocks, timestamp in bytes 0-7, amount in bytes 9-10
    for(size_t i = 0; i < 3; i++) {
        int block_num = 10 * 4 + i;
        if(is_bip_block_empty(data[block_num])) {
            continue;
        }
        BipTransaction* top_up = &bip_data->top_ups[i];
        parse_bip_timestamp(data[block_num], &top_up->timestamp);
        top_up->amount = leToNumeric(&data[block_num][9], 2);
    }
    
    // Last 3 charges (i.e. trips), sector 11, ring-buffer of 3 blocks, timestamp in bytes 0-7, amount in bytes 10-11
    for(size_t i = 0; i < 3; i++) {
        int block_num = 11 * 4 + i;
        if(is_bip_block_empty(data[block_num])) {
            continue;
        }
        BipTransaction* charge = &bip_data->charges[i];
        parse_bip_timestamp(data[block_num], &charge->timestamp);
        charge->amount = leToNumeric(&data[block_num][10], 2);
    }
}

void print_bip_dump(byte data[64][16]) {
    Serial.println();
    Serial.println("BIP Data:");
    for (int i = 0; i < TOTAL_BLOCKS; i++) {
        Serial.print("Block ");
        if (i < 10) {
            Serial.print("0");
        }
        Serial.print(i);
        Serial.print(": ");
        for (int j = 0; j < BLOCK_SIZE; j++) {
            if (data[i][j] < 0x10) {
                Serial.print("0");
            } 
            Serial.print(data[i][j], HEX);
            Serial.print(" ");
        }
        Serial.println();
    }
}

