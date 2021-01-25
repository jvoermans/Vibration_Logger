#ifndef TEMPERATURE_SENSOR_HEADER
#define TEMPERATURE_SENSOR_HEADER

#include <Wire.h>
#include "params.h"

// TODO: the temperature stuff buffer length and writing if a big mess: FIXME!!!
// a solution: log each temperature sensor as a cstring separately
constexpr size_t length_tmp_msg_buffer = 5 + 10 * nbr_temp_sensors + 1;
constexpr size_t extra_length_tmp_msg_buffer = length_tmp_msg_buffer + 100;

constexpr byte TCAADDR = 0x70;

constexpr byte TSYS01_ADDR                      = 0x77; 
constexpr byte TSYS01_RESET                     = 0x1E;
constexpr byte TSYS01_ADC_READ                  = 0x00;
constexpr byte TSYS01_ADC_TEMP_CONV             = 0x48;
constexpr byte TSYS01_PROM_READ                 = 0XA0;

// Manage a set of temperature sensors on I2C
// the temperature sensors are some TSYS01 from BlueOcean
// https://github.com/bluerobotics/BlueRobotics_TSYS01_Library
// they have fixed I2C address, so using an I2C multiplexer to read them
// https://learn.adafruit.com/adafruit-tca9548a-1-to-8-i2c-multiplexer-breakout/wiring-and-test
// this class wraps all the sensors and the multiplexer, in order to provide a single
// message with all the data
class TemperatureSensorsManager{
    public:
        void enable_serial_output(bool enable_serial = true);

        void start_sensors(void);

        void start_new_measurement(void);
        bool is_available(void);
        char * get_message(void);
    
    private:
        unsigned long time_start_measurement_micros = (1 << 31);
        bool serial_output {false};
        char buffer_message[extra_length_tmp_msg_buffer]; // length should depend on number of sensors; add some extra to avoid bad overflows (bad way to do...)
        uint16_t calibration_data[nbr_temp_sensors][8];

        void set_multiplexer_channel(uint8_t channel_nbr);

        void send_i2c_command_start_tmp_sensor(uint8_t tmp_sensor_nbr);
        void send_i2c_command_start_tmp_measurement(uint8_t tmp_sensor_nbr);
        void get_i2c_tmp_sensor_coefficients(uint8_t tmp_sensor_nbr);
        uint32_t get_i2c_tmp_sensor_reading(uint8_t tmp_sensor_nbr);
        float convert_i2c_tmp_reading_to_degrees_celcius(uint32_t reading, uint8_t tmp_sensor_nbr);
};

#endif // !TEMPERATURE_SENSOR_HEADER

// TODO
// have it manage all sensors at the same time? ie go through the multiplexer chip
// make sure no need to wait too long, by checking the time needed through is_available
// know in advance how many sensors used
