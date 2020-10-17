// the main parameters to use

#ifndef PARAMS_HFLOGGER
#define PARAMS_HFLOGGER

#include "Arduino.h"

constexpr uint8_t adc_channels[] = {7, 6, 5, 4, 3};
// constexpr int adc_sampling_frequency = 1000;
constexpr int adc_sampling_frequency = 100;

// static constexpr int file_duration_seconds = 15 * 60;
constexpr int logger_file_duration_seconds = 15;

#endif // !PARAMS_HFLOGGER