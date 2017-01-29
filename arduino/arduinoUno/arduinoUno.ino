/*
    # --------------------------------------------------------------------------------------------
    # Copyright (C) 2015, 2016  Gerhard Hepp
    #
    # This program is free software; you can redistribute it and/or modify it under the terms of
    # the GNU General Public License as published by the Free Software Foundation; either version 2
    # of the License, or (at your option) any later version.
    #
    # This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
    # without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
    # See the GNU General Public License for more details.
    #
    # You should have received a copy of the GNU General Public License along with this program; if
    # not, write to the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
    # MA 02110, USA
    # ---------------------------------------------------------------------------------------------
*/
/*
    This program is a remote control of port pins, pwm, analog from serial line.
    Designed for Arduino UNO or Nano.

    version 2017-01-12 rework on Servo hotfix, added additional state '1' for initialization
    version 2017-01-11 fixed problem with initialization of Servo
    version 2016-11-13 reorganized state handling
    version 2016-02-28 Pin 7 on analog output does not work
    version 2016-02-31
        A6, A7 as digitial io removed
*/

#include <avr/pgmspace.h>
#include <Servo.h>
#include <EEPROM.h>

char version[] = "arduinoUno, version 2017-01-12";


#define FALSE (1==0)
#define TRUE (1==1)

// -------------------------------------------
// debug settings
// bit 0: command debug out  (debug)
// bit 1: input char out (slow, verbose)
// bit 2: switch off blink LED13, but use for toggle on each main loop (runtime analysis)
//
unsigned long debug = 0L;
//
// -------------------------------------------

#define STATEMACHINE_EVENT_START  10
#define STATEMACHINE_EVENT_CONFIG  20

#define STATEMACHINE_EVENT_TIMEOUT 1000

// -------------------------------------------
// id command variables
#define N_ID 16

// add one char for terminating zero '\0'
char id[N_ID + 1];
int nId = 0;

// -------------------------------------------
// data aquisition control variables
int cnt = 0;
int acnt = 0;
int aval = 0;
// -------------------------------------------

#define N_LASTRESULT 32
int lastResult[N_LASTRESULT];
unsigned int lastAnalogAnalogResult[N_LASTRESULT];
unsigned int lastAnalogDigitalResult[N_LASTRESULT];


void stateMachine(int event);

void setup() {
  for ( int i = 0; i < N_LASTRESULT; i ++ ) {
    lastResult[i] = 2;
    lastAnalogAnalogResult[i] = 0xffff;
    lastAnalogDigitalResult[i] = 0xffff;
  }
  servoInit();

  // initialize serial communication at 115200 bits per second:
  Serial.begin(115200);

  Serial.println("arduino sending@115200 Bd");
  Serial.println(version);

  pinMode(13, OUTPUT);
  blinkModeFast();
  stateMachine( STATEMACHINE_EVENT_START);
}


// -------------------------------------------------
void setEEPROM() {

  for ( int i = 0; i < N_ID; i ++ )
  {
    EEPROM.write(i, id[i]);
  }
}
void getEEPROM() {

  for ( int i = 0; (i < N_ID + 1); i ++ )
  {
    id[i] = 0;
  }
  for ( int i = 0; i < N_ID; i ++ )
  {
    id[i] = EEPROM.read( i );
  }

  Serial.print( F("ident:") );
  Serial.println(id);


}
// -------------------------------------------------
// BLINK LED 13
//
unsigned long blinkPreviousMillis = 0L;
int blinkState = 10;

void blinkModeFast() {
  blinkState = 10;
}

void blinkModeSlow() {
  blinkState = 20;
}

void _blinking() {

  unsigned long blinkCurrentMillis = millis();

  switch (blinkState) {
    case 10:
      if ( debug & 0b100 ) {
        blinkState = 40;
        break;
      }
      {
        blinkState = 11;
      }
      break;
    case 11:
      if ((unsigned long)(blinkCurrentMillis - blinkPreviousMillis) >= 100L ) {
        blinkPreviousMillis = blinkCurrentMillis;

        digitalWrite(13, LOW);

        blinkState = 12;
      }
      break;
    case 12:
      if ((unsigned long)(blinkCurrentMillis - blinkPreviousMillis) >= 80L ) {
        blinkPreviousMillis = blinkCurrentMillis;

        digitalWrite(13, HIGH);

        blinkState = 11;
      }
      break;

    case 20:  {
        blinkPreviousMillis = blinkCurrentMillis;
        blinkState = 21;
      }
      break;

    case 21: if ( debug & 0b100 ) {
        blinkState = 40;
        break;
      }
      if ((unsigned long)(blinkCurrentMillis - blinkPreviousMillis) >= 1000L ) {
        blinkPreviousMillis = blinkCurrentMillis;

        digitalWrite(13, LOW);

        blinkState = 22;
      }
      break;

    case 22:
      if ((unsigned long)(blinkCurrentMillis - blinkPreviousMillis) >= 1000L ) {
        blinkPreviousMillis = blinkCurrentMillis;

        digitalWrite(13, HIGH);

        blinkState = 21;
      }
      break;

    case 40:
      digitalWrite(13, HIGH);
      blinkState = 41;
      break;

    case 41:
      digitalWrite(13, LOW);
      blinkState = 40;
      break;
  }

}


// -------------------------------------------------

unsigned long nextTimeoutEvent = 0L;
boolean  timeoutEnable = FALSE;

unsigned long digitalPreviousMicros = 0L;

void _timeout() {
  if ( ! timeoutEnable)
    return;
  if ( millis() > nextTimeoutEvent ) {
    timeoutEnable = FALSE;
    stateMachine(STATEMACHINE_EVENT_TIMEOUT);
  }
}
void setTimeout(int t) {
  timeoutEnable = TRUE;
  nextTimeoutEvent = millis() + t;
}

/** Statemachine state handling */
int STATEMACHINE_waitState = 0;

// ---------------------------------------------------
// state entry and exit methods
//

// --- executed once after reset
//
void statemachine_0000_exit() {
  timeoutEnable = FALSE;
}

void statemachine_0001_entry() {
  setTimeout( 500);
  // if it is needed to set servo to a defined position after reset,
  // then for servo pin N
  // servoObject [N] = new Servo();
  // servoObject[N]->attach(N);
  // sample write with value 23
  // servoObject[N]->write( 23);

}

void statemachine_0001_exit() {
  timeoutEnable = FALSE;
}

// --- executed each 2000 ms until config commads are arriving

void statemachine_1000_entry() {
  Serial.println( F("config?") );
  setTimeout(2000);
}
void statemachine_1000_exit() {
  timeoutEnable = FALSE;
}

// --- executed once after a config command
void statemachine_2000_entry() {
  timeoutEnable = FALSE;
  blinkModeSlow();
}

void stateMachine(int event) {

  if (debug & 1) {
    if ( event == STATEMACHINE_EVENT_START) {
      Serial.println( F("STATEMACHINE_EVENT_START") );
    }
    if ( event == STATEMACHINE_EVENT_TIMEOUT) {
      Serial.println( F("STATEMACHINE_EVENT_TIMEOUT") );
    }
    if ( event == STATEMACHINE_EVENT_CONFIG) {
      Serial.println( F("STATEMACHINE_EVENT_CONFIG") );
    }
  }

  switch (STATEMACHINE_waitState) {
    // State 0 is left immediately after reset
    case 0:
      switch (event) {
        case STATEMACHINE_EVENT_START:

          STATEMACHINE_waitState = 1;
          statemachine_0000_exit();
          statemachine_0001_entry();
          break;
      }
      break;

    // State 1 is an intermediate state which is usually kept for 0.5 sec.
    // This can be used to set up additional resources, e.g, servo
    //
    case 1:
      switch (event) {
        case STATEMACHINE_EVENT_TIMEOUT:

          STATEMACHINE_waitState = 1000;
          statemachine_0001_exit();
          statemachine_1000_entry();
          break;
 
        case STATEMACHINE_EVENT_CONFIG:
          STATEMACHINE_waitState = 2000;
          statemachine_0001_exit();
          statemachine_2000_entry();

          break;
      }
      break;

    case 1000:
      switch (event) {
        case STATEMACHINE_EVENT_CONFIG:
          STATEMACHINE_waitState = 2000;
          statemachine_1000_exit();
          statemachine_2000_entry();

          break;

        case STATEMACHINE_EVENT_TIMEOUT:
          STATEMACHINE_waitState = 1000;
          statemachine_1000_exit();
          statemachine_1000_entry();
          break;
      }
      break;

    case 2000:
      break;
  }
}

unsigned long digitalInputs = 0L;
unsigned long analogAnalogInputs = 0L;
unsigned long analogDigitalInputs = 0L;
unsigned long analogDigitalOutputs = 0L;

unsigned long pwms = 0L;
unsigned long servos = 0L;


void setDigitalInput(long data) {
  data &= (
            ( 1 <<  2) |
            ( 1 <<  3) |
            ( 1 <<  4) |
            ( 1 <<  5) |
            ( 1 <<  6) |
            ( 1 <<  7) |
            ( 1 <<  8) |
            ( 1 <<  9) |
            ( 1 << 10) |
            ( 1 << 11) |
            ( 1 << 12) );
  digitalInputs |= data;

  if ( data & ( 1 <<  2) ) pinMode( 2, INPUT);
  if ( data & ( 1 <<  3) ) pinMode( 3, INPUT);
  if ( data & ( 1 <<  4) ) pinMode( 4, INPUT);
  if ( data & ( 1 <<  5) ) pinMode( 5, INPUT);
  if ( data & ( 1 <<  6) ) pinMode( 6, INPUT);
  if ( data & ( 1 <<  7) ) pinMode( 7, INPUT);
  if ( data & ( 1 <<  8) ) pinMode( 8, INPUT);
  if ( data & ( 1 <<  9) ) pinMode( 9, INPUT);
  if ( data & ( 1 << 10) ) pinMode(10, INPUT);
  if ( data & ( 1 << 11) ) pinMode(11, INPUT);
  if ( data & ( 1 << 12) ) pinMode(12, INPUT);
}
void setDigitalOutput(long data) {

  if ( data & ( 1 <<  2) ) pinMode( 2, OUTPUT);
  if ( data & ( 1 <<  3) ) pinMode( 3, OUTPUT);
  if ( data & ( 1 <<  4) ) pinMode( 4, OUTPUT);
  if ( data & ( 1 <<  5) ) pinMode( 5, OUTPUT);
  if ( data & ( 1 <<  6) ) pinMode( 6, OUTPUT);
  if ( data & ( 1 <<  7) ) pinMode( 7, OUTPUT);
  if ( data & ( 1 <<  8) ) pinMode( 8, OUTPUT);
  if ( data & ( 1 <<  9) ) pinMode( 9, OUTPUT);
  if ( data & ( 1 << 10) ) pinMode(10, OUTPUT);
  if ( data & ( 1 << 11) ) pinMode(11, OUTPUT);
  if ( data & ( 1 << 12) ) pinMode(12, OUTPUT);
}
void setDigitalPWMOutput(long data) {

  if ( data & ( 1 <<  3) ) pinMode( 3, OUTPUT);

  if ( data & ( 1 <<  5) ) pinMode( 5, OUTPUT);
  if ( data & ( 1 <<  6) ) pinMode( 6, OUTPUT);

  if ( data & ( 1 <<  9) ) pinMode( 9, OUTPUT);
  if ( data & ( 1 << 10) ) pinMode(10, OUTPUT);
  if ( data & ( 1 << 11) ) pinMode(11, OUTPUT);
}

