// A simple script to illustrate the fast logger
// this will:
// record data in blocks of 512
// push it to SD cards updating regularly the filename

// set the frequency of ADC logging at params.h -> adc_sampling_frequency
// set the duration of each file at params.h -> file_duration_seconds

#include <Arduino.h>

#include <FastLogger.h>
#include <GPS_manager.h>

FastLogger fast_logger;
static constexpr bool use_serial_debug = true;
static constexpr bool disable_sd_card = false;

// trash, just to make sure we write a bit...
unsigned long last_micros = 0;

void setup() {
  if (use_serial_debug){
    Serial.begin(115200);
    delay(100);
  }

  if (use_serial_debug){
    fast_logger.enable_serial_debug_output();
  }

  if (disable_sd_card){
    fast_logger.disable_SD();
  }

  fast_logger.start_recording();

  fast_logger.log_cstring("Start!");

}

void loop() {
  // internal update must be called quite often so as to check if some ADC data to log
  fast_logger.internal_update();

  // just to make sure we write a bit...
  if (micros() - last_micros > 100000){
    last_micros = micros();
    fast_logger.log_cstring("test");
  }
}
