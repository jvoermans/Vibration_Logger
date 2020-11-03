// the main parameters to use

#ifndef PARAMS_HFLOGGER
#define PARAMS_HFLOGGER

#include "Arduino.h"
#include "SdFat.h"

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
constexpr unsigned long duration_reading_micros = 100000;

#endif // !PARAMS_HFLOGGER