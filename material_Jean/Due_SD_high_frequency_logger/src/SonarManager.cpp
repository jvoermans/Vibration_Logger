#include "SonarManager.h"

void SonarManager::start_sonar(FastLogger * fast_logger, bool use_serial_debug){
  fast_logger = fast_logger;
  use_serial_debug = use_serial_debug;

  for (int i=0; i<size_buffer_status_message; i++){
      buffer_status_messages[i] = '\0';
  }

  // start the sonar
  selected_sonar_serial->begin(9600);

  // try at most 5 times to start
  for(int i=0; i < 5; i++){
      if (!sonar_ping.initialize()){
          if (use_serial_debug){
              Serial.println(F("fail start sonar ping"));
              delay(250);
          }
      }
      else{
          working_sonar = true;
          break;
      }
  }

  // make time ready to measureSerial.printlnSerial.printlnSerial.println
  time_last_measurement_ms = millis() - sample_period_sonar_ms - 1;
}

bool SonarManager::ready_to_measure(void){
    if (millis() - time_last_measurement_ms > sample_period_sonar_ms){
        return true;
    }
    else{
        return false;
    }
}

void SonarManager::measure_and_log(void){
    if (working_sonar){

        if (use_serial_debug){
            Serial.println(F("rqst snr"));
        }

        buffer_status_messages[0] = 'r'; // msg flag request
        buffer_status_messages[1] = 's';
        buffer_status_messages[2] = 'n';
        buffer_status_messages[3] = 'r';
        buffer_status_messages[4] = ';';
        buffer_status_messages[5] = '\0';

        fast_logger->log_cstring(buffer_status_messages);  // request sonar

        time_last_measurement_ms = millis();

        if (sonar_ping.request(Ping1DNamespace::Profile)) {
            for (int i = 0; i < sonar_ping.profile_data_length(); i++) {
                uint8_t crrt_ping = sonar_ping.profile_data()[i];
                fast_logger->log_char(static_cast<char>(crrt_ping));
            }

        buffer_status_messages[0] = 'e';  // msg flag end
        buffer_status_messages[1] = 's';
        buffer_status_messages[2] = 'n';
        buffer_status_messages[3] = 'r';
        buffer_status_messages[4] = ';';
        buffer_status_messages[5] = '\0';

        fast_logger->log_cstring(buffer_status_messages);

        } else {
            if (use_serial_debug){
                Serial.println(F("get sonar profile fail"));
            }
        }
    }
    else{
        if (use_serial_debug){
            Serial.println(F("non working sonar"));
        }
    }
}
