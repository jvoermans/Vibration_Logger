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