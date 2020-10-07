// this is for Arduino Due only!

// the interrupt based ADC measurement is adapted from : https://forum.arduino.cc/index.php?topic=589213.0
// i.e. adc_setup(), tc_setup(), ADC_handler()

// some notes about SD cards
// a nice testing: https://jitter.company/blog/2019/07/31/microsd-performance-on-memory-constrained-devices/
// some in depth intro: https://www.parallax.com/sites/default/files/downloads/AN006-SD-FFS-Drivers-v1.0.pdf

// ---------------------------------------------------------------------
// interrupt driven ADC convertion on n adc_channels for Arduino Due
// ---------------------------------------------------------------------

#include "Arduino.h" // make my linter happy

// sample rate in Hz
constexpr int adc_sample_rate = 1;

// size of the data buffers "in time"
// i.e. how many consecutive measurements we buffer for each channel
constexpr size_t adc_buffer_nbr_consec_meas= 5;

// the adc_channels to read, in uC reference, NOT in Arduino Due pinout reference
// for a mapping, see: https://components101.com/microcontrollers/arduino-due
// i.e. A0 is AD7
//      A1    AD6
//      A2    AD5
//      A3    AD4
//      A4    AD3
//      A5    AD2
//      A6    AD1
//      A7    AD0
constexpr uint8_t adc_channels[] = {7, 6, 5, 4, 3};
constexpr int nbr_adc_channels = sizeof(adc_channels);

// the buffer containing the measurements for all adc_channels over several measurements in time
volatile uint16_t adc_meas_buffer[adc_buffer_nbr_consec_meas][nbr_adc_channels];

// flag when a full vector of conversions is available
volatile bool adc_flag_conversion = false;

// time index of the current measurement in the adc reads buffer
volatile size_t crrt_adc_meas_buffer_idx = 0;

void setup()
{
  Serial.begin(115200);
  adc_setup();
  tc_setup();
}

void loop()
{
  if (adc_flag_conversion == true)
  {
    adc_flag_conversion = false;

    Serial.println(F("ADC avail at uS:"));
    Serial.println(micros());
    Serial.println(F("updated reading idx:"));
    Serial.println((crrt_adc_meas_buffer_idx - 1) % adc_buffer_nbr_consec_meas);

    for (size_t i = 0; i < nbr_adc_channels; i++)
    {
      Serial.print(F(" ADC "));
      Serial.print(adc_channels[i]);
      Serial.println(F(" meas in time:"));

      for (size_t j = 0; j < adc_buffer_nbr_consec_meas; j++)
      {
        Serial.print(adc_meas_buffer[j][i]);
        Serial.print(F(" "));
      }
      Serial.println();
    }
  }
}

// start ADC conversion on rising edge on time counter 0 channel 2
// perform ADC conversion on several adc_channels in a row one after the other
// report finished conversion using ADC interrupt
// TODO: check the value of the pre-scalor
void adc_setup() {
  PMC->PMC_PCER1 |= PMC_PCER1_PID37;                     // ADC power on
  ADC->ADC_CR = ADC_CR_SWRST;                            // Reset ADC
  ADC->ADC_MR |=  ADC_MR_TRGEN_EN |                      // Hardware trigger select
                  ADC_MR_PRESCAL(1) |                    // the pre-scaler: as high as possible for better accuracy, while still fast enough to measure everything
                                                         // see: https://arduino.stackexchange.com/questions/12723/how-to-slow-adc-clock-speed-to-1mhz-on-arduino-due
                  ADC_MR_TRGSEL_ADC_TRIG3;               // Trigger by TIOA2 Rising edge

  ADC->ADC_IDR = ~(0ul);
  ADC->ADC_CHDR = ~(0ul);
  for (int i = 0; i < nbr_adc_channels; i++)
  {
    ADC->ADC_CHER |= ADC_CHER_CH0 << adc_channels[i];
  }
  ADC->ADC_IER |= ADC_IER_EOC0 << adc_channels[nbr_adc_channels - 1];
  ADC->ADC_PTCR |= ADC_PTCR_RXTDIS | ADC_PTCR_TXTDIS;    // Disable PDC DMA
  NVIC_EnableIRQ(ADC_IRQn);                              // Enable ADC interrupt
}

// use time counter 0 channel 2 to generate the ADC start of conversion signal
// i.e. this sets a rising edge with the right frequency for triggering ADC conversions corresponding to adc_sample_rate
// for more information about the timers: https://github.com/ivanseidel/DueTimer/blob/master/TimerCounter.md
// NOTE: TIOA2 should not be available on any due pin https://github.com/ivanseidel/DueTimer/issues/11
void tc_setup() {
  PMC->PMC_PCER0 |= PMC_PCER0_PID29;                       // TC2 power ON : Timer Counter 0 channel 2 IS TC2
  TC0->TC_CHANNEL[2].TC_CMR = TC_CMR_TCCLKS_TIMER_CLOCK2   // clock 2 has frequency MCK/8, clk on rising edge
                              | TC_CMR_WAVE                // Waveform mode
                              | TC_CMR_WAVSEL_UP_RC        // UP mode with automatic trigger on RC Compare
                              | TC_CMR_ACPA_CLEAR          // Clear TIOA2 on RA compare match
                              | TC_CMR_ACPC_SET;           // Set TIOA2 on RC compare match

  constexpr int ticks_per_sample = F_CPU / 8 / adc_sample_rate; // F_CPU / 8 is the timer clock frequency, see MCK/8 setup
  constexpr int ticks_duty_cycle = ticks_per_sample / 2; // duty rate up vs down ticks over timer cycle; use 50%
  TC0->TC_CHANNEL[2].TC_RC = ticks_per_sample;
  TC0->TC_CHANNEL[2].TC_RA = ticks_duty_cycle;

  TC0->TC_CHANNEL[2].TC_CCR = TC_CCR_SWTRG | TC_CCR_CLKEN; // Software trigger TC2 counter and enable
}

// ISR for the ADC ready readout interrupt
// push the current ADC data on all adc_channels to the buffer
// update the time index
// set flag conversion ready
void ADC_Handler() {
  for (size_t i = 0; i < nbr_adc_channels; i++)
  {
      adc_meas_buffer[crrt_adc_meas_buffer_idx][i] = static_cast<volatile uint16_t>( * (ADC->ADC_CDR + adc_channels[i]) & 0x0FFFF );
  }

  crrt_adc_meas_buffer_idx = (crrt_adc_meas_buffer_idx + 1) % adc_buffer_nbr_consec_meas;

  adc_flag_conversion = true;
}

// TODO: test
// get good readings
// no cross talks in ADC conversions
// update is ok across all channels
// works at higher frequency (check 1kHz, 10kHz)

// TODO: 10 bytes header on each 512 bytes SD card segment
// 1 byte type, 1 byte channel number, 4 bytes micros at start, 4 bytes micro at write
// TODO: micro at start should be set from the ADC_handler
// TODO: micro at write should be set at the writing

// TODO:
// take care of:
// instead of a flag, use an int to say when to read and where; -1: do not read; >0: where to start reading.
// put conversion flag when the half buffer has been filled
// provide some function to access the current first index of the buffer to read

// TODO:
// test / check that able to successfully run and log adc_channels

// TODO: write header

// TODO:
// add writing to SD card
// https://forum.arduino.cc/index.php?topic=462059.0
// https://forum.arduino.cc/index.php?topic=229731.0
// https://forum.arduino.cc/index.php?topic=232914.msg1679019#msg1679019