Servo* servoObject [16];

void servoInit() {
  for ( int i = 0; i < 16; i ++ ) {
    servoObject [i] = NULL;
  }
}
void setDigitalServoOutput(long data) {

  for ( int i = 0; i < 16; i ++ ) {
    if ( data & ( 1 <<  i) ) {
      if ( servoObject [i] == NULL ) {
        servoObject [i] = new Servo();
      }
      servoObject[i]->attach(i);
    }
    else {
      if ( servoObject [i] != NULL ) {
        servoObject[i]->detach();
      }
    }
  }
}

void setDigitalInputPullup(long data) {
  data &= (
            ( 1 <<  2) |
            ( 1 <<  3) |
            ( 1 <<  4) |
            ( 1 <<  5) |
            ( 1 <<  6) |
            ( 1 <<  7) |
            ( 1 <<  8) |
            ( 1 <<  9) |
            ( 1 << 10) |
            ( 1 << 11) |
            ( 1 << 12) );
  digitalInputs |= data;

  if ( data & ( 1 <<  2) ) pinMode( 2, INPUT_PULLUP);
  if ( data & ( 1 <<  3) ) pinMode( 3, INPUT_PULLUP);
  if ( data & ( 1 <<  4) ) pinMode( 4, INPUT_PULLUP);
  if ( data & ( 1 <<  5) ) pinMode( 5, INPUT_PULLUP);
  if ( data & ( 1 <<  6) ) pinMode( 6, INPUT_PULLUP);
  if ( data & ( 1 <<  7) ) pinMode( 7, INPUT_PULLUP);
  if ( data & ( 1 <<  8) ) pinMode( 8, INPUT_PULLUP);
  if ( data & ( 1 <<  9) ) pinMode( 9, INPUT_PULLUP);
  if ( data & ( 1 << 10) ) pinMode(10, INPUT_PULLUP);
  if ( data & ( 1 << 11) ) pinMode(11, INPUT_PULLUP);
  if ( data & ( 1 << 12) ) pinMode(12, INPUT_PULLUP);
}
void setAnalogDigitalInputPullup(long data) {
  data &= (
            ( 1 <<  0) |
            ( 1 <<  1) |
            ( 1 <<  2) |
            ( 1 <<  3) |
            ( 1 <<  4) |
            ( 1 <<  5) |
            ( 1 <<  6) |
            ( 1 <<  7) );
  analogDigitalInputs |= data;

  if ( data & ( 1 <<  0) ) pinMode( A0, INPUT_PULLUP);
  if ( data & ( 1 <<  1) ) pinMode( A1, INPUT_PULLUP);
  if ( data & ( 1 <<  2) ) pinMode( A2, INPUT_PULLUP);
  if ( data & ( 1 <<  3) ) pinMode( A3, INPUT_PULLUP);
  if ( data & ( 1 <<  4) ) pinMode( A4, INPUT_PULLUP);
  if ( data & ( 1 <<  5) ) pinMode( A5, INPUT_PULLUP);
  // if ( data & ( 1 <<  6) ) pinMode( A6, INPUT_PULLUP);
  // if ( data & ( 1 <<  7) ) pinMode( A7, INPUT_PULLUP);
}
void setAnalogDigitalInput(long data) {
  data &= (
            ( 1 <<  0) |
            ( 1 <<  1) |
            ( 1 <<  2) |
            ( 1 <<  3) |
            ( 1 <<  4) |
            ( 1 <<  5) |
            ( 1 <<  6) |
            ( 1 <<  7) );
  analogDigitalInputs |= data;

  if ( data & ( 1 <<  0) ) pinMode( A0, INPUT);
  if ( data & ( 1 <<  1) ) pinMode( A1, INPUT);
  if ( data & ( 1 <<  2) ) pinMode( A2, INPUT);
  if ( data & ( 1 <<  3) ) pinMode( A3, INPUT);
  if ( data & ( 1 <<  4) ) pinMode( A4, INPUT);
  if ( data & ( 1 <<  5) ) pinMode( A5, INPUT);
  // if ( data & ( 1 <<  6) ) pinMode( A6, INPUT);
  // if ( data & ( 1 <<  7) ) pinMode( A7, INPUT);
}
void setAnalogDigitalOutput(long data) {
  data &= (
            ( 1 <<  0) |
            ( 1 <<  1) |
            ( 1 <<  2) |
            ( 1 <<  3) |
            ( 1 <<  4) |
            ( 1 <<  5) |
            ( 1 <<  6) |
            ( 1 <<  7) );
  analogDigitalOutputs |= data;

  if ( data & ( 1 <<  0) ) pinMode( A0, OUTPUT);
  if ( data & ( 1 <<  1) ) pinMode( A1, OUTPUT);
  if ( data & ( 1 <<  2) ) pinMode( A2, OUTPUT);
  if ( data & ( 1 <<  3) ) pinMode( A3, OUTPUT);
  if ( data & ( 1 <<  4) ) pinMode( A4, OUTPUT);
  if ( data & ( 1 <<  5) ) pinMode( A5, OUTPUT);
  // if ( data & ( 1 <<  6) ) pinMode( A6, OUTPUT);
  // if ( data & ( 1 <<  7) ) pinMode( A7, OUTPUT);
}

int isHex(char c) {
  switch (c) {
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':

    case 'A':
    case 'B':
    case 'C':
    case 'D':
    case 'E':
    case 'F':

    case 'a':
    case 'b':
    case 'c':
    case 'd':
    case 'e':
    case 'f':
      return TRUE;
  }
  return FALSE;
}


unsigned int  valueHex(char c) {
  switch (c) {
    case '0': return 0;
    case '1': return 1;
    case '2': return 2;
    case '3': return 3;
    case '4': return 4;
    case '5': return 5;
    case '6': return 6;
    case '7': return 7;
    case '8': return 8;
    case '9': return 9;

    case 'A': return 10;
    case 'B': return 11;
    case 'C': return 12;
    case 'D': return 13;
    case 'E': return 14;
    case 'F': return 15;

    case 'a': return 10;
    case 'b': return 11;
    case 'c': return 12;
    case 'd': return 13;
    case 'e': return 14;
    case 'f': return 15;
  }
  return 0;
}
int isDecimal(char c) {
  switch (c) {
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      return TRUE;
  }
  return FALSE;
}

unsigned int  valueDecimal(char c) {
  switch (c) {
    case '0': return 0;
    case '1': return 1;
    case '2': return 2;
    case '3': return 3;
    case '4': return 4;
    case '5': return 5;
    case '6': return 6;
    case '7': return 7;
    case '8': return 8;
    case '9': return 9;
  }
  return 0;
}
#define NBUFFER 100
char buffer[NBUFFER ];
int nBuffer = 0;

void analogDigitalWrite (int port, int value) {

  switch (port) {
    case 0: digitalWrite(A0, value); break;
    case 1: digitalWrite(A1, value); break;
    case 2: digitalWrite(A2, value); break;
    case 3: digitalWrite(A3, value); break;
    case 4: digitalWrite(A4, value); break;
    case 5: digitalWrite(A5, value); break;
      // case 6: digitalWrite(A6, value); break;
      // case 7: digitalWrite(A7, value); break;
  }

}

void handleInput(int port, int value) {

  if ( value != lastResult[port] ) {
    Serial.print( F("i:"));
    Serial.print(port);
    Serial.print(F(","));
    Serial.println(value);
    lastResult[port] = value;
  }
}
void handleAnalogDigitalInput(int port, int value) {

  if ( value != lastAnalogDigitalResult[port] ) {
    Serial.print( F("ai:") );
    Serial.print(port);
    Serial.print(F(","));
    Serial.println(value);
    lastAnalogDigitalResult[port] = value;
  }
}

void handleAnalogAnalogInput(int port, int value) {

  if ( value != lastAnalogAnalogResult[port] ) {
    Serial.print(F("a:"));
    Serial.print(port);
    Serial.print(F(","));
    Serial.println(value);
    lastAnalogAnalogResult[port] = value;
  }
}

const char helpText[] PROGMEM =
  "arduino requests configuration with 'config?' on reset\n"
  "\n"
  "configuration commands start with 'c'\n"
  " cdebug:<data> debug settings, data are hex (0,1,2,3)\n"
  " cr: dummy request, just get a newline and clean buffer\n"
  " cversion? request version string\n"
  " cerr?     request error count for parser\n"
  " cident?   request idcode\n"
  " cident:<char16> write idcode\n"
  "\n"
  " cdin:<data> digital inputs, data are hex\n"
  " cdinp:<data> digital inputs, pullup enabled, data are hex\n"
  " cdout:<data> digital outputs, data are hex\n"
  " cdpwm:<data> digital pwm, data are hex\n"
  " cdservo:<data> digital servo, data are hex\n"

  " caain:<data> analog line, data are hex [a0..a5]\n"
  " cadin:<data> analog line, digital input [a0..a5]\n"
  " cadinp:<data> analog line, digital input, pullup [a0..a5]\n"
  " cadout:<data> analog line, digital output\n"
  "data give bit patterns for IO pins, Bits 1,2,3... are used\n"
  "\n"
  "Commands to set values in arduino\n"
  " o:<port>,<value>     write output, shortcut\n"
  " p:<port>,<value>     write pwm, shortcut\n"
  " s:<port>,<value>     write servo, shortcut\n"
  "\n"
  "Values reported from arduino to host\n"
  " v:<version>       arduino reports version\n"
  " ident:<char16>    arduino reports ident from EEPROM\n"
  " e:<errors>        arduino reports number of errors (decimal)\n"
  " a:<port>,<value>  arduino reports analog input\n"
  " i:<port>,<value>  arduino reports digital input\n"
  " ai:<port>,<value> arduino reports digital input\n";

unsigned long errorCount = 0;

unsigned int value;
unsigned int port;
unsigned long data;
int state = 0;

void printDebug_cdebug() {
  Serial.print( F("cdebug=") );
  Serial.println(data, HEX);
}
void printDebug_cdinp() {
  Serial.print( F("cdinp=") );
  Serial.println(data, HEX);
}
void printDebug_cdout() {
  Serial.print( F("cdout=") );
  Serial.println(data, HEX);
}
void printDebug_cdpwm() {
  Serial.print( F("cdpwm=") );
  Serial.println(data, HEX);
}
void printDebug_cdservo() {
  Serial.print( F("cdservo=") );
  Serial.println(data, HEX);
}
void printDebug_caain() {
  Serial.print( F("caain=") );
  Serial.println(data, HEX);
}
void printDebug_cadin() {
  Serial.print( F("cadin=") );
  Serial.println(data, HEX);
}
void printDebug_cadout() {
  Serial.print( F("cadout=") );
  Serial.println(data, HEX);
}
void printDebug_cadinp() {
  Serial.print( F("cadinp=") );
  Serial.println(data, HEX);
}

