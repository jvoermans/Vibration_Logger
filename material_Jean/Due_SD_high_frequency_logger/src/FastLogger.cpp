#include <FastLogger.h>

////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////

volatile bool blocks_to_write[nbr_of_adc_channels] = {false};

volatile int crrt_adc_block_index_to_write = 0;
volatile int crrt_adc_data_index_to_write = 0;

volatile BlockADCWithMetadata blocks_adc_with_metdata[nbr_of_adc_channels][nbr_blocks_per_adc_channel];

void setup_adc_buffer_metadata()
{
    for (size_t crrt_adc_channel_index = 0; crrt_adc_channel_index < nbr_of_adc_channels; crrt_adc_channel_index++)
    {
        unsigned long int crrt_micros = micros();
        blocks_adc_with_metdata[crrt_adc_channel_index][0].metadata.micros_start = crrt_micros;

        for (size_t crrt_adc_block_index = 0; crrt_adc_block_index < nbr_blocks_per_adc_channel; crrt_adc_block_index++)
        {
            blocks_adc_with_metdata[crrt_adc_channel_index][crrt_adc_block_index].metadata.metadata_id = static_cast<uint8_t>('A');
            blocks_adc_with_metdata[crrt_adc_channel_index][crrt_adc_block_index].metadata.block_number = static_cast<uint8_t>(crrt_adc_channel_index);
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

// the wrapper class stuff

bool FastLogger::start_recording()
{
    // setup the metadata in char data
    for (size_t crrt_char_block = 0; crrt_char_block < nbr_blocks_char; crrt_char_block++){
        blocks_cstring_with_metadata[crrt_char_block].metadata.metadata_id = 'C';
        blocks_cstring_with_metadata[crrt_char_block].metadata.block_number = crrt_char_block;
    }

    // setup the SD card
    const uint8_t SD_CS_PIN = sd_card_select_pin;
    #define SD_CONFIG SdSpiConfig(SD_CS_PIN, DEDICATED_SPI)

    if (sd_is_active){
        if (!sd_object.begin(SD_CONFIG))
        {
            sd_object.initErrorHalt(&Serial);
        }
    }

    delay(5);

    // create a new file
    open_new_file();

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

    char micros_timestamp[10];
    sprintf(micros_timestamp, "%09lu", crrt_micros);
    for (int i = 0; i < 9; i++)
    {
        log_char(micros_timestamp[i]);
    }

    log_char(';');

    // log the data string
    size_t i = 0;
    while (cstring[i] != '\0')
    {
        log_char(cstring[i]);
        i++;
    }

    log_char(';');
}

void FastLogger::internal_update()
{
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

        // check if should use new file
        if (need_new_file())
        {
            close_crrt_file();
            open_new_file();
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
