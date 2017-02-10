/*
    # --------------------------------------------------------------------------------------------
    # Copyright (C) 2015, 2017  Gerhard Hepp
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

    version 2017-02-01 optimized protocol handling state machine
    version 2017-01-27 optimized protocol, allow to reduce ':'
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

char version[] = "arduinoUno, version 2017-02-01";


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

  Serial.println( F("arduino sending@115200 Bd") );
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
// first char of ID, must be char

int isChar(char c) {
  if ( 'A' <= c  && c <= 'Z' ) return TRUE;
  if ( 'a' <= c  && c <= 'z' ) return TRUE;
  return FALSE;
}

// second, third char of label,must be char, digit or some special chars
int isLabel(char c) {
  if ( 'A' <= c  && c <= 'Z' ) return TRUE;
  if ( 'a' <= c  && c <= 'z' ) return TRUE;
  if ( '0' <= c  && c <= '9' ) return TRUE;
  if ( '-' == c  ) return TRUE;
  if ( '_' == c  ) return TRUE;
  if ( '.' == c  ) return TRUE;
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
    Serial.print( F("i"));
    Serial.print(port);
    Serial.print(F(","));
    Serial.println(value);
    lastResult[port] = value;
  }
}
void handleAnalogDigitalInput(int port, int value) {

  if ( value != lastAnalogDigitalResult[port] ) {
    Serial.print( F("ai") );
    Serial.print(port);
    Serial.print(F(","));
    Serial.println(value);
    lastAnalogDigitalResult[port] = value;
  }
}

void handleAnalogAnalogInput(int port, int value) {

  if ( value != lastAnalogAnalogResult[port] ) {
    Serial.print(F("a"));
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
  "char16 = [A-Za-z][A-Za-z0-9-_.]{1,15} \n"
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
  "data give bit patterns for IO pins\n"
  "\n"
  "Commands to set values in arduino\n"
  " o:<port>,<value>     write output\n"
  " o<port>,<value>      write output, shortcut\n"
  " oa:<port>,<value>    write output analog line\n"
  " oa<port>,<value>     write output analog line, shortcut\n"
  " pwm:<port>,<value>   write pwm\n"
  " p:<port>,<value>     write pwm\n"
  " p<port>,<value>      write pwm, shortcut\n"
  " servo:<port>,<value> write servo\n"
  " s:<port>,<value>     write servo\n"
  " s<port>,<value>      write servo, shortcut\n"
  "\n"
  "Values reported from arduino to host\n"
  " v:<version>       arduino reports version\n"
  " ident:<char16>    arduino reports ident from EEPROM\n"
  " e:<errors>        arduino reports number of errors (decimal)\n"
  " a<port>,<value>   arduino reports analog input\n"
  " i<port>,<value>   arduino reports digital input\n"
  " ai<port>,<value>  arduino reports digital input on analog line\n";

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
  // generated code 2017-02-01 19:58:46
  switch( state) { 
                                        // --0--
    case 0:                     // --0-- CAS[          0--( c == 'o' )-->1 ]
      { 
        if  ( c == 'o' )        // --0-- CAS[          0--( c == 'o' )-->1 ]
        {
           
          state = 1; 
        }
                                        // --1--
        else if  ( c == 'p' )        // --1-- CAS[          0--( c == 'p' )-->29 ]
        {
           
          state = 29; 
        }
                                        // --2--
        else if  ( c == 's' )        // --2-- CAS[          0--( c == 's' )-->52 ]
        {
           
          state = 52; 
        }
                                        // --3--
        else if  ( c == 'h' )        // --3-- CAS[          0--( c == 'h' )-->77 ]
        {
           
          state = 77; 
        }
                                        // --4--
        else if  ( c == 'c' )        // --4-- CAS[          0--( c == 'c' )-->81 ]
        {
           
          state = 81; 
        }
                                        // --5--
        else if  ( c == 'v' )        // --5-- CAS[          0--( c == 'v' )-->225 ]
        {
           
          state = 225; 
        }
                                        // --6--
        else {
          state = 246;                // --6-- ERROR 
        }
      } 
    break; 
    case 1:                     // --6-- CAS[          1--( c == ':' )-->2 ]
      { 
        if  ( c == ':' )        // --6-- CAS[          1--( c == ':' )-->2 ]
        {
           
          state = 2; 
        }
                                        // --7--
        else if  ( isDigit(c) )        // --7-- CAS[          1--( isDigit(c) )-->9 ]
        {
          port = valueDecimal(c); 
          state = 9; 
        }
                                        // --8--
        else if  ( c == 'a' )        // --8-- CAS[          1--( c == 'a' )-->15 ]
        {
           
          state = 15; 
        }
                                        // --9--
        else {
          state = 246;                // --9-- ERROR 
        }
      } 
    break; 
    case 2:                     // --9-- CAS[          2--( isDigit(c) )-->3 ]
      { 
        if  ( isDigit(c) )        // --9-- CAS[          2--( isDigit(c) )-->3 ]
        {
          port = valueDecimal(c); 
          state = 3; 
        }
                                        // --10--
        else {
          state = 246;                // --10-- ERROR 
        }
      } 
    break; 
    case 3:                     // --10-- CAS[          3--( isDigit(c) )-->4 ]
      { 
        if  ( isDigit(c) )        // --10-- CAS[          3--( isDigit(c) )-->4 ]
        {
          port = port * 10 + valueDecimal(c); 
          state = 4; 
        }
                                        // --11--
        else if  ( c == ',' )        // --11-- CAS[          3--( c == ',' )-->5 ]
        {
           
          state = 5; 
        }
                                        // --12--
        else {
          state = 246;                // --12-- ERROR 
        }
      } 
    break; 
    case 4:                     // --12-- CAS[          4--( c == ',' )-->5 ]
      { 
        if  ( c == ',' )        // --12-- CAS[          4--( c == ',' )-->5 ]
        {
           
          state = 5; 
        }
                                        // --13--
        else {
          state = 246;                // --13-- ERROR 
        }
      } 
    break; 
    case 5:                     // --13-- CAS[          5--( isDigit(c) )-->6 ]
      { 
        if  ( isDigit(c) )        // --13-- CAS[          5--( isDigit(c) )-->6 ]
        {
          value = valueDecimal(c); 
          state = 6; 
        }
                                        // --14--
        else {
          state = 246;                // --14-- ERROR 
        }
      } 
    break; 
    case 6:                     // --14-- CAS[          6--( isDigit(c) )-->7 ]
      { 
        if  ( isDigit(c) )        // --14-- CAS[          6--( isDigit(c) )-->7 ]
        {
          value = value * 10 + valueDecimal(c); 
          state = 7; 
        }
                                        // --15--
        else if  ( c == '\n' )        // --15-- CAS[          6--( c == '\n' )-->245 ]
        {
           
              {
                  if (debug & 1){
                      printDebug_o_port_value();
                  }
                  if ( value == 0){
                      digitalWrite(port, LOW);
                  }
                  else {
                     digitalWrite(port, HIGH);
                  }
              } 
               
          state = 245; 
        }
                                        // --16--
        else {
          state = 246;                // --16-- ERROR 
        }
      } 
    break; 
    case 7:                     // --16-- CAS[          7--( isDigit(c) )-->8 ]
      { 
        if  ( isDigit(c) )        // --16-- CAS[          7--( isDigit(c) )-->8 ]
        {
          value = value * 10 + valueDecimal(c); 
          state = 8; 
        }
                                        // --17--
        else if  ( c == '\n' )        // --17-- CAS[          7--( c == '\n' )-->245 ]
        {
           
              {
                  if (debug & 1){
                      printDebug_o_port_value();
                  }
                  if ( value == 0){
                      digitalWrite(port, LOW);
                  }
                  else {
                     digitalWrite(port, HIGH);
                  }
              } 
               
          state = 245; 
        }
                                        // --18--
        else {
          state = 246;                // --18-- ERROR 
        }
      } 
    break; 
    case 8:                     // --18-- CAS[          8--( c == '\n' )-->245 ]
      { 
        if  ( c == '\n' )        // --18-- CAS[          8--( c == '\n' )-->245 ]
        {
           
              {
                  if (debug & 1){
                      printDebug_o_port_value();
                  }
                  if ( value == 0){
                      digitalWrite(port, LOW);
                  }
                  else {
                     digitalWrite(port, HIGH);
                  }
              } 
               
          state = 245; 
        }
                                        // --19--
        else {
          state = 246;                // --19-- ERROR 
        }
      } 
    break; 
    case 9:                     // --19-- CAS[          9--( isDigit(c) )-->10 ]
      { 
        if  ( isDigit(c) )        // --19-- CAS[          9--( isDigit(c) )-->10 ]
        {
          port = port * 10 + valueDecimal(c); 
          state = 10; 
        }
                                        // --20--
        else if  ( c == ',' )        // --20-- CAS[          9--( c == ',' )-->11 ]
        {
           
          state = 11; 
        }
                                        // --21--
        else {
          state = 246;                // --21-- ERROR 
        }
      } 
    break; 
    case 10:                     // --21-- CAS[          10--( c == ',' )-->11 ]
      { 
        if  ( c == ',' )        // --21-- CAS[          10--( c == ',' )-->11 ]
        {
           
          state = 11; 
        }
                                        // --22--
        else {
          state = 246;                // --22-- ERROR 
        }
      } 
    break; 
    case 11:                     // --22-- CAS[          11--( isDigit(c) )-->12 ]
      { 
        if  ( isDigit(c) )        // --22-- CAS[          11--( isDigit(c) )-->12 ]
        {
          value = valueDecimal(c); 
          state = 12; 
        }
                                        // --23--
        else {
          state = 246;                // --23-- ERROR 
        }
      } 
    break; 
    case 12:                     // --23-- CAS[          12--( isDigit(c) )-->13 ]
      { 
        if  ( isDigit(c) )        // --23-- CAS[          12--( isDigit(c) )-->13 ]
        {
          value = value * 10 + valueDecimal(c); 
          state = 13; 
        }
                                        // --24--
        else if  ( c == '\n' )        // --24-- CAS[          12--( c == '\n' )-->245 ]
        {
           
          {
              if (debug & 1){
                  printDebug_o_port_value();
              }
              if ( value == 0){
                  digitalWrite(port, LOW);
              }
              else {
                 digitalWrite(port, HIGH);
              }
          } 
           
          state = 245; 
        }
                                        // --25--
        else {
          state = 246;                // --25-- ERROR 
        }
      } 
    break; 
    case 13:                     // --25-- CAS[          13--( isDigit(c) )-->14 ]
      { 
        if  ( isDigit(c) )        // --25-- CAS[          13--( isDigit(c) )-->14 ]
        {
          value = value * 10 + valueDecimal(c); 
          state = 14; 
        }
                                        // --26--
        else if  ( c == '\n' )        // --26-- CAS[          13--( c == '\n' )-->245 ]
        {
           
          {
              if (debug & 1){
                  printDebug_o_port_value();
              }
              if ( value == 0){
                  digitalWrite(port, LOW);
              }
              else {
                 digitalWrite(port, HIGH);
              }
          } 
           
          state = 245; 
        }
                                        // --27--
        else {
          state = 246;                // --27-- ERROR 
        }
      } 
    break; 
    case 14:                     // --27-- CAS[          14--( c == '\n' )-->245 ]
      { 
        if  ( c == '\n' )        // --27-- CAS[          14--( c == '\n' )-->245 ]
        {
           
          {
              if (debug & 1){
                  printDebug_o_port_value();
              }
              if ( value == 0){
                  digitalWrite(port, LOW);
              }
              else {
                 digitalWrite(port, HIGH);
              }
          } 
           
          state = 245; 
        }
                                        // --28--
        else {
          state = 246;                // --28-- ERROR 
        }
      } 
    break; 
    case 15:                     // --28-- CAS[          15--( c == ':' )-->16 ]
      { 
        if  ( c == ':' )        // --28-- CAS[          15--( c == ':' )-->16 ]
        {
           
          state = 16; 
        }
                                        // --29--
        else if  ( isDigit(c) )        // --29-- CAS[          15--( isDigit(c) )-->23 ]
        {
          port = valueDecimal(c); 
          state = 23; 
        }
                                        // --30--
        else {
          state = 246;                // --30-- ERROR 
        }
      } 
    break; 
    case 16:                     // --30-- CAS[          16--( isDigit(c) )-->17 ]
      { 
        if  ( isDigit(c) )        // --30-- CAS[          16--( isDigit(c) )-->17 ]
        {
          port = valueDecimal(c); 
          state = 17; 
        }
                                        // --31--
        else {
          state = 246;                // --31-- ERROR 
        }
      } 
    break; 
    case 17:                     // --31-- CAS[          17--( isDigit(c) )-->18 ]
      { 
        if  ( isDigit(c) )        // --31-- CAS[          17--( isDigit(c) )-->18 ]
        {
          port = port * 10 + valueDecimal(c); 
          state = 18; 
        }
                                        // --32--
        else if  ( c == ',' )        // --32-- CAS[          17--( c == ',' )-->19 ]
        {
           
          state = 19; 
        }
                                        // --33--
        else {
          state = 246;                // --33-- ERROR 
        }
      } 
    break; 
    case 18:                     // --33-- CAS[          18--( c == ',' )-->19 ]
      { 
        if  ( c == ',' )        // --33-- CAS[          18--( c == ',' )-->19 ]
        {
           
          state = 19; 
        }
                                        // --34--
        else {
          state = 246;                // --34-- ERROR 
        }
      } 
    break; 
    case 19:                     // --34-- CAS[          19--( isDigit(c) )-->20 ]
      { 
        if  ( isDigit(c) )        // --34-- CAS[          19--( isDigit(c) )-->20 ]
        {
          value = valueDecimal(c); 
          state = 20; 
        }
                                        // --35--
        else {
          state = 246;                // --35-- ERROR 
        }
      } 
    break; 
    case 20:                     // --35-- CAS[          20--( isDigit(c) )-->21 ]
      { 
        if  ( isDigit(c) )        // --35-- CAS[          20--( isDigit(c) )-->21 ]
        {
          value = value * 10 + valueDecimal(c); 
          state = 21; 
        }
                                        // --36--
        else if  ( c == '\n' )        // --36-- CAS[          20--( c == '\n' )-->245 ]
        {
           
              {
                  if (debug & 1){
                      printDebug_oa_port_value();
                      
                  }
                  if ( value == 0){
                      analogDigitalWrite(port, LOW);
                  }
                  else {
                     analogDigitalWrite(port, HIGH);
                  }
              } 
               
          state = 245; 
        }
                                        // --37--
        else {
          state = 246;                // --37-- ERROR 
        }
      } 
    break; 
    case 21:                     // --37-- CAS[          21--( isDigit(c) )-->22 ]
      { 
        if  ( isDigit(c) )        // --37-- CAS[          21--( isDigit(c) )-->22 ]
        {
          value = value * 10 + valueDecimal(c); 
          state = 22; 
        }
                                        // --38--
        else if  ( c == '\n' )        // --38-- CAS[          21--( c == '\n' )-->245 ]
        {
           
              {
                  if (debug & 1){
                      printDebug_oa_port_value();
                      
                  }
                  if ( value == 0){
                      analogDigitalWrite(port, LOW);
                  }
                  else {
                     analogDigitalWrite(port, HIGH);
                  }
              } 
               
          state = 245; 
        }
                                        // --39--
        else {
          state = 246;                // --39-- ERROR 
        }
      } 
    break; 
    case 22:                     // --39-- CAS[          22--( c == '\n' )-->245 ]
      { 
        if  ( c == '\n' )        // --39-- CAS[          22--( c == '\n' )-->245 ]
        {
           
              {
                  if (debug & 1){
                      printDebug_oa_port_value();
                      
                  }
                  if ( value == 0){
                      analogDigitalWrite(port, LOW);
                  }
                  else {
                     analogDigitalWrite(port, HIGH);
                  }
              } 
               
          state = 245; 
        }
                                        // --40--
        else {
          state = 246;                // --40-- ERROR 
        }
      } 
    break; 
    case 23:                     // --40-- CAS[          23--( isDigit(c) )-->24 ]
      { 
        if  ( isDigit(c) )        // --40-- CAS[          23--( isDigit(c) )-->24 ]
        {
          port = port * 10 + valueDecimal(c); 
          state = 24; 
        }
                                        // --41--
        else if  ( c == ',' )        // --41-- CAS[          23--( c == ',' )-->25 ]
        {
           
          state = 25; 
        }
                                        // --42--
        else {
          state = 246;                // --42-- ERROR 
        }
      } 
    break; 
    case 24:                     // --42-- CAS[          24--( c == ',' )-->25 ]
      { 
        if  ( c == ',' )        // --42-- CAS[          24--( c == ',' )-->25 ]
        {
           
          state = 25; 
        }
                                        // --43--
        else {
          state = 246;                // --43-- ERROR 
        }
      } 
    break; 
    case 25:                     // --43-- CAS[          25--( isDigit(c) )-->26 ]
      { 
        if  ( isDigit(c) )        // --43-- CAS[          25--( isDigit(c) )-->26 ]
        {
          value = valueDecimal(c); 
          state = 26; 
        }
                                        // --44--
        else {
          state = 246;                // --44-- ERROR 
        }
      } 
    break; 
    case 26:                     // --44-- CAS[          26--( isDigit(c) )-->27 ]
      { 
        if  ( isDigit(c) )        // --44-- CAS[          26--( isDigit(c) )-->27 ]
        {
          value = value * 10 + valueDecimal(c); 
          state = 27; 
        }
                                        // --45--
        else if  ( c == '\n' )        // --45-- CAS[          26--( c == '\n' )-->245 ]
        {
           
          {
              if (debug & 1){
                  printDebug_oa_port_value();
                  
              }
              if ( value == 0){
                  analogDigitalWrite(port, LOW);
              }
              else {
                 analogDigitalWrite(port, HIGH);
              }
          } 
           
          state = 245; 
        }
                                        // --46--
        else {
          state = 246;                // --46-- ERROR 
        }
      } 
    break; 
    case 27:                     // --46-- CAS[          27--( isDigit(c) )-->28 ]
      { 
        if  ( isDigit(c) )        // --46-- CAS[          27--( isDigit(c) )-->28 ]
        {
          value = value * 10 + valueDecimal(c); 
          state = 28; 
        }
                                        // --47--
        else if  ( c == '\n' )        // --47-- CAS[          27--( c == '\n' )-->245 ]
        {
           
          {
              if (debug & 1){
                  printDebug_oa_port_value();
                  
              }
              if ( value == 0){
                  analogDigitalWrite(port, LOW);
              }
              else {
                 analogDigitalWrite(port, HIGH);
              }
          } 
           
          state = 245; 
        }
                                        // --48--
        else {
          state = 246;                // --48-- ERROR 
        }
      } 
    break; 
    case 28:                     // --48-- CAS[          28--( c == '\n' )-->245 ]
      { 
        if  ( c == '\n' )        // --48-- CAS[          28--( c == '\n' )-->245 ]
        {
           
          {
              if (debug & 1){
                  printDebug_oa_port_value();
                  
              }
              if ( value == 0){
                  analogDigitalWrite(port, LOW);
              }
              else {
                 analogDigitalWrite(port, HIGH);
              }
          } 
           
          state = 245; 
        }
                                        // --49--
        else {
          state = 246;                // --49-- ERROR 
        }
      } 
    break; 
    case 29:                     // --49-- CAS[          29--( c == 'w' )-->30 ]
      { 
        if  ( c == 'w' )        // --49-- CAS[          29--( c == 'w' )-->30 ]
        {
           
          state = 30; 
        }
                                        // --50--
        else if  ( c == ':' )        // --50-- CAS[          29--( c == ':' )-->39 ]
        {
           
          state = 39; 
        }
                                        // --51--
        else if  ( isDigit(c) )        // --51-- CAS[          29--( isDigit(c) )-->46 ]
        {
          port = valueDecimal(c); 
          state = 46; 
        }
                                        // --52--
        else {
          state = 246;                // --52-- ERROR 
        }
      } 
    break; 
    case 30:                     // --52-- CAS[          30--( c == 'm' )-->31 ]
      { 
        if  ( c == 'm' )        // --52-- CAS[          30--( c == 'm' )-->31 ]
        {
           
          state = 31; 
        }
                                        // --53--
        else {
          state = 246;                // --53-- ERROR 
        }
      } 
    break; 
    case 31:                     // --53-- CAS[          31--( c == ':' )-->32 ]
      { 
        if  ( c == ':' )        // --53-- CAS[          31--( c == ':' )-->32 ]
        {
           
          state = 32; 
        }
                                        // --54--
        else {
          state = 246;                // --54-- ERROR 
        }
      } 
    break; 
    case 32:                     // --54-- CAS[          32--( isDigit(c) )-->33 ]
      { 
        if  ( isDigit(c) )        // --54-- CAS[          32--( isDigit(c) )-->33 ]
        {
          port = valueDecimal(c); 
          state = 33; 
        }
                                        // --55--
        else {
          state = 246;                // --55-- ERROR 
        }
      } 
    break; 
    case 33:                     // --55-- CAS[          33--( isDigit(c) )-->34 ]
      { 
        if  ( isDigit(c) )        // --55-- CAS[          33--( isDigit(c) )-->34 ]
        {
          port = port * 10 + valueDecimal(c); 
          state = 34; 
        }
                                        // --56--
        else if  ( c == ',' )        // --56-- CAS[          33--( c == ',' )-->35 ]
        {
           
          state = 35; 
        }
                                        // --57--
        else {
          state = 246;                // --57-- ERROR 
        }
      } 
    break; 
    case 34:                     // --57-- CAS[          34--( c == ',' )-->35 ]
      { 
        if  ( c == ',' )        // --57-- CAS[          34--( c == ',' )-->35 ]
        {
           
          state = 35; 
        }
                                        // --58--
        else {
          state = 246;                // --58-- ERROR 
        }
      } 
    break; 
    case 35:                     // --58-- CAS[          35--( isDigit(c) )-->36 ]
      { 
        if  ( isDigit(c) )        // --58-- CAS[          35--( isDigit(c) )-->36 ]
        {
          value = valueDecimal(c); 
          state = 36; 
        }
                                        // --59--
        else {
          state = 246;                // --59-- ERROR 
        }
      } 
    break; 
    case 36:                     // --59-- CAS[          36--( isDigit(c) )-->37 ]
      { 
        if  ( isDigit(c) )        // --59-- CAS[          36--( isDigit(c) )-->37 ]
        {
          value = value * 10 + valueDecimal(c); 
          state = 37; 
        }
                                        // --60--
        else if  ( c == '\n' )        // --60-- CAS[          36--( c == '\n' )-->245 ]
        {
          
              {
                  if (debug & 1){
                      printDebug_p_port_value();
                  }
                  if ( pwms & (1 << port)){
                      analogWrite(port, value);
                  }
              }
               
          state = 245; 
        }
                                        // --61--
        else {
          state = 246;                // --61-- ERROR 
        }
      } 
    break; 
    case 37:                     // --61-- CAS[          37--( isDigit(c) )-->38 ]
      { 
        if  ( isDigit(c) )        // --61-- CAS[          37--( isDigit(c) )-->38 ]
        {
          value = value * 10 + valueDecimal(c); 
          state = 38; 
        }
                                        // --62--
        else if  ( c == '\n' )        // --62-- CAS[          37--( c == '\n' )-->245 ]
        {
          
              {
                  if (debug & 1){
                      printDebug_p_port_value();
                  }
                  if ( pwms & (1 << port)){
                      analogWrite(port, value);
                  }
              }
               
          state = 245; 
        }
                                        // --63--
        else {
          state = 246;                // --63-- ERROR 
        }
      } 
    break; 
    case 38:                     // --63-- CAS[          38--( c == '\n' )-->245 ]
      { 
        if  ( c == '\n' )        // --63-- CAS[          38--( c == '\n' )-->245 ]
        {
          
              {
                  if (debug & 1){
                      printDebug_p_port_value();
                  }
                  if ( pwms & (1 << port)){
                      analogWrite(port, value);
                  }
              }
               
          state = 245; 
        }
                                        // --64--
        else {
          state = 246;                // --64-- ERROR 
        }
      } 
    break; 
    case 39:                     // --64-- CAS[          39--( isDigit(c) )-->40 ]
      { 
        if  ( isDigit(c) )        // --64-- CAS[          39--( isDigit(c) )-->40 ]
        {
          port = valueDecimal(c); 
          state = 40; 
        }
                                        // --65--
        else {
          state = 246;                // --65-- ERROR 
        }
      } 
    break; 
    case 40:                     // --65-- CAS[          40--( isDigit(c) )-->41 ]
      { 
        if  ( isDigit(c) )        // --65-- CAS[          40--( isDigit(c) )-->41 ]
        {
          port = port * 10 + valueDecimal(c); 
          state = 41; 
        }
                                        // --66--
        else if  ( c == ',' )        // --66-- CAS[          40--( c == ',' )-->42 ]
        {
           
          state = 42; 
        }
                                        // --67--
        else {
          state = 246;                // --67-- ERROR 
        }
      } 
    break; 
    case 41:                     // --67-- CAS[          41--( c == ',' )-->42 ]
      { 
        if  ( c == ',' )        // --67-- CAS[          41--( c == ',' )-->42 ]
        {
           
          state = 42; 
        }
                                        // --68--
        else {
          state = 246;                // --68-- ERROR 
        }
      } 
    break; 
    case 42:                     // --68-- CAS[          42--( isDigit(c) )-->43 ]
      { 
        if  ( isDigit(c) )        // --68-- CAS[          42--( isDigit(c) )-->43 ]
        {
          value = valueDecimal(c); 
          state = 43; 
        }
                                        // --69--
        else {
          state = 246;                // --69-- ERROR 
        }
      } 
    break; 
    case 43:                     // --69-- CAS[          43--( isDigit(c) )-->44 ]
      { 
        if  ( isDigit(c) )        // --69-- CAS[          43--( isDigit(c) )-->44 ]
        {
          value = value * 10 + valueDecimal(c); 
          state = 44; 
        }
                                        // --70--
        else if  ( c == '\n' )        // --70-- CAS[          43--( c == '\n' )-->245 ]
        {
          
              {
                  if (debug & 1){
                      printDebug_p_port_value();
                  }
                  if ( pwms & (1 << port)){
                      analogWrite(port, value);
                  }
              }
               
          state = 245; 
        }
                                        // --71--
        else {
          state = 246;                // --71-- ERROR 
        }
      } 
    break; 
    case 44:                     // --71-- CAS[          44--( isDigit(c) )-->45 ]
      { 
        if  ( isDigit(c) )        // --71-- CAS[          44--( isDigit(c) )-->45 ]
        {
          value = value * 10 + valueDecimal(c); 
          state = 45; 
        }
                                        // --72--
        else if  ( c == '\n' )        // --72-- CAS[          44--( c == '\n' )-->245 ]
        {
          
              {
                  if (debug & 1){
                      printDebug_p_port_value();
                  }
                  if ( pwms & (1 << port)){
                      analogWrite(port, value);
                  }
              }
               
          state = 245; 
        }
                                        // --73--
        else {
          state = 246;                // --73-- ERROR 
        }
      } 
    break; 
    case 45:                     // --73-- CAS[          45--( c == '\n' )-->245 ]
      { 
        if  ( c == '\n' )        // --73-- CAS[          45--( c == '\n' )-->245 ]
        {
          
              {
                  if (debug & 1){
                      printDebug_p_port_value();
                  }
                  if ( pwms & (1 << port)){
                      analogWrite(port, value);
                  }
              }
               
          state = 245; 
        }
                                        // --74--
        else {
          state = 246;                // --74-- ERROR 
        }
      } 
    break; 
    case 46:                     // --74-- CAS[          46--( isDigit(c) )-->47 ]
      { 
        if  ( isDigit(c) )        // --74-- CAS[          46--( isDigit(c) )-->47 ]
        {
          port = port * 10 + valueDecimal(c); 
          state = 47; 
        }
                                        // --75--
        else if  ( c == ',' )        // --75-- CAS[          46--( c == ',' )-->48 ]
        {
           
          state = 48; 
        }
                                        // --76--
        else {
          state = 246;                // --76-- ERROR 
        }
      } 
    break; 
    case 47:                     // --76-- CAS[          47--( c == ',' )-->48 ]
      { 
        if  ( c == ',' )        // --76-- CAS[          47--( c == ',' )-->48 ]
        {
           
          state = 48; 
        }
                                        // --77--
        else {
          state = 246;                // --77-- ERROR 
        }
      } 
    break; 
    case 48:                     // --77-- CAS[          48--( isDigit(c) )-->49 ]
      { 
        if  ( isDigit(c) )        // --77-- CAS[          48--( isDigit(c) )-->49 ]
        {
          value = valueDecimal(c); 
          state = 49; 
        }
                                        // --78--
        else {
          state = 246;                // --78-- ERROR 
        }
      } 
    break; 
    case 49:                     // --78-- CAS[          49--( isDigit(c) )-->50 ]
      { 
        if  ( isDigit(c) )        // --78-- CAS[          49--( isDigit(c) )-->50 ]
        {
          value = value * 10 + valueDecimal(c); 
          state = 50; 
        }
                                        // --79--
        else if  ( c == '\n' )        // --79-- CAS[          49--( c == '\n' )-->245 ]
        {
          
          {
              if (debug & 1){
                  printDebug_p_port_value();
              }
              if ( pwms & (1 << port)){
                  analogWrite(port, value);
              }
          }
           
          state = 245; 
        }
                                        // --80--
        else {
          state = 246;                // --80-- ERROR 
        }
      } 
    break; 
    case 50:                     // --80-- CAS[          50--( isDigit(c) )-->51 ]
      { 
        if  ( isDigit(c) )        // --80-- CAS[          50--( isDigit(c) )-->51 ]
        {
          value = value * 10 + valueDecimal(c); 
          state = 51; 
        }
                                        // --81--
        else if  ( c == '\n' )        // --81-- CAS[          50--( c == '\n' )-->245 ]
        {
          
          {
              if (debug & 1){
                  printDebug_p_port_value();
              }
              if ( pwms & (1 << port)){
                  analogWrite(port, value);
              }
          }
           
          state = 245; 
        }
                                        // --82--
        else {
          state = 246;                // --82-- ERROR 
        }
      } 
    break; 
    case 51:                     // --82-- CAS[          51--( c == '\n' )-->245 ]
      { 
        if  ( c == '\n' )        // --82-- CAS[          51--( c == '\n' )-->245 ]
        {
          
          {
              if (debug & 1){
                  printDebug_p_port_value();
              }
              if ( pwms & (1 << port)){
                  analogWrite(port, value);
              }
          }
           
          state = 245; 
        }
                                        // --83--
        else {
          state = 246;                // --83-- ERROR 
        }
      } 
    break; 
    case 52:                     // --83-- CAS[          52--( c == 'e' )-->53 ]
      { 
        if  ( c == 'e' )        // --83-- CAS[          52--( c == 'e' )-->53 ]
        {
           
          state = 53; 
        }
                                        // --84--
        else if  ( c == ':' )        // --84-- CAS[          52--( c == ':' )-->64 ]
        {
           
          state = 64; 
        }
                                        // --85--
        else if  ( isDigit(c) )        // --85-- CAS[          52--( isDigit(c) )-->71 ]
        {
          port = valueDecimal(c); 
          state = 71; 
        }
                                        // --86--
        else {
          state = 246;                // --86-- ERROR 
        }
      } 
    break; 
    case 53:                     // --86-- CAS[          53--( c == 'r' )-->54 ]
      { 
        if  ( c == 'r' )        // --86-- CAS[          53--( c == 'r' )-->54 ]
        {
           
          state = 54; 
        }
                                        // --87--
        else {
          state = 246;                // --87-- ERROR 
        }
      } 
    break; 
    case 54:                     // --87-- CAS[          54--( c == 'v' )-->55 ]
      { 
        if  ( c == 'v' )        // --87-- CAS[          54--( c == 'v' )-->55 ]
        {
           
          state = 55; 
        }
                                        // --88--
        else {
          state = 246;                // --88-- ERROR 
        }
      } 
    break; 
    case 55:                     // --88-- CAS[          55--( c == 'o' )-->56 ]
      { 
        if  ( c == 'o' )        // --88-- CAS[          55--( c == 'o' )-->56 ]
        {
           
          state = 56; 
        }
                                        // --89--
        else {
          state = 246;                // --89-- ERROR 
        }
      } 
    break; 
    case 56:                     // --89-- CAS[          56--( c == ':' )-->57 ]
      { 
        if  ( c == ':' )        // --89-- CAS[          56--( c == ':' )-->57 ]
        {
           
          state = 57; 
        }
                                        // --90--
        else {
          state = 246;                // --90-- ERROR 
        }
      } 
    break; 
    case 57:                     // --90-- CAS[          57--( isDigit(c) )-->58 ]
      { 
        if  ( isDigit(c) )        // --90-- CAS[          57--( isDigit(c) )-->58 ]
        {
          port = valueDecimal(c); 
          state = 58; 
        }
                                        // --91--
        else {
          state = 246;                // --91-- ERROR 
        }
      } 
    break; 
    case 58:                     // --91-- CAS[          58--( isDigit(c) )-->59 ]
      { 
        if  ( isDigit(c) )        // --91-- CAS[          58--( isDigit(c) )-->59 ]
        {
          port = port * 10 + valueDecimal(c); 
          state = 59; 
        }
                                        // --92--
        else if  ( c == ',' )        // --92-- CAS[          58--( c == ',' )-->60 ]
        {
           
          state = 60; 
        }
                                        // --93--
        else {
          state = 246;                // --93-- ERROR 
        }
      } 
    break; 
    case 59:                     // --93-- CAS[          59--( c == ',' )-->60 ]
      { 
        if  ( c == ',' )        // --93-- CAS[          59--( c == ',' )-->60 ]
        {
           
          state = 60; 
        }
                                        // --94--
        else {
          state = 246;                // --94-- ERROR 
        }
      } 
    break; 
    case 60:                     // --94-- CAS[          60--( isDigit(c) )-->61 ]
      { 
        if  ( isDigit(c) )        // --94-- CAS[          60--( isDigit(c) )-->61 ]
        {
          value = valueDecimal(c); 
          state = 61; 
        }
                                        // --95--
        else {
          state = 246;                // --95-- ERROR 
        }
      } 
    break; 
    case 61:                     // --95-- CAS[          61--( isDigit(c) )-->62 ]
      { 
        if  ( isDigit(c) )        // --95-- CAS[          61--( isDigit(c) )-->62 ]
        {
          value = value * 10 + valueDecimal(c); 
          state = 62; 
        }
                                        // --96--
        else if  ( c == '\n' )        // --96-- CAS[          61--( c == '\n' )-->245 ]
        {
           
              {
                  if (debug & 1){
                      printDebug_s_port_value();
                  }
                  if ( servos & (1 << port)) {
                      // Serial.println("config ok");
                      if ( servoObject[port] != NULL )
                          //Serial.println("config not null");
                      servoObject[port]->write( value);
                  }
              }
               
          state = 245; 
        }
                                        // --97--
        else {
          state = 246;                // --97-- ERROR 
        }
      } 
    break; 
    case 62:                     // --97-- CAS[          62--( isDigit(c) )-->63 ]
      { 
        if  ( isDigit(c) )        // --97-- CAS[          62--( isDigit(c) )-->63 ]
        {
          value = value * 10 + valueDecimal(c); 
          state = 63; 
        }
                                        // --98--
        else if  ( c == '\n' )        // --98-- CAS[          62--( c == '\n' )-->245 ]
        {
           
              {
                  if (debug & 1){
                      printDebug_s_port_value();
                  }
                  if ( servos & (1 << port)) {
                      // Serial.println("config ok");
                      if ( servoObject[port] != NULL )
                          //Serial.println("config not null");
                      servoObject[port]->write( value);
                  }
              }
               
          state = 245; 
        }
                                        // --99--
        else {
          state = 246;                // --99-- ERROR 
        }
      } 
    break; 
    case 63:                     // --99-- CAS[          63--( c == '\n' )-->245 ]
      { 
        if  ( c == '\n' )        // --99-- CAS[          63--( c == '\n' )-->245 ]
        {
           
              {
                  if (debug & 1){
                      printDebug_s_port_value();
                  }
                  if ( servos & (1 << port)) {
                      // Serial.println("config ok");
                      if ( servoObject[port] != NULL )
                          //Serial.println("config not null");
                      servoObject[port]->write( value);
                  }
              }
               
          state = 245; 
        }
                                        // --100--
        else {
          state = 246;                // --100-- ERROR 
        }
      } 
    break; 
    case 64:                     // --100-- CAS[          64--( isDigit(c) )-->65 ]
      { 
        if  ( isDigit(c) )        // --100-- CAS[          64--( isDigit(c) )-->65 ]
        {
          port = valueDecimal(c); 
          state = 65; 
        }
                                        // --101--
        else {
          state = 246;                // --101-- ERROR 
        }
      } 
    break; 
    case 65:                     // --101-- CAS[          65--( isDigit(c) )-->66 ]
      { 
        if  ( isDigit(c) )        // --101-- CAS[          65--( isDigit(c) )-->66 ]
        {
          port = port * 10 + valueDecimal(c); 
          state = 66; 
        }
                                        // --102--
        else if  ( c == ',' )        // --102-- CAS[          65--( c == ',' )-->67 ]
        {
           
          state = 67; 
        }
                                        // --103--
        else {
          state = 246;                // --103-- ERROR 
        }
      } 
    break; 
    case 66:                     // --103-- CAS[          66--( c == ',' )-->67 ]
      { 
        if  ( c == ',' )        // --103-- CAS[          66--( c == ',' )-->67 ]
        {
           
          state = 67; 
        }
                                        // --104--
        else {
          state = 246;                // --104-- ERROR 
        }
      } 
    break; 
    case 67:                     // --104-- CAS[          67--( isDigit(c) )-->68 ]
      { 
        if  ( isDigit(c) )        // --104-- CAS[          67--( isDigit(c) )-->68 ]
        {
          value = valueDecimal(c); 
          state = 68; 
        }
                                        // --105--
        else {
          state = 246;                // --105-- ERROR 
        }
      } 
    break; 
    case 68:                     // --105-- CAS[          68--( isDigit(c) )-->69 ]
      { 
        if  ( isDigit(c) )        // --105-- CAS[          68--( isDigit(c) )-->69 ]
        {
          value = value * 10 + valueDecimal(c); 
          state = 69; 
        }
                                        // --106--
        else if  ( c == '\n' )        // --106-- CAS[          68--( c == '\n' )-->245 ]
        {
           
              {
                  if (debug & 1){
                      printDebug_s_port_value();
                  }
                  if ( servos & (1 << port)) {
                      // Serial.println("config ok");
                      if ( servoObject[port] != NULL )
                          //Serial.println("config not null");
                      servoObject[port]->write( value);
                  }
              }
               
          state = 245; 
        }
                                        // --107--
        else {
          state = 246;                // --107-- ERROR 
        }
      } 
    break; 
    case 69:                     // --107-- CAS[          69--( isDigit(c) )-->70 ]
      { 
        if  ( isDigit(c) )        // --107-- CAS[          69--( isDigit(c) )-->70 ]
        {
          value = value * 10 + valueDecimal(c); 
          state = 70; 
        }
                                        // --108--
        else if  ( c == '\n' )        // --108-- CAS[          69--( c == '\n' )-->245 ]
        {
           
              {
                  if (debug & 1){
                      printDebug_s_port_value();
                  }
                  if ( servos & (1 << port)) {
                      // Serial.println("config ok");
                      if ( servoObject[port] != NULL )
                          //Serial.println("config not null");
                      servoObject[port]->write( value);
                  }
              }
               
          state = 245; 
        }
                                        // --109--
        else {
          state = 246;                // --109-- ERROR 
        }
      } 
    break; 
    case 70:                     // --109-- CAS[          70--( c == '\n' )-->245 ]
      { 
        if  ( c == '\n' )        // --109-- CAS[          70--( c == '\n' )-->245 ]
        {
           
              {
                  if (debug & 1){
                      printDebug_s_port_value();
                  }
                  if ( servos & (1 << port)) {
                      // Serial.println("config ok");
                      if ( servoObject[port] != NULL )
                          //Serial.println("config not null");
                      servoObject[port]->write( value);
                  }
              }
               
          state = 245; 
        }
                                        // --110--
        else {
          state = 246;                // --110-- ERROR 
        }
      } 
    break; 
    case 71:                     // --110-- CAS[          71--( isDigit(c) )-->72 ]
      { 
        if  ( isDigit(c) )        // --110-- CAS[          71--( isDigit(c) )-->72 ]
        {
          port = port * 10 + valueDecimal(c); 
          state = 72; 
        }
                                        // --111--
        else if  ( c == ',' )        // --111-- CAS[          71--( c == ',' )-->73 ]
        {
           
          state = 73; 
        }
                                        // --112--
        else {
          state = 246;                // --112-- ERROR 
        }
      } 
    break; 
    case 72:                     // --112-- CAS[          72--( c == ',' )-->73 ]
      { 
        if  ( c == ',' )        // --112-- CAS[          72--( c == ',' )-->73 ]
        {
           
          state = 73; 
        }
                                        // --113--
        else {
          state = 246;                // --113-- ERROR 
        }
      } 
    break; 
    case 73:                     // --113-- CAS[          73--( isDigit(c) )-->74 ]
      { 
        if  ( isDigit(c) )        // --113-- CAS[          73--( isDigit(c) )-->74 ]
        {
          value = valueDecimal(c); 
          state = 74; 
        }
                                        // --114--
        else {
          state = 246;                // --114-- ERROR 
        }
      } 
    break; 
    case 74:                     // --114-- CAS[          74--( isDigit(c) )-->75 ]
      { 
        if  ( isDigit(c) )        // --114-- CAS[          74--( isDigit(c) )-->75 ]
        {
          value = value * 10 + valueDecimal(c); 
          state = 75; 
        }
                                        // --115--
        else if  ( c == '\n' )        // --115-- CAS[          74--( c == '\n' )-->245 ]
        {
           
          {
              if (debug & 1){
                  printDebug_s_port_value();
              }
              if ( servos & (1 << port)) {
                  // Serial.println("config ok");
                  if ( servoObject[port] != NULL )
                      //Serial.println("config not null");
                  servoObject[port]->write( value);
              }
          }
           
          state = 245; 
        }
                                        // --116--
        else {
          state = 246;                // --116-- ERROR 
        }
      } 
    break; 
    case 75:                     // --116-- CAS[          75--( isDigit(c) )-->76 ]
      { 
        if  ( isDigit(c) )        // --116-- CAS[          75--( isDigit(c) )-->76 ]
        {
          value = value * 10 + valueDecimal(c); 
          state = 76; 
        }
                                        // --117--
        else if  ( c == '\n' )        // --117-- CAS[          75--( c == '\n' )-->245 ]
        {
           
          {
              if (debug & 1){
                  printDebug_s_port_value();
              }
              if ( servos & (1 << port)) {
                  // Serial.println("config ok");
                  if ( servoObject[port] != NULL )
                      //Serial.println("config not null");
                  servoObject[port]->write( value);
              }
          }
           
          state = 245; 
        }
                                        // --118--
        else {
          state = 246;                // --118-- ERROR 
        }
      } 
    break; 
    case 76:                     // --118-- CAS[          76--( c == '\n' )-->245 ]
      { 
        if  ( c == '\n' )        // --118-- CAS[          76--( c == '\n' )-->245 ]
        {
           
          {
              if (debug & 1){
                  printDebug_s_port_value();
              }
              if ( servos & (1 << port)) {
                  // Serial.println("config ok");
                  if ( servoObject[port] != NULL )
                      //Serial.println("config not null");
                  servoObject[port]->write( value);
              }
          }
           
          state = 245; 
        }
                                        // --119--
        else {
          state = 246;                // --119-- ERROR 
        }
      } 
    break; 
    case 77:                     // --119-- CAS[          77--( c == 'e' )-->78 ]
      { 
        if  ( c == 'e' )        // --119-- CAS[          77--( c == 'e' )-->78 ]
        {
           
          state = 78; 
        }
                                        // --120--
        else {
          state = 246;                // --120-- ERROR 
        }
      } 
    break; 
    case 78:                     // --120-- CAS[          78--( c == 'l' )-->79 ]
      { 
        if  ( c == 'l' )        // --120-- CAS[          78--( c == 'l' )-->79 ]
        {
           
          state = 79; 
        }
                                        // --121--
        else {
          state = 246;                // --121-- ERROR 
        }
      } 
    break; 
    case 79:                     // --121-- CAS[          79--( c == 'p' )-->80 ]
      { 
        if  ( c == 'p' )        // --121-- CAS[          79--( c == 'p' )-->80 ]
        {
           
          state = 80; 
        }
                                        // --122--
        else {
          state = 246;                // --122-- ERROR 
        }
      } 
    break; 
    case 80:                     // --122-- CAS[          80--( c == '\n' )-->245 ]
      { 
        if  ( c == '\n' )        // --122-- CAS[          80--( c == '\n' )-->245 ]
        {
          
          {
              for ( int k = 0; TRUE; k ++ ) {
                  char  c =  pgm_read_byte_near(helpText + k);
                  if ( c == 0 ) break;
                  Serial.print(c);
              }
          }
           
          state = 245; 
        }
                                        // --123--
        else {
          state = 246;                // --123-- ERROR 
        }
      } 
    break; 
    case 81:                     // --123-- CAS[          81--( c == 'i' )-->82 ]
      { 
        if  ( c == 'i' )        // --123-- CAS[          81--( c == 'i' )-->82 ]
        {
           
          state = 82; 
        }
                                        // --124--
        else if  ( c == 'd' )        // --124-- CAS[          81--( c == 'd' )-->105 ]
        {
           
          state = 105; 
        }
                                        // --125--
        else if  ( c == 'a' )        // --125-- CAS[          81--( c == 'a' )-->178 ]
        {
           
          state = 178; 
        }
                                        // --126--
        else if  ( c == 'v' )        // --126-- CAS[          81--( c == 'v' )-->233 ]
        {
           
          state = 233; 
        }
                                        // --127--
        else if  ( c == 'e' )        // --127-- CAS[          81--( c == 'e' )-->241 ]
        {
           
          state = 241; 
        }
                                        // --128--
        else {
          state = 246;                // --128-- ERROR 
        }
      } 
    break; 
    case 82:                     // --128-- CAS[          82--( c == 'd' )-->83 ]
      { 
        if  ( c == 'd' )        // --128-- CAS[          82--( c == 'd' )-->83 ]
        {
           
          state = 83; 
        }
                                        // --129--
        else {
          state = 246;                // --129-- ERROR 
        }
      } 
    break; 
    case 83:                     // --129-- CAS[          83--( c == 'e' )-->84 ]
      { 
        if  ( c == 'e' )        // --129-- CAS[          83--( c == 'e' )-->84 ]
        {
           
          state = 84; 
        }
                                        // --130--
        else {
          state = 246;                // --130-- ERROR 
        }
      } 
    break; 
    case 84:                     // --130-- CAS[          84--( c == 'n' )-->85 ]
      { 
        if  ( c == 'n' )        // --130-- CAS[          84--( c == 'n' )-->85 ]
        {
           
          state = 85; 
        }
                                        // --131--
        else {
          state = 246;                // --131-- ERROR 
        }
      } 
    break; 
    case 85:                     // --131-- CAS[          85--( c == 't' )-->86 ]
      { 
        if  ( c == 't' )        // --131-- CAS[          85--( c == 't' )-->86 ]
        {
           
          state = 86; 
        }
                                        // --132--
        else {
          state = 246;                // --132-- ERROR 
        }
      } 
    break; 
    case 86:                     // --132-- CAS[          86--( c == ':' )-->87 ]
      { 
        if  ( c == ':' )        // --132-- CAS[          86--( c == ':' )-->87 ]
        {
           
          state = 87; 
        }
                                        // --133--
        else if  ( c == '?' )        // --133-- CAS[          86--( c == '?' )-->104 ]
        {
           
          state = 104; 
        }
                                        // --134--
        else {
          state = 246;                // --134-- ERROR 
        }
      } 
    break; 
    case 87:                     // --134-- CAS[          87--( isChar(c) )-->88 ]
      { 
        if  ( isChar(c) )        // --134-- CAS[          87--( isChar(c) )-->88 ]
        {
          id[0] = c;id[0+1] = 0; 
          state = 88; 
        }
                                        // --135--
        else {
          state = 246;                // --135-- ERROR 
        }
      } 
    break; 
    case 88:                     // --135-- CAS[          88--( isLabel(c) )-->89 ]
      { 
        if  ( isLabel(c) )        // --135-- CAS[          88--( isLabel(c) )-->89 ]
        {
          id[1] = c;id[1+1] = 0; 
          state = 89; 
        }
                                        // --136--
        else if  ( c == '\n' )        // --136-- CAS[          88--( c == '\n' )-->245 ]
        {
          
          {
              setEEPROM();
              if ( debug & 1) {
                  Serial.print("cident=");
                  Serial.println(id);
              }
          }
           
          state = 245; 
        }
                                        // --137--
        else {
          state = 246;                // --137-- ERROR 
        }
      } 
    break; 
    case 89:                     // --137-- CAS[          89--( isLabel(c) )-->90 ]
      { 
        if  ( isLabel(c) )        // --137-- CAS[          89--( isLabel(c) )-->90 ]
        {
          id[2] = c;id[2+1] = 0; 
          state = 90; 
        }
                                        // --138--
        else if  ( c == '\n' )        // --138-- CAS[          89--( c == '\n' )-->245 ]
        {
          
          {
              setEEPROM();
              if ( debug & 1) {
                  Serial.print("cident=");
                  Serial.println(id);
              }
          }
           
          state = 245; 
        }
                                        // --139--
        else {
          state = 246;                // --139-- ERROR 
        }
      } 
    break; 
    case 90:                     // --139-- CAS[          90--( isLabel(c) )-->91 ]
      { 
        if  ( isLabel(c) )        // --139-- CAS[          90--( isLabel(c) )-->91 ]
        {
          id[3] = c;id[3+1] = 0; 
          state = 91; 
        }
                                        // --140--
        else if  ( c == '\n' )        // --140-- CAS[          90--( c == '\n' )-->245 ]
        {
          
          {
              setEEPROM();
              if ( debug & 1) {
                  Serial.print("cident=");
                  Serial.println(id);
              }
          }
           
          state = 245; 
        }
                                        // --141--
        else {
          state = 246;                // --141-- ERROR 
        }
      } 
    break; 
    case 91:                     // --141-- CAS[          91--( isLabel(c) )-->92 ]
      { 
        if  ( isLabel(c) )        // --141-- CAS[          91--( isLabel(c) )-->92 ]
        {
          id[4] = c;id[4+1] = 0; 
          state = 92; 
        }
                                        // --142--
        else if  ( c == '\n' )        // --142-- CAS[          91--( c == '\n' )-->245 ]
        {
          
          {
              setEEPROM();
              if ( debug & 1) {
                  Serial.print("cident=");
                  Serial.println(id);
              }
          }
           
          state = 245; 
        }
                                        // --143--
        else {
          state = 246;                // --143-- ERROR 
        }
      } 
    break; 
    case 92:                     // --143-- CAS[          92--( isLabel(c) )-->93 ]
      { 
        if  ( isLabel(c) )        // --143-- CAS[          92--( isLabel(c) )-->93 ]
        {
          id[5] = c;id[5+1] = 0; 
          state = 93; 
        }
                                        // --144--
        else if  ( c == '\n' )        // --144-- CAS[          92--( c == '\n' )-->245 ]
        {
          
          {
              setEEPROM();
              if ( debug & 1) {
                  Serial.print("cident=");
                  Serial.println(id);
              }
          }
           
          state = 245; 
        }
                                        // --145--
        else {
          state = 246;                // --145-- ERROR 
        }
      } 
    break; 
    case 93:                     // --145-- CAS[          93--( isLabel(c) )-->94 ]
      { 
        if  ( isLabel(c) )        // --145-- CAS[          93--( isLabel(c) )-->94 ]
        {
          id[6] = c;id[6+1] = 0; 
          state = 94; 
        }
                                        // --146--
        else if  ( c == '\n' )        // --146-- CAS[          93--( c == '\n' )-->245 ]
        {
          
          {
              setEEPROM();
              if ( debug & 1) {
                  Serial.print("cident=");
                  Serial.println(id);
              }
          }
           
          state = 245; 
        }
                                        // --147--
        else {
          state = 246;                // --147-- ERROR 
        }
      } 
    break; 
    case 94:                     // --147-- CAS[          94--( isLabel(c) )-->95 ]
      { 
        if  ( isLabel(c) )        // --147-- CAS[          94--( isLabel(c) )-->95 ]
        {
          id[7] = c;id[7+1] = 0; 
          state = 95; 
        }
                                        // --148--
        else if  ( c == '\n' )        // --148-- CAS[          94--( c == '\n' )-->245 ]
        {
          
          {
              setEEPROM();
              if ( debug & 1) {
                  Serial.print("cident=");
                  Serial.println(id);
              }
          }
           
          state = 245; 
        }
                                        // --149--
        else {
          state = 246;                // --149-- ERROR 
        }
      } 
    break; 
    case 95:                     // --149-- CAS[          95--( isLabel(c) )-->96 ]
      { 
        if  ( isLabel(c) )        // --149-- CAS[          95--( isLabel(c) )-->96 ]
        {
          id[8] = c;id[8+1] = 0; 
          state = 96; 
        }
                                        // --150--
        else if  ( c == '\n' )        // --150-- CAS[          95--( c == '\n' )-->245 ]
        {
          
          {
              setEEPROM();
              if ( debug & 1) {
                  Serial.print("cident=");
                  Serial.println(id);
              }
          }
           
          state = 245; 
        }
                                        // --151--
        else {
          state = 246;                // --151-- ERROR 
        }
      } 
    break; 
    case 96:                     // --151-- CAS[          96--( isLabel(c) )-->97 ]
      { 
        if  ( isLabel(c) )        // --151-- CAS[          96--( isLabel(c) )-->97 ]
        {
          id[9] = c;id[9+1] = 0; 
          state = 97; 
        }
                                        // --152--
        else if  ( c == '\n' )        // --152-- CAS[          96--( c == '\n' )-->245 ]
        {
          
          {
              setEEPROM();
              if ( debug & 1) {
                  Serial.print("cident=");
                  Serial.println(id);
              }
          }
           
          state = 245; 
        }
                                        // --153--
        else {
          state = 246;                // --153-- ERROR 
        }
      } 
    break; 
    case 97:                     // --153-- CAS[          97--( isLabel(c) )-->98 ]
      { 
        if  ( isLabel(c) )        // --153-- CAS[          97--( isLabel(c) )-->98 ]
        {
          id[10] = c;id[10+1] = 0; 
          state = 98; 
        }
                                        // --154--
        else if  ( c == '\n' )        // --154-- CAS[          97--( c == '\n' )-->245 ]
        {
          
          {
              setEEPROM();
              if ( debug & 1) {
                  Serial.print("cident=");
                  Serial.println(id);
              }
          }
           
          state = 245; 
        }
                                        // --155--
        else {
          state = 246;                // --155-- ERROR 
        }
      } 
    break; 
    case 98:                     // --155-- CAS[          98--( isLabel(c) )-->99 ]
      { 
        if  ( isLabel(c) )        // --155-- CAS[          98--( isLabel(c) )-->99 ]
        {
          id[11] = c;id[11+1] = 0; 
          state = 99; 
        }
                                        // --156--
        else if  ( c == '\n' )        // --156-- CAS[          98--( c == '\n' )-->245 ]
        {
          
          {
              setEEPROM();
              if ( debug & 1) {
                  Serial.print("cident=");
                  Serial.println(id);
              }
          }
           
          state = 245; 
        }
                                        // --157--
        else {
          state = 246;                // --157-- ERROR 
        }
      } 
    break; 
    case 99:                     // --157-- CAS[          99--( isLabel(c) )-->100 ]
      { 
        if  ( isLabel(c) )        // --157-- CAS[          99--( isLabel(c) )-->100 ]
        {
          id[12] = c;id[12+1] = 0; 
          state = 100; 
        }
                                        // --158--
        else if  ( c == '\n' )        // --158-- CAS[          99--( c == '\n' )-->245 ]
        {
          
          {
              setEEPROM();
              if ( debug & 1) {
                  Serial.print("cident=");
                  Serial.println(id);
              }
          }
           
          state = 245; 
        }
                                        // --159--
        else {
          state = 246;                // --159-- ERROR 
        }
      } 
    break; 
    case 100:                     // --159-- CAS[          100--( isLabel(c) )-->101 ]
      { 
        if  ( isLabel(c) )        // --159-- CAS[          100--( isLabel(c) )-->101 ]
        {
          id[13] = c;id[13+1] = 0; 
          state = 101; 
        }
                                        // --160--
        else if  ( c == '\n' )        // --160-- CAS[          100--( c == '\n' )-->245 ]
        {
          
          {
              setEEPROM();
              if ( debug & 1) {
                  Serial.print("cident=");
                  Serial.println(id);
              }
          }
           
          state = 245; 
        }
                                        // --161--
        else {
          state = 246;                // --161-- ERROR 
        }
      } 
    break; 
    case 101:                     // --161-- CAS[          101--( isLabel(c) )-->102 ]
      { 
        if  ( isLabel(c) )        // --161-- CAS[          101--( isLabel(c) )-->102 ]
        {
          id[14] = c;id[14+1] = 0; 
          state = 102; 
        }
                                        // --162--
        else if  ( c == '\n' )        // --162-- CAS[          101--( c == '\n' )-->245 ]
        {
          
          {
              setEEPROM();
              if ( debug & 1) {
                  Serial.print("cident=");
                  Serial.println(id);
              }
          }
           
          state = 245; 
        }
                                        // --163--
        else {
          state = 246;                // --163-- ERROR 
        }
      } 
    break; 
    case 102:                     // --163-- CAS[          102--( isLabel(c) )-->103 ]
      { 
        if  ( isLabel(c) )        // --163-- CAS[          102--( isLabel(c) )-->103 ]
        {
          id[15] = c;id[15+1] = 0; 
          state = 103; 
        }
                                        // --164--
        else if  ( c == '\n' )        // --164-- CAS[          102--( c == '\n' )-->245 ]
        {
          
          {
              setEEPROM();
              if ( debug & 1) {
                  Serial.print("cident=");
                  Serial.println(id);
              }
          }
           
          state = 245; 
        }
                                        // --165--
        else {
          state = 246;                // --165-- ERROR 
        }
      } 
    break; 
    case 103:                     // --165-- CAS[          103--( c == '\n' )-->245 ]
      { 
        if  ( c == '\n' )        // --165-- CAS[          103--( c == '\n' )-->245 ]
        {
          
          {
              setEEPROM();
              if ( debug & 1) {
                  Serial.print("cident=");
                  Serial.println(id);
              }
          }
           
          state = 245; 
        }
                                        // --166--
        else {
          state = 246;                // --166-- ERROR 
        }
      } 
    break; 
    case 104:                     // --166-- CAS[          104--( c == '\n' )-->245 ]
      { 
        if  ( c == '\n' )        // --166-- CAS[          104--( c == '\n' )-->245 ]
        {
          
          {
              getEEPROM();
          }
           
          state = 245; 
        }
                                        // --167--
        else {
          state = 246;                // --167-- ERROR 
        }
      } 
    break; 
    case 105:                     // --167-- CAS[          105--( c == 'i' )-->106 ]
      { 
        if  ( c == 'i' )        // --167-- CAS[          105--( c == 'i' )-->106 ]
        {
           
          state = 106; 
        }
                                        // --168--
        else if  ( c == 'e' )        // --168-- CAS[          105--( c == 'e' )-->117 ]
        {
           
          state = 117; 
        }
                                        // --169--
        else if  ( c == 'o' )        // --169-- CAS[          105--( c == 'o' )-->140 ]
        {
           
          state = 140; 
        }
                                        // --170--
        else if  ( c == 'p' )        // --170-- CAS[          105--( c == 'p' )-->152 ]
        {
           
          state = 152; 
        }
                                        // --171--
        else if  ( c == 's' )        // --171-- CAS[          105--( c == 's' )-->164 ]
        {
           
          state = 164; 
        }
                                        // --172--
        else {
          state = 246;                // --172-- ERROR 
        }
      } 
    break; 
    case 106:                     // --172-- CAS[          106--( c == 'n' )-->107 ]
      { 
        if  ( c == 'n' )        // --172-- CAS[          106--( c == 'n' )-->107 ]
        {
           
          state = 107; 
        }
                                        // --173--
        else {
          state = 246;                // --173-- ERROR 
        }
      } 
    break; 
    case 107:                     // --173-- CAS[          107--( c == ':' )-->108 ]
      { 
        if  ( c == ':' )        // --173-- CAS[          107--( c == ':' )-->108 ]
        {
           
          state = 108; 
        }
                                        // --174--
        else if  ( c == 'p' )        // --174-- CAS[          107--( c == 'p' )-->130 ]
        {
           
          state = 130; 
        }
                                        // --175--
        else {
          state = 246;                // --175-- ERROR 
        }
      } 
    break; 
    case 108:                     // --175-- CAS[          108--( isHex(c) )-->109 ]
      { 
        if  ( isHex(c) )        // --175-- CAS[          108--( isHex(c) )-->109 ]
        {
          data = valueHex(c); 
          state = 109; 
        }
                                        // --176--
        else {
          state = 246;                // --176-- ERROR 
        }
      } 
    break; 
    case 109:                     // --176-- CAS[          109--( isHex(c) )-->110 ]
      { 
        if  ( isHex(c) )        // --176-- CAS[          109--( isHex(c) )-->110 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 110; 
        }
                                        // --177--
        else if  ( c == '\n' )        // --177-- CAS[          109--( c == '\n' )-->245 ]
        {
          
          {
              setDigitalInput(data);
              if ( debug & 1) {
                  Serial.print("cdin=");
                  Serial.println(data,HEX);
              }
          }
           
          state = 245; 
        }
                                        // --178--
        else {
          state = 246;                // --178-- ERROR 
        }
      } 
    break; 
    case 110:                     // --178-- CAS[          110--( isHex(c) )-->111 ]
      { 
        if  ( isHex(c) )        // --178-- CAS[          110--( isHex(c) )-->111 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 111; 
        }
                                        // --179--
        else if  ( c == '\n' )        // --179-- CAS[          110--( c == '\n' )-->245 ]
        {
          
          {
              setDigitalInput(data);
              if ( debug & 1) {
                  Serial.print("cdin=");
                  Serial.println(data,HEX);
              }
          }
           
          state = 245; 
        }
                                        // --180--
        else {
          state = 246;                // --180-- ERROR 
        }
      } 
    break; 
    case 111:                     // --180-- CAS[          111--( isHex(c) )-->112 ]
      { 
        if  ( isHex(c) )        // --180-- CAS[          111--( isHex(c) )-->112 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 112; 
        }
                                        // --181--
        else if  ( c == '\n' )        // --181-- CAS[          111--( c == '\n' )-->245 ]
        {
          
          {
              setDigitalInput(data);
              if ( debug & 1) {
                  Serial.print("cdin=");
                  Serial.println(data,HEX);
              }
          }
           
          state = 245; 
        }
                                        // --182--
        else {
          state = 246;                // --182-- ERROR 
        }
      } 
    break; 
    case 112:                     // --182-- CAS[          112--( isHex(c) )-->113 ]
      { 
        if  ( isHex(c) )        // --182-- CAS[          112--( isHex(c) )-->113 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 113; 
        }
                                        // --183--
        else if  ( c == '\n' )        // --183-- CAS[          112--( c == '\n' )-->245 ]
        {
          
          {
              setDigitalInput(data);
              if ( debug & 1) {
                  Serial.print("cdin=");
                  Serial.println(data,HEX);
              }
          }
           
          state = 245; 
        }
                                        // --184--
        else {
          state = 246;                // --184-- ERROR 
        }
      } 
    break; 
    case 113:                     // --184-- CAS[          113--( isHex(c) )-->114 ]
      { 
        if  ( isHex(c) )        // --184-- CAS[          113--( isHex(c) )-->114 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 114; 
        }
                                        // --185--
        else if  ( c == '\n' )        // --185-- CAS[          113--( c == '\n' )-->245 ]
        {
          
          {
              setDigitalInput(data);
              if ( debug & 1) {
                  Serial.print("cdin=");
                  Serial.println(data,HEX);
              }
          }
           
          state = 245; 
        }
                                        // --186--
        else {
          state = 246;                // --186-- ERROR 
        }
      } 
    break; 
    case 114:                     // --186-- CAS[          114--( isHex(c) )-->115 ]
      { 
        if  ( isHex(c) )        // --186-- CAS[          114--( isHex(c) )-->115 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 115; 
        }
                                        // --187--
        else if  ( c == '\n' )        // --187-- CAS[          114--( c == '\n' )-->245 ]
        {
          
          {
              setDigitalInput(data);
              if ( debug & 1) {
                  Serial.print("cdin=");
                  Serial.println(data,HEX);
              }
          }
           
          state = 245; 
        }
                                        // --188--
        else {
          state = 246;                // --188-- ERROR 
        }
      } 
    break; 
    case 115:                     // --188-- CAS[          115--( isHex(c) )-->116 ]
      { 
        if  ( isHex(c) )        // --188-- CAS[          115--( isHex(c) )-->116 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 116; 
        }
                                        // --189--
        else if  ( c == '\n' )        // --189-- CAS[          115--( c == '\n' )-->245 ]
        {
          
          {
              setDigitalInput(data);
              if ( debug & 1) {
                  Serial.print("cdin=");
                  Serial.println(data,HEX);
              }
          }
           
          state = 245; 
        }
                                        // --190--
        else {
          state = 246;                // --190-- ERROR 
        }
      } 
    break; 
    case 116:                     // --190-- CAS[          116--( c == '\n' )-->245 ]
      { 
        if  ( c == '\n' )        // --190-- CAS[          116--( c == '\n' )-->245 ]
        {
          
          {
              setDigitalInput(data);
              if ( debug & 1) {
                  Serial.print("cdin=");
                  Serial.println(data,HEX);
              }
          }
           
          state = 245; 
        }
                                        // --191--
        else {
          state = 246;                // --191-- ERROR 
        }
      } 
    break; 
    case 117:                     // --191-- CAS[          117--( c == 'b' )-->118 ]
      { 
        if  ( c == 'b' )        // --191-- CAS[          117--( c == 'b' )-->118 ]
        {
           
          state = 118; 
        }
                                        // --192--
        else {
          state = 246;                // --192-- ERROR 
        }
      } 
    break; 
    case 118:                     // --192-- CAS[          118--( c == 'u' )-->119 ]
      { 
        if  ( c == 'u' )        // --192-- CAS[          118--( c == 'u' )-->119 ]
        {
           
          state = 119; 
        }
                                        // --193--
        else {
          state = 246;                // --193-- ERROR 
        }
      } 
    break; 
    case 119:                     // --193-- CAS[          119--( c == 'g' )-->120 ]
      { 
        if  ( c == 'g' )        // --193-- CAS[          119--( c == 'g' )-->120 ]
        {
           
          state = 120; 
        }
                                        // --194--
        else {
          state = 246;                // --194-- ERROR 
        }
      } 
    break; 
    case 120:                     // --194-- CAS[          120--( c == ':' )-->121 ]
      { 
        if  ( c == ':' )        // --194-- CAS[          120--( c == ':' )-->121 ]
        {
           
          state = 121; 
        }
                                        // --195--
        else {
          state = 246;                // --195-- ERROR 
        }
      } 
    break; 
    case 121:                     // --195-- CAS[          121--( isHex(c) )-->122 ]
      { 
        if  ( isHex(c) )        // --195-- CAS[          121--( isHex(c) )-->122 ]
        {
          data = valueHex(c); 
          state = 122; 
        }
                                        // --196--
        else {
          state = 246;                // --196-- ERROR 
        }
      } 
    break; 
    case 122:                     // --196-- CAS[          122--( isHex(c) )-->123 ]
      { 
        if  ( isHex(c) )        // --196-- CAS[          122--( isHex(c) )-->123 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 123; 
        }
                                        // --197--
        else if  ( c == '\n' )        // --197-- CAS[          122--( c == '\n' )-->245 ]
        {
           
          {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              debug =  data;
              if ( debug & 1) {
                  printDebug_cdebug();
              }
          
           }
           
          state = 245; 
        }
                                        // --198--
        else {
          state = 246;                // --198-- ERROR 
        }
      } 
    break; 
    case 123:                     // --198-- CAS[          123--( isHex(c) )-->124 ]
      { 
        if  ( isHex(c) )        // --198-- CAS[          123--( isHex(c) )-->124 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 124; 
        }
                                        // --199--
        else if  ( c == '\n' )        // --199-- CAS[          123--( c == '\n' )-->245 ]
        {
           
          {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              debug =  data;
              if ( debug & 1) {
                  printDebug_cdebug();
              }
          
           }
           
          state = 245; 
        }
                                        // --200--
        else {
          state = 246;                // --200-- ERROR 
        }
      } 
    break; 
    case 124:                     // --200-- CAS[          124--( isHex(c) )-->125 ]
      { 
        if  ( isHex(c) )        // --200-- CAS[          124--( isHex(c) )-->125 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 125; 
        }
                                        // --201--
        else if  ( c == '\n' )        // --201-- CAS[          124--( c == '\n' )-->245 ]
        {
           
          {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              debug =  data;
              if ( debug & 1) {
                  printDebug_cdebug();
              }
          
           }
           
          state = 245; 
        }
                                        // --202--
        else {
          state = 246;                // --202-- ERROR 
        }
      } 
    break; 
    case 125:                     // --202-- CAS[          125--( isHex(c) )-->126 ]
      { 
        if  ( isHex(c) )        // --202-- CAS[          125--( isHex(c) )-->126 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 126; 
        }
                                        // --203--
        else if  ( c == '\n' )        // --203-- CAS[          125--( c == '\n' )-->245 ]
        {
           
          {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              debug =  data;
              if ( debug & 1) {
                  printDebug_cdebug();
              }
          
           }
           
          state = 245; 
        }
                                        // --204--
        else {
          state = 246;                // --204-- ERROR 
        }
      } 
    break; 
    case 126:                     // --204-- CAS[          126--( isHex(c) )-->127 ]
      { 
        if  ( isHex(c) )        // --204-- CAS[          126--( isHex(c) )-->127 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 127; 
        }
                                        // --205--
        else if  ( c == '\n' )        // --205-- CAS[          126--( c == '\n' )-->245 ]
        {
           
          {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              debug =  data;
              if ( debug & 1) {
                  printDebug_cdebug();
              }
          
           }
           
          state = 245; 
        }
                                        // --206--
        else {
          state = 246;                // --206-- ERROR 
        }
      } 
    break; 
    case 127:                     // --206-- CAS[          127--( isHex(c) )-->128 ]
      { 
        if  ( isHex(c) )        // --206-- CAS[          127--( isHex(c) )-->128 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 128; 
        }
                                        // --207--
        else if  ( c == '\n' )        // --207-- CAS[          127--( c == '\n' )-->245 ]
        {
           
          {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              debug =  data;
              if ( debug & 1) {
                  printDebug_cdebug();
              }
          
           }
           
          state = 245; 
        }
                                        // --208--
        else {
          state = 246;                // --208-- ERROR 
        }
      } 
    break; 
    case 128:                     // --208-- CAS[          128--( isHex(c) )-->129 ]
      { 
        if  ( isHex(c) )        // --208-- CAS[          128--( isHex(c) )-->129 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 129; 
        }
                                        // --209--
        else if  ( c == '\n' )        // --209-- CAS[          128--( c == '\n' )-->245 ]
        {
           
          {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              debug =  data;
              if ( debug & 1) {
                  printDebug_cdebug();
              }
          
           }
           
          state = 245; 
        }
                                        // --210--
        else {
          state = 246;                // --210-- ERROR 
        }
      } 
    break; 
    case 129:                     // --210-- CAS[          129--( c == '\n' )-->245 ]
      { 
        if  ( c == '\n' )        // --210-- CAS[          129--( c == '\n' )-->245 ]
        {
           
          {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              debug =  data;
              if ( debug & 1) {
                  printDebug_cdebug();
              }
          
           }
           
          state = 245; 
        }
                                        // --211--
        else {
          state = 246;                // --211-- ERROR 
        }
      } 
    break; 
    case 130:                     // --211-- CAS[          130--( c == ':' )-->131 ]
      { 
        if  ( c == ':' )        // --211-- CAS[          130--( c == ':' )-->131 ]
        {
           
          state = 131; 
        }
                                        // --212--
        else {
          state = 246;                // --212-- ERROR 
        }
      } 
    break; 
    case 131:                     // --212-- CAS[          131--( isHex(c) )-->132 ]
      { 
        if  ( isHex(c) )        // --212-- CAS[          131--( isHex(c) )-->132 ]
        {
          data = valueHex(c); 
          state = 132; 
        }
                                        // --213--
        else {
          state = 246;                // --213-- ERROR 
        }
      } 
    break; 
    case 132:                     // --213-- CAS[          132--( isHex(c) )-->133 ]
      { 
        if  ( isHex(c) )        // --213-- CAS[          132--( isHex(c) )-->133 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 133; 
        }
                                        // --214--
        else if  ( c == '\n' )        // --214-- CAS[          132--( c == '\n' )-->245 ]
        {
           
          {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalInputPullup(data);
              if ( debug & 1) {
                  printDebug_cdinp();
                  
              }
          }
           
          state = 245; 
        }
                                        // --215--
        else {
          state = 246;                // --215-- ERROR 
        }
      } 
    break; 
    case 133:                     // --215-- CAS[          133--( isHex(c) )-->134 ]
      { 
        if  ( isHex(c) )        // --215-- CAS[          133--( isHex(c) )-->134 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 134; 
        }
                                        // --216--
        else if  ( c == '\n' )        // --216-- CAS[          133--( c == '\n' )-->245 ]
        {
           
          {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalInputPullup(data);
              if ( debug & 1) {
                  printDebug_cdinp();
                  
              }
          }
           
          state = 245; 
        }
                                        // --217--
        else {
          state = 246;                // --217-- ERROR 
        }
      } 
    break; 
    case 134:                     // --217-- CAS[          134--( isHex(c) )-->135 ]
      { 
        if  ( isHex(c) )        // --217-- CAS[          134--( isHex(c) )-->135 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 135; 
        }
                                        // --218--
        else if  ( c == '\n' )        // --218-- CAS[          134--( c == '\n' )-->245 ]
        {
           
          {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalInputPullup(data);
              if ( debug & 1) {
                  printDebug_cdinp();
                  
              }
          }
           
          state = 245; 
        }
                                        // --219--
        else {
          state = 246;                // --219-- ERROR 
        }
      } 
    break; 
    case 135:                     // --219-- CAS[          135--( isHex(c) )-->136 ]
      { 
        if  ( isHex(c) )        // --219-- CAS[          135--( isHex(c) )-->136 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 136; 
        }
                                        // --220--
        else if  ( c == '\n' )        // --220-- CAS[          135--( c == '\n' )-->245 ]
        {
           
          {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalInputPullup(data);
              if ( debug & 1) {
                  printDebug_cdinp();
                  
              }
          }
           
          state = 245; 
        }
                                        // --221--
        else {
          state = 246;                // --221-- ERROR 
        }
      } 
    break; 
    case 136:                     // --221-- CAS[          136--( isHex(c) )-->137 ]
      { 
        if  ( isHex(c) )        // --221-- CAS[          136--( isHex(c) )-->137 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 137; 
        }
                                        // --222--
        else if  ( c == '\n' )        // --222-- CAS[          136--( c == '\n' )-->245 ]
        {
           
          {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalInputPullup(data);
              if ( debug & 1) {
                  printDebug_cdinp();
                  
              }
          }
           
          state = 245; 
        }
                                        // --223--
        else {
          state = 246;                // --223-- ERROR 
        }
      } 
    break; 
    case 137:                     // --223-- CAS[          137--( isHex(c) )-->138 ]
      { 
        if  ( isHex(c) )        // --223-- CAS[          137--( isHex(c) )-->138 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 138; 
        }
                                        // --224--
        else if  ( c == '\n' )        // --224-- CAS[          137--( c == '\n' )-->245 ]
        {
           
          {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalInputPullup(data);
              if ( debug & 1) {
                  printDebug_cdinp();
                  
              }
          }
           
          state = 245; 
        }
                                        // --225--
        else {
          state = 246;                // --225-- ERROR 
        }
      } 
    break; 
    case 138:                     // --225-- CAS[          138--( isHex(c) )-->139 ]
      { 
        if  ( isHex(c) )        // --225-- CAS[          138--( isHex(c) )-->139 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 139; 
        }
                                        // --226--
        else if  ( c == '\n' )        // --226-- CAS[          138--( c == '\n' )-->245 ]
        {
           
          {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalInputPullup(data);
              if ( debug & 1) {
                  printDebug_cdinp();
                  
              }
          }
           
          state = 245; 
        }
                                        // --227--
        else {
          state = 246;                // --227-- ERROR 
        }
      } 
    break; 
    case 139:                     // --227-- CAS[          139--( c == '\n' )-->245 ]
      { 
        if  ( c == '\n' )        // --227-- CAS[          139--( c == '\n' )-->245 ]
        {
           
          {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalInputPullup(data);
              if ( debug & 1) {
                  printDebug_cdinp();
                  
              }
          }
           
          state = 245; 
        }
                                        // --228--
        else {
          state = 246;                // --228-- ERROR 
        }
      } 
    break; 
    case 140:                     // --228-- CAS[          140--( c == 'u' )-->141 ]
      { 
        if  ( c == 'u' )        // --228-- CAS[          140--( c == 'u' )-->141 ]
        {
           
          state = 141; 
        }
                                        // --229--
        else {
          state = 246;                // --229-- ERROR 
        }
      } 
    break; 
    case 141:                     // --229-- CAS[          141--( c == 't' )-->142 ]
      { 
        if  ( c == 't' )        // --229-- CAS[          141--( c == 't' )-->142 ]
        {
           
          state = 142; 
        }
                                        // --230--
        else {
          state = 246;                // --230-- ERROR 
        }
      } 
    break; 
    case 142:                     // --230-- CAS[          142--( c == ':' )-->143 ]
      { 
        if  ( c == ':' )        // --230-- CAS[          142--( c == ':' )-->143 ]
        {
           
          state = 143; 
        }
                                        // --231--
        else {
          state = 246;                // --231-- ERROR 
        }
      } 
    break; 
    case 143:                     // --231-- CAS[          143--( isHex(c) )-->144 ]
      { 
        if  ( isHex(c) )        // --231-- CAS[          143--( isHex(c) )-->144 ]
        {
          data = valueHex(c); 
          state = 144; 
        }
                                        // --232--
        else {
          state = 246;                // --232-- ERROR 
        }
      } 
    break; 
    case 144:                     // --232-- CAS[          144--( isHex(c) )-->145 ]
      { 
        if  ( isHex(c) )        // --232-- CAS[          144--( isHex(c) )-->145 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 145; 
        }
                                        // --233--
        else if  ( c == '\n' )        // --233-- CAS[          144--( c == '\n' )-->245 ]
        {
          
          {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalOutput(data);
              if ( debug & 1) {
                  printDebug_cdout();
              }
          }
           
          state = 245; 
        }
                                        // --234--
        else {
          state = 246;                // --234-- ERROR 
        }
      } 
    break; 
    case 145:                     // --234-- CAS[          145--( isHex(c) )-->146 ]
      { 
        if  ( isHex(c) )        // --234-- CAS[          145--( isHex(c) )-->146 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 146; 
        }
                                        // --235--
        else if  ( c == '\n' )        // --235-- CAS[          145--( c == '\n' )-->245 ]
        {
          
          {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalOutput(data);
              if ( debug & 1) {
                  printDebug_cdout();
              }
          }
           
          state = 245; 
        }
                                        // --236--
        else {
          state = 246;                // --236-- ERROR 
        }
      } 
    break; 
    case 146:                     // --236-- CAS[          146--( isHex(c) )-->147 ]
      { 
        if  ( isHex(c) )        // --236-- CAS[          146--( isHex(c) )-->147 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 147; 
        }
                                        // --237--
        else if  ( c == '\n' )        // --237-- CAS[          146--( c == '\n' )-->245 ]
        {
          
          {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalOutput(data);
              if ( debug & 1) {
                  printDebug_cdout();
              }
          }
           
          state = 245; 
        }
                                        // --238--
        else {
          state = 246;                // --238-- ERROR 
        }
      } 
    break; 
    case 147:                     // --238-- CAS[          147--( isHex(c) )-->148 ]
      { 
        if  ( isHex(c) )        // --238-- CAS[          147--( isHex(c) )-->148 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 148; 
        }
                                        // --239--
        else if  ( c == '\n' )        // --239-- CAS[          147--( c == '\n' )-->245 ]
        {
          
          {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalOutput(data);
              if ( debug & 1) {
                  printDebug_cdout();
              }
          }
           
          state = 245; 
        }
                                        // --240--
        else {
          state = 246;                // --240-- ERROR 
        }
      } 
    break; 
    case 148:                     // --240-- CAS[          148--( isHex(c) )-->149 ]
      { 
        if  ( isHex(c) )        // --240-- CAS[          148--( isHex(c) )-->149 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 149; 
        }
                                        // --241--
        else if  ( c == '\n' )        // --241-- CAS[          148--( c == '\n' )-->245 ]
        {
          
          {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalOutput(data);
              if ( debug & 1) {
                  printDebug_cdout();
              }
          }
           
          state = 245; 
        }
                                        // --242--
        else {
          state = 246;                // --242-- ERROR 
        }
      } 
    break; 
    case 149:                     // --242-- CAS[          149--( isHex(c) )-->150 ]
      { 
        if  ( isHex(c) )        // --242-- CAS[          149--( isHex(c) )-->150 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 150; 
        }
                                        // --243--
        else if  ( c == '\n' )        // --243-- CAS[          149--( c == '\n' )-->245 ]
        {
          
          {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalOutput(data);
              if ( debug & 1) {
                  printDebug_cdout();
              }
          }
           
          state = 245; 
        }
                                        // --244--
        else {
          state = 246;                // --244-- ERROR 
        }
      } 
    break; 
    case 150:                     // --244-- CAS[          150--( isHex(c) )-->151 ]
      { 
        if  ( isHex(c) )        // --244-- CAS[          150--( isHex(c) )-->151 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 151; 
        }
                                        // --245--
        else if  ( c == '\n' )        // --245-- CAS[          150--( c == '\n' )-->245 ]
        {
          
          {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalOutput(data);
              if ( debug & 1) {
                  printDebug_cdout();
              }
          }
           
          state = 245; 
        }
                                        // --246--
        else {
          state = 246;                // --246-- ERROR 
        }
      } 
    break; 
    case 151:                     // --246-- CAS[          151--( c == '\n' )-->245 ]
      { 
        if  ( c == '\n' )        // --246-- CAS[          151--( c == '\n' )-->245 ]
        {
          
          {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalOutput(data);
              if ( debug & 1) {
                  printDebug_cdout();
              }
          }
           
          state = 245; 
        }
                                        // --247--
        else {
          state = 246;                // --247-- ERROR 
        }
      } 
    break; 
    case 152:                     // --247-- CAS[          152--( c == 'w' )-->153 ]
      { 
        if  ( c == 'w' )        // --247-- CAS[          152--( c == 'w' )-->153 ]
        {
           
          state = 153; 
        }
                                        // --248--
        else {
          state = 246;                // --248-- ERROR 
        }
      } 
    break; 
    case 153:                     // --248-- CAS[          153--( c == 'm' )-->154 ]
      { 
        if  ( c == 'm' )        // --248-- CAS[          153--( c == 'm' )-->154 ]
        {
           
          state = 154; 
        }
                                        // --249--
        else {
          state = 246;                // --249-- ERROR 
        }
      } 
    break; 
    case 154:                     // --249-- CAS[          154--( c == ':' )-->155 ]
      { 
        if  ( c == ':' )        // --249-- CAS[          154--( c == ':' )-->155 ]
        {
           
          state = 155; 
        }
                                        // --250--
        else {
          state = 246;                // --250-- ERROR 
        }
      } 
    break; 
    case 155:                     // --250-- CAS[          155--( isHex(c) )-->156 ]
      { 
        if  ( isHex(c) )        // --250-- CAS[          155--( isHex(c) )-->156 ]
        {
          data = valueHex(c); 
          state = 156; 
        }
                                        // --251--
        else {
          state = 246;                // --251-- ERROR 
        }
      } 
    break; 
    case 156:                     // --251-- CAS[          156--( isHex(c) )-->157 ]
      { 
        if  ( isHex(c) )        // --251-- CAS[          156--( isHex(c) )-->157 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 157; 
        }
                                        // --252--
        else if  ( c == '\n' )        // --252-- CAS[          156--( c == '\n' )-->245 ]
        {
          
          {
              pwms = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalPWMOutput(data);
              if ( debug & 1) {
                  printDebug_cdpwm();
              }
          }
           
          state = 245; 
        }
                                        // --253--
        else {
          state = 246;                // --253-- ERROR 
        }
      } 
    break; 
    case 157:                     // --253-- CAS[          157--( isHex(c) )-->158 ]
      { 
        if  ( isHex(c) )        // --253-- CAS[          157--( isHex(c) )-->158 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 158; 
        }
                                        // --254--
        else if  ( c == '\n' )        // --254-- CAS[          157--( c == '\n' )-->245 ]
        {
          
          {
              pwms = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalPWMOutput(data);
              if ( debug & 1) {
                  printDebug_cdpwm();
              }
          }
           
          state = 245; 
        }
                                        // --255--
        else {
          state = 246;                // --255-- ERROR 
        }
      } 
    break; 
    case 158:                     // --255-- CAS[          158--( isHex(c) )-->159 ]
      { 
        if  ( isHex(c) )        // --255-- CAS[          158--( isHex(c) )-->159 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 159; 
        }
                                        // --256--
        else if  ( c == '\n' )        // --256-- CAS[          158--( c == '\n' )-->245 ]
        {
          
          {
              pwms = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalPWMOutput(data);
              if ( debug & 1) {
                  printDebug_cdpwm();
              }
          }
           
          state = 245; 
        }
                                        // --257--
        else {
          state = 246;                // --257-- ERROR 
        }
      } 
    break; 
    case 159:                     // --257-- CAS[          159--( isHex(c) )-->160 ]
      { 
        if  ( isHex(c) )        // --257-- CAS[          159--( isHex(c) )-->160 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 160; 
        }
                                        // --258--
        else if  ( c == '\n' )        // --258-- CAS[          159--( c == '\n' )-->245 ]
        {
          
          {
              pwms = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalPWMOutput(data);
              if ( debug & 1) {
                  printDebug_cdpwm();
              }
          }
           
          state = 245; 
        }
                                        // --259--
        else {
          state = 246;                // --259-- ERROR 
        }
      } 
    break; 
    case 160:                     // --259-- CAS[          160--( isHex(c) )-->161 ]
      { 
        if  ( isHex(c) )        // --259-- CAS[          160--( isHex(c) )-->161 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 161; 
        }
                                        // --260--
        else if  ( c == '\n' )        // --260-- CAS[          160--( c == '\n' )-->245 ]
        {
          
          {
              pwms = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalPWMOutput(data);
              if ( debug & 1) {
                  printDebug_cdpwm();
              }
          }
           
          state = 245; 
        }
                                        // --261--
        else {
          state = 246;                // --261-- ERROR 
        }
      } 
    break; 
    case 161:                     // --261-- CAS[          161--( isHex(c) )-->162 ]
      { 
        if  ( isHex(c) )        // --261-- CAS[          161--( isHex(c) )-->162 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 162; 
        }
                                        // --262--
        else if  ( c == '\n' )        // --262-- CAS[          161--( c == '\n' )-->245 ]
        {
          
          {
              pwms = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalPWMOutput(data);
              if ( debug & 1) {
                  printDebug_cdpwm();
              }
          }
           
          state = 245; 
        }
                                        // --263--
        else {
          state = 246;                // --263-- ERROR 
        }
      } 
    break; 
    case 162:                     // --263-- CAS[          162--( isHex(c) )-->163 ]
      { 
        if  ( isHex(c) )        // --263-- CAS[          162--( isHex(c) )-->163 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 163; 
        }
                                        // --264--
        else if  ( c == '\n' )        // --264-- CAS[          162--( c == '\n' )-->245 ]
        {
          
          {
              pwms = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalPWMOutput(data);
              if ( debug & 1) {
                  printDebug_cdpwm();
              }
          }
           
          state = 245; 
        }
                                        // --265--
        else {
          state = 246;                // --265-- ERROR 
        }
      } 
    break; 
    case 163:                     // --265-- CAS[          163--( c == '\n' )-->245 ]
      { 
        if  ( c == '\n' )        // --265-- CAS[          163--( c == '\n' )-->245 ]
        {
          
          {
              pwms = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalPWMOutput(data);
              if ( debug & 1) {
                  printDebug_cdpwm();
              }
          }
           
          state = 245; 
        }
                                        // --266--
        else {
          state = 246;                // --266-- ERROR 
        }
      } 
    break; 
    case 164:                     // --266-- CAS[          164--( c == 'e' )-->165 ]
      { 
        if  ( c == 'e' )        // --266-- CAS[          164--( c == 'e' )-->165 ]
        {
           
          state = 165; 
        }
                                        // --267--
        else {
          state = 246;                // --267-- ERROR 
        }
      } 
    break; 
    case 165:                     // --267-- CAS[          165--( c == 'r' )-->166 ]
      { 
        if  ( c == 'r' )        // --267-- CAS[          165--( c == 'r' )-->166 ]
        {
           
          state = 166; 
        }
                                        // --268--
        else {
          state = 246;                // --268-- ERROR 
        }
      } 
    break; 
    case 166:                     // --268-- CAS[          166--( c == 'v' )-->167 ]
      { 
        if  ( c == 'v' )        // --268-- CAS[          166--( c == 'v' )-->167 ]
        {
           
          state = 167; 
        }
                                        // --269--
        else {
          state = 246;                // --269-- ERROR 
        }
      } 
    break; 
    case 167:                     // --269-- CAS[          167--( c == 'o' )-->168 ]
      { 
        if  ( c == 'o' )        // --269-- CAS[          167--( c == 'o' )-->168 ]
        {
           
          state = 168; 
        }
                                        // --270--
        else {
          state = 246;                // --270-- ERROR 
        }
      } 
    break; 
    case 168:                     // --270-- CAS[          168--( c == ':' )-->169 ]
      { 
        if  ( c == ':' )        // --270-- CAS[          168--( c == ':' )-->169 ]
        {
           
          state = 169; 
        }
                                        // --271--
        else {
          state = 246;                // --271-- ERROR 
        }
      } 
    break; 
    case 169:                     // --271-- CAS[          169--( isHex(c) )-->170 ]
      { 
        if  ( isHex(c) )        // --271-- CAS[          169--( isHex(c) )-->170 ]
        {
          data = valueHex(c); 
          state = 170; 
        }
                                        // --272--
        else {
          state = 246;                // --272-- ERROR 
        }
      } 
    break; 
    case 170:                     // --272-- CAS[          170--( isHex(c) )-->171 ]
      { 
        if  ( isHex(c) )        // --272-- CAS[          170--( isHex(c) )-->171 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 171; 
        }
                                        // --273--
        else if  ( c == '\n' )        // --273-- CAS[          170--( c == '\n' )-->245 ]
        {
           
          {
              servos = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalServoOutput(data);
              if ( debug & 1) {
                  printDebug_cdservo();
                  
              }
          }
           
          state = 245; 
        }
                                        // --274--
        else {
          state = 246;                // --274-- ERROR 
        }
      } 
    break; 
    case 171:                     // --274-- CAS[          171--( isHex(c) )-->172 ]
      { 
        if  ( isHex(c) )        // --274-- CAS[          171--( isHex(c) )-->172 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 172; 
        }
                                        // --275--
        else if  ( c == '\n' )        // --275-- CAS[          171--( c == '\n' )-->245 ]
        {
           
          {
              servos = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalServoOutput(data);
              if ( debug & 1) {
                  printDebug_cdservo();
                  
              }
          }
           
          state = 245; 
        }
                                        // --276--
        else {
          state = 246;                // --276-- ERROR 
        }
      } 
    break; 
    case 172:                     // --276-- CAS[          172--( isHex(c) )-->173 ]
      { 
        if  ( isHex(c) )        // --276-- CAS[          172--( isHex(c) )-->173 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 173; 
        }
                                        // --277--
        else if  ( c == '\n' )        // --277-- CAS[          172--( c == '\n' )-->245 ]
        {
           
          {
              servos = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalServoOutput(data);
              if ( debug & 1) {
                  printDebug_cdservo();
                  
              }
          }
           
          state = 245; 
        }
                                        // --278--
        else {
          state = 246;                // --278-- ERROR 
        }
      } 
    break; 
    case 173:                     // --278-- CAS[          173--( isHex(c) )-->174 ]
      { 
        if  ( isHex(c) )        // --278-- CAS[          173--( isHex(c) )-->174 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 174; 
        }
                                        // --279--
        else if  ( c == '\n' )        // --279-- CAS[          173--( c == '\n' )-->245 ]
        {
           
          {
              servos = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalServoOutput(data);
              if ( debug & 1) {
                  printDebug_cdservo();
                  
              }
          }
           
          state = 245; 
        }
                                        // --280--
        else {
          state = 246;                // --280-- ERROR 
        }
      } 
    break; 
    case 174:                     // --280-- CAS[          174--( isHex(c) )-->175 ]
      { 
        if  ( isHex(c) )        // --280-- CAS[          174--( isHex(c) )-->175 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 175; 
        }
                                        // --281--
        else if  ( c == '\n' )        // --281-- CAS[          174--( c == '\n' )-->245 ]
        {
           
          {
              servos = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalServoOutput(data);
              if ( debug & 1) {
                  printDebug_cdservo();
                  
              }
          }
           
          state = 245; 
        }
                                        // --282--
        else {
          state = 246;                // --282-- ERROR 
        }
      } 
    break; 
    case 175:                     // --282-- CAS[          175--( isHex(c) )-->176 ]
      { 
        if  ( isHex(c) )        // --282-- CAS[          175--( isHex(c) )-->176 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 176; 
        }
                                        // --283--
        else if  ( c == '\n' )        // --283-- CAS[          175--( c == '\n' )-->245 ]
        {
           
          {
              servos = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalServoOutput(data);
              if ( debug & 1) {
                  printDebug_cdservo();
                  
              }
          }
           
          state = 245; 
        }
                                        // --284--
        else {
          state = 246;                // --284-- ERROR 
        }
      } 
    break; 
    case 176:                     // --284-- CAS[          176--( isHex(c) )-->177 ]
      { 
        if  ( isHex(c) )        // --284-- CAS[          176--( isHex(c) )-->177 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 177; 
        }
                                        // --285--
        else if  ( c == '\n' )        // --285-- CAS[          176--( c == '\n' )-->245 ]
        {
           
          {
              servos = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalServoOutput(data);
              if ( debug & 1) {
                  printDebug_cdservo();
                  
              }
          }
           
          state = 245; 
        }
                                        // --286--
        else {
          state = 246;                // --286-- ERROR 
        }
      } 
    break; 
    case 177:                     // --286-- CAS[          177--( c == '\n' )-->245 ]
      { 
        if  ( c == '\n' )        // --286-- CAS[          177--( c == '\n' )-->245 ]
        {
           
          {
              servos = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalServoOutput(data);
              if ( debug & 1) {
                  printDebug_cdservo();
                  
              }
          }
           
          state = 245; 
        }
                                        // --287--
        else {
          state = 246;                // --287-- ERROR 
        }
      } 
    break; 
    case 178:                     // --287-- CAS[          178--( c == 'a' )-->179 ]
      { 
        if  ( c == 'a' )        // --287-- CAS[          178--( c == 'a' )-->179 ]
        {
           
          state = 179; 
        }
                                        // --288--
        else if  ( c == 'd' )        // --288-- CAS[          178--( c == 'd' )-->191 ]
        {
           
          state = 191; 
        }
                                        // --289--
        else {
          state = 246;                // --289-- ERROR 
        }
      } 
    break; 
    case 179:                     // --289-- CAS[          179--( c == 'i' )-->180 ]
      { 
        if  ( c == 'i' )        // --289-- CAS[          179--( c == 'i' )-->180 ]
        {
           
          state = 180; 
        }
                                        // --290--
        else {
          state = 246;                // --290-- ERROR 
        }
      } 
    break; 
    case 180:                     // --290-- CAS[          180--( c == 'n' )-->181 ]
      { 
        if  ( c == 'n' )        // --290-- CAS[          180--( c == 'n' )-->181 ]
        {
           
          state = 181; 
        }
                                        // --291--
        else {
          state = 246;                // --291-- ERROR 
        }
      } 
    break; 
    case 181:                     // --291-- CAS[          181--( c == ':' )-->182 ]
      { 
        if  ( c == ':' )        // --291-- CAS[          181--( c == ':' )-->182 ]
        {
           
          state = 182; 
        }
                                        // --292--
        else {
          state = 246;                // --292-- ERROR 
        }
      } 
    break; 
    case 182:                     // --292-- CAS[          182--( isHex(c) )-->183 ]
      { 
        if  ( isHex(c) )        // --292-- CAS[          182--( isHex(c) )-->183 ]
        {
          data = valueHex(c); 
          state = 183; 
        }
                                        // --293--
        else {
          state = 246;                // --293-- ERROR 
        }
      } 
    break; 
    case 183:                     // --293-- CAS[          183--( isHex(c) )-->184 ]
      { 
        if  ( isHex(c) )        // --293-- CAS[          183--( isHex(c) )-->184 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 184; 
        }
                                        // --294--
        else if  ( c == '\n' )        // --294-- CAS[          183--( c == '\n' )-->245 ]
        {
          
          { 
              analogAnalogInputs = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_caain();
              }
          }
           
          state = 245; 
        }
                                        // --295--
        else {
          state = 246;                // --295-- ERROR 
        }
      } 
    break; 
    case 184:                     // --295-- CAS[          184--( isHex(c) )-->185 ]
      { 
        if  ( isHex(c) )        // --295-- CAS[          184--( isHex(c) )-->185 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 185; 
        }
                                        // --296--
        else if  ( c == '\n' )        // --296-- CAS[          184--( c == '\n' )-->245 ]
        {
          
          { 
              analogAnalogInputs = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_caain();
              }
          }
           
          state = 245; 
        }
                                        // --297--
        else {
          state = 246;                // --297-- ERROR 
        }
      } 
    break; 
    case 185:                     // --297-- CAS[          185--( isHex(c) )-->186 ]
      { 
        if  ( isHex(c) )        // --297-- CAS[          185--( isHex(c) )-->186 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 186; 
        }
                                        // --298--
        else if  ( c == '\n' )        // --298-- CAS[          185--( c == '\n' )-->245 ]
        {
          
          { 
              analogAnalogInputs = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_caain();
              }
          }
           
          state = 245; 
        }
                                        // --299--
        else {
          state = 246;                // --299-- ERROR 
        }
      } 
    break; 
    case 186:                     // --299-- CAS[          186--( isHex(c) )-->187 ]
      { 
        if  ( isHex(c) )        // --299-- CAS[          186--( isHex(c) )-->187 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 187; 
        }
                                        // --300--
        else if  ( c == '\n' )        // --300-- CAS[          186--( c == '\n' )-->245 ]
        {
          
          { 
              analogAnalogInputs = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_caain();
              }
          }
           
          state = 245; 
        }
                                        // --301--
        else {
          state = 246;                // --301-- ERROR 
        }
      } 
    break; 
    case 187:                     // --301-- CAS[          187--( isHex(c) )-->188 ]
      { 
        if  ( isHex(c) )        // --301-- CAS[          187--( isHex(c) )-->188 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 188; 
        }
                                        // --302--
        else if  ( c == '\n' )        // --302-- CAS[          187--( c == '\n' )-->245 ]
        {
          
          { 
              analogAnalogInputs = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_caain();
              }
          }
           
          state = 245; 
        }
                                        // --303--
        else {
          state = 246;                // --303-- ERROR 
        }
      } 
    break; 
    case 188:                     // --303-- CAS[          188--( isHex(c) )-->189 ]
      { 
        if  ( isHex(c) )        // --303-- CAS[          188--( isHex(c) )-->189 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 189; 
        }
                                        // --304--
        else if  ( c == '\n' )        // --304-- CAS[          188--( c == '\n' )-->245 ]
        {
          
          { 
              analogAnalogInputs = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_caain();
              }
          }
           
          state = 245; 
        }
                                        // --305--
        else {
          state = 246;                // --305-- ERROR 
        }
      } 
    break; 
    case 189:                     // --305-- CAS[          189--( isHex(c) )-->190 ]
      { 
        if  ( isHex(c) )        // --305-- CAS[          189--( isHex(c) )-->190 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 190; 
        }
                                        // --306--
        else if  ( c == '\n' )        // --306-- CAS[          189--( c == '\n' )-->245 ]
        {
          
          { 
              analogAnalogInputs = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_caain();
              }
          }
           
          state = 245; 
        }
                                        // --307--
        else {
          state = 246;                // --307-- ERROR 
        }
      } 
    break; 
    case 190:                     // --307-- CAS[          190--( c == '\n' )-->245 ]
      { 
        if  ( c == '\n' )        // --307-- CAS[          190--( c == '\n' )-->245 ]
        {
          
          { 
              analogAnalogInputs = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_caain();
              }
          }
           
          state = 245; 
        }
                                        // --308--
        else {
          state = 246;                // --308-- ERROR 
        }
      } 
    break; 
    case 191:                     // --308-- CAS[          191--( c == 'i' )-->192 ]
      { 
        if  ( c == 'i' )        // --308-- CAS[          191--( c == 'i' )-->192 ]
        {
           
          state = 192; 
        }
                                        // --309--
        else if  ( c == 'o' )        // --309-- CAS[          191--( c == 'o' )-->203 ]
        {
           
          state = 203; 
        }
                                        // --310--
        else {
          state = 246;                // --310-- ERROR 
        }
      } 
    break; 
    case 192:                     // --310-- CAS[          192--( c == 'n' )-->193 ]
      { 
        if  ( c == 'n' )        // --310-- CAS[          192--( c == 'n' )-->193 ]
        {
           
          state = 193; 
        }
                                        // --311--
        else {
          state = 246;                // --311-- ERROR 
        }
      } 
    break; 
    case 193:                     // --311-- CAS[          193--( c == ':' )-->194 ]
      { 
        if  ( c == ':' )        // --311-- CAS[          193--( c == ':' )-->194 ]
        {
           
          state = 194; 
        }
                                        // --312--
        else if  ( c == 'p' )        // --312-- CAS[          193--( c == 'p' )-->215 ]
        {
           
          state = 215; 
        }
                                        // --313--
        else {
          state = 246;                // --313-- ERROR 
        }
      } 
    break; 
    case 194:                     // --313-- CAS[          194--( isHex(c) )-->195 ]
      { 
        if  ( isHex(c) )        // --313-- CAS[          194--( isHex(c) )-->195 ]
        {
          data = valueHex(c); 
          state = 195; 
        }
                                        // --314--
        else {
          state = 246;                // --314-- ERROR 
        }
      } 
    break; 
    case 195:                     // --314-- CAS[          195--( isHex(c) )-->196 ]
      { 
        if  ( isHex(c) )        // --314-- CAS[          195--( isHex(c) )-->196 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 196; 
        }
                                        // --315--
        else if  ( c == '\n' )        // --315-- CAS[          195--( c == '\n' )-->245 ]
        {
          
          { 
              setAnalogDigitalInput(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cadin();
              }
          }
           
          state = 245; 
        }
                                        // --316--
        else {
          state = 246;                // --316-- ERROR 
        }
      } 
    break; 
    case 196:                     // --316-- CAS[          196--( isHex(c) )-->197 ]
      { 
        if  ( isHex(c) )        // --316-- CAS[          196--( isHex(c) )-->197 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 197; 
        }
                                        // --317--
        else if  ( c == '\n' )        // --317-- CAS[          196--( c == '\n' )-->245 ]
        {
          
          { 
              setAnalogDigitalInput(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cadin();
              }
          }
           
          state = 245; 
        }
                                        // --318--
        else {
          state = 246;                // --318-- ERROR 
        }
      } 
    break; 
    case 197:                     // --318-- CAS[          197--( isHex(c) )-->198 ]
      { 
        if  ( isHex(c) )        // --318-- CAS[          197--( isHex(c) )-->198 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 198; 
        }
                                        // --319--
        else if  ( c == '\n' )        // --319-- CAS[          197--( c == '\n' )-->245 ]
        {
          
          { 
              setAnalogDigitalInput(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cadin();
              }
          }
           
          state = 245; 
        }
                                        // --320--
        else {
          state = 246;                // --320-- ERROR 
        }
      } 
    break; 
    case 198:                     // --320-- CAS[          198--( isHex(c) )-->199 ]
      { 
        if  ( isHex(c) )        // --320-- CAS[          198--( isHex(c) )-->199 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 199; 
        }
                                        // --321--
        else if  ( c == '\n' )        // --321-- CAS[          198--( c == '\n' )-->245 ]
        {
          
          { 
              setAnalogDigitalInput(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cadin();
              }
          }
           
          state = 245; 
        }
                                        // --322--
        else {
          state = 246;                // --322-- ERROR 
        }
      } 
    break; 
    case 199:                     // --322-- CAS[          199--( isHex(c) )-->200 ]
      { 
        if  ( isHex(c) )        // --322-- CAS[          199--( isHex(c) )-->200 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 200; 
        }
                                        // --323--
        else if  ( c == '\n' )        // --323-- CAS[          199--( c == '\n' )-->245 ]
        {
          
          { 
              setAnalogDigitalInput(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cadin();
              }
          }
           
          state = 245; 
        }
                                        // --324--
        else {
          state = 246;                // --324-- ERROR 
        }
      } 
    break; 
    case 200:                     // --324-- CAS[          200--( isHex(c) )-->201 ]
      { 
        if  ( isHex(c) )        // --324-- CAS[          200--( isHex(c) )-->201 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 201; 
        }
                                        // --325--
        else if  ( c == '\n' )        // --325-- CAS[          200--( c == '\n' )-->245 ]
        {
          
          { 
              setAnalogDigitalInput(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cadin();
              }
          }
           
          state = 245; 
        }
                                        // --326--
        else {
          state = 246;                // --326-- ERROR 
        }
      } 
    break; 
    case 201:                     // --326-- CAS[          201--( isHex(c) )-->202 ]
      { 
        if  ( isHex(c) )        // --326-- CAS[          201--( isHex(c) )-->202 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 202; 
        }
                                        // --327--
        else if  ( c == '\n' )        // --327-- CAS[          201--( c == '\n' )-->245 ]
        {
          
          { 
              setAnalogDigitalInput(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cadin();
              }
          }
           
          state = 245; 
        }
                                        // --328--
        else {
          state = 246;                // --328-- ERROR 
        }
      } 
    break; 
    case 202:                     // --328-- CAS[          202--( c == '\n' )-->245 ]
      { 
        if  ( c == '\n' )        // --328-- CAS[          202--( c == '\n' )-->245 ]
        {
          
          { 
              setAnalogDigitalInput(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cadin();
              }
          }
           
          state = 245; 
        }
                                        // --329--
        else {
          state = 246;                // --329-- ERROR 
        }
      } 
    break; 
    case 203:                     // --329-- CAS[          203--( c == 'u' )-->204 ]
      { 
        if  ( c == 'u' )        // --329-- CAS[          203--( c == 'u' )-->204 ]
        {
           
          state = 204; 
        }
                                        // --330--
        else {
          state = 246;                // --330-- ERROR 
        }
      } 
    break; 
    case 204:                     // --330-- CAS[          204--( c == 't' )-->205 ]
      { 
        if  ( c == 't' )        // --330-- CAS[          204--( c == 't' )-->205 ]
        {
           
          state = 205; 
        }
                                        // --331--
        else {
          state = 246;                // --331-- ERROR 
        }
      } 
    break; 
    case 205:                     // --331-- CAS[          205--( c == ':' )-->206 ]
      { 
        if  ( c == ':' )        // --331-- CAS[          205--( c == ':' )-->206 ]
        {
           
          state = 206; 
        }
                                        // --332--
        else {
          state = 246;                // --332-- ERROR 
        }
      } 
    break; 
    case 206:                     // --332-- CAS[          206--( isHex(c) )-->207 ]
      { 
        if  ( isHex(c) )        // --332-- CAS[          206--( isHex(c) )-->207 ]
        {
          data = valueHex(c); 
          state = 207; 
        }
                                        // --333--
        else {
          state = 246;                // --333-- ERROR 
        }
      } 
    break; 
    case 207:                     // --333-- CAS[          207--( isHex(c) )-->208 ]
      { 
        if  ( isHex(c) )        // --333-- CAS[          207--( isHex(c) )-->208 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 208; 
        }
                                        // --334--
        else if  ( c == '\n' )        // --334-- CAS[          207--( c == '\n' )-->245 ]
        {
          
          { 
              setAnalogDigitalOutput(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cadout();
              }
          }
           
          state = 245; 
        }
                                        // --335--
        else {
          state = 246;                // --335-- ERROR 
        }
      } 
    break; 
    case 208:                     // --335-- CAS[          208--( isHex(c) )-->209 ]
      { 
        if  ( isHex(c) )        // --335-- CAS[          208--( isHex(c) )-->209 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 209; 
        }
                                        // --336--
        else if  ( c == '\n' )        // --336-- CAS[          208--( c == '\n' )-->245 ]
        {
          
          { 
              setAnalogDigitalOutput(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cadout();
              }
          }
           
          state = 245; 
        }
                                        // --337--
        else {
          state = 246;                // --337-- ERROR 
        }
      } 
    break; 
    case 209:                     // --337-- CAS[          209--( isHex(c) )-->210 ]
      { 
        if  ( isHex(c) )        // --337-- CAS[          209--( isHex(c) )-->210 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 210; 
        }
                                        // --338--
        else if  ( c == '\n' )        // --338-- CAS[          209--( c == '\n' )-->245 ]
        {
          
          { 
              setAnalogDigitalOutput(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cadout();
              }
          }
           
          state = 245; 
        }
                                        // --339--
        else {
          state = 246;                // --339-- ERROR 
        }
      } 
    break; 
    case 210:                     // --339-- CAS[          210--( isHex(c) )-->211 ]
      { 
        if  ( isHex(c) )        // --339-- CAS[          210--( isHex(c) )-->211 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 211; 
        }
                                        // --340--
        else if  ( c == '\n' )        // --340-- CAS[          210--( c == '\n' )-->245 ]
        {
          
          { 
              setAnalogDigitalOutput(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cadout();
              }
          }
           
          state = 245; 
        }
                                        // --341--
        else {
          state = 246;                // --341-- ERROR 
        }
      } 
    break; 
    case 211:                     // --341-- CAS[          211--( isHex(c) )-->212 ]
      { 
        if  ( isHex(c) )        // --341-- CAS[          211--( isHex(c) )-->212 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 212; 
        }
                                        // --342--
        else if  ( c == '\n' )        // --342-- CAS[          211--( c == '\n' )-->245 ]
        {
          
          { 
              setAnalogDigitalOutput(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cadout();
              }
          }
           
          state = 245; 
        }
                                        // --343--
        else {
          state = 246;                // --343-- ERROR 
        }
      } 
    break; 
    case 212:                     // --343-- CAS[          212--( isHex(c) )-->213 ]
      { 
        if  ( isHex(c) )        // --343-- CAS[          212--( isHex(c) )-->213 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 213; 
        }
                                        // --344--
        else if  ( c == '\n' )        // --344-- CAS[          212--( c == '\n' )-->245 ]
        {
          
          { 
              setAnalogDigitalOutput(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cadout();
              }
          }
           
          state = 245; 
        }
                                        // --345--
        else {
          state = 246;                // --345-- ERROR 
        }
      } 
    break; 
    case 213:                     // --345-- CAS[          213--( isHex(c) )-->214 ]
      { 
        if  ( isHex(c) )        // --345-- CAS[          213--( isHex(c) )-->214 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 214; 
        }
                                        // --346--
        else if  ( c == '\n' )        // --346-- CAS[          213--( c == '\n' )-->245 ]
        {
          
          { 
              setAnalogDigitalOutput(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cadout();
              }
          }
           
          state = 245; 
        }
                                        // --347--
        else {
          state = 246;                // --347-- ERROR 
        }
      } 
    break; 
    case 214:                     // --347-- CAS[          214--( c == '\n' )-->245 ]
      { 
        if  ( c == '\n' )        // --347-- CAS[          214--( c == '\n' )-->245 ]
        {
          
          { 
              setAnalogDigitalOutput(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cadout();
              }
          }
           
          state = 245; 
        }
                                        // --348--
        else {
          state = 246;                // --348-- ERROR 
        }
      } 
    break; 
    case 215:                     // --348-- CAS[          215--( c == ':' )-->216 ]
      { 
        if  ( c == ':' )        // --348-- CAS[          215--( c == ':' )-->216 ]
        {
           
          state = 216; 
        }
                                        // --349--
        else {
          state = 246;                // --349-- ERROR 
        }
      } 
    break; 
    case 216:                     // --349-- CAS[          216--( isHex(c) )-->217 ]
      { 
        if  ( isHex(c) )        // --349-- CAS[          216--( isHex(c) )-->217 ]
        {
          data = valueHex(c); 
          state = 217; 
        }
                                        // --350--
        else {
          state = 246;                // --350-- ERROR 
        }
      } 
    break; 
    case 217:                     // --350-- CAS[          217--( isHex(c) )-->218 ]
      { 
        if  ( isHex(c) )        // --350-- CAS[          217--( isHex(c) )-->218 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 218; 
        }
                                        // --351--
        else if  ( c == '\n' )        // --351-- CAS[          217--( c == '\n' )-->245 ]
        {
          
          { 
              setAnalogDigitalInputPullup(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cadinp();
              }
          }
           
          state = 245; 
        }
                                        // --352--
        else {
          state = 246;                // --352-- ERROR 
        }
      } 
    break; 
    case 218:                     // --352-- CAS[          218--( isHex(c) )-->219 ]
      { 
        if  ( isHex(c) )        // --352-- CAS[          218--( isHex(c) )-->219 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 219; 
        }
                                        // --353--
        else if  ( c == '\n' )        // --353-- CAS[          218--( c == '\n' )-->245 ]
        {
          
          { 
              setAnalogDigitalInputPullup(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cadinp();
              }
          }
           
          state = 245; 
        }
                                        // --354--
        else {
          state = 246;                // --354-- ERROR 
        }
      } 
    break; 
    case 219:                     // --354-- CAS[          219--( isHex(c) )-->220 ]
      { 
        if  ( isHex(c) )        // --354-- CAS[          219--( isHex(c) )-->220 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 220; 
        }
                                        // --355--
        else if  ( c == '\n' )        // --355-- CAS[          219--( c == '\n' )-->245 ]
        {
          
          { 
              setAnalogDigitalInputPullup(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cadinp();
              }
          }
           
          state = 245; 
        }
                                        // --356--
        else {
          state = 246;                // --356-- ERROR 
        }
      } 
    break; 
    case 220:                     // --356-- CAS[          220--( isHex(c) )-->221 ]
      { 
        if  ( isHex(c) )        // --356-- CAS[          220--( isHex(c) )-->221 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 221; 
        }
                                        // --357--
        else if  ( c == '\n' )        // --357-- CAS[          220--( c == '\n' )-->245 ]
        {
          
          { 
              setAnalogDigitalInputPullup(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cadinp();
              }
          }
           
          state = 245; 
        }
                                        // --358--
        else {
          state = 246;                // --358-- ERROR 
        }
      } 
    break; 
    case 221:                     // --358-- CAS[          221--( isHex(c) )-->222 ]
      { 
        if  ( isHex(c) )        // --358-- CAS[          221--( isHex(c) )-->222 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 222; 
        }
                                        // --359--
        else if  ( c == '\n' )        // --359-- CAS[          221--( c == '\n' )-->245 ]
        {
          
          { 
              setAnalogDigitalInputPullup(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cadinp();
              }
          }
           
          state = 245; 
        }
                                        // --360--
        else {
          state = 246;                // --360-- ERROR 
        }
      } 
    break; 
    case 222:                     // --360-- CAS[          222--( isHex(c) )-->223 ]
      { 
        if  ( isHex(c) )        // --360-- CAS[          222--( isHex(c) )-->223 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 223; 
        }
                                        // --361--
        else if  ( c == '\n' )        // --361-- CAS[          222--( c == '\n' )-->245 ]
        {
          
          { 
              setAnalogDigitalInputPullup(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cadinp();
              }
          }
           
          state = 245; 
        }
                                        // --362--
        else {
          state = 246;                // --362-- ERROR 
        }
      } 
    break; 
    case 223:                     // --362-- CAS[          223--( isHex(c) )-->224 ]
      { 
        if  ( isHex(c) )        // --362-- CAS[          223--( isHex(c) )-->224 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 224; 
        }
                                        // --363--
        else if  ( c == '\n' )        // --363-- CAS[          223--( c == '\n' )-->245 ]
        {
          
          { 
              setAnalogDigitalInputPullup(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cadinp();
              }
          }
           
          state = 245; 
        }
                                        // --364--
        else {
          state = 246;                // --364-- ERROR 
        }
      } 
    break; 
    case 224:                     // --364-- CAS[          224--( c == '\n' )-->245 ]
      { 
        if  ( c == '\n' )        // --364-- CAS[          224--( c == '\n' )-->245 ]
        {
          
          { 
              setAnalogDigitalInputPullup(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cadinp();
              }
          }
           
          state = 245; 
        }
                                        // --365--
        else {
          state = 246;                // --365-- ERROR 
        }
      } 
    break; 
    case 225:                     // --365-- CAS[          225--( c == 'e' )-->226 ]
      { 
        if  ( c == 'e' )        // --365-- CAS[          225--( c == 'e' )-->226 ]
        {
           
          state = 226; 
        }
                                        // --366--
        else {
          state = 246;                // --366-- ERROR 
        }
      } 
    break; 
    case 226:                     // --366-- CAS[          226--( c == 'r' )-->227 ]
      { 
        if  ( c == 'r' )        // --366-- CAS[          226--( c == 'r' )-->227 ]
        {
           
          state = 227; 
        }
                                        // --367--
        else {
          state = 246;                // --367-- ERROR 
        }
      } 
    break; 
    case 227:                     // --367-- CAS[          227--( c == 's' )-->228 ]
      { 
        if  ( c == 's' )        // --367-- CAS[          227--( c == 's' )-->228 ]
        {
           
          state = 228; 
        }
                                        // --368--
        else {
          state = 246;                // --368-- ERROR 
        }
      } 
    break; 
    case 228:                     // --368-- CAS[          228--( c == 'i' )-->229 ]
      { 
        if  ( c == 'i' )        // --368-- CAS[          228--( c == 'i' )-->229 ]
        {
           
          state = 229; 
        }
                                        // --369--
        else {
          state = 246;                // --369-- ERROR 
        }
      } 
    break; 
    case 229:                     // --369-- CAS[          229--( c == 'o' )-->230 ]
      { 
        if  ( c == 'o' )        // --369-- CAS[          229--( c == 'o' )-->230 ]
        {
           
          state = 230; 
        }
                                        // --370--
        else {
          state = 246;                // --370-- ERROR 
        }
      } 
    break; 
    case 230:                     // --370-- CAS[          230--( c == 'n' )-->231 ]
      { 
        if  ( c == 'n' )        // --370-- CAS[          230--( c == 'n' )-->231 ]
        {
           
          state = 231; 
        }
                                        // --371--
        else {
          state = 246;                // --371-- ERROR 
        }
      } 
    break; 
    case 231:                     // --371-- CAS[          231--( c == ':' )-->232 ]
      { 
        if  ( c == ':' )        // --371-- CAS[          231--( c == ':' )-->232 ]
        {
           
          state = 232; 
        }
                                        // --372--
        else {
          state = 246;                // --372-- ERROR 
        }
      } 
    break; 
    case 232:                     // --372-- CAS[          232--( c == '\n' )-->245 ]
      { 
        if  ( c == '\n' )        // --372-- CAS[          232--( c == '\n' )-->245 ]
        {
          
          {
              Serial.print("v:");
              Serial.println(version);
          }
           
          state = 245; 
        }
                                        // --373--
        else {
          state = 246;                // --373-- ERROR 
        }
      } 
    break; 
    case 233:                     // --373-- CAS[          233--( c == 'e' )-->234 ]
      { 
        if  ( c == 'e' )        // --373-- CAS[          233--( c == 'e' )-->234 ]
        {
           
          state = 234; 
        }
                                        // --374--
        else {
          state = 246;                // --374-- ERROR 
        }
      } 
    break; 
    case 234:                     // --374-- CAS[          234--( c == 'r' )-->235 ]
      { 
        if  ( c == 'r' )        // --374-- CAS[          234--( c == 'r' )-->235 ]
        {
           
          state = 235; 
        }
                                        // --375--
        else {
          state = 246;                // --375-- ERROR 
        }
      } 
    break; 
    case 235:                     // --375-- CAS[          235--( c == 's' )-->236 ]
      { 
        if  ( c == 's' )        // --375-- CAS[          235--( c == 's' )-->236 ]
        {
           
          state = 236; 
        }
                                        // --376--
        else {
          state = 246;                // --376-- ERROR 
        }
      } 
    break; 
    case 236:                     // --376-- CAS[          236--( c == 'i' )-->237 ]
      { 
        if  ( c == 'i' )        // --376-- CAS[          236--( c == 'i' )-->237 ]
        {
           
          state = 237; 
        }
                                        // --377--
        else {
          state = 246;                // --377-- ERROR 
        }
      } 
    break; 
    case 237:                     // --377-- CAS[          237--( c == 'o' )-->238 ]
      { 
        if  ( c == 'o' )        // --377-- CAS[          237--( c == 'o' )-->238 ]
        {
           
          state = 238; 
        }
                                        // --378--
        else {
          state = 246;                // --378-- ERROR 
        }
      } 
    break; 
    case 238:                     // --378-- CAS[          238--( c == 'n' )-->239 ]
      { 
        if  ( c == 'n' )        // --378-- CAS[          238--( c == 'n' )-->239 ]
        {
           
          state = 239; 
        }
                                        // --379--
        else {
          state = 246;                // --379-- ERROR 
        }
      } 
    break; 
    case 239:                     // --379-- CAS[          239--( c == '?' )-->240 ]
      { 
        if  ( c == '?' )        // --379-- CAS[          239--( c == '?' )-->240 ]
        {
           
          state = 240; 
        }
                                        // --380--
        else {
          state = 246;                // --380-- ERROR 
        }
      } 
    break; 
    case 240:                     // --380-- CAS[          240--( c == '\n' )-->245 ]
      { 
        if  ( c == '\n' )        // --380-- CAS[          240--( c == '\n' )-->245 ]
        {
          
          {
              Serial.print("v:");
              Serial.println(version);
          }
           
          state = 245; 
        }
                                        // --381--
        else {
          state = 246;                // --381-- ERROR 
        }
      } 
    break; 
    case 241:                     // --381-- CAS[          241--( c == 'r' )-->242 ]
      { 
        if  ( c == 'r' )        // --381-- CAS[          241--( c == 'r' )-->242 ]
        {
           
          state = 242; 
        }
                                        // --382--
        else {
          state = 246;                // --382-- ERROR 
        }
      } 
    break; 
    case 242:                     // --382-- CAS[          242--( c == 'r' )-->243 ]
      { 
        if  ( c == 'r' )        // --382-- CAS[          242--( c == 'r' )-->243 ]
        {
           
          state = 243; 
        }
                                        // --383--
        else {
          state = 246;                // --383-- ERROR 
        }
      } 
    break; 
    case 243:                     // --383-- CAS[          243--( c == '?' )-->244 ]
      { 
        if  ( c == '?' )        // --383-- CAS[          243--( c == '?' )-->244 ]
        {
           
          state = 244; 
        }
                                        // --384--
        else {
          state = 246;                // --384-- ERROR 
        }
      } 
    break; 
    case 244:                     // --384-- CAS[          244--( c == '\n' )-->245 ]
      { 
        if  ( c == '\n' )        // --384-- CAS[          244--( c == '\n' )-->245 ]
        {
          
          {
              Serial.print("e:");
              Serial.println(errorCount);
          }
           
          state = 245; 
        }
        else {
          state = 246; // ERROR 
        }
      } 
    break; 
} // end switch state
  if ( state == 245) 
    {
      state = 0;
    }
    if ( state == 246) 
    {
      errorCount ++;
      state = 0;
    }
    //--END

  } // is Serial.available()

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

