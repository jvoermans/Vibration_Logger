#ifndef SONAR_MANAGER
#define SONAR_MANAGER

#include "params.h"
#include "FastLogger.h"

#include "ping1d.h"

static Ping1D sonar_ping {*selected_sonar_serial};

class SonarManager{
    public:
        void start_sonar(FastLogger * fast_logger, bool use_serial_debug);

        bool ready_to_measure(void);

        void measure_and_log(void);
    private:
        unsigned long time_last_measurement_ms {1UL << 31};
        FastLogger * fast_logger;
        bool use_serial_debug {false};
        bool working_sonar {false};
};


#endif // !SONAR_MANAGER