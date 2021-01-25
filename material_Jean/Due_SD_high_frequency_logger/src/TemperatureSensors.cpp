#include "TemperatureSensors.h"

// TODO: add the serial debug information

void TemperatureSensorsManager::enable_serial_output(bool enable_serial){
    serial_output = enable_serial;
}

void TemperatureSensorsManager::start_sensors(void){
    if (serial_output){
        Serial.println(F("Start temperature sensors"));
    }

    for (size_t i=0; i<extra_length_tmp_msg_buffer; i++){
        buffer_message[i] = '\0';
    }

    // reset all temperature sensors
    for (size_t i = 0; i < nbr_temp_sensors; i++){
        send_i2c_command_start_tmp_sensor(i);
    }

    delay(10);

    if (serial_output){
        Serial.println(F("get temperature calibration values"));
    }

    // get the calibration coefficients of each sensor
    for (size_t i = 0; i < nbr_temp_sensors; i++){
        get_i2c_tmp_sensor_coefficients(i);
    }

    delay(10);

    // prepare the message buffer
    buffer_message[0] = 'T';
    buffer_message[1] = 'M';
    buffer_message[2] = 'P';
    buffer_message[3] = ',';

    buffer_message[length_tmp_msg_buffer - 1] = '\0';

    // set the reference time for start of measurements and start first measurement
    time_start_measurement_micros = micros() - duration_reading_micros - 1;
    start_new_measurement();
}

void TemperatureSensorsManager::start_new_measurement(void){
    // if enough tie since last measurement, start a new one
    if (micros() - time_start_measurement_micros > duration_reading_micros){

        if (serial_output){
            Serial.println(F("start new measurement"));
        }

        time_start_measurement_micros += duration_reading_micros;

        // ask for a new measurement on each of the sensors
        for (uint8_t channel_nbr = 0; channel_nbr < nbr_temp_sensors; channel_nbr++){
            send_i2c_command_start_tmp_measurement(channel_nbr);
        }
    }
}

bool TemperatureSensorsManager::is_available(void){
    if (micros() - time_start_measurement_micros > duration_reading_micros){
        return true;
    }
    else{
        return false;
    }
}

char * TemperatureSensorsManager::get_message(void){
    if (is_available()){
        if (serial_output){
            Serial.println(F("update temperature message"));
        }

        // loop over channels
        // TODO: log each temperature sensor as a cstring separately
        for (size_t crrt_channel = 0; crrt_channel < nbr_temp_sensors; crrt_channel++){
            // get reading
            uint32_t crrt_reading = get_i2c_tmp_sensor_reading(crrt_channel);

            // convert to temperature
            float crrt_temperature = convert_i2c_tmp_reading_to_degrees_celcius(crrt_reading, crrt_channel);

            // add to message
            // TODO: what if negative?...
            size_t position_start_message = 4 + (2 + 1 + 2 + 1) * crrt_channel;
            sprintf(&(buffer_message[position_start_message]), "%05.2f,", crrt_temperature);
            buffer_message[4 + (2 + 1 + 2 + 1) * (crrt_channel + 1)] = '\0';
        }
    }

    buffer_message[length_tmp_msg_buffer - 2] = '\0';

    if (serial_output){
        Serial.println(buffer_message);
    }

    return buffer_message;
}

void TemperatureSensorsManager::set_multiplexer_channel(uint8_t channel_nbr){
    if (channel_nbr > 7){
        return;
    }

    Wire.beginTransmission(TCAADDR);
    Wire.write(1 << channel_nbr);
    Wire.endTransmission();

    delay(2);
}

void TemperatureSensorsManager::send_i2c_command_start_tmp_sensor(uint8_t tmp_sensor_nbr){
    set_multiplexer_channel(tmp_sensor_nbr);

	Wire.beginTransmission(TSYS01_ADDR);
	Wire.write(TSYS01_RESET);
	Wire.endTransmission();
}

void TemperatureSensorsManager::send_i2c_command_start_tmp_measurement(uint8_t tmp_sensor_nbr){
    set_multiplexer_channel(tmp_sensor_nbr);

    Wire.beginTransmission(TSYS01_ADDR);
	Wire.write(TSYS01_ADC_TEMP_CONV);
	Wire.endTransmission();
}

void TemperatureSensorsManager::get_i2c_tmp_sensor_coefficients(uint8_t tmp_sensor_nbr){
    set_multiplexer_channel(tmp_sensor_nbr);

    for (uint8_t i = 0; i < 8; i++ ) {
		Wire.beginTransmission(TSYS01_ADDR);
		Wire.write(TSYS01_PROM_READ + i*2);
		Wire.endTransmission();
        delay(2);

		Wire.requestFrom(TSYS01_ADDR, 2);

		calibration_data[tmp_sensor_nbr][i] = (
            (Wire.read() << 8) | Wire.read()
        );
	}
}

uint32_t TemperatureSensorsManager::get_i2c_tmp_sensor_reading(uint8_t tmp_sensor_nbr){
    uint32_t result = 0;

    set_multiplexer_channel(tmp_sensor_nbr);

    Wire.beginTransmission(TSYS01_ADDR);
	Wire.write(TSYS01_ADC_READ);
	Wire.endTransmission();

	Wire.requestFrom(TSYS01_ADDR, 3);

	result = Wire.read();
	result = (result << 8) | Wire.read();
	result = (result << 8) | Wire.read();

    return result;
}

float TemperatureSensorsManager::convert_i2c_tmp_reading_to_degrees_celcius(uint32_t reading, uint8_t tmp_sensor_nbr){
    float tmp_value = 0;

    uint32_t adc_16 = reading / 256;

    tmp_value = (-2) * static_cast<float>(calibration_data[tmp_sensor_nbr][1]) / 1000000000000000000000.0f * pow(adc_16, 4) + 
                4 * static_cast<float>(calibration_data[tmp_sensor_nbr][2]) / 10000000000000000.0f * pow(adc_16, 3) +
                (-2) * static_cast<float>(calibration_data[tmp_sensor_nbr][3]) / 100000000000.0f * pow(adc_16, 2) +
                1 * static_cast<float>(calibration_data[tmp_sensor_nbr][4]) / 1000000.0f * adc_16 +
                (-1.5) * static_cast<float>(calibration_data[tmp_sensor_nbr][5]) / 100 ;

    return tmp_value;
}
