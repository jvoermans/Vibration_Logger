// A simple script to illustrate the fast logger
// this will:
// record data in blocks of 512 bytes
// push it to SD cards updating regularly the filename
// two kind of blocks will be pushed: ADC channels and chars
// there is a python parser that can read and parse the SD card information, see BinarySdDataParser.

// for parameters and setup see the params.h file

// wiring

// SD card     |     Arduino Due on the SPI MAX header

// 3V          |     3.3V
// GND         |     GND
// CLK         |     SPI SLK
// DO          |     MISO
// DI          |     MOSI
// CS          |     SS = 10

// GPS         |     Arduino Due

// 3.3V        |     3.3V
// GND         |     GND
// TX          |     RX1
// RX          |     TX1
// PPS         |     digital pin 2

// notes about the SD card
// make sure it is formatted in FAT
// adapt to fat type in params.h when the typedefs are performed
// to check that the formatting is optimal, run the:
// ExFatFormatter.ino to format in the best way
// SdInfo.ino to check that the formatting went well

// Temperature sensors

// the wiring is:
// Arduino Due -> multiplexer -> channel 0: temperature sensor 0
//                            -> channel 1: temperature sensor 1
//                            ...
//                            -> channel n: temperature sensor n

#include <Arduino.h>

#include <FastLogger.h>

#include <GPS_manager.h>

FastLogger fast_logger;

GPSManager gps_manager;

static constexpr bool use_serial_debug = true;
static constexpr bool disable_sd_card = false;

void setup() {
  delay(10000);

  constexpr unsigned long watchdog_firing_ms = 1000UL;
  watchdogEnable(watchdog_firing_ms);

  if (use_serial_debug){
    Serial.begin(115200);
    delay(100);
    Serial.println();
  }

  if (use_serial_debug){
    fast_logger.enable_serial_debug_output();
    gps_manager.enable_serial_debug_output();
  }

  watchdogReset();


  if (disable_sd_card){
    fast_logger.disable_SD();
  }

  fast_logger.start_recording();
  gps_manager.start_gps();

  fast_logger.log_cstring("Start!");

  watchdogReset();
}

void loop() {
  watchdogReset();

  // internal update must be called quite often so as to check if some ADC data to log
  fast_logger.internal_update();
  watchdogReset();

  if (fast_logger.is_active()){

    // take care of the GPS and log the GPS output
    gps_manager.update_status();
    if (gps_manager.pps_available()){
      if (use_serial_debug){
        Serial.println(F("PPS updt"));
      }
      fast_logger.log_cstring(gps_manager.get_pps_message());
    }

    fast_logger.internal_update();

    if (gps_manager.message_available()){
      if (use_serial_debug){
        Serial.println(F("GPS updt"));
      }
      fast_logger.log_cstring(gps_manager.get_message());
    }

    fast_logger.internal_update();
    watchdogReset();
  }
}
