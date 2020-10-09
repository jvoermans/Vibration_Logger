
// TODO: clean this note
// some notes about SD cards
// a nice testing: https://jitter.company/blog/2019/07/31/microsd-performance-on-memory-constrained-devices/
// some in depth intro: https://www.parallax.com/sites/default/files/downloads/AN006-SD-FFS-Drivers-v1.0.pdf

// TODO on other extended scripts
// 512 bytes data for each channel as buffer (double to make sure ok with race conditions)
// TODO: 12 bytes header on each 512 bytes SD card segment, data comes only after
// 1 byte type, 1 byte channel number, 4 bytes micros at start, 4 bytes micro at write, 2 bytes non used
// TODO: micro at start should be set from the ADC_handler
// TODO: micro at write should be set at the writing

// TODO:
// take care of:
// instead of a flag, use an int to say when to read and where; -1: do not read; >0: where to start reading.
// put conversion flag when the half buffer has been filled
// provide some function to access the current first index of the buffer to read

// TODO:
// test / check that able to successfully run and log adc_channels

// TODO: write header

// TODO:
// add writing to SD card
// https://forum.arduino.cc/index.php?topic=462059.0
// https://forum.arduino.cc/index.php?topic=229731.0
// https://forum.arduino.cc/index.php?topic=232914.msg1679019#msg1679019


// perform some high speed writing to SD card

#include <Arduino.h>

#include <PersistentStorage.h>

#include <SPI.h>
#include "SdFat.h"

// SD card configuration
constexpr unsigned int sd_slave_select = 53;
SdFat sd;
SdFile sd_file;



void setup() {
  Serial.begin(115200);
  delay(100);
}

void loop() {
  uint32_t file_number;

  file_number = get_file_number();
  Serial.print(F("got file_number:"));
  Serial.println(file_number);

  delay(500);
  increment_file_number();
}

// TODO
// check persistent file numbering

// TODO
// set up raw writing with "beta" SD library; if does not work, use "old" library
// write for 1 minute per file and then new file
// write in chunks of 512 bytes
// test of speed / latency / consistency: write 5 x 1kHz equivalent, 5 x 10kHz equivalent with increasing values for test

// TODO
// check power resets and how affects
// may need to detect power loss and stop recording on SD card then
// i.e.: raw power supply -> 3.3V supply -> supercap -> Arduino Due
//          |
//          ---> measure voltage: if falls under 2.7V, stop the recording

// TODO
// use SD card library v 1 vs v2 beta