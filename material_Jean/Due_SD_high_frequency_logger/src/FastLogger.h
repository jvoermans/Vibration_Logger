#ifndef FAST_LOGGER
#define FAST_LOGGER

#include "Arduino.h"

#include <PersistentFilenumber.h>

////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////

// store all data to be written in some 512 bytes blocks

// a block of 512 bytes including metadata
// content is: 12 bytes metadata
// data are: 250 uint16_t entries i.e. 500 bytes
struct BlockWithMetadata
{
    // the ID of the data
    uint16_t metadata_id;

    // some timestamping for the start and end of the data
    unsigned long micros_start;
    unsigned long micros_end;

    uint16_t data[250];
};

// check that there are no data alignment problems or similar
static_assert(sizeof(BlockWithMetadata) == 512);

////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////

class FastLogger
{
public:
    bool start_recording(uint32_t file_number);
    bool stop_recording();

    void log_cstring(char crrt_char);

private:
    static PersistentFilenumber persistent_filenumber;

    static constexpr uint8_t adc_channels[] = {7, 6, 5, 4, 3};
    static constexpr int adc_sampling_frequency = 1000;
    static constexpr int nbr_blocks_per_adc_channel = 4;
    static constexpr int file_duration_seconds = 15 * 60;
    static constexpr int file_duration_milliseconds = 1000 * file_duration_seconds;

    static constexpr int nbr_of_adc_channels = sizeof(adc_channels);
    static constexpr int nbr_adc_measurements_per_block = 250;

    static unsigned long time_opening_crrt_file = 0;

    static int first_adc_block_to_write = -1;
    static int last_adc_block_to_write = 0;

    static volatile BlockWithMetadata blocks_adc_with_metdata[nbr_of_adc_channels][nbr_blocks_per_adc_channel];
    static BlockWithMetadata blocks_cstring_with_metadata[2];

    // TODO: set the Fat exFat or similar object under

    bool write_block_to_sd_card();
    bool write_adc_blocks_to_sd_card();

    bool open_crrt_file();
    bool close_crrt_file();

    void check_need_new_file();
};

#endif // FAST_LOGGER