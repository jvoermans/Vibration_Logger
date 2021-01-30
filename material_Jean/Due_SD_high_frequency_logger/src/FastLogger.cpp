#include <FastLogger.h>

////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////

volatile bool blocks_to_write[nbr_blocks_per_adc_channel] = {false};

volatile int crrt_adc_block_index_to_write = 0;
volatile int crrt_adc_data_index_to_write = 0;

volatile BlockADCWithMetadata blocks_adc_with_metdata[nbr_of_adc_channels][nbr_blocks_per_adc_channel];

TimeSeriesAnalyzer analyzers_adc_channels[nbr_of_adc_channels];
char timeseries_buffer_stats_dump[256];

void setup_adc_buffer_metadata()
{
    for (size_t crrt_adc_channel_index = 0; crrt_adc_channel_index < nbr_of_adc_channels; crrt_adc_channel_index++)
    {
        unsigned long int crrt_micros = micros();
        blocks_adc_with_metdata[crrt_adc_channel_index][0].metadata.micros_start = crrt_micros;

        for (size_t crrt_adc_block_index = 0; crrt_adc_block_index < nbr_blocks_per_adc_channel; crrt_adc_block_index++)
        {
            blocks_adc_with_metdata[crrt_adc_channel_index][crrt_adc_block_index].metadata.metadata_id = static_cast<uint16_t>('A');
            blocks_adc_with_metdata[crrt_adc_channel_index][crrt_adc_block_index].metadata.block_number = static_cast<uint16_t>(crrt_adc_channel_index);
        }
    }
}

void adc_setup()
{

    PMC->PMC_PCER1 |= PMC_PCER1_PID37;      // ADC power on
    ADC->ADC_CR = ADC_CR_SWRST;             // Reset ADC
    ADC->ADC_MR |= ADC_MR_TRGEN_EN |        // Hardware trigger select
                   ADC_MR_PRESCAL(adc_prescale) |    // the pre-scaler: as high as possible for better accuracy, while still fast enough to measure everything
                                            // see: https://arduino.stackexchange.com/questions/12723/how-to-slow-adc-clock-speed-to-1mhz-on-arduino-due
                                            // unclear, asked: https://stackoverflow.com/questions/64243073/setting-right-adc-prescaler-on-the-arduino-due-in-timer-and-interrupt-driven-mul
                                            // see the Due_ADC_reading sketch for more details, CCL was use ps 2 at 100kHz with 5 channels,  20 at 10kHz, 200 at 1kHz
                   ADC_MR_TRGSEL_ADC_TRIG3; // Trigger by TIOA2 Rising edge

    ADC->ADC_IDR = ~(0ul);
    ADC->ADC_CHDR = ~(0ul);
    for (int i = 0; i < nbr_of_adc_channels; i++)
    {
        ADC->ADC_CHER |= ADC_CHER_CH0 << adc_channels[i];
    }
    ADC->ADC_IER |= ADC_IER_EOC0 << adc_channels[nbr_of_adc_channels - 1];
    ADC->ADC_PTCR |= ADC_PTCR_RXTDIS | ADC_PTCR_TXTDIS; // Disable PDC DMA
    NVIC_EnableIRQ(ADC_IRQn);                           // Enable ADC interrupt
}

void tc_setup()
{
    PMC->PMC_PCER0 |= PMC_PCER0_PID29;                     // TC2 power ON : Timer Counter 0 channel 2 IS TC2
    TC0->TC_CHANNEL[2].TC_CMR = TC_CMR_TCCLKS_TIMER_CLOCK2 // clock 2 has frequency MCK/8, clk on rising edge
                                | TC_CMR_WAVE              // Waveform mode
                                | TC_CMR_WAVSEL_UP_RC      // UP mode with automatic trigger on RC Compare
                                | TC_CMR_ACPA_CLEAR        // Clear TIOA2 on RA compare match
                                | TC_CMR_ACPC_SET;         // Set TIOA2 on RC compare match

    constexpr int ticks_per_sample = F_CPU / 8 / adc_sampling_frequency; // F_CPU / 8 is the timer clock frequency, see MCK/8 setup
    constexpr int ticks_duty_cycle = ticks_per_sample / 2;               // duty rate up vs down ticks over timer cycle; use 50%
    TC0->TC_CHANNEL[2].TC_RC = ticks_per_sample;
    TC0->TC_CHANNEL[2].TC_RA = ticks_duty_cycle;

    TC0->TC_CHANNEL[2].TC_CCR = TC_CCR_SWTRG | TC_CCR_CLKEN; // Software trigger TC2 counter and enable
}

