#include <Arduino.h>

#define DELAY_TIME (631600)
#define DEBOUNCE_TIME (50)


uint8_t relay_on;
uint64_t disable_time;

void setup() {
  // put your setup code here, to run once:
  pinMode(7, INPUT);
  pinMode(8, OUTPUT);
  pinMode(9, OUTPUT);

}

void loop() {
  // put your main code here, to run repeatedly:
  
  if(!relay_on) {
    if(digitalRead(7)) {
      delay(DEBOUNCE_TIME);
      if(digitalRead(7)) {
        digitalWrite(8, HIGH);
        digitalWrite(9, HIGH);
        disable_time = millis() + DELAY_TIME;
        relay_on = 1;
      }
    }
  }
  else if(millis() >= disable_time) {
    digitalWrite(8, LOW);
    digitalWrite(9, LOW);
    relay_on = 0;
  }
}
