// the main parameters to use

#ifndef PARAMS_HFLOGGER
#define PARAMS_HFLOGGER

#include "Arduino.h"

constexpr uint8_t adc_channels[] = {7, 6, 5, 4, 3};
// constexpr int adc_sampling_frequency = 1000;
constexpr int adc_sampling_frequency = 100;

// static constexpr int file_duration_seconds = 15 * 60;
constexpr int logger_file_duration_seconds = 15;

// which kind of card format is used
// this is what works on my 32 GB SD card
typedef SdFs sd_t;
typedef FsFile file_t;
// may need to use ExFat so that can have large SD cards
// typedef SdExFat sd_t;
// typedef ExFile file_t;


#endif // !PARAMS_HFLOGGER