#ifndef TEMPERATURE_SENSOR_HEADER

#include <Wire.h>

#include "TSYS01.h"

class TSYS01Manager{
    public:
        void start_sensors(void);
        bool is_available(void);
        char * get_message(void);
    
    private:
        char buffer_message[16];
        unsigned long time_last_measurement;
};

#endif // !TEMPERATURE_SENSOR_HEADER

// TODO
// have it manage all sensors at the same time?
// make sure no need to wait too long, by checking the time needed through is_available
// know in advance how many
