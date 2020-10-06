// adapted from : https://forum.arduino.cc/index.php?topic=589213.0

// ---------------------------------------------------------------------
// interrupt driven ADC convertion on n channels for Arduino Due
// ---------------------------------------------------------------------

// sample rate in Hz
constexpr int SampleRate = 10;

// size of the data buffers "in time"
// i.e. how many consecutive measurements we buffer for each channel
constexpr size_t SizeBuffer = 5;

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
constexpr uint8_t channels[] = {7, 6, 5, 4, 3};
constexpr int nchannels = sizeof(channels);
volatile uint16_t Buf[SizeBuffer][nchannels];

// whether a full vector of conversions is available
volatile bool FlagConversion = false;
// where we are on the SizeBuffer filling
volatile size_t CrrtBufferFilling = 0;

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
      for (size_t j = 0; j < SizeBuffer; j++)
      {
        Serial.print(" ADC ");
        Serial.print(channels[i]);
        Serial.print(" = ");
        Serial.print(Buf[j][i]);
        Serial.println();
      }
    }
  }
}

// set the ADC to:
// be triggered by the clock rising edge
// perform ADC conversion on several channels in a row
// report finished conversion using ADC interrupt
// this implies that the ADC is used and analogRead etc should not be called otherwise this stops
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
// this uses Time Counter 0 channel 2
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
      Buf[CrrtBufferFilling][i] = static_cast<volatile uint16_t>( * (ADC->ADC_CDR + channels[i]) & 0x0FFFF );
  }

  CrrtBufferFilling = (CrrtBufferFilling + 1) % SizeBuffer;

  if 
  FlagConversion = true;
}


// TODO:
// take care of:
// instead of a flag, use an int to say when to read and where; -1: do not read; >0: where to start reading.
// put conversion flag when the half buffer has been filled
// provide some function to access the current first index of the buffer to read

// TODO:
// make naming conventions homogeneous

// TODO:
// consider wrapping in a class, or at least moving to a separate file

// TODO:
// test / check that able to successfully run and log channels

// TODO:
// add writing to SD card
// https://forum.arduino.cc/index.php?topic=462059.0
// https://forum.arduino.cc/index.php?topic=229731.0
// https://forum.arduino.cc/index.php?topic=232914.msg1679019#msg1679019