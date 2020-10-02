// Test code for Adafruit GPS modules using MTK3329/MTK3339 driver
//
// This code shows how to listen to the GPS module in an interrupt
// which allows the program to have more 'freedom' - just parse
// when a new NMEA sentence is available! Then access data when
// desired.
//
// Tested and works great with the Adafruit Ultimate GPS module
// using MTK33x9 chipset
//    ------> http://www.adafruit.com/products/746
// Pick one up today at the Adafruit electronics shop 
// and help support open source hardware & software! -ada

#include <Adafruit_GPS.h>

// This sketch is ONLY for the Arduino Due!
// You should make the following connections with the Due and GPS module:
// GPS power pin to Arduino Due 3.3V output.
// GPS ground pin to Arduino Due ground.
// For hardware serial 1 (recommended):
//   GPS TX to Arduino Due Serial1 RX pin 19
//   GPS RX to Arduino Due Serial1 TX pin 18
#define mySerial Serial1

Adafruit_GPS GPS(&mySerial);


// Set GPSECHO to 'false' to turn off echoing the GPS data to the Serial console
// Set to 'true' if you want to debug and listen to the raw GPS sentences. 
#define GPSECHO  false

// this keeps track of whether we're using the interrupt
// off by default!
boolean usingInterrupt = false;
void useInterrupt(boolean); // Func prototype keeps Arduino 0023 happy


int conn = 0;
int no_conn = 0;
int count = 0;
int count_ave = 0;
float tT1;
float tT2;
float tP1;
float T1;
float T1min;
float T1max;
float T2;
float P1;

#include <Arduino.h>
#include <Wire.h>
#include "TSYS01.h"
TSYS01 sensorT;
#include "MS5837.h"
MS5837 sensorP;

void TCA9548A(uint8_t bus)
{
  Wire.beginTransmission(0x70);  // TCA9548A address is 0x70
  Wire.write(1 << bus);          // send byte to select bus
  Wire.endTransmission();
}


#include <SPI.h>
#include <SD.h>

unsigned long timecounter;
File myFile;


void setup()  
{
    
  // connect at 115200 so we can read the GPS fast enough and echo without dropping chars
  // also spit it out
  Serial.begin(115200);
  Serial.println("Adafruit GPS library basic test!");

  // 9600 NMEA is the default baud rate for Adafruit MTK GPS's- some use 4800
  GPS.begin(9600);
  mySerial.begin(9600);
  
  // uncomment this line to turn on RMC (recommended minimum) and GGA (fix data) including altitude
  GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCGGA);
  // uncomment this line to turn on only the "minimum recommended" data
  //GPS.sendCommand(PMTK_SET_NMEA_OUTPUT_RMCONLY);
  // For parsing data, we don't suggest using anything but either RMC only or RMC+GGA since
  // the parser doesn't care about other sentences at this time
  
  // Set the update rate
  GPS.sendCommand(PMTK_SET_NMEA_UPDATE_1HZ);   // 1 Hz update rate
  // For the parsing code to work nicely and have time to sort thru the data, and
  // print it out we don't suggest using anything higher than 1 Hz

  // Request updates on antenna status, comment out to keep quiet
  //GPS.sendCommand(PGCMD_ANTENNA);

  // the nice thing about this code is you can have a timer0 interrupt go off
  // every 1 millisecond, and read data from the GPS for you. that makes the
  // loop code a heck of a lot easier!

#ifdef __arm__
  usingInterrupt = false;  //NOTE - we don't want to use interrupts on the Due
#else
  useInterrupt(true);
#endif

  delay(1000);
  // Ask for firmware version
  mySerial.println(PMTK_Q_RELEASE);








Serial.print("Initializing SD card...");

  if (!SD.begin(53)) {
    Serial.println("initialization failed!");
    while (1);
  }
  Serial.println("initialization done.");

  // Check to see if the file exists:
  if (SD.exists("DATAFILE.txt")) {
    Serial.println("DATAFILE exists.");
    myFile = SD.open("DATAFILE.txt", FILE_WRITE);
  } else {
    Serial.println("DATAFILE.txt doesn't exist.");
    myFile = SD.open("DATAFILE.txt", FILE_WRITE);
    Serial.println("File created");
  }
  myFile.close();











  Wire.begin();

  sensorT.init();
  sensorP.init();
  sensorP.setModel(MS5837::MS5837_02BA);

  pinMode(13, OUTPUT);
//  pinMode(12, OUTPUT);
//  digitalWrite(12, HIGH);
  digitalWrite(13, LOW);






  
}

#ifdef __AVR__
// Interrupt is called once a millisecond, looks for any new GPS data, and stores it
SIGNAL(TIMER0_COMPA_vect) {
  char c = GPS.read();
  // if you want to debug, this is a good time to do it!
#ifdef UDR0
  if (GPSECHO)
    if (c) UDR0 = c;  
    // writing direct to UDR0 is much much faster than Serial.print 
    // but only one character can be written at a time. 
#endif
}

