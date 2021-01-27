#include "SonarManager.h"

void SonarManager::start_sonar(FastLogger * fast_logger, bool use_serial_debug){
  fast_logger = fast_logger;
  use_serial_debug = use_serial_debug;

  for (size_t i=0; i<size_buffer_status_message; i++){
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
          if (use_serial_debug){
              Serial.println(F("sonar started"));
          }
          break;
      }
  }

  // make time ready to measureSerial.printlnSerial.printlnSerial.println
  time_last_measurement_ms = millis() - sample_period_sonar_ms - 1;

  if (use_serial_debug){
      Serial.println(F("sonar initialized"));
  }
}

bool SonarManager::ready_to_measure(void){
    if (millis() - time_last_measurement_ms > sample_period_sonar_ms){
        if (use_serial_debug){
            Serial.println(F("SNR meas rdy"));
        }
        return true;
    }
    else{
        return false;
    }
}

void SonarManager::measure_and_log(void){
    time_last_measurement_ms = millis();

    if (use_serial_debug){
        Serial.println(F("millis"));
    }

    if (working_sonar){

        if (use_serial_debug){
            Serial.println(F("rqst snr"));
            // TODO: remove
            Serial.flush();
        }

        buffer_status_messages[0] = 'r'; // msg flag request
        buffer_status_messages[1] = 's';
        buffer_status_messages[2] = 'n';
        buffer_status_messages[3] = 'r';
        buffer_status_messages[4] = ':';
        buffer_status_messages[5] = '\0';

        fast_logger->log_cstring(buffer_status_messages);  // request sonar

        /* this would be the ideal: logging the full sonar profile; however, I somehow cannot get this to work!
        if (sonar_ping.request(Ping1DNamespace::Profile)) {
            if (use_serial_debug){
                Serial.print(F("nbr sonar points: "));
                Serial.println(sonar_ping.profile_data_length());
                // TODO: remove
                Serial.flush();
            }

            for (unsigned int i = 0; i < sonar_ping.profile_data_length(); i++) {
                uint8_t crrt_ping = sonar_ping.profile_data()[i];
                fast_logger->log_char(static_cast<char>(crrt_ping));
            }
        */

       // */ this is less information; sending only the distance to max hit and confidence
       int sonar_distance;
       int sonar_confidence;

        if (sonar_ping.update()) {
            sonar_distance = sonar_ping.distance();
            sonar_confidence = sonar_ping.confidence();

            char buffer_sonar_message[64];

            for (size_t i=0; i<64; i++){
                buffer_sonar_message[i] = '\0';
            }

            buffer_sonar_message[0] = 'D';
            buffer_sonar_message[1] = 'S';
            buffer_sonar_message[2] = 'T';
            buffer_sonar_message[3] = ':';

            sprintf(&buffer_sonar_message[4], "%+09i,", sonar_distance);

            fast_logger->log_cstring(buffer_sonar_message);

            for (size_t i=0; i<64; i++){
                buffer_sonar_message[i] = '\0';
            }

            buffer_sonar_message[0] = 'C';
            buffer_sonar_message[1] = 'F';
            buffer_sonar_message[2] = 'D';
            buffer_sonar_message[3] = ':';

            sprintf(&buffer_sonar_message[4], "%+09i,", sonar_confidence);

            fast_logger->log_cstring(buffer_sonar_message);
        }
        // *

        buffer_status_messages[0] = 'e';  // msg flag end
        buffer_status_messages[1] = 's';
        buffer_status_messages[2] = 'n';
        buffer_status_messages[3] = 'r';
        buffer_status_messages[4] = ':';
        buffer_status_messages[5] = '\0';

        fast_logger->log_cstring(buffer_status_messages);

        } else {
            if (use_serial_debug){
                Serial.println(F("get sonar profile fail"));
                // TODO: remove
                Serial.flush();
            }
        }
    }
    else{
        if (use_serial_debug){
            Serial.println(F("non working sonar"));
        }
    }
}
