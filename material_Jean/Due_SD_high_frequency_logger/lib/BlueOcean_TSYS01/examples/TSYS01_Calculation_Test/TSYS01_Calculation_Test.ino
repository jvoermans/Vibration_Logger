/* Blue Robotics TSYS01 Library Calculation Test Code
-----------------------------------------------------
 
Title: Blue Robotics TSYS01 Library Calculation Test Code

Description: This example is only used to test the conversion calculations
from the part datasheet. It does not actually communicate with the sensor.

The code is designed for the Arduino Uno board and can be compiled and 
uploaded via the Arduino 1.0+ software.

-------------------------------
The MIT License (MIT)

Copyright (c) 2016 Blue Robotics Inc.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
-------------------------------*/

#include <Wire.h>
#include "TSYS01.h"

TSYS01 sensor;

void setup() {
  Serial.begin(9600);
  
  Wire.begin();
}

void loop() {
  sensor.readTestCase();

  Serial.print("Temperature: "); Serial.print(sensor.temperature()); Serial.print(" deg C");

  Serial.print("(Should be 10.58 deg C)"); Serial.println();

  delay(10000);
}