void ADC_Handler()
{
    for (size_t crrt_adc_channel = 0; crrt_adc_channel < nbr_of_adc_channels; crrt_adc_channel++)
    {
        uint16_t crrt_value = static_cast<volatile uint16_t>(*(ADC->ADC_CDR + adc_channels[crrt_adc_channel]) & 0x0FFFF);
        analyzers_adc_channels[crrt_adc_channel].register_value(static_cast<int>(crrt_value));
        blocks_adc_with_metdata[crrt_adc_channel][crrt_adc_block_index_to_write].data[crrt_adc_data_index_to_write] = crrt_value;
    }

    if (crrt_adc_data_index_to_write == 0){
        unsigned long crrt_micros = micros();

        for (size_t crrt_adc_channel = 0; crrt_adc_channel < nbr_of_adc_channels; crrt_adc_channel++)
        {
            blocks_adc_with_metdata[crrt_adc_channel][crrt_adc_block_index_to_write].metadata.micros_start = crrt_micros;
        }
    }

    crrt_adc_data_index_to_write += 1;

    if (crrt_adc_data_index_to_write == nbr_adc_measurements_per_block)
    {
        crrt_adc_data_index_to_write = 0;
        blocks_to_write[crrt_adc_block_index_to_write] = true;

        unsigned long crrt_micros = micros();

        for (size_t crrt_adc_channel = 0; crrt_adc_channel < nbr_of_adc_channels; crrt_adc_channel++)
        {
            blocks_adc_with_metdata[crrt_adc_channel][crrt_adc_block_index_to_write].metadata.micros_end = crrt_micros;
        }

        crrt_adc_block_index_to_write = (crrt_adc_block_index_to_write + 1) % nbr_blocks_per_adc_channel;
    }
}

////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////

// the statistics class

void TimeSeriesAnalyzer::init(void){
    flag_stats_available = false;
    crrt_nbr_registered_values = 0;
    reset_filling_stats();
}

bool TimeSeriesAnalyzer::stats_are_available(void) const{
    return flag_stats_available;
}

TimeSeriesStatistics const & TimeSeriesAnalyzer::get_stats(void){
    flag_stats_available = false;
    return available_stats;
}

void TimeSeriesAnalyzer::register_value(int value_in){
    crrt_nbr_registered_values += 1;

    double value_in_dbl = static_cast<double>(value_in - middle_adc_value);

    // update the current working structure
    crrt_filling_stats.mean += value_in_dbl / static_cast<double>(nbr_of_samples_per_analysis);
    crrt_filling_stats.mean_of_square += value_in_dbl * value_in_dbl / static_cast<double>(nbr_of_samples_per_analysis);
    crrt_filling_stats.max = max(value_in_dbl, crrt_filling_stats.max);
    crrt_filling_stats.min = min(value_in_dbl, crrt_filling_stats.min);

    if ( (value_in_dbl > threshold_high) || (value_in_dbl < threshold_low) ){
        crrt_filling_stats.extremal_count += 1;
    }

    // what to do if finished with the current working structure
    if (crrt_nbr_registered_values >= nbr_of_samples_per_analysis){
        // copy the crrt struct to the output one
        available_stats.mean = crrt_filling_stats.mean;
        available_stats.mean_of_square = crrt_filling_stats.mean_of_square;
        available_stats.max = crrt_filling_stats.max;
        available_stats.min = crrt_filling_stats.min;
        available_stats.extremal_count = crrt_filling_stats.extremal_count;

        // reset all crrt analysis values
        reset_filling_stats();
        crrt_nbr_registered_values = 0;

        // stats are available now
        flag_stats_available = true;
    }
}

void TimeSeriesAnalyzer::reset_filling_stats(void){
    crrt_filling_stats.mean = 0;
    crrt_filling_stats.mean_of_square = 0;
    crrt_filling_stats.max = -999999.0;
    crrt_filling_stats.min = 999999.0;
    crrt_filling_stats.extremal_count = 0;
}

void append_buffer(char * main_buffer, char * to_add_buffer, int & pos_in_main_buffer){
    strcpy(&main_buffer[pos_in_main_buffer], to_add_buffer);
    pos_in_main_buffer += strlen(to_add_buffer);
}

int write_statistics(char * buffer, TimeSeriesStatistics const & to_dump){
    char crrt_writeout[128];
    for (size_t i=0; i<128; i++){
        crrt_writeout[i] = '\0';
    }
    int crrt_buffer_pos_to_write = 0;

    sprintf(crrt_writeout, "%.3E,", to_dump.mean);
    append_buffer(buffer, crrt_writeout, crrt_buffer_pos_to_write);

    sprintf(crrt_writeout, "%.3E,", to_dump.mean_of_square);
    append_buffer(buffer, crrt_writeout, crrt_buffer_pos_to_write);

    sprintf(crrt_writeout, "%.3E,", to_dump.max);
    append_buffer(buffer, crrt_writeout, crrt_buffer_pos_to_write);

    sprintf(crrt_writeout, "%.3E,", to_dump.min);
    append_buffer(buffer, crrt_writeout, crrt_buffer_pos_to_write);

    sprintf(crrt_writeout, "%.3E", static_cast<double>(to_dump.extremal_count));
    append_buffer(buffer, crrt_writeout, crrt_buffer_pos_to_write);

    return crrt_buffer_pos_to_write;
}

