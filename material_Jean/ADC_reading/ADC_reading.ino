// adapted from : https://forum.arduino.cc/index.php?topic=589213.0

// ---------------------------------------------------------------------
// interrupt driven ADC convertion on n channels for Arduino Due
// ---------------------------------------------------------------------

// sample rate in Hz
// #define SampleRate  (10) // my modif
constexpr int SampleRate = 10;

// the channels to read
// for a mapping, see: https://components101.com/microcontrollers/arduino-due
// i.e. A0 is AD7
//      A1    AD6
//      A2    AD5
//      A3    AD4
//      A4    AD3
//      A5    AD2
//      A6    AD1
//      A7    AD0
constexpr uint8_t channels[] = {7,6,5};
// #define nchannels (sizeof(channels)) // my modif
constexpr int nchannels = sizeof(channels);
// uint16_t Buf[nchannels]; // my modif
volatile uint16_t Buf[nchannels];

// whether a full vector of conversions is available
volatile boolean FlagConversion;

void setup()
{
  Serial.begin(115200);
  adc_setup();
  tc_setup();
}

void loop()
{
  if (FlagConversion == true)
  {
    FlagConversion = false;
    for (int i = 0; i < nchannels; i++)
    {
      printf(" ADC[%d] = %d\n", channels[i], Buf[i]);
    }
  }
}

// set the ADC to:
// be triggered by the clock rising edge
// perform ADC conversion on several channels in a row
// report finished conversion using ADC interrupt
void adc_setup() {

  PMC->PMC_PCER1 |= PMC_PCER1_PID37;                     // ADC power on
  ADC->ADC_CR = ADC_CR_SWRST;                            // Reset ADC
  ADC->ADC_MR |=  ADC_MR_TRGEN_EN |                      // Hardware trigger select
                  ADC_MR_PRESCAL(1) |
                  ADC_MR_TRGSEL_ADC_TRIG3;               // Trigger by TIOA2 Rising edge

  ADC->ADC_IDR = ~(0ul);
  ADC->ADC_CHDR = ~(0ul);
  for (int i = 0; i < nchannels; i++)
  {
    ADC->ADC_CHER |= ADC_CHER_CH0 << channels[i];
  }
  ADC->ADC_IER |= ADC_IER_EOC0 << channels[nchannels - 1];
  ADC->ADC_PTCR |= ADC_PTCR_RXTDIS | ADC_PTCR_TXTDIS;    // Disable PDC DMA
  NVIC_EnableIRQ(ADC_IRQn);                              // Enable ADC interrupt
}

// use time counter 0 channel 2 to generate the ADC start of conversion signal
// i.e. this sets a rising edge with the right frequency for performing the conversions
// for more information about the timers: https://github.com/ivanseidel/DueTimer/blob/master/TimerCounter.md
void tc_setup() {
  PMC->PMC_PCER0 |= PMC_PCER0_PID29;                       // TC2 power ON : Timer Counter 0 channel 2 IS TC2
  TC0->TC_CHANNEL[2].TC_CMR = TC_CMR_TCCLKS_TIMER_CLOCK2   // MCK/8, clk on rising edge
                              | TC_CMR_WAVE                // Waveform mode
                              | TC_CMR_WAVSEL_UP_RC        // UP mode with automatic trigger on RC Compare
                              | TC_CMR_ACPA_CLEAR          // Clear TIOA2 on RA compare match
                              | TC_CMR_ACPC_SET;           // Set TIOA2 on RC compare match

  TC0->TC_CHANNEL[2].TC_RC = F_CPU / 8 / SampleRate; //<*********************  Frequency = (Mck/8)/TC_RC  Hz = 10 Hz
  TC0->TC_CHANNEL[2].TC_RA = 50;  //<********************   Any Duty cycle in between 1 and (TC_RC - 1)

  TC0->TC_CHANNEL[2].TC_CCR = TC_CCR_SWTRG | TC_CCR_CLKEN; // Software trigger TC2 counter and enable
}

void ADC_Handler() {
  for (int i = 0; i < nchannels; i++)
  {
      // Buf[i] = (uint16_t) * (ADC->ADC_CDR + channels[i]); // my modification
      Buf[i] = static_cast<uint16_t *>(ADC->ADC_CDR + channels[i]);
  }
   FlagConversion = true;
}