void useInterrupt(boolean v) {
  if (v) {
    // Timer0 is already used for millis() - we'll just interrupt somewhere
    // in the middle and call the "Compare A" function above
    OCR0A = 0xAF;
    TIMSK0 |= _BV(OCIE0A);
    usingInterrupt = true;
  } else {
    // do not call the interrupt function COMPA anymore
    TIMSK0 &= ~_BV(OCIE0A);
    usingInterrupt = false;
  }
}
#endif //#ifdef__AVR__

uint32_t timer = millis();
void loop()                     // run over and over again
{
  // in case you are not using the interrupt above, you'll
  // need to 'hand query' the GPS, not suggested :(
  if (! usingInterrupt) {
    // read data from the GPS in the 'main loop'
    char c = GPS.read();
    // if you want to debug, this is a good time to do it!
    if (GPSECHO)
      if (c) Serial.print(c);
  }
  
  // if a sentence is received, we can check the checksum, parse it...
  if (GPS.newNMEAreceived()) {
    // a tricky thing here is if we print the NMEA sentence, or data
    // we end up not listening and catching other sentences! 
    // so be very wary if using OUTPUT_ALLDATA and trytng to print out data
    //Serial.println(GPS.lastNMEA());   // this also sets the newNMEAreceived() flag to false
  
    if (!GPS.parse(GPS.lastNMEA()))   // this also sets the newNMEAreceived() flag to false
      return;  // we can fail to parse a sentence in which case we should just wait for another
  }

  // if millis() or timer wraps around, we'll just reset it
  if (timer > millis())  timer = millis();

  // approximately every 2 seconds or so, print out the current stats
  if (millis() - timer > 2000) { 
    timer = millis(); // reset the timer
      Serial.print("\nTime: ");
      Serial.print(GPS.hour, DEC); Serial.print(':');
      Serial.print(GPS.minute, DEC); Serial.print(':');
      Serial.println(GPS.seconds, DEC);
      Serial.print("Date: ");
      Serial.print(GPS.day, DEC); Serial.print('/');
      Serial.print(GPS.month, DEC); Serial.print("/20");
      Serial.println(GPS.year, DEC);
      Serial.print("Location: ");
      Serial.print(GPS.latitude, 6); Serial.print(GPS.lat);
      Serial.print(", "); 
      Serial.println(GPS.longitude, 6); Serial.println(GPS.lon);

//      count = count + 1;

      if (GPS.fix) {
        Serial.print("Connection: ");Serial.println(conn);
        conn++;
      }
      else {
        Serial.print("No connection: ");Serial.println(no_conn);
        no_conn++;
      }
  }

  if (conn > 3 || no_conn > 10) {
//    digitalWrite(12, LOW);


    myFile = SD.open("DATAFILE.txt", FILE_WRITE);
      myFile.print("Time: ");
      myFile.print(GPS.hour, DEC); myFile.print(':');
      myFile.print(GPS.minute, DEC); myFile.print(':');
      myFile.print(GPS.seconds, DEC);
      myFile.print(", Date: ");
      myFile.print(GPS.day, DEC); myFile.print('/');
      myFile.print(GPS.month, DEC); myFile.print("/20");
      myFile.print(GPS.year, DEC);
      myFile.print(", Location: ");
      myFile.print(GPS.latitude, 6); myFile.print(GPS.lat);myFile.print(", "); 
      myFile.print(GPS.longitude, 6); myFile.println(GPS.lon);
      myFile.close();
    
    delay(10);
    digitalWrite(13, HIGH);
    
    TCA9548A(0);
    sensorT.init();
    TCA9548A(1);
    sensorP.init();

    tT1 = 0;
    tT2 = 0;
    tP1 = 0;
    count_ave = 0;
    while (count_ave < 5) {

      TCA9548A(0);
      sensorT.read();
      T1=sensorT.temperature();

      TCA9548A(1);
      sensorP.read();
      T2=sensorP.temperature();
      P1=sensorP.pressure();
      
        if (count_ave == 0) {
        T1min=T1;
        T1max=T1;
        }
        T1min = min(T1min,T1);
        T1max = max(T1max,T1);
    
      
      tT1=tT1+T1;

      tT2=tT2+T2;

      tP1=tP1+P1;
      
      count_ave ++;
      Serial.println(count_ave);
      delay(1000);
    }
    Serial.print("Temperature: ");
    Serial.print(tT1/count_ave); 
    Serial.print(" deg C; ");
    Serial.print("Temperature: ");
    Serial.print(tT2/count_ave); 
    Serial.print(" deg C; ");
    Serial.print("Temperature: ");
    Serial.print(tP1/(100*count_ave)); 
    Serial.println(" hPa; ");

    Serial.print("Tmin: ");
    Serial.print(T1min); 
    Serial.print(" deg C; ");
    Serial.print("Tmax: ");
    Serial.print(T1max); 
    Serial.println(" deg C; ");


    myFile = SD.open("DATAFILE.txt", FILE_WRITE);
      timecounter = millis();
      myFile.print(timecounter);
      myFile.print(", ");
      myFile.print(tT1/count_ave);
      myFile.print(", ");
      myFile.print(tT2/count_ave);
      myFile.print(", ");
      myFile.println(tP1/count_ave);
      myFile.close();

      

    delay(1000);
    digitalWrite(13, LOW);

    count = 0;
    conn = 0;
    no_conn = 0;
//    digitalWrite(12, HIGH);
  }
}
