#ifndef PERSISTENT_STORAGE
#define PERSISTENT_STORAGE

#include "Arduino.h"
#include <DueFlashStorage.h>

DueFlashStorage due_flash_storage;

// we store the following:
// 0: whether started (0) or any other value
// 1-4: 32-bit int that is file index

constexpr uint32_t address_flash_started = 0;
constexpr uint32_t address_flash_start_filenbr = 1;

// get the filenumber to which should write next
// if this is the first time we boot after sketch uploading, initialize to 0
uint32_t get_file_number(){
    uint8_t flag_started = due_flash_storage.read(address_flash_started);

    // if necessary, "reset" the storing: this will be at first booting
    if (flag_started > 0){
        due_flash_storage.write(address_flash_started, 0);

        for (uint32_t i = 0; i < 4; i++){
            due_flash_storage.write(address_flash_start_filenbr + i, 0);
        }
    }

    // get the 4 bytes from the flash and reconstruct file number
    uint32_t file_nbr = 0;

    for (uint32_t i = 0; i < 4; i++){
        uint8_t crrt_part = due_flash_storage.read(address_flash_start_filenbr + i);
        file_nbr += (static_cast<uint32_t>(crrt_part) << (8 * i));
    }

    return(file_nbr);
}

// increment the stored filenumber
void increment_file_number(){
    uint32_t file_number = get_file_number();
    file_number += 1;

    for (uint32_t i = 0; i < 4; i++){
        uint8_t crrt_part = static_cast<uint8_t>((file_number >> (8 * i)) & 0xFF);
        due_flash_storage.write(address_flash_start_filenbr + i, crrt_part);
    }
}

#endif