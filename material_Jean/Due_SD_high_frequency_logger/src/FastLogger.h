#ifndef FAST_LOGGER
#define FAST_LOGGER

#include "Arduino.h"

#include <PersistentFilenumber.h>
#include "SdFat.h"


////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////

// a bit of Sd card setup
// inspired from: https://github.com/greiman/SdFat-beta/blob/master/examples/ExFatLogger/ExFatLogger.ino

// need to use ExFat so that can have large SD cards
typedef SdExFat sd_t;
typedef ExFile file_t;

////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////

// store all data to be written in some 512 bytes blocks
// all sizes are statically checked to make sure no alignment problem or similar

// metadata is a 12 bytes sub-block
struct BlockMetadata{
    // source ID
    uint8_t metadata_id;
    // block number, to keep track of dropouts 
    uint8_t block_number;

    // start and end of the block in micros
    unsigned long micros_start;
    unsigned long micros_end;
};

static_assert(sizeof(BlockMetadata) == 12);

// a block of 512 bytes including metadata
// data are: 250 uint16_t entries i.e. 500 bytes
struct BlockADCWithMetadata{
    BlockMetadata metadata;

    uint16_t data[250];
};

static_assert(sizeof(BlockADCWithMetadata) == 512);

// a block of 512 bytes including metadata
// data are plain chars
struct BlockCharsWithMetadata{
    BlockMetadata metadata;

    char data[500];
};

static_assert(sizeof(BlockCharsWithMetadata) == 512);

////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////

// getting the interrupt related stuff into the class is tricky, keep it outside
// TODO: read about ISRs, classes, etc
// TODO: ask for explanation why did not work in SO issue

// the properties for ADC channels logging

// the adc_channels to read, in uC reference, NOT in Arduino Due pinout reference
// for a mapping, see: https://components101.com/microcontrollers/arduino-due
// i.e. A0 is AD7
//      A1    AD6
//      A2    AD5
//      A3    AD4
//      A4    AD3
//      A5    AD2
//      A6    AD1
//      A7    AD
constexpr uint8_t adc_channels[] = {7, 6, 5, 4, 3};
constexpr int adc_sampling_frequency = 1000;
constexpr int nbr_blocks_per_adc_channel = 4;

constexpr int nbr_of_adc_channels = sizeof(adc_channels);
constexpr int nbr_adc_measurements_per_block = 250;

extern volatile bool blocks_to_write[nbr_of_adc_channels];

extern volatile int crrt_adc_block_index_to_write;
extern volatile int crrt_adc_data_index_to_write;

extern volatile BlockADCWithMetadata blocks_adc_with_metdata[nbr_of_adc_channels][nbr_blocks_per_adc_channel];

// start ADC conversion on rising edge on time counter 0 channel 2
// perform ADC conversion on several adc_channels in a row one after the other
// report finished conversion using ADC interrupt
void adc_setup();

// use time counter 0 channel 2 to generate the ADC start of conversion signal
// i.e. this sets a rising edge with the right frequency for triggering ADC conversions corresponding to adc_sample_rate
// for more information about the timers: https://github.com/ivanseidel/DueTimer/blob/master/TimerCounter.md
// NOTE: TIOA2 should not be available on any due pin https://github.com/ivanseidel/DueTimer/issues/11
void tc_setup();

// ISR for the ADC ready readout interrupt
// push the current ADC data on all adc_channels to the buffer
// update the time index
// set flag conversion ready
void ADC_Handler();

// the wrapper class for simple user interface
class FastLogger
{
public:
    // start recording to a new file
    bool start_recording();

    // stop recording, flush buffers and close active file
    bool stop_recording();

    // log a char to the char block
    void log_char(char crrt_char);

    // perform necessary internal requests, such as updating file number
    void internal_update();

    // enable Serial debug output on the "USB" serial
    void enable_serial_debug_output();

private:
    // how to keep track of file numbering between reboots
    PersistentFilenumber persistent_filenumber;

    // a few meta properties of the logging
    static constexpr int file_duration_seconds = 15 * 60;
    static constexpr int file_duration_milliseconds = 1000 * file_duration_seconds;
    static constexpr int file_duration_microseconds = 1000 * file_duration_milliseconds;
    unsigned long time_opening_crrt_file = 0;

    bool logging_is_active = false;

    bool serial_debug_output_is_active = false;

    // the properties for char logging
    static constexpr int nbr_blocks_char = 2;
    BlockCharsWithMetadata blocks_cstring_with_metadata[nbr_blocks_char];

    static constexpr int nbr_chars_per_block = 500;

    int crrt_char_block_index_to_write = 0;
    int crrt_char_data_index_to_write = 0;

    // the Sd interfacing and some SD file properties

    sd_t sd_object;
    file_t binary_file;

    char filename[14] = "F00000000.bin";
    static constexpr int nbr_of_zeros_in_filename = 8;

    // the pre-allocated size in bytes; we count in number of 512 bytes blocks (2**9 = 512)
    // the number of blocks is the sum of how many chars logging blocks, and how many ADC blocks
    // to be on the safe side, be a bit generous
    static constexpr uint64_t preallocate_nbr_blocks = file_duration_seconds * adc_sampling_frequency / nbr_adc_measurements_per_block
                                                           + file_duration_seconds * 2;
    static constexpr uint64_t preallocate_size = preallocate_nbr_blocks << 9;

    // write a block, i.e. the next 512 bytes, to the SD card
    bool write_block_to_sd_card(void * block_start);

    // write the blocks for all active ADC channels
    bool write_adc_blocks_to_sd_card(int adc_blocks_index);

    // open and start logging on a new file
    bool open_new_file();

    // close the current file
    bool close_crrt_file();

    // check if a new file is needed because of timer
    bool need_new_file();
};

#endif // FAST_LOGGER