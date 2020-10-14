#include <Arduino.h>

/* 
  par Andre DURAND, Octobre 2020
*/

#define RXD2   16           // linky rx  = Serial2 Gpio 16 
#define TXD2   17

#include "linky.h"

void setup() { 
  Serial.begin(115200);     //  sortie moniteur              
  delay(500);
  Serial.println(" ");
  Serial.println(F("Boot: version Linky_standart 9600"));

  linky_setup();
}

//--------------------------------------- LOOP -------------------------------
void loop() {
  
            linky_loop();
}
//-------------------------- fin du programme 
