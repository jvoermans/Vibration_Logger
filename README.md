**Discontinued!!**: the content of this repository is mostly discontinued / old and untidy scripts and code that were used during development (though some of the code and scripts and discussions in issue may still be of interest for people looking for more insights in the development process, how the design decisions were made, etc). We **strongly recommend** to look at the updated and curated repository instead: https://github.com/jvoermans/Geophone_Logger .

---

Aim is to build a logger to measure sea ice vibrations induced by ice cracking/breaking.
Vibrations are (based on literature in the 80ies) in the 50-200Hz frequency range.

To record at high sampling rate (1000Hz), we use the LowLatencyLogger sketch from Greiman (https://github.com/greiman/SdFat/tree/master/examples/LowLatencyLogger)
Arduino Due is used as it has 12-bit ADC's and is faster than other Arduino boards.

TO-DO:
- Add I2C sensors (BlueRobotics Temperature, Pressure, Sonar), [conceptual sketch works]
- Add GPS Ultimate breakout
- Iridium modem

Problems right now:
- GPS doesn't work with my LowLatencyLogger sketch

Comments:
- ADS1115 (adafruit 16-bit ADC) has been tested to increase resolution, but is not fast enough to record at 1000Hz, their 12-bit version might be fast enough to use a Mega instead of Due

















