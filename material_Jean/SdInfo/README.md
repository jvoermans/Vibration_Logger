A simple example of timed ADC reading on several channels:

- use a hardware timer to create rising edge to trigger ADC conversion
- perform ADC conversion on several multiplexed channels one after the other and report finished conversion with ADC interrupt
- use ADC ISR to grab the ADC reading

This should enable reliable, high frequency ADC readings.