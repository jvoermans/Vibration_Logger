#ifndef FAST_LOGGER
#define FAST_LOGGER

#include "Arduino.h"

#include <PersistentFilenumber.h>
#include "SdFat.h"

#include <params.h>


////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////

// a bit of Sd card setup
// inspired from: https://github.com/greiman/SdFat-beta/blob/master/examples/ExFatLogger/ExFatLogger.ino

////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////

// store all data to be written in some 512 bytes blocks
// all sizes are statically checked to make sure no alignment problem or similar

// metadata is a 12 bytes sub-block
struct BlockMetadata{
    // source ID
    uint16_t metadata_id;
    // block number, to keep track of dropouts 
    uint16_t block_number;

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

// a struct containing statistics about a time series
struct TimeSeriesStatistics{
    double mean; // mean(X)
    double mean_of_square; // mean(X**2)
    double max; // max value reached
    double min; // min value reached
    unsigned long extremal_count; // nbr of readings over or under mean +- percent_threshold
};

////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////

// getting the interrupt related stuff into the class is tricky, keep it outside
// TODO: read about ISRs, classes, etc
// TODO: ask for explanation why did not work in SO issue

constexpr int nbr_blocks_per_adc_channel = 16;

constexpr int nbr_adc_measurements_per_block = 250;

extern volatile bool blocks_to_write[nbr_blocks_per_adc_channel];

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

// a class to take care of tracking time series statistics
// this will "eat" readings from an ADC channel, and generate on-the-fly stats about this channel
// the stats are the ones descrived in the TimeSeriesStatistics
// an updated stat struct is made available regularly, as soon as fully computed
class TimeSeriesAnalyzer{
    public:
        void init(void);

        // is there a stat struct available readily computed?
        bool stats_are_available(void) const;

        // get access to the computed stat struct available, and reset the availability flag
        TimeSeriesStatistics const & get_stats(void);

        // register a new value inside the current building stat
        void register_value(int value_in);

    private:
        // if an unread TimeSeriesStatistics is available
        bool flag_stats_available;

        // how many points we have currently been registering into the stat under building
        int crrt_nbr_registered_values;

        // reset the stat under buildind and the nbr of registered values
        void reset_filling_stats(void);

        // the stats under building
        TimeSeriesStatistics crrt_filling_stats;

        // the available stat
        TimeSeriesStatistics available_stats;
};

// return the position where the next write must be done
int write_statistics(char * buffer, TimeSeriesStatistics const & to_dump);

// the wrapper class for simple user interface
// this will allow to log fast both chars and ADC channels
class FastLogger
{
public:
    // start recording
    bool start_recording();

    // stop recording
    bool stop_recording();

    // status of the logging
    bool is_active();

    // log a char to the char block
    void log_char(const char crrt_char);

    // log a cstring to the char block; this will write both the cstring and timestamping
    // NOTE that the input must be a pointer to a cstring, i.e. a null terminated string!
    // NOTE that the char ";" is used as a delimiter internally, so cannot be part of the cstring!
    void log_cstring(const char * cstring_start);

    // perform necessary internal requests, such as updating file number, dumping to SD card etc
    // must be called in the main loop at regular intervals
    void internal_update();

    // enable Serial debug output on the "USB" serial
    void enable_serial_debug_output();

    // disable SD card, for example for testing, and perform some sample serial printing instead
    void disable_SD();

private:
    // how to keep track of file numbering between reboots
    PersistentFilenumber persistent_filenumber;

    // a few meta properties of the logging
    static constexpr int file_duration_seconds = logger_file_duration_seconds;

    static constexpr int file_duration_milliseconds = 1000 * file_duration_seconds;
    static constexpr int file_duration_microseconds = 1000 * file_duration_milliseconds;
    unsigned long time_opening_crrt_file = 0;

    bool logging_is_active = false;

    bool sd_is_active = true;

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
                                                           + file_duration_seconds * 2 + 10;
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