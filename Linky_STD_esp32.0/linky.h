// **********************************************************************************
// Arduino Teleinfo sample, display information on teleinfo values received
// **********************************************************************************
// Creative Commons Attrib Share-Alike License
// You are free to use/extend this library but please abide with the CC-BY-SA license:
// http://creativecommons.org/licenses/by-sa/4.0/
//
// for detailled explanation of this library see dedicated article
// https://hallard.me/libteleinfo/
//
// For any explanation about teleinfo or use, see my blog
// https://hallard.me/category/tinfo
//
// connect Teleinfo RXD pin To Arduin D3
// see schematic here https://hallard.me/demystifier-la-teleinfo/
// and dedicated article here 
//
// Written by Charles-Henri Hallard (https://hallard.me)
//
// History : V1.00 2015-06-14 - First release
//
// All text above must be included in any redistribution.
//
// **********************************************************************************
//
// adaptation pour LINKY mode Standart sur ESP32 

#include "Teleinfo.h" 
#include "Teleinfo_cpp.h" 
TInfo    tinfo; // Teleinfo object
ValueList * me_fist;
bool  ok_me_fist = false;

/* ======================================================================
Function: printUptime 
Purpose : print pseudo uptime value
Input   : -
Output  : - 
Comments: compteur de secondes basique sans controle de dépassement
          En plus SoftwareSerial rend le compteur de millis() totalement
          A la rue, donc la precision de ce compteur de seconde n'est
          pas fiable du tout, dont acte !!!
====================================================================== */
void printUptime(void)
{
  Serial.print(millis()/1000);
  Serial.print(F("s\t"));
}

/* ======================================================================
Function: DataCallback 
Purpose : callback when we detected new or modified data received
Input   : linked list pointer on the concerned data
          current flags value
Output  : - 
Comments: -
====================================================================== */
void DataCallback(ValueList * me, uint8_t  flags)
{
  // Show our not accurate second counter
  //printUptime();

  if (flags & TINFO_FLAGS_ADDED || flags & TINFO_FLAGS_UPDATED)
    if  ( !ok_me_fist) {
       ok_me_fist = true;
       me_fist = me;
    }

  // Display values
  Serial.print("==> ");
  Serial.print(me->name);
  if (strlen(me->name) < 4 ) Serial.print(F("\t")) ;
  Serial.print(F("\t")) ; 
  if ( me->value[0] != '*') {
    Serial.print(F(" = ")) ; 
    Serial.print(me->value) ;
  }
  if ( me->date[0] != '*' ) {
    Serial.print(" : ");
    Serial.print(me->date);
  }
  Serial.println("");
}

/* ======================================================================
Function: sendJSON 
Purpose : dump teleinfo values on serial
Input   : linked list pointer on the concerned data
          true to dump all values, false for only modified ones
Output  : - 
Comments: -
====================================================================== */
void sendAll()
{
  ValueList * me;
  me = me_fist;                                   // reprendre le 1er de la chaine
  // Got at least one ?
  if (me) {

      Serial.print(F("\"all\":"));     
      Serial.print(me->name) ;
      Serial.print(F("\t")) ;
      if ( me->value[0] != '*') {
        Serial.print(F(" = ")) ; 
        Serial.print(me->value) ;
      }
      if ( me->date[0] != '*') {
        Serial.print(" : ");
        Serial.print(me->date);
      }
      Serial.println("");

      // Loop thru the node
      while (me->next) {
          // go to next node
          me = me->next;                                // suivant 
          //sprintf(msg, "%s=%s", me->name, me->value );
          //Mqtt_emettre(me->name, me->value);

          Serial.print(F("\"all\":"));     
          Serial.print(me->name) ;
          //if (strlen(me->name) < 4 ) Serial.print(F("\t")) ;
          Serial.print(F("\t")) ;
          if ( me->value[0] != '*') {
            Serial.print(F(" = ")) ; 
            Serial.print(me->value) ;
          }
          if (me->date[0] != '*') {
            Serial.print(" : ");
            Serial.print(me->date);
          } 
          Serial.println("");
        
      }
   }
}

/* ======================================================================
Function: setup
Purpose : Setup I/O and other one time startup stuff
Input   : -
Output  : - 
Comments: -
====================================================================== */
void linky_setup()
{

  Serial.println(F("======== Setup Teleinfo Linky ======="));
  delay(200);

  Serial2.begin(9600, SERIAL_7N1, RXD2, TXD2); // Teleinfo Serial pin Rx16, Tx17

  // Init teleinfo
  tinfo.init();

  // Attacher les callback dont nous avons besoin
  // pour cette demo, ici attach data
  tinfo.attachData(DataCallback);

}

/* ======================================================================
Function: loop
Purpose : infinite loop main code
Input   : -
Output  : - 
Comments: -
====================================================================== */
long MQAll;
void linky_loop()
{
  // Teleinformation processing
  if ( Serial2.available() ) {
    /*
     char inByte = Serial2.read();
     Serial.print(inByte);
     Serial.print(".");
     Serial.print(inByte,HEX);
     Serial.print(";");
    */
     tinfo.process(Serial2.read());
  }
  
  // Afficher la liste complete toutes les 1 minute
    if (millis() > MQAll + 60000 ) {
        MQAll = millis();
        sendAll();
    }
    
}

  