////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////

// the wrapper class stuff

bool FastLogger::start_recording()
{
    // prepare all analyzers
    for (size_t crrt_channel = 0; crrt_channel < nbr_of_adc_channels; crrt_channel++){
        analyzers_adc_channels[crrt_channel].init();
    }

    // setup the metadata in char data
    for (size_t crrt_char_block = 0; crrt_char_block < nbr_blocks_char; crrt_char_block++){
        blocks_cstring_with_metadata[crrt_char_block].metadata.metadata_id = 'C';
        blocks_cstring_with_metadata[crrt_char_block].metadata.block_number = crrt_char_block;
    }

    // setup the SD card
    const uint8_t SD_CS_PIN = sd_card_select_pin;
    SdSpiConfig sd_config{SD_CS_PIN, DEDICATED_SPI, SD_SCK_MHZ(25)};

    if (sd_is_active){
        while (!sd_object.begin(sd_config))
        {
            sd_object.initErrorHalt(&Serial);
            delay(100);  // enough to force re-start
        }
    }

    delay(5);

    // create a new file
    while (!open_new_file()){
        delay(100);
    }

    // set the timer and ADC
    setup_adc_buffer_metadata();
    adc_setup();
    tc_setup();

    logging_is_active = true;

    return true;
}

bool FastLogger::stop_recording()
{
    logging_is_active = false;
    close_crrt_file();

    return true;
}

bool FastLogger::is_active(){
    return logging_is_active;
}

void FastLogger::log_char(const char crrt_char)
{
    // add to char buffer
    blocks_cstring_with_metadata[crrt_char_block_index_to_write].data[crrt_char_data_index_to_write] = crrt_char;
    crrt_char_data_index_to_write += 1;

    // if end of block, update metadata, write to SD, update indexes etc
    if (crrt_char_data_index_to_write >= nbr_chars_per_block)
    {
        if (serial_debug_output_is_active){
            Serial.println(F("chars dump"));
        }

        unsigned long int crrt_micros = micros();
        blocks_cstring_with_metadata[crrt_char_block_index_to_write].metadata.micros_end = crrt_micros;

        crrt_char_data_index_to_write = 0;
        write_block_to_sd_card(&blocks_cstring_with_metadata[crrt_char_block_index_to_write]);

        crrt_char_block_index_to_write = (crrt_char_block_index_to_write + 1) % nbr_blocks_char;
        blocks_cstring_with_metadata[crrt_char_block_index_to_write].metadata.micros_start = crrt_micros;
    }
}

void FastLogger::log_cstring(const char *cstring)
{
    // log the time
    unsigned long crrt_micros = micros();

    log_char('M');

    char micros_timestamp[32];
    for (size_t i=0; i<32; i++){
        micros_timestamp[i] = '\0';
    }
    sprintf(micros_timestamp, "%010lu", crrt_micros);
    for (int i = 0; i < 10; i++)
    {
        log_char(micros_timestamp[i]);
    }

    log_char(';');

    // log the data string
    size_t i = 0;
    while (cstring[i] != '\0')
    {
        // put a max length to the string, in case the string in is not 0 terminated
        if (i > 1024){
            break;
        }

        char crrt_char = cstring[i];

        if (crrt_char == ';'){
            crrt_char = ':';
        }

        log_char(crrt_char);
        i++;
    }

    log_char(';');
}

