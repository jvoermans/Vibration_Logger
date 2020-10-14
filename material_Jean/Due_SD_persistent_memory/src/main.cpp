// write information to the flash memory to allow persistent data storing as there is no EEPROM on the Due
// this allows to take care of tracking file number even in case of reset

#include <Arduino.h>
#include <PersistentStorage.h>


void setup() {
  Serial.begin(115200);
  delay(100);
}

void loop() {
  uint32_t file_number;

  file_number = get_file_number();
  Serial.print(F("got file_number:"));
  Serial.println(file_number);

  delay(500);
  increment_file_number();
}