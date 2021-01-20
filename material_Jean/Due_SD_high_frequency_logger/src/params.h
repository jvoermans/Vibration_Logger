// the main parameters to use

#ifndef PARAMS_HFLOGGER
#define PARAMS_HFLOGGER

#include "Arduino.h"
#include "SdFat.h"

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
// some I2C properties

// avoid locking the MCU by putting some timeout
constexpr unsigned long i2c_timeout_micro_seconds = 100UL;
constexpr unsigned long i2c_clock_frequency = 50000UL;  // I think the default is 100000UL; may need to test by hand which values work

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
// parameters related to the ADC channels logging

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
constexpr int nbr_of_adc_channels = sizeof(adc_channels);

// the frequency of logging, in samples per seconds, ie 1000 for 1kHz
constexpr int adc_sampling_frequency = 1000;
// constexpr int adc_sampling_frequency = 100;

// the prescaler should be 100 for 1kHz, 15 for 10kHz, 2 for 100kHz
constexpr uint8_t adc_prescale = 100;

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
// parameters for calculating statistics on ADC time series

// how often to report an updated statistics
constexpr int nbr_of_seconds_per_analysis = 0.1 * 60;  // i.e. nbr_minutes * seconds_per_minute
constexpr int nbr_of_samples_per_analysis = nbr_of_seconds_per_analysis * adc_sampling_frequency;
constexpr int middle_adc_value = (0b1 << (12-1)) -1;
constexpr float threshold_extrema = 0.20;
constexpr int threshold_low = static_cast<int>(threshold_extrema * ((0b1 << 12) - 1) - middle_adc_value);
constexpr int threshold_high = static_cast<int>((1.0 - threshold_extrema) * ((0b1 << 12) - 1) - middle_adc_value);

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
// parameters related to the SD card and logging file

// the duration of each individual binary file in seconds
// static constexpr int file_duration_seconds = 15 * 60;
// constexpr int logger_file_duration_seconds = 15;
constexpr int logger_file_duration_seconds = 15;

// which kind of card format is used
// this is what works on my 32 GB SD card
typedef SdFs sd_t;
typedef FsFile file_t;
// may need to use ExFat so that can have large SD cards
// typedef SdExFat sd_t;
// typedef ExFile file_t;

// which slave select pin to use
// the default SS pin on due is the digital pin 10
const uint8_t sd_card_select_pin = SS;

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
// parameters related to the GPS

constexpr HardwareSerial * selected_gps_serial = &Serial1;
constexpr uint8_t selected_PPS_digital_pin = 2;

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
// parameters related to the temperature sensors

// using the I2C multiplexer TCA9548A
// https://learn.adafruit.com/adafruit-tca9548a-1-to-8-i2c-multiplexer-breakout/wiring-and-test
constexpr byte i2c_multiplexer_address = 0x70;

// how many temperature sensors to use, multiplexed by the I2C multiplier
constexpr int nbr_temp_sensors = 4;

// the duration we allow for taking one measurement
// needs to be slow enough that the conversion is finished
// the datasheet says that it can take up to 10ms, so saying 100ms to have quite a bit of margin
// using a larger value will further reduce the frequency of temperature measurements
constexpr unsigned long duration_reading_micros = 100000UL;

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
// parameters related to the sonar
constexpr HardwareSerial * selected_sonar_serial = &Serial2;

#endif // !PARAMS_HFLOGGER