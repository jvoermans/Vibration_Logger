#include "PersistentStorage.h"

uint32_t PersistentStorage::get_file_number(void){
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

void PersistentStorage::increment_file_number(void){
    uint32_t file_number = get_file_number();
    file_number += 1;

    for (uint32_t i = 0; i < 4; i++){
        uint8_t crrt_part = static_cast<uint8_t>((file_number >> (8 * i)) & 0xFF);
        due_flash_storage.write(address_flash_start_filenbr + i, crrt_part);
    }
}