void printDebug_oa_port_value () {
  Serial.print(F("oa("));
  Serial.print(port);
  Serial.print(F(","));
  Serial.print(value);
  Serial.println(F(")"));
}

void printDebug_o_port_value () {
  Serial.print(F("o("));
  Serial.print(port);
  Serial.print(F(","));
  Serial.print(value);
  Serial.println(F(")"));
}

void printDebug_p_port_value () {
  Serial.print(F("p("));
  Serial.print(port);
  Serial.print(F(","));
  Serial.print(value);
  Serial.println(F(")"));
}

void printDebug_s_port_value () {
  Serial.print(F("s("));
  Serial.print(port);
  Serial.print(F(","));
  Serial.print(value);
  Serial.println(F(")"));
}

void loop() {
  _blinking();
  _timeout();

  if ( Serial.available()) {
    char c = Serial.read();
    if ( debug & 0b010) {
      Serial.print( F("state ")) ;
      Serial.print(state );

      Serial.print(" c ");
      if ( c == '\n') {
        Serial.println(F("\\n"));
      }
      else {
        Serial.println(c);
      }
    }
    //
    // the following code is generated. It forms a state based parser
    // for the incoming char stream.
    //
    //--BEGIN
    // ------------------- STATE 0
    // ---------- generate() by TP id 0
    //
    switch ( state) {
      case 0: // 0
        {
          if ( c == 'o' ) {
            state = 1;
          }
          else if ( c == 'p' ) {
            state = 21;
          }
          else if ( c == 's' ) {
            state = 31;
          }
          else if ( c == 'h' ) {
            state = 41;
          }
          else if ( c == 'c' ) {
            state = 46;
          }
          else if ( c == 'v' ) {
            state = 203;
          }
          else {
            state = 9999;
          }
        } break; // end switch 0
      // ------------------- STATE 1
      // ---------- generated by TPC id 1
      //
      case 1: // 1
        {
          if ( c == ':' ) {
            state = 2;
          }
          else if ( c == 'a' ) {
            state = 11;
          }
          else {
            state = 9998;
          }
        } break; // end case state1
      // ------------------- STATE 2
      // ---------- generated by TPC id 2
      //
      case 2: // 2
        {
          if ( isDecimal(c) ) {
            port = valueDecimal(c);
            state = 3;
          }
          else {
            state = 9998;
          }
        } break; // end case state2
      // ------------------- generate STATE 3
      // ---------- generate() by TPV_PORT id 3
      //
      case 3: // 3
        {
          if ( isDecimal(c) ) {
            port = port * 10 + valueDecimal(c);
            state = 4;
          }
          else if ( c == ',' ) {
            state = 5;
          }
          else {
            state = 9998;
          }
        } break; // end case state3
      // create sub-states of VALUE
      // ------------------- generate STATE 4
      // ---------- generate() by TPV_PORT2 id 4
      //
      case 4: // 4
        {
          if ( c == ',' ) {
            state = 5;
          }
          else {
            state = 9998;
          }
        } break; // end case state4
      // create child nodes of VALUE
      // ------------------- STATE 5
      // ---------- generated by TPC id 5
      //
      case 5: // 5
        {
          if ( isDecimal(c) ) {
            value = valueDecimal(c);
            state = 6;
          }
          else {
            state = 9998;
          }
        } break; // end case state5
      // ------------------- generate STATE 6
      // ---------- generate() by TPV_VALUE id 6
      //
      case 6: // 6
        {
          if ( isDecimal(c) ) {
            value = value * 10 + valueDecimal(c);
            state = 7;
          }
          else if ( c == '\n' ) {

            {
              if (debug & 1) {
                printDebug_o_port_value();
              }
              if ( value == 0) {
                digitalWrite(port, LOW);
              }
              else {
                digitalWrite(port, HIGH);
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state6
      // create sub-states of VALUE
      // ------------------- STATE 7
      // ---------- generate() by TPV_VALUE2 id 7
      //
      case 7: // 7
        {
          if ( isDecimal(c) ) {
            value = value * 10 + valueDecimal(c);
            state = 8;
          }
          else if ( c == '\n' ) {

            {
              if (debug & 1) {
                printDebug_o_port_value();
              }
              if ( value == 0) {
                digitalWrite(port, LOW);
              }
              else {
                digitalWrite(port, HIGH);
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state7
      // ------------------- STATE 8
      // ---------- generate() by TPV_VALUE2 id 8
      //
      case 8: // 8
        {
          if ( c == '\n' ) {

            {
              if (debug & 1) {
                printDebug_o_port_value();
              }
              if ( value == 0) {
                digitalWrite(port, LOW);
              }
              else {
                digitalWrite(port, HIGH);
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state8
      // ------------------- STATE 9
      // ---------- generate() by TPV_VALUE3 id 9
      //
      case 9: // 9
        {
          {
            state = 9998;
          }
        } break; // end case state9
      // create child nodes of VALUE
      // ------------------- STATE 11
      // ---------- generated by TPC id 11
      //
      case 11: // 11
        {
          if ( c == ':' ) {
            state = 12;
          }
          else {
            state = 9998;
          }
        } break; // end case state11
      // ------------------- STATE 12
      // ---------- generated by TPC id 12
      //
      case 12: // 12
        {
          if ( isDecimal(c) ) {
            port = valueDecimal(c);
            state = 13;
          }
          else {
            state = 9998;
          }
        } break; // end case state12
      // ------------------- generate STATE 13
      // ---------- generate() by TPV_PORT id 13
      //
      case 13: // 13
        {
          if ( isDecimal(c) ) {
            port = port * 10 + valueDecimal(c);
            state = 14;
          }
          else if ( c == ',' ) {
            state = 15;
          }
          else {
            state = 9998;
          }
        } break; // end case state13
      // create sub-states of VALUE
      // ------------------- generate STATE 14
      // ---------- generate() by TPV_PORT2 id 14
      //
      case 14: // 14
        {
          if ( c == ',' ) {
            state = 15;
          }
          else {
            state = 9998;
          }
        } break; // end case state14
      // create child nodes of VALUE
      // ------------------- STATE 15
      // ---------- generated by TPC id 15
      //
      case 15: // 15
        {
          if ( isDecimal(c) ) {
            value = valueDecimal(c);
            state = 16;
          }
          else {
            state = 9998;
          }
        } break; // end case state15
      // ------------------- generate STATE 16
      // ---------- generate() by TPV_VALUE id 16
      //
      case 16: // 16
        {
          if ( isDecimal(c) ) {
            value = value * 10 + valueDecimal(c);
            state = 17;
          }
          else if ( c == '\n' ) {

            {
              if (debug & 1) {
                printDebug_oa_port_value();

              }
              if ( value == 0) {
                analogDigitalWrite(port, LOW);
              }
              else {
                analogDigitalWrite(port, HIGH);
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state16
      // create sub-states of VALUE
      // ------------------- STATE 17
      // ---------- generate() by TPV_VALUE2 id 17
      //
      case 17: // 17
        {
          if ( isDecimal(c) ) {
            value = value * 10 + valueDecimal(c);
            state = 18;
          }
          else if ( c == '\n' ) {

            {
              if (debug & 1) {
                printDebug_oa_port_value();

              }
              if ( value == 0) {
                analogDigitalWrite(port, LOW);
              }
              else {
                analogDigitalWrite(port, HIGH);
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state17
      // ------------------- STATE 18
      // ---------- generate() by TPV_VALUE2 id 18
      //
      case 18: // 18
        {
          if ( c == '\n' ) {

            {
              if (debug & 1) {
                printDebug_oa_port_value();

              }
              if ( value == 0) {
                analogDigitalWrite(port, LOW);
              }
              else {
                analogDigitalWrite(port, HIGH);
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state18
      // ------------------- STATE 19
      // ---------- generate() by TPV_VALUE3 id 19
      //
      case 19: // 19
        {
          {
            state = 9998;
          }
        } break; // end case state19
      // create child nodes of VALUE
      // ------------------- STATE 21
      // ---------- generated by TPC id 21
      //
      case 21: // 21
        {
          if ( c == ':' ) {
            state = 22;
          }
          else {
            state = 9998;
          }
        } break; // end case state21
      // ------------------- STATE 22
      // ---------- generated by TPC id 22
      //
      case 22: // 22
        {
          if ( isDecimal(c) ) {
            port = valueDecimal(c);
            state = 23;
          }
          else {
            state = 9998;
          }
        } break; // end case state22
      // ------------------- generate STATE 23
      // ---------- generate() by TPV_PORT id 23
      //
      case 23: // 23
        {
          if ( isDecimal(c) ) {
            port = port * 10 + valueDecimal(c);
            state = 24;
          }
          else if ( c == ',' ) {
            state = 25;
          }
          else {
            state = 9998;
          }
        } break; // end case state23
      // create sub-states of VALUE
      // ------------------- generate STATE 24
      // ---------- generate() by TPV_PORT2 id 24
      //
      case 24: // 24
        {
          if ( c == ',' ) {
            state = 25;
          }
          else {
            state = 9998;
          }
        } break; // end case state24
      // create child nodes of VALUE
      // ------------------- STATE 25
      // ---------- generated by TPC id 25
      //
      case 25: // 25
        {
          if ( isDecimal(c) ) {
            value = valueDecimal(c);
            state = 26;
          }
          else {
            state = 9998;
          }
        } break; // end case state25
      // ------------------- generate STATE 26
      // ---------- generate() by TPV_VALUE id 26
      //
      case 26: // 26
        {
          if ( isDecimal(c) ) {
            value = value * 10 + valueDecimal(c);
            state = 27;
          }
          else if ( c == '\n' ) {

            {
              if (debug & 1) {
                printDebug_p_port_value();
              }
              if ( pwms & (1 << port)) {
                analogWrite(port, value);
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state26
      // create sub-states of VALUE
      // ------------------- STATE 27
      // ---------- generate() by TPV_VALUE2 id 27
      //
      case 27: // 27
        {
          if ( isDecimal(c) ) {
            value = value * 10 + valueDecimal(c);
            state = 28;
          }
          else if ( c == '\n' ) {

            {
              if (debug & 1) {
                printDebug_p_port_value();
              }
              if ( pwms & (1 << port)) {
                analogWrite(port, value);
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state27
      // ------------------- STATE 28
      // ---------- generate() by TPV_VALUE2 id 28
      //
      case 28: // 28
        {
          if ( c == '\n' ) {

            {
              if (debug & 1) {
                printDebug_p_port_value();
              }
              if ( pwms & (1 << port)) {
                analogWrite(port, value);
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state28
      // ------------------- STATE 29
      // ---------- generate() by TPV_VALUE3 id 29
      //
      case 29: // 29
        {
          {
            state = 9998;
          }
        } break; // end case state29
      // create child nodes of VALUE
      // ------------------- STATE 31
      // ---------- generated by TPC id 31
      //
      case 31: // 31
        {
          if ( c == ':' ) {
            state = 32;
          }
          else {
            state = 9998;
          }
        } break; // end case state31
      // ------------------- STATE 32
      // ---------- generated by TPC id 32
      //
      case 32: // 32
        {
          if ( isDecimal(c) ) {
            port = valueDecimal(c);
            state = 33;
          }
          else {
            state = 9998;
          }
        } break; // end case state32
      // ------------------- generate STATE 33
      // ---------- generate() by TPV_PORT id 33
      //
      case 33: // 33
        {
          if ( isDecimal(c) ) {
            port = port * 10 + valueDecimal(c);
            state = 34;
          }
          else if ( c == ',' ) {
            state = 35;
          }
          else {
            state = 9998;
          }
        } break; // end case state33
      // create sub-states of VALUE
      // ------------------- generate STATE 34
      // ---------- generate() by TPV_PORT2 id 34
      //
      case 34: // 34
        {
          if ( c == ',' ) {
            state = 35;
          }
          else {
            state = 9998;
          }
        } break; // end case state34
      // create child nodes of VALUE
      // ------------------- STATE 35
      // ---------- generated by TPC id 35
      //
      case 35: // 35
        {
          if ( isDecimal(c) ) {
            value = valueDecimal(c);
            state = 36;
          }
          else {
            state = 9998;
          }
        } break; // end case state35
      // ------------------- generate STATE 36
      // ---------- generate() by TPV_VALUE id 36
      //
      case 36: // 36
        {
          if ( isDecimal(c) ) {
            value = value * 10 + valueDecimal(c);
            state = 37;
          }
          else if ( c == '\n' ) {

            {
              if (debug & 1) {
                printDebug_s_port_value();
              }
              if ( servos & (1 << port)) {
                // Serial.println("config ok");
                if ( servoObject[port] != NULL )
                  //Serial.println("config not null");
                  servoObject[port]->write( value);
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state36
      // create sub-states of VALUE
      // ------------------- STATE 37
      // ---------- generate() by TPV_VALUE2 id 37
      //
      case 37: // 37
        {
          if ( isDecimal(c) ) {
            value = value * 10 + valueDecimal(c);
            state = 38;
          }
          else if ( c == '\n' ) {

            {
              if (debug & 1) {
                printDebug_s_port_value();
              }
              if ( servos & (1 << port)) {
                // Serial.println("config ok");
                if ( servoObject[port] != NULL )
                  //Serial.println("config not null");
                  servoObject[port]->write( value);
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state37
      // ------------------- STATE 38
      // ---------- generate() by TPV_VALUE2 id 38
      //
      case 38: // 38
        {
          if ( c == '\n' ) {

            {
              if (debug & 1) {
                printDebug_s_port_value();
              }
              if ( servos & (1 << port)) {
                // Serial.println("config ok");
                if ( servoObject[port] != NULL )
                  //Serial.println("config not null");
                  servoObject[port]->write( value);
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state38
      // ------------------- STATE 39
      // ---------- generate() by TPV_VALUE3 id 39
      //
      case 39: // 39
        {
          {
            state = 9998;
          }
        } break; // end case state39
      // create child nodes of VALUE
      // ------------------- STATE 41
      // ---------- generated by TPC id 41
      //
      case 41: // 41
        {
          if ( c == 'e' ) {
            state = 42;
          }
          else {
            state = 9998;
          }
        } break; // end case state41
      // ------------------- STATE 42
      // ---------- generated by TPC id 42
      //
      case 42: // 42
        {
          if ( c == 'l' ) {
            state = 43;
          }
          else {
            state = 9998;
          }
        } break; // end case state42
      // ------------------- STATE 43
      // ---------- generated by TPC id 43
      //
      case 43: // 43
        {
          if ( c == 'p' ) {
            state = 44;
          }
          else {
            state = 9998;
          }
        } break; // end case state43
      // ------------------- STATE 44
      // ---------- generated by TPC id 44
      //
      case 44: // 44
        {
          if ( c == '\n' ) {

            {
              for ( int k = 0; TRUE; k ++ ) {
                char  c =  pgm_read_byte_near(helpText + k);
                if ( c == 0 ) break;
                Serial.print(c);
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state44
      // ------------------- STATE 46
      // ---------- generated by TPC id 46
      //
      case 46: // 46
        {
          if ( c == 'i' ) {
            state = 47;
          }
          else if ( c == 'd' ) {
            state = 73;
          }
          else if ( c == 'a' ) {
            state = 152;
          }
          else if ( c == 'v' ) {
            state = 212;
          }
          else if ( c == 'e' ) {
            state = 221;
          }
          else {
            state = 9998;
          }
        } break; // end case state46
      // ------------------- STATE 47
      // ---------- generated by TPC id 47
      //
      case 47: // 47
        {
          if ( c == 'd' ) {
            state = 48;
          }
          else {
            state = 9998;
          }
        } break; // end case state47
      // ------------------- STATE 48
      // ---------- generated by TPC id 48
      //
      case 48: // 48
        {
          if ( c == 'e' ) {
            state = 49;
          }
          else {
            state = 9998;
          }
        } break; // end case state48
      // ------------------- STATE 49
      // ---------- generated by TPC id 49
      //
      case 49: // 49
        {
          if ( c == 'n' ) {
            state = 50;
          }
          else {
            state = 9998;
          }
        } break; // end case state49
      // ------------------- STATE 50
      // ---------- generated by TPC id 50
      //
      case 50: // 50
        {
          if ( c == 't' ) {
            state = 51;
          }
          else {
            state = 9998;
          }
        } break; // end case state50
      // ------------------- STATE 51
      // ---------- generated by TPC id 51
      //
      case 51: // 51
        {
          if ( c == ':' ) {
            state = 52;
          }
          else if ( c == '?' ) {
            state = 71;
          }
          else {
            state = 9998;
          }
        } break; // end case state51
      // ------------------- STATE 52
      // ---------- generated by TPC id 52
      //
      case 52: // 52
        {
          if ( (c != '\n') ) {
            id[1] = 0; id[0] = c;
            state = 53;
          }
          else {
            state = 9998;
          }
        } break; // end case state52
      // ------------------- generate STATE 53
      // ---------- generate() by TPV_CHAR id 53
      //
      case 53: // 53
        {
          if ( (c != '\n') ) {
            id[ 1 + 1] = 0;
            id[ 1 ] = c;
            state = 54;
          }
          else {
            state = 9998;
          }
        } break; // end case state53
      // create sub-states of CHAR
      // ------------------- generate STATE 54
      // ---------- generate() by TPV_CHAR2 id 54
      //
      case 54: // 54
        {
          if ( (c != '\n') ) {
            id[ 2 + 1] = 0;
            id[ 2 ] = c;
            state = 55;
          }
          else if ( c == '\n' ) {

            {
              setEEPROM();
              if ( debug & 1) {
                Serial.print("cident=");
                Serial.println(id);
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state54
      // ------------------- generate STATE 55
      // ---------- generate() by TPV_CHAR2 id 55
      //
      case 55: // 55
        {
          if ( (c != '\n') ) {
            id[ 3 + 1] = 0;
            id[ 3 ] = c;
            state = 56;
          }
          else if ( c == '\n' ) {

            {
              setEEPROM();
              if ( debug & 1) {
                Serial.print("cident=");
                Serial.println(id);
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state55
      // ------------------- generate STATE 56
      // ---------- generate() by TPV_CHAR2 id 56
      //
      case 56: // 56
        {
          if ( (c != '\n') ) {
            id[ 4 + 1] = 0;
            id[ 4 ] = c;
            state = 57;
          }
          else if ( c == '\n' ) {

            {
              setEEPROM();
              if ( debug & 1) {
                Serial.print("cident=");
                Serial.println(id);
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state56
      // ------------------- generate STATE 57
      // ---------- generate() by TPV_CHAR2 id 57
      //
      case 57: // 57
        {
          if ( (c != '\n') ) {
            id[ 5 + 1] = 0;
            id[ 5 ] = c;
            state = 58;
          }
          else if ( c == '\n' ) {

            {
              setEEPROM();
              if ( debug & 1) {
                Serial.print("cident=");
                Serial.println(id);
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state57
      // ------------------- generate STATE 58
      // ---------- generate() by TPV_CHAR2 id 58
      //
      case 58: // 58
        {
          if ( (c != '\n') ) {
            id[ 6 + 1] = 0;
            id[ 6 ] = c;
            state = 59;
          }
          else if ( c == '\n' ) {

            {
              setEEPROM();
              if ( debug & 1) {
                Serial.print("cident=");
                Serial.println(id);
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state58
      // ------------------- generate STATE 59
      // ---------- generate() by TPV_CHAR2 id 59
      //
      case 59: // 59
        {
          if ( (c != '\n') ) {
            id[ 7 + 1] = 0;
            id[ 7 ] = c;
            state = 60;
          }
          else if ( c == '\n' ) {

            {
              setEEPROM();
              if ( debug & 1) {
                Serial.print("cident=");
                Serial.println(id);
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state59
      // ------------------- generate STATE 60
      // ---------- generate() by TPV_CHAR2 id 60
      //
      case 60: // 60
        {
          if ( (c != '\n') ) {
            id[ 8 + 1] = 0;
            id[ 8 ] = c;
            state = 61;
          }
          else if ( c == '\n' ) {

            {
              setEEPROM();
              if ( debug & 1) {
                Serial.print("cident=");
                Serial.println(id);
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state60
      // ------------------- generate STATE 61
      // ---------- generate() by TPV_CHAR2 id 61
      //
      case 61: // 61
        {
          if ( (c != '\n') ) {
            id[ 9 + 1] = 0;
            id[ 9 ] = c;
            state = 62;
          }
          else if ( c == '\n' ) {

            {
              setEEPROM();
              if ( debug & 1) {
                Serial.print("cident=");
                Serial.println(id);
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state61
      // ------------------- generate STATE 62
      // ---------- generate() by TPV_CHAR2 id 62
      //
      case 62: // 62
        {
          if ( (c != '\n') ) {
            id[ 10 + 1] = 0;
            id[ 10 ] = c;
            state = 63;
          }
          else if ( c == '\n' ) {

            {
              setEEPROM();
              if ( debug & 1) {
                Serial.print("cident=");
                Serial.println(id);
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state62
      // ------------------- generate STATE 63
      // ---------- generate() by TPV_CHAR2 id 63
      //
      case 63: // 63
        {
          if ( (c != '\n') ) {
            id[ 11 + 1] = 0;
            id[ 11 ] = c;
            state = 64;
          }
          else if ( c == '\n' ) {

            {
              setEEPROM();
              if ( debug & 1) {
                Serial.print("cident=");
                Serial.println(id);
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state63
      // ------------------- generate STATE 64
      // ---------- generate() by TPV_CHAR2 id 64
      //
      case 64: // 64
        {
          if ( (c != '\n') ) {
            id[ 12 + 1] = 0;
            id[ 12 ] = c;
            state = 65;
          }
          else if ( c == '\n' ) {

            {
              setEEPROM();
              if ( debug & 1) {
                Serial.print("cident=");
                Serial.println(id);
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state64
      // ------------------- generate STATE 65
      // ---------- generate() by TPV_CHAR2 id 65
      //
      case 65: // 65
        {
          if ( (c != '\n') ) {
            id[ 13 + 1] = 0;
            id[ 13 ] = c;
            state = 66;
          }
          else if ( c == '\n' ) {

            {
              setEEPROM();
              if ( debug & 1) {
                Serial.print("cident=");
                Serial.println(id);
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state65
      // ------------------- generate STATE 66
      // ---------- generate() by TPV_CHAR2 id 66
      //
      case 66: // 66
        {
          if ( (c != '\n') ) {
            id[ 14 + 1] = 0;
            id[ 14 ] = c;
            state = 67;
          }
          else if ( c == '\n' ) {

            {
              setEEPROM();
              if ( debug & 1) {
                Serial.print("cident=");
                Serial.println(id);
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state66
      // ------------------- generate STATE 67
      // ---------- generate() by TPV_CHAR2 id 67
      //
      case 67: // 67
        {
          if ( (c != '\n') ) {
            id[ 15 + 1] = 0;
            id[ 15 ] = c;
            state = 68;
          }
          else if ( c == '\n' ) {

            {
              setEEPROM();
              if ( debug & 1) {
                Serial.print("cident=");
                Serial.println(id);
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state67
      // ------------------- generate STATE 68
      // ---------- generate() by TPV_CHAR2 id 68
      //
      case 68: // 68
        {
          if ( c == '\n' ) {

            {
              setEEPROM();
              if ( debug & 1) {
                Serial.print("cident=");
                Serial.println(id);
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state68
      // ------------------- generate STATE 69
      // ---------- generate() by TPV_CHAR3 id 69
      //
      case 69: // 69
        {
          if ( c == '\n' ) {

            {
              setEEPROM();
              if ( debug & 1) {
                Serial.print("cident=");
                Serial.println(id);
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state69
      // create child nodes of CHAR
      // ------------------- STATE 71
      // ---------- generated by TPC id 71
      //
      case 71: // 71
        {
          if ( c == '\n' ) {

            {
              getEEPROM();
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state71
      // ------------------- STATE 73
      // ---------- generated by TPC id 73
      //
      case 73: // 73
        {
          if ( c == 'i' ) {
            state = 74;
          }
          else if ( c == 'e' ) {
            state = 86;
          }
          else if ( c == 'o' ) {
            state = 111;
          }
          else if ( c == 'p' ) {
            state = 124;
          }
          else if ( c == 's' ) {
            state = 137;
          }
          else {
            state = 9998;
          }
        } break; // end case state73
      // ------------------- STATE 74
      // ---------- generated by TPC id 74
      //
      case 74: // 74
        {
          if ( c == 'n' ) {
            state = 75;
          }
          else {
            state = 9998;
          }
        } break; // end case state74
      // ------------------- STATE 75
      // ---------- generated by TPC id 75
      //
      case 75: // 75
        {
          if ( c == ':' ) {
            state = 76;
          }
          else if ( c == 'p' ) {
            state = 100;
          }
          else {
            state = 9998;
          }
        } break; // end case state75
      // ------------------- STATE 76
      // ---------- generated by TPC id 76
      //
      case 76: // 76
        {
          if ( isHex(c) ) {
            data = valueHex(c);
            state = 77;
          }
          else {
            state = 9998;
          }
        } break; // end case state76
      // ------------------- generate STATE 77
      // ---------- generate() by TPV_HEX id 77
      //
      case 77: // 77
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 78;
          }
          else {
            state = 9998;
          }
        } break; // end case state77
      // create sub-states of VALUE
      // ------------------- generate STATE 78
      // ---------- generate() by TPV_HEX2 id 78
      //
      case 78: // 78
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 79;
          }
          else if ( c == '\n' ) {

            {
              setDigitalInput(data);
              if ( debug & 1) {
                Serial.print("cdin=");
                Serial.println(data, HEX);
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state78
      // ------------------- generate STATE 79
      // ---------- generate() by TPV_HEX2 id 79
      //
      case 79: // 79
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 80;
          }
          else if ( c == '\n' ) {

            {
              setDigitalInput(data);
              if ( debug & 1) {
                Serial.print("cdin=");
                Serial.println(data, HEX);
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state79
      // ------------------- generate STATE 80
      // ---------- generate() by TPV_HEX2 id 80
      //
      case 80: // 80
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 81;
          }
          else if ( c == '\n' ) {

            {
              setDigitalInput(data);
              if ( debug & 1) {
                Serial.print("cdin=");
                Serial.println(data, HEX);
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state80
      // ------------------- generate STATE 81
      // ---------- generate() by TPV_HEX2 id 81
      //
      case 81: // 81
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 82;
          }
          else if ( c == '\n' ) {

            {
              setDigitalInput(data);
              if ( debug & 1) {
                Serial.print("cdin=");
                Serial.println(data, HEX);
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state81
      // ------------------- generate STATE 82
      // ---------- generate() by TPV_HEX2 id 82
      //
      case 82: // 82
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 83;
          }
          else if ( c == '\n' ) {

            {
              setDigitalInput(data);
              if ( debug & 1) {
                Serial.print("cdin=");
                Serial.println(data, HEX);
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state82
      // ------------------- generate STATE 83
      // ---------- generate() by TPV_HEX2 id 83
      //
      case 83: // 83
        {
          if ( c == '\n' ) {

            {
              setDigitalInput(data);
              if ( debug & 1) {
                Serial.print("cdin=");
                Serial.println(data, HEX);
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state83
      // ------------------- generate STATE 84
      // ---------- generate() by TPV_HEX3 id 84
      //
      case 84: // 84
        {
          if ( c == '\n' ) {

            {
              setDigitalInput(data);
              if ( debug & 1) {
                Serial.print("cdin=");
                Serial.println(data, HEX);
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state84
      // create child nodes of VALUE
      // ------------------- STATE 100
      // ---------- generated by TPC id 100
      //
      case 100: // 100
        {
          if ( c == ':' ) {
            state = 101;
          }
          else {
            state = 9998;
          }
        } break; // end case state100
      // ------------------- STATE 101
      // ---------- generated by TPC id 101
      //
      case 101: // 101
        {
          if ( isHex(c) ) {
            data = valueHex(c);
            state = 102;
          }
          else {
            state = 9998;
          }
        } break; // end case state101
      // ------------------- generate STATE 102
      // ---------- generate() by TPV_HEX id 102
      //
      case 102: // 102
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 103;
          }
          else {
            state = 9998;
          }
        } break; // end case state102
      // create sub-states of VALUE
      // ------------------- generate STATE 103
      // ---------- generate() by TPV_HEX2 id 103
      //
      case 103: // 103
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 104;
          }
          else if ( c == '\n' ) {

            {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalInputPullup(data);
              if ( debug & 1) {
                printDebug_cdinp();

              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state103
      // ------------------- generate STATE 104
      // ---------- generate() by TPV_HEX2 id 104
      //
      case 104: // 104
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 105;
          }
          else if ( c == '\n' ) {

            {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalInputPullup(data);
              if ( debug & 1) {
                printDebug_cdinp();

              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state104
      // ------------------- generate STATE 105
      // ---------- generate() by TPV_HEX2 id 105
      //
      case 105: // 105
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 106;
          }
          else if ( c == '\n' ) {

            {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalInputPullup(data);
              if ( debug & 1) {
                printDebug_cdinp();

              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state105
      // ------------------- generate STATE 106
      // ---------- generate() by TPV_HEX2 id 106
      //
      case 106: // 106
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 107;
          }
          else if ( c == '\n' ) {

            {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalInputPullup(data);
              if ( debug & 1) {
                printDebug_cdinp();

              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state106
      // ------------------- generate STATE 107
      // ---------- generate() by TPV_HEX2 id 107
      //
      case 107: // 107
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 108;
          }
          else if ( c == '\n' ) {

            {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalInputPullup(data);
              if ( debug & 1) {
                printDebug_cdinp();

              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state107
      // ------------------- generate STATE 108
      // ---------- generate() by TPV_HEX2 id 108
      //
      case 108: // 108
        {
          if ( c == '\n' ) {

            {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalInputPullup(data);
              if ( debug & 1) {
                printDebug_cdinp();

              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state108
      // ------------------- generate STATE 109
      // ---------- generate() by TPV_HEX3 id 109
      //
      case 109: // 109
        {
          if ( c == '\n' ) {

            {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalInputPullup(data);
              if ( debug & 1) {
                printDebug_cdinp();

              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state109
      // create child nodes of VALUE
      // ------------------- STATE 86
      // ---------- generated by TPC id 86
      //
      case 86: // 86
        {
          if ( c == 'b' ) {
            state = 87;
          }
          else {
            state = 9998;
          }
        } break; // end case state86
      // ------------------- STATE 87
      // ---------- generated by TPC id 87
      //
      case 87: // 87
        {
          if ( c == 'u' ) {
            state = 88;
          }
          else {
            state = 9998;
          }
        } break; // end case state87
      // ------------------- STATE 88
      // ---------- generated by TPC id 88
      //
      case 88: // 88
        {
          if ( c == 'g' ) {
            state = 89;
          }
          else {
            state = 9998;
          }
        } break; // end case state88
      // ------------------- STATE 89
      // ---------- generated by TPC id 89
      //
      case 89: // 89
        {
          if ( c == ':' ) {
            state = 90;
          }
          else {
            state = 9998;
          }
        } break; // end case state89
      // ------------------- STATE 90
      // ---------- generated by TPC id 90
      //
      case 90: // 90
        {
          if ( isHex(c) ) {
            data = valueHex(c);
            state = 91;
          }
          else {
            state = 9998;
          }
        } break; // end case state90
      // ------------------- generate STATE 91
      // ---------- generate() by TPV_HEX id 91
      //
      case 91: // 91
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 92;
          }
          else {
            state = 9998;
          }
        } break; // end case state91
      // create sub-states of VALUE
      // ------------------- generate STATE 92
      // ---------- generate() by TPV_HEX2 id 92
      //
      case 92: // 92
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 93;
          }
          else if ( c == '\n' ) {

            {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              debug =  data;
              if ( debug & 1) {
                printDebug_cdebug();
              }

            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state92
      // ------------------- generate STATE 93
      // ---------- generate() by TPV_HEX2 id 93
      //
      case 93: // 93
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 94;
          }
          else if ( c == '\n' ) {

            {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              debug =  data;
              if ( debug & 1) {
                printDebug_cdebug();
              }

            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state93
      // ------------------- generate STATE 94
      // ---------- generate() by TPV_HEX2 id 94
      //
      case 94: // 94
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 95;
          }
          else if ( c == '\n' ) {

            {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              debug =  data;
              if ( debug & 1) {
                printDebug_cdebug();
              }

            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state94
      // ------------------- generate STATE 95
      // ---------- generate() by TPV_HEX2 id 95
      //
      case 95: // 95
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 96;
          }
          else if ( c == '\n' ) {

            {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              debug =  data;
              if ( debug & 1) {
                printDebug_cdebug();
              }

            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state95
      // ------------------- generate STATE 96
      // ---------- generate() by TPV_HEX2 id 96
      //
      case 96: // 96
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 97;
          }
          else if ( c == '\n' ) {

            {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              debug =  data;
              if ( debug & 1) {
                printDebug_cdebug();
              }

            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state96
      // ------------------- generate STATE 97
      // ---------- generate() by TPV_HEX2 id 97
      //
      case 97: // 97
        {
          if ( c == '\n' ) {

            {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              debug =  data;
              if ( debug & 1) {
                printDebug_cdebug();
              }

            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state97
      // ------------------- generate STATE 98
      // ---------- generate() by TPV_HEX3 id 98
      //
      case 98: // 98
        {
          if ( c == '\n' ) {

            {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              debug =  data;
              if ( debug & 1) {
                printDebug_cdebug();
              }

            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state98
      // create child nodes of VALUE
      // ------------------- STATE 111
      // ---------- generated by TPC id 111
      //
      case 111: // 111
        {
          if ( c == 'u' ) {
            state = 112;
          }
          else {
            state = 9998;
          }
        } break; // end case state111
      // ------------------- STATE 112
      // ---------- generated by TPC id 112
      //
      case 112: // 112
        {
          if ( c == 't' ) {
            state = 113;
          }
          else {
            state = 9998;
          }
        } break; // end case state112
      // ------------------- STATE 113
      // ---------- generated by TPC id 113
      //
      case 113: // 113
        {
          if ( c == ':' ) {
            state = 114;
          }
          else {
            state = 9998;
          }
        } break; // end case state113
      // ------------------- STATE 114
      // ---------- generated by TPC id 114
      //
      case 114: // 114
        {
          if ( isHex(c) ) {
            data = valueHex(c);
            state = 115;
          }
          else {
            state = 9998;
          }
        } break; // end case state114
      // ------------------- generate STATE 115
      // ---------- generate() by TPV_HEX id 115
      //
      case 115: // 115
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 116;
          }
          else {
            state = 9998;
          }
        } break; // end case state115
      // create sub-states of VALUE
      // ------------------- generate STATE 116
      // ---------- generate() by TPV_HEX2 id 116
      //
      case 116: // 116
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 117;
          }
          else if ( c == '\n' ) {

            {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalOutput(data);
              if ( debug & 1) {
                printDebug_cdout();
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state116
      // ------------------- generate STATE 117
      // ---------- generate() by TPV_HEX2 id 117
      //
      case 117: // 117
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 118;
          }
          else if ( c == '\n' ) {

            {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalOutput(data);
              if ( debug & 1) {
                printDebug_cdout();
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state117
      // ------------------- generate STATE 118
      // ---------- generate() by TPV_HEX2 id 118
      //
      case 118: // 118
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 119;
          }
          else if ( c == '\n' ) {

            {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalOutput(data);
              if ( debug & 1) {
                printDebug_cdout();
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state118
      // ------------------- generate STATE 119
      // ---------- generate() by TPV_HEX2 id 119
      //
      case 119: // 119
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 120;
          }
          else if ( c == '\n' ) {

            {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalOutput(data);
              if ( debug & 1) {
                printDebug_cdout();
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state119
      // ------------------- generate STATE 120
      // ---------- generate() by TPV_HEX2 id 120
      //
      case 120: // 120
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 121;
          }
          else if ( c == '\n' ) {

            {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalOutput(data);
              if ( debug & 1) {
                printDebug_cdout();
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state120
      // ------------------- generate STATE 121
      // ---------- generate() by TPV_HEX2 id 121
      //
      case 121: // 121
        {
          if ( c == '\n' ) {

            {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalOutput(data);
              if ( debug & 1) {
                printDebug_cdout();
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state121
      // ------------------- generate STATE 122
      // ---------- generate() by TPV_HEX3 id 122
      //
      case 122: // 122
        {
          if ( c == '\n' ) {

            {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalOutput(data);
              if ( debug & 1) {
                printDebug_cdout();
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state122
      // create child nodes of VALUE
      // ------------------- STATE 124
      // ---------- generated by TPC id 124
      //
      case 124: // 124
        {
          if ( c == 'w' ) {
            state = 125;
          }
          else {
            state = 9998;
          }
        } break; // end case state124
      // ------------------- STATE 125
      // ---------- generated by TPC id 125
      //
      case 125: // 125
        {
          if ( c == 'm' ) {
            state = 126;
          }
          else {
            state = 9998;
          }
        } break; // end case state125
      // ------------------- STATE 126
      // ---------- generated by TPC id 126
      //
      case 126: // 126
        {
          if ( c == ':' ) {
            state = 127;
          }
          else {
            state = 9998;
          }
        } break; // end case state126
      // ------------------- STATE 127
      // ---------- generated by TPC id 127
      //
      case 127: // 127
        {
          if ( isHex(c) ) {
            data = valueHex(c);
            state = 128;
          }
          else {
            state = 9998;
          }
        } break; // end case state127
      // ------------------- generate STATE 128
      // ---------- generate() by TPV_HEX id 128
      //
      case 128: // 128
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 129;
          }
          else {
            state = 9998;
          }
        } break; // end case state128
      // create sub-states of VALUE
      // ------------------- generate STATE 129
      // ---------- generate() by TPV_HEX2 id 129
      //
      case 129: // 129
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 130;
          }
          else if ( c == '\n' ) {

            {
              pwms = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalPWMOutput(data);
              if ( debug & 1) {
                printDebug_cdpwm();
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state129
      // ------------------- generate STATE 130
      // ---------- generate() by TPV_HEX2 id 130
      //
      case 130: // 130
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 131;
          }
          else if ( c == '\n' ) {

            {
              pwms = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalPWMOutput(data);
              if ( debug & 1) {
                printDebug_cdpwm();
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state130
      // ------------------- generate STATE 131
      // ---------- generate() by TPV_HEX2 id 131
      //
      case 131: // 131
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 132;
          }
          else if ( c == '\n' ) {

            {
              pwms = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalPWMOutput(data);
              if ( debug & 1) {
                printDebug_cdpwm();
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state131
      // ------------------- generate STATE 132
      // ---------- generate() by TPV_HEX2 id 132
      //
      case 132: // 132
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 133;
          }
          else if ( c == '\n' ) {

            {
              pwms = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalPWMOutput(data);
              if ( debug & 1) {
                printDebug_cdpwm();
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state132
      // ------------------- generate STATE 133
      // ---------- generate() by TPV_HEX2 id 133
      //
      case 133: // 133
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 134;
          }
          else if ( c == '\n' ) {

            {
              pwms = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalPWMOutput(data);
              if ( debug & 1) {
                printDebug_cdpwm();
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state133
      // ------------------- generate STATE 134
      // ---------- generate() by TPV_HEX2 id 134
      //
      case 134: // 134
        {
          if ( c == '\n' ) {

            {
              pwms = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalPWMOutput(data);
              if ( debug & 1) {
                printDebug_cdpwm();
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state134
      // ------------------- generate STATE 135
      // ---------- generate() by TPV_HEX3 id 135
      //
      case 135: // 135
        {
          if ( c == '\n' ) {

            {
              pwms = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalPWMOutput(data);
              if ( debug & 1) {
                printDebug_cdpwm();
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state135
      // create child nodes of VALUE
      // ------------------- STATE 137
      // ---------- generated by TPC id 137
      //
      case 137: // 137
        {
          if ( c == 'e' ) {
            state = 138;
          }
          else {
            state = 9998;
          }
        } break; // end case state137
      // ------------------- STATE 138
      // ---------- generated by TPC id 138
      //
      case 138: // 138
        {
          if ( c == 'r' ) {
            state = 139;
          }
          else {
            state = 9998;
          }
        } break; // end case state138
      // ------------------- STATE 139
      // ---------- generated by TPC id 139
      //
      case 139: // 139
        {
          if ( c == 'v' ) {
            state = 140;
          }
          else {
            state = 9998;
          }
        } break; // end case state139
      // ------------------- STATE 140
      // ---------- generated by TPC id 140
      //
      case 140: // 140
        {
          if ( c == 'o' ) {
            state = 141;
          }
          else {
            state = 9998;
          }
        } break; // end case state140
      // ------------------- STATE 141
      // ---------- generated by TPC id 141
      //
      case 141: // 141
        {
          if ( c == ':' ) {
            state = 142;
          }
          else {
            state = 9998;
          }
        } break; // end case state141
      // ------------------- STATE 142
      // ---------- generated by TPC id 142
      //
      case 142: // 142
        {
          if ( isHex(c) ) {
            data = valueHex(c);
            state = 143;
          }
          else {
            state = 9998;
          }
        } break; // end case state142
      // ------------------- generate STATE 143
      // ---------- generate() by TPV_HEX id 143
      //
      case 143: // 143
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 144;
          }
          else {
            state = 9998;
          }
        } break; // end case state143
      // create sub-states of VALUE
      // ------------------- generate STATE 144
      // ---------- generate() by TPV_HEX2 id 144
      //
      case 144: // 144
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 145;
          }
          else if ( c == '\n' ) {

            {
              servos = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalServoOutput(data);
              if ( debug & 1) {
                printDebug_cdservo();

              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state144
      // ------------------- generate STATE 145
      // ---------- generate() by TPV_HEX2 id 145
      //
      case 145: // 145
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 146;
          }
          else if ( c == '\n' ) {

            {
              servos = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalServoOutput(data);
              if ( debug & 1) {
                printDebug_cdservo();

              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state145
      // ------------------- generate STATE 146
      // ---------- generate() by TPV_HEX2 id 146
      //
      case 146: // 146
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 147;
          }
          else if ( c == '\n' ) {

            {
              servos = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalServoOutput(data);
              if ( debug & 1) {
                printDebug_cdservo();

              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state146
      // ------------------- generate STATE 147
      // ---------- generate() by TPV_HEX2 id 147
      //
      case 147: // 147
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 148;
          }
          else if ( c == '\n' ) {

            {
              servos = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalServoOutput(data);
              if ( debug & 1) {
                printDebug_cdservo();

              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state147
      // ------------------- generate STATE 148
      // ---------- generate() by TPV_HEX2 id 148
      //
      case 148: // 148
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 149;
          }
          else if ( c == '\n' ) {

            {
              servos = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalServoOutput(data);
              if ( debug & 1) {
                printDebug_cdservo();

              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state148
      // ------------------- generate STATE 149
      // ---------- generate() by TPV_HEX2 id 149
      //
      case 149: // 149
        {
          if ( c == '\n' ) {

            {
              servos = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalServoOutput(data);
              if ( debug & 1) {
                printDebug_cdservo();

              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state149
      // ------------------- generate STATE 150
      // ---------- generate() by TPV_HEX3 id 150
      //
      case 150: // 150
        {
          if ( c == '\n' ) {

            {
              servos = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalServoOutput(data);
              if ( debug & 1) {
                printDebug_cdservo();

              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state150
      // create child nodes of VALUE
      // ------------------- STATE 152
      // ---------- generated by TPC id 152
      //
      case 152: // 152
        {
          if ( c == 'a' ) {
            state = 153;
          }
          else if ( c == 'd' ) {
            state = 166;
          }
          else {
            state = 9998;
          }
        } break; // end case state152
      // ------------------- STATE 153
      // ---------- generated by TPC id 153
      //
      case 153: // 153
        {
          if ( c == 'i' ) {
            state = 154;
          }
          else {
            state = 9998;
          }
        } break; // end case state153
      // ------------------- STATE 154
      // ---------- generated by TPC id 154
      //
      case 154: // 154
        {
          if ( c == 'n' ) {
            state = 155;
          }
          else {
            state = 9998;
          }
        } break; // end case state154
      // ------------------- STATE 155
      // ---------- generated by TPC id 155
      //
      case 155: // 155
        {
          if ( c == ':' ) {
            state = 156;
          }
          else {
            state = 9998;
          }
        } break; // end case state155
      // ------------------- STATE 156
      // ---------- generated by TPC id 156
      //
      case 156: // 156
        {
          if ( isHex(c) ) {
            data = valueHex(c);
            state = 157;
          }
          else {
            state = 9998;
          }
        } break; // end case state156
      // ------------------- generate STATE 157
      // ---------- generate() by TPV_HEX id 157
      //
      case 157: // 157
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 158;
          }
          else {
            state = 9998;
          }
        } break; // end case state157
      // create sub-states of VALUE
      // ------------------- generate STATE 158
      // ---------- generate() by TPV_HEX2 id 158
      //
      case 158: // 158
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 159;
          }
          else if ( c == '\n' ) {

            {
              analogAnalogInputs = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                printDebug_caain();
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state158
      // ------------------- generate STATE 159
      // ---------- generate() by TPV_HEX2 id 159
      //
      case 159: // 159
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 160;
          }
          else if ( c == '\n' ) {

            {
              analogAnalogInputs = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                printDebug_caain();
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state159
      // ------------------- generate STATE 160
      // ---------- generate() by TPV_HEX2 id 160
      //
      case 160: // 160
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 161;
          }
          else if ( c == '\n' ) {

            {
              analogAnalogInputs = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                printDebug_caain();
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state160
      // ------------------- generate STATE 161
      // ---------- generate() by TPV_HEX2 id 161
      //
      case 161: // 161
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 162;
          }
          else if ( c == '\n' ) {

            {
              analogAnalogInputs = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                printDebug_caain();
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state161
      // ------------------- generate STATE 162
      // ---------- generate() by TPV_HEX2 id 162
      //
      case 162: // 162
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 163;
          }
          else if ( c == '\n' ) {

            {
              analogAnalogInputs = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                printDebug_caain();
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state162
      // ------------------- generate STATE 163
      // ---------- generate() by TPV_HEX2 id 163
      //
      case 163: // 163
        {
          if ( c == '\n' ) {

            {
              analogAnalogInputs = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                printDebug_caain();
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state163
      // ------------------- generate STATE 164
      // ---------- generate() by TPV_HEX3 id 164
      //
      case 164: // 164
        {
          if ( c == '\n' ) {

            {
              analogAnalogInputs = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                printDebug_caain();
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state164
      // create child nodes of VALUE
      // ------------------- STATE 166
      // ---------- generated by TPC id 166
      //
      case 166: // 166
        {
          if ( c == 'i' ) {
            state = 167;
          }
          else if ( c == 'o' ) {
            state = 179;
          }
          else {
            state = 9998;
          }
        } break; // end case state166
      // ------------------- STATE 167
      // ---------- generated by TPC id 167
      //
      case 167: // 167
        {
          if ( c == 'n' ) {
            state = 168;
          }
          else {
            state = 9998;
          }
        } break; // end case state167
      // ------------------- STATE 168
      // ---------- generated by TPC id 168
      //
      case 168: // 168
        {
          if ( c == ':' ) {
            state = 169;
          }
          else if ( c == 'p' ) {
            state = 192;
          }
          else {
            state = 9998;
          }
        } break; // end case state168
      // ------------------- STATE 169
      // ---------- generated by TPC id 169
      //
      case 169: // 169
        {
          if ( isHex(c) ) {
            data = valueHex(c);
            state = 170;
          }
          else {
            state = 9998;
          }
        } break; // end case state169
      // ------------------- generate STATE 170
      // ---------- generate() by TPV_HEX id 170
      //
      case 170: // 170
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 171;
          }
          else {
            state = 9998;
          }
        } break; // end case state170
      // create sub-states of VALUE
      // ------------------- generate STATE 171
      // ---------- generate() by TPV_HEX2 id 171
      //
      case 171: // 171
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 172;
          }
          else if ( c == '\n' ) {

            {
              setAnalogDigitalInput(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                printDebug_cadin();
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state171
      // ------------------- generate STATE 172
      // ---------- generate() by TPV_HEX2 id 172
      //
      case 172: // 172
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 173;
          }
          else if ( c == '\n' ) {

            {
              setAnalogDigitalInput(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                printDebug_cadin();
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state172
      // ------------------- generate STATE 173
      // ---------- generate() by TPV_HEX2 id 173
      //
      case 173: // 173
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 174;
          }
          else if ( c == '\n' ) {

            {
              setAnalogDigitalInput(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                printDebug_cadin();
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state173
      // ------------------- generate STATE 174
      // ---------- generate() by TPV_HEX2 id 174
      //
      case 174: // 174
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 175;
          }
          else if ( c == '\n' ) {

            {
              setAnalogDigitalInput(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                printDebug_cadin();
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state174
      // ------------------- generate STATE 175
      // ---------- generate() by TPV_HEX2 id 175
      //
      case 175: // 175
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 176;
          }
          else if ( c == '\n' ) {

            {
              setAnalogDigitalInput(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                printDebug_cadin();
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state175
      // ------------------- generate STATE 176
      // ---------- generate() by TPV_HEX2 id 176
      //
      case 176: // 176
        {
          if ( c == '\n' ) {

            {
              setAnalogDigitalInput(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                printDebug_cadin();
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state176
      // ------------------- generate STATE 177
      // ---------- generate() by TPV_HEX3 id 177
      //
      case 177: // 177
        {
          if ( c == '\n' ) {

            {
              setAnalogDigitalInput(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                printDebug_cadin();
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state177
      // create child nodes of VALUE
      // ------------------- STATE 192
      // ---------- generated by TPC id 192
      //
      case 192: // 192
        {
          if ( c == ':' ) {
            state = 193;
          }
          else {
            state = 9998;
          }
        } break; // end case state192
      // ------------------- STATE 193
      // ---------- generated by TPC id 193
      //
      case 193: // 193
        {
          if ( isHex(c) ) {
            data = valueHex(c);
            state = 194;
          }
          else {
            state = 9998;
          }
        } break; // end case state193
      // ------------------- generate STATE 194
      // ---------- generate() by TPV_HEX id 194
      //
      case 194: // 194
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 195;
          }
          else {
            state = 9998;
          }
        } break; // end case state194
      // create sub-states of VALUE
      // ------------------- generate STATE 195
      // ---------- generate() by TPV_HEX2 id 195
      //
      case 195: // 195
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 196;
          }
          else if ( c == '\n' ) {

            {
              setAnalogDigitalInputPullup(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                printDebug_cadinp();
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state195
      // ------------------- generate STATE 196
      // ---------- generate() by TPV_HEX2 id 196
      //
      case 196: // 196
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 197;
          }
          else if ( c == '\n' ) {

            {
              setAnalogDigitalInputPullup(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                printDebug_cadinp();
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state196
      // ------------------- generate STATE 197
      // ---------- generate() by TPV_HEX2 id 197
      //
      case 197: // 197
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 198;
          }
          else if ( c == '\n' ) {

            {
              setAnalogDigitalInputPullup(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                printDebug_cadinp();
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state197
      // ------------------- generate STATE 198
      // ---------- generate() by TPV_HEX2 id 198
      //
      case 198: // 198
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 199;
          }
          else if ( c == '\n' ) {

            {
              setAnalogDigitalInputPullup(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                printDebug_cadinp();
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state198
      // ------------------- generate STATE 199
      // ---------- generate() by TPV_HEX2 id 199
      //
      case 199: // 199
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 200;
          }
          else if ( c == '\n' ) {

            {
              setAnalogDigitalInputPullup(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                printDebug_cadinp();
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state199
      // ------------------- generate STATE 200
      // ---------- generate() by TPV_HEX2 id 200
      //
      case 200: // 200
        {
          if ( c == '\n' ) {

            {
              setAnalogDigitalInputPullup(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                printDebug_cadinp();
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state200
      // ------------------- generate STATE 201
      // ---------- generate() by TPV_HEX3 id 201
      //
      case 201: // 201
        {
          if ( c == '\n' ) {

            {
              setAnalogDigitalInputPullup(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                printDebug_cadinp();
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state201
      // create child nodes of VALUE
      // ------------------- STATE 179
      // ---------- generated by TPC id 179
      //
      case 179: // 179
        {
          if ( c == 'u' ) {
            state = 180;
          }
          else {
            state = 9998;
          }
        } break; // end case state179
      // ------------------- STATE 180
      // ---------- generated by TPC id 180
      //
      case 180: // 180
        {
          if ( c == 't' ) {
            state = 181;
          }
          else {
            state = 9998;
          }
        } break; // end case state180
      // ------------------- STATE 181
      // ---------- generated by TPC id 181
      //
      case 181: // 181
        {
          if ( c == ':' ) {
            state = 182;
          }
          else {
            state = 9998;
          }
        } break; // end case state181
      // ------------------- STATE 182
      // ---------- generated by TPC id 182
      //
      case 182: // 182
        {
          if ( isHex(c) ) {
            data = valueHex(c);
            state = 183;
          }
          else {
            state = 9998;
          }
        } break; // end case state182
      // ------------------- generate STATE 183
      // ---------- generate() by TPV_HEX id 183
      //
      case 183: // 183
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 184;
          }
          else {
            state = 9998;
          }
        } break; // end case state183
      // create sub-states of VALUE
      // ------------------- generate STATE 184
      // ---------- generate() by TPV_HEX2 id 184
      //
      case 184: // 184
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 185;
          }
          else if ( c == '\n' ) {

            {
              setAnalogDigitalOutput(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                printDebug_cadout();
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state184
      // ------------------- generate STATE 185
      // ---------- generate() by TPV_HEX2 id 185
      //
      case 185: // 185
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 186;
          }
          else if ( c == '\n' ) {

            {
              setAnalogDigitalOutput(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                printDebug_cadout();
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state185
      // ------------------- generate STATE 186
      // ---------- generate() by TPV_HEX2 id 186
      //
      case 186: // 186
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 187;
          }
          else if ( c == '\n' ) {

            {
              setAnalogDigitalOutput(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                printDebug_cadout();
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state186
      // ------------------- generate STATE 187
      // ---------- generate() by TPV_HEX2 id 187
      //
      case 187: // 187
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 188;
          }
          else if ( c == '\n' ) {

            {
              setAnalogDigitalOutput(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                printDebug_cadout();
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state187
      // ------------------- generate STATE 188
      // ---------- generate() by TPV_HEX2 id 188
      //
      case 188: // 188
        {
          if ( isHex(c) ) {
            data = (data << 4) | valueHex(c);
            state = 189;
          }
          else if ( c == '\n' ) {

            {
              setAnalogDigitalOutput(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                printDebug_cadout();
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state188
      // ------------------- generate STATE 189
      // ---------- generate() by TPV_HEX2 id 189
      //
      case 189: // 189
        {
          if ( c == '\n' ) {

            {
              setAnalogDigitalOutput(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                printDebug_cadout();
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state189
      // ------------------- generate STATE 190
      // ---------- generate() by TPV_HEX3 id 190
      //
      case 190: // 190
        {
          if ( c == '\n' ) {

            {
              setAnalogDigitalOutput(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                printDebug_cadout();
              }
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state190
      // create child nodes of VALUE
      // ------------------- STATE 212
      // ---------- generated by TPC id 212
      //
      case 212: // 212
        {
          if ( c == 'e' ) {
            state = 213;
          }
          else {
            state = 9998;
          }
        } break; // end case state212
      // ------------------- STATE 213
      // ---------- generated by TPC id 213
      //
      case 213: // 213
        {
          if ( c == 'r' ) {
            state = 214;
          }
          else {
            state = 9998;
          }
        } break; // end case state213
      // ------------------- STATE 214
      // ---------- generated by TPC id 214
      //
      case 214: // 214
        {
          if ( c == 's' ) {
            state = 215;
          }
          else {
            state = 9998;
          }
        } break; // end case state214
      // ------------------- STATE 215
      // ---------- generated by TPC id 215
      //
      case 215: // 215
        {
          if ( c == 'i' ) {
            state = 216;
          }
          else {
            state = 9998;
          }
        } break; // end case state215
      // ------------------- STATE 216
      // ---------- generated by TPC id 216
      //
      case 216: // 216
        {
          if ( c == 'o' ) {
            state = 217;
          }
          else {
            state = 9998;
          }
        } break; // end case state216
      // ------------------- STATE 217
      // ---------- generated by TPC id 217
      //
      case 217: // 217
        {
          if ( c == 'n' ) {
            state = 218;
          }
          else {
            state = 9998;
          }
        } break; // end case state217
      // ------------------- STATE 218
      // ---------- generated by TPC id 218
      //
      case 218: // 218
        {
          if ( c == '?' ) {
            state = 219;
          }
          else {
            state = 9998;
          }
        } break; // end case state218
      // ------------------- STATE 219
      // ---------- generated by TPC id 219
      //
      case 219: // 219
        {
          if ( c == '\n' ) {

            {
              Serial.print("v:");
              Serial.println(version);
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state219
      // ------------------- STATE 221
      // ---------- generated by TPC id 221
      //
      case 221: // 221
        {
          if ( c == 'r' ) {
            state = 222;
          }
          else {
            state = 9998;
          }
        } break; // end case state221
      // ------------------- STATE 222
      // ---------- generated by TPC id 222
      //
      case 222: // 222
        {
          if ( c == 'r' ) {
            state = 223;
          }
          else {
            state = 9998;
          }
        } break; // end case state222
      // ------------------- STATE 223
      // ---------- generated by TPC id 223
      //
      case 223: // 223
        {
          if ( c == '?' ) {
            state = 224;
          }
          else {
            state = 9998;
          }
        } break; // end case state223
      // ------------------- STATE 224
      // ---------- generated by TPC id 224
      //
      case 224: // 224
        {
          if ( c == '\n' ) {

            {
              Serial.print("e:");
              Serial.println(errorCount);
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state224
      // ------------------- STATE 203
      // ---------- generated by TPC id 203
      //
      case 203: // 203
        {
          if ( c == 'e' ) {
            state = 204;
          }
          else {
            state = 9998;
          }
        } break; // end case state203
      // ------------------- STATE 204
      // ---------- generated by TPC id 204
      //
      case 204: // 204
        {
          if ( c == 'r' ) {
            state = 205;
          }
          else {
            state = 9998;
          }
        } break; // end case state204
      // ------------------- STATE 205
      // ---------- generated by TPC id 205
      //
      case 205: // 205
        {
          if ( c == 's' ) {
            state = 206;
          }
          else {
            state = 9998;
          }
        } break; // end case state205
      // ------------------- STATE 206
      // ---------- generated by TPC id 206
      //
      case 206: // 206
        {
          if ( c == 'i' ) {
            state = 207;
          }
          else {
            state = 9998;
          }
        } break; // end case state206
      // ------------------- STATE 207
      // ---------- generated by TPC id 207
      //
      case 207: // 207
        {
          if ( c == 'o' ) {
            state = 208;
          }
          else {
            state = 9998;
          }
        } break; // end case state207
      // ------------------- STATE 208
      // ---------- generated by TPC id 208
      //
      case 208: // 208
        {
          if ( c == 'n' ) {
            state = 209;
          }
          else {
            state = 9998;
          }
        } break; // end case state208
      // ------------------- STATE 209
      // ---------- generated by TPC id 209
      //
      case 209: // 209
        {
          if ( c == ':' ) {
            state = 210;
          }
          else {
            state = 9998;
          }
        } break; // end case state209
      // ------------------- STATE 210
      // ---------- generated by TPC id 210
      //
      case 210: // 210
        {
          if ( c == '\n' ) {

            {
              Serial.print("v:");
              Serial.println(version);
            }

            state = 9999;
          }
          else {
            state = 9998;
          }
        } break; // end case state210
    } // end switch state 0
    //--END

    if ( state == 9999) {
      state = 0;
    }
    if ( state == 9998) {
      errorCount ++;
      state = 0;
    }
  }

  if ( STATEMACHINE_waitState == 2000 ) {
    unsigned long currentMicros = micros();
    //
    // restrict update rate to 20Hz (performance reasons)
    //
    // generate 'events' each ms
    //
    if ((unsigned long)(currentMicros - digitalPreviousMicros) >= 1000L) {
      digitalPreviousMicros = currentMicros;

      if ( cnt == 0 )
        if ( digitalInputs & ( 1 <<  2) ) handleInput( 2, digitalRead( 2 ) );
      if ( cnt == 1 )
        if ( digitalInputs & ( 1 <<  3) ) handleInput( 3, digitalRead( 3 ) );
      if ( cnt == 2 )
        if ( digitalInputs & ( 1 <<  4) ) handleInput( 4, digitalRead( 4 ) );
      if ( cnt == 3 )
        if ( digitalInputs & ( 1 <<  5) ) handleInput( 5, digitalRead( 5 ) );
      if ( cnt == 4 )
        if ( digitalInputs & ( 1 <<  6) ) handleInput( 6, digitalRead( 6 ) );
      if ( cnt == 5 )
        if ( digitalInputs & ( 1 <<  7) ) handleInput( 7, digitalRead( 7 ) );
      if ( cnt == 6 )
        if ( digitalInputs & ( 1 <<  8) ) handleInput( 8, digitalRead( 8 ) );
      if ( cnt == 7 )
        if ( digitalInputs & ( 1 <<  9) ) handleInput( 9, digitalRead( 9 ) );
      if ( cnt == 8 )
        if ( digitalInputs & ( 1 << 10) ) handleInput(10, digitalRead(10 ) );
      if ( cnt == 9 )
        if ( digitalInputs & ( 1 << 11) ) handleInput(11, digitalRead(11 ) );
      if ( cnt == 10 )
        if ( digitalInputs & ( 1 << 12) ) handleInput(12, digitalRead(12 ) );

      if ( cnt == 11 )
        if ( analogDigitalInputs & ( 1 <<  0) ) handleAnalogDigitalInput( 0, digitalRead( A0 ) );
      if ( cnt == 12 )
        if ( analogDigitalInputs & ( 1 <<  1) ) handleAnalogDigitalInput( 1, digitalRead( A1 ) );
      if ( cnt == 13 )
        if ( analogDigitalInputs & ( 1 <<  2) ) handleAnalogDigitalInput( 2, digitalRead( A2 ) );
      if ( cnt == 14 )
        if ( analogDigitalInputs & ( 1 <<  3) ) handleAnalogDigitalInput( 3, digitalRead( A3 ) );
      if ( cnt == 15 )
        if ( analogDigitalInputs & ( 1 <<  4) ) handleAnalogDigitalInput( 4, digitalRead( A4 ) );
      if ( cnt == 16 )
        if ( analogDigitalInputs & ( 1 <<  5) ) handleAnalogDigitalInput( 5, digitalRead( A5 ) );
      // if ( cnt == 17 )
      //   if ( analogDigitalInputs & ( 1 <<  6) ) handleAnalogDigitalInput( 6, digitalRead( A6 ) );
      // if ( cnt == 18 )
      //   if ( analogDigitalInputs & ( 1 <<  7) ) handleAnalogDigitalInput( 7, digitalRead( A7 ) );

      cnt ++;
      if (cnt > 50 )
        cnt = 0;

      // ------------
      if ( analogAnalogInputs & ( 1 <<  0) ) {


        if (acnt == 0) {
          aval = analogRead( A0 );
        }
        if (acnt == 1) {
          aval += analogRead( A0 );
        }
        if (acnt == 2) {
          aval += analogRead( A0 );
        }
        if (acnt == 3) {
          handleAnalogAnalogInput( 0, aval / 3);
        }
      }
      // ------------
      if ( analogAnalogInputs & ( 1 <<  1) ) {
        if (acnt == 5)
          aval = analogRead( A1 );
        if (acnt == 6)
          aval += analogRead( A1 );
        if (acnt == 7)
          aval += analogRead( A1 );
        if (acnt == 8)
          handleAnalogAnalogInput( 1, aval / 3);
      }
      // ------------
      if ( analogAnalogInputs & ( 1 <<  2) ) {
        if (acnt == 10)
          aval = analogRead( A2 );
        if (acnt == 11)
          aval += analogRead( A2 );
        if (acnt == 12)
          aval += analogRead( A2 );
        if (acnt == 13)
          handleAnalogAnalogInput( 2, aval / 3);
      }
      // ------------
      if ( analogAnalogInputs & ( 1 <<  3) ) {
        if (acnt == 15)
          aval = analogRead( A3 );
        if (acnt == 16)
          aval += analogRead( A3 );
        if (acnt == 17)
          aval += analogRead( A3 );
        if (acnt == 18)
          handleAnalogAnalogInput( 3, aval / 3);
      }
      // ------------
      if ( analogAnalogInputs & ( 1 <<  4) ) {
        if (acnt == 20)
          aval = analogRead( A4 );
        if (acnt == 21)
          aval += analogRead( A4 );
        if (acnt == 22)
          aval += analogRead( A4 );
        if (acnt == 23)
          handleAnalogAnalogInput( 4, aval / 3);
      }
      // ------------
      if ( analogAnalogInputs & ( 1 <<  5) ) {
        if (acnt == 25)
          aval = analogRead( A5 );
        if (acnt == 26)
          aval += analogRead( A5 );
        if (acnt == 27)
          aval += analogRead( A5 );
        if (acnt == 28)
          handleAnalogAnalogInput( 5, aval / 3);
      }
      // ------------
      if ( analogAnalogInputs & ( 1 <<  6) ) {
        if (acnt == 30)
          aval = analogRead( A6 );
        if (acnt == 31)
          aval += analogRead( A6 );
        if (acnt == 32)
          aval += analogRead( A6 );
        if (acnt == 33)
          handleAnalogAnalogInput( 6, aval / 3);
      }
      // ------------
      if ( analogAnalogInputs & ( 1 <<  7) ) {
        if (acnt == 35)
          aval = analogRead( A7 );
        if (acnt == 36)
          aval += analogRead( A7 );
        if (acnt == 37)
          aval += analogRead( A7 );
        if (acnt == 38)
          handleAnalogAnalogInput( 7, aval / 3);
      }
      acnt ++;

      if (acnt > 100)
        acnt = 0;

    }
  }
}