void FastLogger::internal_update(){
    if (logging_is_active)
    {
        // check if some data to write from the ADC
        // for this check if some readily available data in blocks, starting at the next one to be over-written, checking
        // until the current block being written non included to avoid overlapping read and writes
        for (size_t offset_block_index = 0; offset_block_index < nbr_blocks_per_adc_channel - 1; offset_block_index++)
        {
            int index_to_examine = (crrt_adc_block_index_to_write + 1 + offset_block_index) % nbr_blocks_per_adc_channel;

            if (blocks_to_write[index_to_examine])
            {
                if (serial_debug_output_is_active)
                {
                    Serial.println(F("ADC dump"));
                }
                blocks_to_write[index_to_examine] = false;
                write_adc_blocks_to_sd_card(index_to_examine);
            }
        }

        // check if some stats data to write
        for (size_t crrt_channel = 0; crrt_channel < nbr_of_adc_channels; crrt_channel++){
            if (analyzers_adc_channels[crrt_channel].stats_are_available()){
                if (serial_debug_output_is_active){
                    Serial.println(F("stats avail"));
                }

                // post the current stats
                TimeSeriesStatistics const & crrt_stats = analyzers_adc_channels[crrt_channel].get_stats();

                for (size_t i=0; i<256; i++){
                    timeseries_buffer_stats_dump[i] = '\0';
                }

                // packet information
                timeseries_buffer_stats_dump[0] = 'S';
                timeseries_buffer_stats_dump[1] = 'T';
                timeseries_buffer_stats_dump[2] = 'A';
                timeseries_buffer_stats_dump[3] = 'T';
                char crrt_chnl[32];
                for (size_t i=0; i<32; i++){
                    crrt_chnl[i] = '\0';
                }
                sprintf(crrt_chnl, "%02i", crrt_channel);
                timeseries_buffer_stats_dump[4] = crrt_chnl[0];
                timeseries_buffer_stats_dump[5] = crrt_chnl[1];
                timeseries_buffer_stats_dump[6] = ',';

                // where to start writing the stats information
                int nbr_chars_written = 0;

                nbr_chars_written = write_statistics(&timeseries_buffer_stats_dump[7], crrt_stats);

                timeseries_buffer_stats_dump[7 + nbr_chars_written + 1] = '\0';

                log_cstring(timeseries_buffer_stats_dump);

                if (serial_debug_output_is_active){
                    Serial.println(timeseries_buffer_stats_dump);
                }
            }
        }

        // check if should use new file
        if (need_new_file())
        {
            close_crrt_file();
            while (!open_new_file()){
                delay(100);
            }
        }
    }
}

void FastLogger::enable_serial_debug_output()
{
    serial_debug_output_is_active = true;
    Serial.println(F("FastLogger enable serial debug"));
}

void FastLogger::disable_SD(){
    sd_is_active = false;
}

bool FastLogger::write_block_to_sd_card(void *block_start)
{
    if (serial_debug_output_is_active){
        for (size_t crrt_byte = 0; crrt_byte<20; crrt_byte++){
            Serial.print(static_cast<uint8_t *>(block_start)[crrt_byte], HEX);
            Serial.print(" ");
        }
        Serial.println();
    }

    if (sd_is_active){
        if (binary_file.write(block_start, 512) != 512)
        {
            if (serial_debug_output_is_active)
            {
                Serial.println(F("problem writing block"));
            }
            return false;
        }
    }

    return true;
}

bool FastLogger::write_adc_blocks_to_sd_card(int adc_blocks_index)
{
    for (int crrt_adc_ind = 0; crrt_adc_ind < nbr_of_adc_channels; crrt_adc_ind++)
    {
        write_block_to_sd_card(
            const_cast<void *>(
                static_cast<volatile void *>(
                    &blocks_adc_with_metdata[crrt_adc_ind][adc_blocks_index])));
    }

    return true;
}

bool FastLogger::open_new_file()
{
    // generate the right filename and increment future filename
    uint32_t file_number;
    file_number = persistent_filenumber.get_file_number();
    persistent_filenumber.increment_file_number();

    // TODO: check that this is ok, possibly needs %08d instead
    sprintf(filename, "F%08lu.bin", file_number);

    if (serial_debug_output_is_active)
    {
        Serial.print(F("new filename "));
        Serial.println(filename);
    }

    if (sd_is_active){
        if (sd_object.exists(filename))
        {
            if (serial_debug_output_is_active)
            {
                Serial.println(F("file already exists"));
            }
            return false;
        }

        // create and pre-allocate the file
        if (!binary_file.open(filename, O_RDWR | O_CREAT))
        {
            if (serial_debug_output_is_active)
            {
                Serial.println(F("cannot open file"));
            }
            return false;
        }

        if (!binary_file.preAllocate(preallocate_size))
        {
            if (serial_debug_output_is_active)
            {
                Serial.println(F("cannot pre-allocate file"));
            }
            return false;
        }
    }

    // keep track of start of file time
    time_opening_crrt_file = micros();

    return true;
}

bool FastLogger::close_crrt_file()
{
    if (serial_debug_output_is_active)
    {
        Serial.println(F("close crrt file"));
    }

    if (sd_is_active){
        if (!binary_file.close())
        {
            if (serial_debug_output_is_active)
            {
                Serial.println(F("cannot close file"));
            }
            return false;
        }
    }

    return true;
}

bool FastLogger::need_new_file()
{
    if (logging_is_active && (micros() - time_opening_crrt_file > file_duration_microseconds))
    {
        if (serial_debug_output_is_active)
        {
            Serial.println(F("need new file"));
        }
        return true;
    }
    else
    {
        return false;
    }
}
