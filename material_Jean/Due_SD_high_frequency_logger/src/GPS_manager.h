// start
// set serial port rx
// set PPS pin: any pin on the due
// all messages are cstrings
// function message_available, including PPS
// get pointer to available message
// TODO: make sure larger buffer on GPS serial receive

// available_message: check PPS, check serial, generate message, return bool
// get_message: pointer to the cstring message, anc reset
// have an internal "ready to send message" cstring buffer, length 256 bytes

#ifndef GPS_MANAGER
#define GPS_MANAGER

#include "params.h"
#include "Adafruit_GPS.h"

// make sure buffers are big enough
// if necessary, modify in the core
static_assert(SERIAL_BUFFER_SIZE >= 512);

class GPSManager{
    public:
        void start_gps(void);

        void update_status(void);

        bool message_available(void);

        char * get_message(void);

        bool pps_available(void);

        char * get_pps_message(void);

        void enable_serial_debug_output(void);

    private:
        bool use_serial_debug_output = false;
        static constexpr int size_message_buffer = 1024;
        char message_buffer[size_message_buffer];
        static constexpr int size_pps_message_buffer = 3 + 1 + 10 + 2 + 16 + 32;
        char pps_message_buffer[size_pps_message_buffer];
        bool message_is_available = false;
        size_t buffer_tail;
        HardwareSerial * serial_gps = selected_gps_serial;
        uint8_t pps_pin = selected_PPS_digital_pin;
        Adafruit_GPS adafruit_gps;
};

void ISR_pps(void);
extern volatile bool pps_read_available;
extern volatile unsigned long pps_micros;

#endif // !GPS_MANAGER
