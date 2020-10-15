#ifndef PERSISTENT_FILENUMBER
#define PERSISTENT_FILENUMBER

#include "Arduino.h"
#include <DueFlashStorage.h>

class PersistentFilenumber{
    public:
        // get the filenumber to which should write next
        // if this is the first time we boot after sketch uploading, initialize to 0
        // if this is the first time we boot after sketch uploading, initialize to 0
        uint32_t get_file_number(void);

        // increment the stored filenumber
        void increment_file_number(void);

    private:
        // we store the following:
        // address 0: whether started (0) or any other value
        // address 1-4: 32-bit int that is file index
        static constexpr uint32_t address_flash_started = 0;
        static constexpr uint32_t address_flash_start_filenbr = 1;
        
        DueFlashStorage due_flash_storage;
};

#endif