# include "GPS_manager.h"

volatile bool pps_read_available = false;
volatile unsigned long pps_micros;

void ISR_pps(void){
    pps_read_available = true;
    pps_micros = micros();
}

bool GPSManager::pps_available(void){
    if (pps_read_available){
        sprintf(&pps_message_buffer[4], "%010lu", pps_micros);
    }
    return pps_read_available;
}

char * GPSManager::get_pps_message(void){
    if (use_serial_debug_output && pps_read_available){
        Serial.println(pps_message_buffer);
    }
    pps_read_available = false;
    return pps_message_buffer;
}

void GPSManager::start_gps(void){
    // TODO: init all buffers to \0

    adafruit_gps = Adafruit_GPS(serial_gps);
    adafruit_gps.begin(9600);

    // instance_GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
    // uncomment this line to turn on only the "minimum recommended" data
    adafruit_gps.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCONLY);

    // Set the update rate
    adafruit_gps.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);   // 1 Hz update rate
    adafruit_gps.sendCommand(PMTK_API_SET_FIX_CTL_1HZ);

    // prepare the PPS pin
    pinMode(pps_pin, INPUT);
    attachInterrupt(digitalPinToInterrupt(pps_pin), ISR_pps, RISING);

    pps_message_buffer[0] = 'P';
    pps_message_buffer[1] = 'P';
    pps_message_buffer[2] = 'S';
    pps_message_buffer[3] = ':';
    pps_message_buffer[14] = ';';
    pps_message_buffer[15] = '\0';

    buffer_tail = 0;
    message_is_available = false;
}

void GPSManager::update_status(void){
    if (!message_is_available){
        while (serial_gps->available() > 0){
            if (buffer_tail < size_message_buffer){
                char crrt_char = serial_gps->read();
                message_buffer[buffer_tail] = crrt_char;
                buffer_tail += 1;

                if (crrt_char == '\n'){
                    message_is_available = true;
                }
            } 
        }
    }
}

bool GPSManager::message_available(void){
    if (use_serial_debug_output && message_is_available){
        Serial.print(message_buffer);
    }

    return message_is_available;
}

void GPSManager::enable_serial_debug_output(void){
    use_serial_debug_output = true;
}

char * GPSManager::get_message(void){
    buffer_tail = 0;
    message_is_available = false;
    
    return message_buffer;
}