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

    version 2017-02-12 added disconnect command
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

char version[] = "arduinoUno, version 2017-02-12";


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
#define STATEMACHINE_EVENT_DISCONNECT 30

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
void statemachine_2000_exit() {
  timeoutEnable = FALSE;
  blinkModeFast();
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
    if ( event == STATEMACHINE_EVENT_DISCONNECT) {
      Serial.println( F("STATEMACHINE_EVENT_DISCONNECT") );
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

    // Configuration data are available
    case 2000:
      switch (event) {
        case STATEMACHINE_EVENT_DISCONNECT:
          STATEMACHINE_waitState = 1000;
          statemachine_2000_exit();
          statemachine_1000_entry();
          break;
      }
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
  " cdebug:<data>        debug settings, data are hex (0,1,2,3)\n"
  " cr:                  dummy request, just get a newline and clean buffer\n"
  " cversion?            request version string\n"
  " cerr?                request error count for parser\n"
  " cident?              request idcode\n"
  " cident:<char16>      write idcode\n"
  "\n"
  "char16 = [A-Za-z][A-Za-z0-9-_.]{1,15} \n"
  "\n"

  " cdin:<data>          digital inputs, data are hex\n"
  " cdinp:<data>         digital inputs, pullup enabled, data are hex\n"
  " cdout:<data>         digital outputs, data are hex\n"
  " cdpwm:<data>         digital pwm, data are hex\n"
  " cdservo:<data>       digital servo, data are hex\n"

  " caain:<data>         analog line, data are hex [a0..a5]\n"
  " cadin:<data>         analog line, digital input [a0..a5]\n"
  " cadinp:<data>        analog line, digital input, pullup [a0..a5]\n"
  " cadout:<data>        analog line, digital output\n"
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
  " v:<version>          arduino reports version\n"
  " ident:<char16>       arduino reports ident from EEPROM\n"
  " e:<errors>           arduino reports number of errors (decimal)\n"
  " a<port>,<value>      arduino reports analog input\n"
  " i<port>,<value>      arduino reports digital input\n"
  " ai<port>,<value>     arduino reports digital input on analog line\n"
  "\n"
  "Disconnect\n"
  " disconnect           stop processing, start asking for config\n"
  "\n";

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
void printDebug_cdin() {
  Serial.print( F("cdin=") );
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
  // generated code 2017-02-12 17:41:37
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
        else if  ( c == 'd' )        // --5-- CAS[          0--( c == 'd' )-->225 ]
        {
           
          state = 225; 
        }
                                        // --6--
        else if  ( c == 'v' )        // --6-- CAS[          0--( c == 'v' )-->235 ]
        {
           
          state = 235; 
        }
                                        // --7--
        else {
          state = 256;                // --7-- ERROR 
        }
      } 
    break; 
    case 1:                     // --7-- CAS[          1--( c == ':' )-->2 ]
      { 
        if  ( c == ':' )        // --7-- CAS[          1--( c == ':' )-->2 ]
        {
           
          state = 2; 
        }
                                        // --8--
        else if  ( isDigit(c) )        // --8-- CAS[          1--( isDigit(c) )-->9 ]
        {
          port = valueDecimal(c); 
          state = 9; 
        }
                                        // --9--
        else if  ( c == 'a' )        // --9-- CAS[          1--( c == 'a' )-->15 ]
        {
           
          state = 15; 
        }
                                        // --10--
        else {
          state = 256;                // --10-- ERROR 
        }
      } 
    break; 
    case 2:                     // --10-- CAS[          2--( isDigit(c) )-->3 ]
      { 
        if  ( isDigit(c) )        // --10-- CAS[          2--( isDigit(c) )-->3 ]
        {
          port = valueDecimal(c); 
          state = 3; 
        }
                                        // --11--
        else {
          state = 256;                // --11-- ERROR 
        }
      } 
    break; 
    case 3:                     // --11-- CAS[          3--( isDigit(c) )-->4 ]
      { 
        if  ( isDigit(c) )        // --11-- CAS[          3--( isDigit(c) )-->4 ]
        {
          port = port * 10 + valueDecimal(c); 
          state = 4; 
        }
                                        // --12--
        else if  ( c == ',' )        // --12-- CAS[          3--( c == ',' )-->5 ]
        {
           
          state = 5; 
        }
                                        // --13--
        else {
          state = 256;                // --13-- ERROR 
        }
      } 
    break; 
    case 4:                     // --13-- CAS[          4--( c == ',' )-->5 ]
      { 
        if  ( c == ',' )        // --13-- CAS[          4--( c == ',' )-->5 ]
        {
           
          state = 5; 
        }
                                        // --14--
        else {
          state = 256;                // --14-- ERROR 
        }
      } 
    break; 
    case 5:                     // --14-- CAS[          5--( isDigit(c) )-->6 ]
      { 
        if  ( isDigit(c) )        // --14-- CAS[          5--( isDigit(c) )-->6 ]
        {
          value = valueDecimal(c); 
          state = 6; 
        }
                                        // --15--
        else {
          state = 256;                // --15-- ERROR 
        }
      } 
    break; 
    case 6:                     // --15-- CAS[          6--( isDigit(c) )-->7 ]
      { 
        if  ( isDigit(c) )        // --15-- CAS[          6--( isDigit(c) )-->7 ]
        {
          value = value * 10 + valueDecimal(c); 
          state = 7; 
        }
                                        // --16--
        else if  ( c == '\n' )        // --16-- CAS[          6--( c == '\n' )-->255 ]
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
               
          state = 255; 
        }
                                        // --17--
        else {
          state = 256;                // --17-- ERROR 
        }
      } 
    break; 
    case 7:                     // --17-- CAS[          7--( isDigit(c) )-->8 ]
      { 
        if  ( isDigit(c) )        // --17-- CAS[          7--( isDigit(c) )-->8 ]
        {
          value = value * 10 + valueDecimal(c); 
          state = 8; 
        }
                                        // --18--
        else if  ( c == '\n' )        // --18-- CAS[          7--( c == '\n' )-->255 ]
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
               
          state = 255; 
        }
                                        // --19--
        else {
          state = 256;                // --19-- ERROR 
        }
      } 
    break; 
    case 8:                     // --19-- CAS[          8--( c == '\n' )-->255 ]
      { 
        if  ( c == '\n' )        // --19-- CAS[          8--( c == '\n' )-->255 ]
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
               
          state = 255; 
        }
                                        // --20--
        else {
          state = 256;                // --20-- ERROR 
        }
      } 
    break; 
    case 9:                     // --20-- CAS[          9--( isDigit(c) )-->10 ]
      { 
        if  ( isDigit(c) )        // --20-- CAS[          9--( isDigit(c) )-->10 ]
        {
          port = port * 10 + valueDecimal(c); 
          state = 10; 
        }
                                        // --21--
        else if  ( c == ',' )        // --21-- CAS[          9--( c == ',' )-->11 ]
        {
           
          state = 11; 
        }
                                        // --22--
        else {
          state = 256;                // --22-- ERROR 
        }
      } 
    break; 
    case 10:                     // --22-- CAS[          10--( c == ',' )-->11 ]
      { 
        if  ( c == ',' )        // --22-- CAS[          10--( c == ',' )-->11 ]
        {
           
          state = 11; 
        }
                                        // --23--
        else {
          state = 256;                // --23-- ERROR 
        }
      } 
    break; 
    case 11:                     // --23-- CAS[          11--( isDigit(c) )-->12 ]
      { 
        if  ( isDigit(c) )        // --23-- CAS[          11--( isDigit(c) )-->12 ]
        {
          value = valueDecimal(c); 
          state = 12; 
        }
                                        // --24--
        else {
          state = 256;                // --24-- ERROR 
        }
      } 
    break; 
    case 12:                     // --24-- CAS[          12--( isDigit(c) )-->13 ]
      { 
        if  ( isDigit(c) )        // --24-- CAS[          12--( isDigit(c) )-->13 ]
        {
          value = value * 10 + valueDecimal(c); 
          state = 13; 
        }
                                        // --25--
        else if  ( c == '\n' )        // --25-- CAS[          12--( c == '\n' )-->255 ]
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
           
          state = 255; 
        }
                                        // --26--
        else {
          state = 256;                // --26-- ERROR 
        }
      } 
    break; 
    case 13:                     // --26-- CAS[          13--( isDigit(c) )-->14 ]
      { 
        if  ( isDigit(c) )        // --26-- CAS[          13--( isDigit(c) )-->14 ]
        {
          value = value * 10 + valueDecimal(c); 
          state = 14; 
        }
                                        // --27--
        else if  ( c == '\n' )        // --27-- CAS[          13--( c == '\n' )-->255 ]
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
           
          state = 255; 
        }
                                        // --28--
        else {
          state = 256;                // --28-- ERROR 
        }
      } 
    break; 
    case 14:                     // --28-- CAS[          14--( c == '\n' )-->255 ]
      { 
        if  ( c == '\n' )        // --28-- CAS[          14--( c == '\n' )-->255 ]
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
           
          state = 255; 
        }
                                        // --29--
        else {
          state = 256;                // --29-- ERROR 
        }
      } 
    break; 
    case 15:                     // --29-- CAS[          15--( c == ':' )-->16 ]
      { 
        if  ( c == ':' )        // --29-- CAS[          15--( c == ':' )-->16 ]
        {
           
          state = 16; 
        }
                                        // --30--
        else if  ( isDigit(c) )        // --30-- CAS[          15--( isDigit(c) )-->23 ]
        {
          port = valueDecimal(c); 
          state = 23; 
        }
                                        // --31--
        else {
          state = 256;                // --31-- ERROR 
        }
      } 
    break; 
    case 16:                     // --31-- CAS[          16--( isDigit(c) )-->17 ]
      { 
        if  ( isDigit(c) )        // --31-- CAS[          16--( isDigit(c) )-->17 ]
        {
          port = valueDecimal(c); 
          state = 17; 
        }
                                        // --32--
        else {
          state = 256;                // --32-- ERROR 
        }
      } 
    break; 
    case 17:                     // --32-- CAS[          17--( isDigit(c) )-->18 ]
      { 
        if  ( isDigit(c) )        // --32-- CAS[          17--( isDigit(c) )-->18 ]
        {
          port = port * 10 + valueDecimal(c); 
          state = 18; 
        }
                                        // --33--
        else if  ( c == ',' )        // --33-- CAS[          17--( c == ',' )-->19 ]
        {
           
          state = 19; 
        }
                                        // --34--
        else {
          state = 256;                // --34-- ERROR 
        }
      } 
    break; 
    case 18:                     // --34-- CAS[          18--( c == ',' )-->19 ]
      { 
        if  ( c == ',' )        // --34-- CAS[          18--( c == ',' )-->19 ]
        {
           
          state = 19; 
        }
                                        // --35--
        else {
          state = 256;                // --35-- ERROR 
        }
      } 
    break; 
    case 19:                     // --35-- CAS[          19--( isDigit(c) )-->20 ]
      { 
        if  ( isDigit(c) )        // --35-- CAS[          19--( isDigit(c) )-->20 ]
        {
          value = valueDecimal(c); 
          state = 20; 
        }
                                        // --36--
        else {
          state = 256;                // --36-- ERROR 
        }
      } 
    break; 
    case 20:                     // --36-- CAS[          20--( isDigit(c) )-->21 ]
      { 
        if  ( isDigit(c) )        // --36-- CAS[          20--( isDigit(c) )-->21 ]
        {
          value = value * 10 + valueDecimal(c); 
          state = 21; 
        }
                                        // --37--
        else if  ( c == '\n' )        // --37-- CAS[          20--( c == '\n' )-->255 ]
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
               
          state = 255; 
        }
                                        // --38--
        else {
          state = 256;                // --38-- ERROR 
        }
      } 
    break; 
    case 21:                     // --38-- CAS[          21--( isDigit(c) )-->22 ]
      { 
        if  ( isDigit(c) )        // --38-- CAS[          21--( isDigit(c) )-->22 ]
        {
          value = value * 10 + valueDecimal(c); 
          state = 22; 
        }
                                        // --39--
        else if  ( c == '\n' )        // --39-- CAS[          21--( c == '\n' )-->255 ]
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
               
          state = 255; 
        }
                                        // --40--
        else {
          state = 256;                // --40-- ERROR 
        }
      } 
    break; 
    case 22:                     // --40-- CAS[          22--( c == '\n' )-->255 ]
      { 
        if  ( c == '\n' )        // --40-- CAS[          22--( c == '\n' )-->255 ]
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
               
          state = 255; 
        }
                                        // --41--
        else {
          state = 256;                // --41-- ERROR 
        }
      } 
    break; 
    case 23:                     // --41-- CAS[          23--( isDigit(c) )-->24 ]
      { 
        if  ( isDigit(c) )        // --41-- CAS[          23--( isDigit(c) )-->24 ]
        {
          port = port * 10 + valueDecimal(c); 
          state = 24; 
        }
                                        // --42--
        else if  ( c == ',' )        // --42-- CAS[          23--( c == ',' )-->25 ]
        {
           
          state = 25; 
        }
                                        // --43--
        else {
          state = 256;                // --43-- ERROR 
        }
      } 
    break; 
    case 24:                     // --43-- CAS[          24--( c == ',' )-->25 ]
      { 
        if  ( c == ',' )        // --43-- CAS[          24--( c == ',' )-->25 ]
        {
           
          state = 25; 
        }
                                        // --44--
        else {
          state = 256;                // --44-- ERROR 
        }
      } 
    break; 
    case 25:                     // --44-- CAS[          25--( isDigit(c) )-->26 ]
      { 
        if  ( isDigit(c) )        // --44-- CAS[          25--( isDigit(c) )-->26 ]
        {
          value = valueDecimal(c); 
          state = 26; 
        }
                                        // --45--
        else {
          state = 256;                // --45-- ERROR 
        }
      } 
    break; 
    case 26:                     // --45-- CAS[          26--( isDigit(c) )-->27 ]
      { 
        if  ( isDigit(c) )        // --45-- CAS[          26--( isDigit(c) )-->27 ]
        {
          value = value * 10 + valueDecimal(c); 
          state = 27; 
        }
                                        // --46--
        else if  ( c == '\n' )        // --46-- CAS[          26--( c == '\n' )-->255 ]
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
           
          state = 255; 
        }
                                        // --47--
        else {
          state = 256;                // --47-- ERROR 
        }
      } 
    break; 
    case 27:                     // --47-- CAS[          27--( isDigit(c) )-->28 ]
      { 
        if  ( isDigit(c) )        // --47-- CAS[          27--( isDigit(c) )-->28 ]
        {
          value = value * 10 + valueDecimal(c); 
          state = 28; 
        }
                                        // --48--
        else if  ( c == '\n' )        // --48-- CAS[          27--( c == '\n' )-->255 ]
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
           
          state = 255; 
        }
                                        // --49--
        else {
          state = 256;                // --49-- ERROR 
        }
      } 
    break; 
    case 28:                     // --49-- CAS[          28--( c == '\n' )-->255 ]
      { 
        if  ( c == '\n' )        // --49-- CAS[          28--( c == '\n' )-->255 ]
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
           
          state = 255; 
        }
                                        // --50--
        else {
          state = 256;                // --50-- ERROR 
        }
      } 
    break; 
    case 29:                     // --50-- CAS[          29--( c == 'w' )-->30 ]
      { 
        if  ( c == 'w' )        // --50-- CAS[          29--( c == 'w' )-->30 ]
        {
           
          state = 30; 
        }
                                        // --51--
        else if  ( c == ':' )        // --51-- CAS[          29--( c == ':' )-->39 ]
        {
           
          state = 39; 
        }
                                        // --52--
        else if  ( isDigit(c) )        // --52-- CAS[          29--( isDigit(c) )-->46 ]
        {
          port = valueDecimal(c); 
          state = 46; 
        }
                                        // --53--
        else {
          state = 256;                // --53-- ERROR 
        }
      } 
    break; 
    case 30:                     // --53-- CAS[          30--( c == 'm' )-->31 ]
      { 
        if  ( c == 'm' )        // --53-- CAS[          30--( c == 'm' )-->31 ]
        {
           
          state = 31; 
        }
                                        // --54--
        else {
          state = 256;                // --54-- ERROR 
        }
      } 
    break; 
    case 31:                     // --54-- CAS[          31--( c == ':' )-->32 ]
      { 
        if  ( c == ':' )        // --54-- CAS[          31--( c == ':' )-->32 ]
        {
           
          state = 32; 
        }
                                        // --55--
        else {
          state = 256;                // --55-- ERROR 
        }
      } 
    break; 
    case 32:                     // --55-- CAS[          32--( isDigit(c) )-->33 ]
      { 
        if  ( isDigit(c) )        // --55-- CAS[          32--( isDigit(c) )-->33 ]
        {
          port = valueDecimal(c); 
          state = 33; 
        }
                                        // --56--
        else {
          state = 256;                // --56-- ERROR 
        }
      } 
    break; 
    case 33:                     // --56-- CAS[          33--( isDigit(c) )-->34 ]
      { 
        if  ( isDigit(c) )        // --56-- CAS[          33--( isDigit(c) )-->34 ]
        {
          port = port * 10 + valueDecimal(c); 
          state = 34; 
        }
                                        // --57--
        else if  ( c == ',' )        // --57-- CAS[          33--( c == ',' )-->35 ]
        {
           
          state = 35; 
        }
                                        // --58--
        else {
          state = 256;                // --58-- ERROR 
        }
      } 
    break; 
    case 34:                     // --58-- CAS[          34--( c == ',' )-->35 ]
      { 
        if  ( c == ',' )        // --58-- CAS[          34--( c == ',' )-->35 ]
        {
           
          state = 35; 
        }
                                        // --59--
        else {
          state = 256;                // --59-- ERROR 
        }
      } 
    break; 
    case 35:                     // --59-- CAS[          35--( isDigit(c) )-->36 ]
      { 
        if  ( isDigit(c) )        // --59-- CAS[          35--( isDigit(c) )-->36 ]
        {
          value = valueDecimal(c); 
          state = 36; 
        }
                                        // --60--
        else {
          state = 256;                // --60-- ERROR 
        }
      } 
    break; 
    case 36:                     // --60-- CAS[          36--( isDigit(c) )-->37 ]
      { 
        if  ( isDigit(c) )        // --60-- CAS[          36--( isDigit(c) )-->37 ]
        {
          value = value * 10 + valueDecimal(c); 
          state = 37; 
        }
                                        // --61--
        else if  ( c == '\n' )        // --61-- CAS[          36--( c == '\n' )-->255 ]
        {
          
              {
                  if (debug & 1){
                      printDebug_p_port_value();
                  }
                  if ( pwms & (1 << port)){
                      analogWrite(port, value);
                  }
              }
               
          state = 255; 
        }
                                        // --62--
        else {
          state = 256;                // --62-- ERROR 
        }
      } 
    break; 
    case 37:                     // --62-- CAS[          37--( isDigit(c) )-->38 ]
      { 
        if  ( isDigit(c) )        // --62-- CAS[          37--( isDigit(c) )-->38 ]
        {
          value = value * 10 + valueDecimal(c); 
          state = 38; 
        }
                                        // --63--
        else if  ( c == '\n' )        // --63-- CAS[          37--( c == '\n' )-->255 ]
        {
          
              {
                  if (debug & 1){
                      printDebug_p_port_value();
                  }
                  if ( pwms & (1 << port)){
                      analogWrite(port, value);
                  }
              }
               
          state = 255; 
        }
                                        // --64--
        else {
          state = 256;                // --64-- ERROR 
        }
      } 
    break; 
    case 38:                     // --64-- CAS[          38--( c == '\n' )-->255 ]
      { 
        if  ( c == '\n' )        // --64-- CAS[          38--( c == '\n' )-->255 ]
        {
          
              {
                  if (debug & 1){
                      printDebug_p_port_value();
                  }
                  if ( pwms & (1 << port)){
                      analogWrite(port, value);
                  }
              }
               
          state = 255; 
        }
                                        // --65--
        else {
          state = 256;                // --65-- ERROR 
        }
      } 
    break; 
    case 39:                     // --65-- CAS[          39--( isDigit(c) )-->40 ]
      { 
        if  ( isDigit(c) )        // --65-- CAS[          39--( isDigit(c) )-->40 ]
        {
          port = valueDecimal(c); 
          state = 40; 
        }
                                        // --66--
        else {
          state = 256;                // --66-- ERROR 
        }
      } 
    break; 
    case 40:                     // --66-- CAS[          40--( isDigit(c) )-->41 ]
      { 
        if  ( isDigit(c) )        // --66-- CAS[          40--( isDigit(c) )-->41 ]
        {
          port = port * 10 + valueDecimal(c); 
          state = 41; 
        }
                                        // --67--
        else if  ( c == ',' )        // --67-- CAS[          40--( c == ',' )-->42 ]
        {
           
          state = 42; 
        }
                                        // --68--
        else {
          state = 256;                // --68-- ERROR 
        }
      } 
    break; 
    case 41:                     // --68-- CAS[          41--( c == ',' )-->42 ]
      { 
        if  ( c == ',' )        // --68-- CAS[          41--( c == ',' )-->42 ]
        {
           
          state = 42; 
        }
                                        // --69--
        else {
          state = 256;                // --69-- ERROR 
        }
      } 
    break; 
    case 42:                     // --69-- CAS[          42--( isDigit(c) )-->43 ]
      { 
        if  ( isDigit(c) )        // --69-- CAS[          42--( isDigit(c) )-->43 ]
        {
          value = valueDecimal(c); 
          state = 43; 
        }
                                        // --70--
        else {
          state = 256;                // --70-- ERROR 
        }
      } 
    break; 
    case 43:                     // --70-- CAS[          43--( isDigit(c) )-->44 ]
      { 
        if  ( isDigit(c) )        // --70-- CAS[          43--( isDigit(c) )-->44 ]
        {
          value = value * 10 + valueDecimal(c); 
          state = 44; 
        }
                                        // --71--
        else if  ( c == '\n' )        // --71-- CAS[          43--( c == '\n' )-->255 ]
        {
          
              {
                  if (debug & 1){
                      printDebug_p_port_value();
                  }
                  if ( pwms & (1 << port)){
                      analogWrite(port, value);
                  }
              }
               
          state = 255; 
        }
                                        // --72--
        else {
          state = 256;                // --72-- ERROR 
        }
      } 
    break; 
    case 44:                     // --72-- CAS[          44--( isDigit(c) )-->45 ]
      { 
        if  ( isDigit(c) )        // --72-- CAS[          44--( isDigit(c) )-->45 ]
        {
          value = value * 10 + valueDecimal(c); 
          state = 45; 
        }
                                        // --73--
        else if  ( c == '\n' )        // --73-- CAS[          44--( c == '\n' )-->255 ]
        {
          
              {
                  if (debug & 1){
                      printDebug_p_port_value();
                  }
                  if ( pwms & (1 << port)){
                      analogWrite(port, value);
                  }
              }
               
          state = 255; 
        }
                                        // --74--
        else {
          state = 256;                // --74-- ERROR 
        }
      } 
    break; 
    case 45:                     // --74-- CAS[          45--( c == '\n' )-->255 ]
      { 
        if  ( c == '\n' )        // --74-- CAS[          45--( c == '\n' )-->255 ]
        {
          
              {
                  if (debug & 1){
                      printDebug_p_port_value();
                  }
                  if ( pwms & (1 << port)){
                      analogWrite(port, value);
                  }
              }
               
          state = 255; 
        }
                                        // --75--
        else {
          state = 256;                // --75-- ERROR 
        }
      } 
    break; 
    case 46:                     // --75-- CAS[          46--( isDigit(c) )-->47 ]
      { 
        if  ( isDigit(c) )        // --75-- CAS[          46--( isDigit(c) )-->47 ]
        {
          port = port * 10 + valueDecimal(c); 
          state = 47; 
        }
                                        // --76--
        else if  ( c == ',' )        // --76-- CAS[          46--( c == ',' )-->48 ]
        {
           
          state = 48; 
        }
                                        // --77--
        else {
          state = 256;                // --77-- ERROR 
        }
      } 
    break; 
    case 47:                     // --77-- CAS[          47--( c == ',' )-->48 ]
      { 
        if  ( c == ',' )        // --77-- CAS[          47--( c == ',' )-->48 ]
        {
           
          state = 48; 
        }
                                        // --78--
        else {
          state = 256;                // --78-- ERROR 
        }
      } 
    break; 
    case 48:                     // --78-- CAS[          48--( isDigit(c) )-->49 ]
      { 
        if  ( isDigit(c) )        // --78-- CAS[          48--( isDigit(c) )-->49 ]
        {
          value = valueDecimal(c); 
          state = 49; 
        }
                                        // --79--
        else {
          state = 256;                // --79-- ERROR 
        }
      } 
    break; 
    case 49:                     // --79-- CAS[          49--( isDigit(c) )-->50 ]
      { 
        if  ( isDigit(c) )        // --79-- CAS[          49--( isDigit(c) )-->50 ]
        {
          value = value * 10 + valueDecimal(c); 
          state = 50; 
        }
                                        // --80--
        else if  ( c == '\n' )        // --80-- CAS[          49--( c == '\n' )-->255 ]
        {
          
          {
              if (debug & 1){
                  printDebug_p_port_value();
              }
              if ( pwms & (1 << port)){
                  analogWrite(port, value);
              }
          }
           
          state = 255; 
        }
                                        // --81--
        else {
          state = 256;                // --81-- ERROR 
        }
      } 
    break; 
    case 50:                     // --81-- CAS[          50--( isDigit(c) )-->51 ]
      { 
        if  ( isDigit(c) )        // --81-- CAS[          50--( isDigit(c) )-->51 ]
        {
          value = value * 10 + valueDecimal(c); 
          state = 51; 
        }
                                        // --82--
        else if  ( c == '\n' )        // --82-- CAS[          50--( c == '\n' )-->255 ]
        {
          
          {
              if (debug & 1){
                  printDebug_p_port_value();
              }
              if ( pwms & (1 << port)){
                  analogWrite(port, value);
              }
          }
           
          state = 255; 
        }
                                        // --83--
        else {
          state = 256;                // --83-- ERROR 
        }
      } 
    break; 
    case 51:                     // --83-- CAS[          51--( c == '\n' )-->255 ]
      { 
        if  ( c == '\n' )        // --83-- CAS[          51--( c == '\n' )-->255 ]
        {
          
          {
              if (debug & 1){
                  printDebug_p_port_value();
              }
              if ( pwms & (1 << port)){
                  analogWrite(port, value);
              }
          }
           
          state = 255; 
        }
                                        // --84--
        else {
          state = 256;                // --84-- ERROR 
        }
      } 
    break; 
    case 52:                     // --84-- CAS[          52--( c == 'e' )-->53 ]
      { 
        if  ( c == 'e' )        // --84-- CAS[          52--( c == 'e' )-->53 ]
        {
           
          state = 53; 
        }
                                        // --85--
        else if  ( c == ':' )        // --85-- CAS[          52--( c == ':' )-->64 ]
        {
           
          state = 64; 
        }
                                        // --86--
        else if  ( isDigit(c) )        // --86-- CAS[          52--( isDigit(c) )-->71 ]
        {
          port = valueDecimal(c); 
          state = 71; 
        }
                                        // --87--
        else {
          state = 256;                // --87-- ERROR 
        }
      } 
    break; 
    case 53:                     // --87-- CAS[          53--( c == 'r' )-->54 ]
      { 
        if  ( c == 'r' )        // --87-- CAS[          53--( c == 'r' )-->54 ]
        {
           
          state = 54; 
        }
                                        // --88--
        else {
          state = 256;                // --88-- ERROR 
        }
      } 
    break; 
    case 54:                     // --88-- CAS[          54--( c == 'v' )-->55 ]
      { 
        if  ( c == 'v' )        // --88-- CAS[          54--( c == 'v' )-->55 ]
        {
           
          state = 55; 
        }
                                        // --89--
        else {
          state = 256;                // --89-- ERROR 
        }
      } 
    break; 
    case 55:                     // --89-- CAS[          55--( c == 'o' )-->56 ]
      { 
        if  ( c == 'o' )        // --89-- CAS[          55--( c == 'o' )-->56 ]
        {
           
          state = 56; 
        }
                                        // --90--
        else {
          state = 256;                // --90-- ERROR 
        }
      } 
    break; 
    case 56:                     // --90-- CAS[          56--( c == ':' )-->57 ]
      { 
        if  ( c == ':' )        // --90-- CAS[          56--( c == ':' )-->57 ]
        {
           
          state = 57; 
        }
                                        // --91--
        else {
          state = 256;                // --91-- ERROR 
        }
      } 
    break; 
    case 57:                     // --91-- CAS[          57--( isDigit(c) )-->58 ]
      { 
        if  ( isDigit(c) )        // --91-- CAS[          57--( isDigit(c) )-->58 ]
        {
          port = valueDecimal(c); 
          state = 58; 
        }
                                        // --92--
        else {
          state = 256;                // --92-- ERROR 
        }
      } 
    break; 
    case 58:                     // --92-- CAS[          58--( isDigit(c) )-->59 ]
      { 
        if  ( isDigit(c) )        // --92-- CAS[          58--( isDigit(c) )-->59 ]
        {
          port = port * 10 + valueDecimal(c); 
          state = 59; 
        }
                                        // --93--
        else if  ( c == ',' )        // --93-- CAS[          58--( c == ',' )-->60 ]
        {
           
          state = 60; 
        }
                                        // --94--
        else {
          state = 256;                // --94-- ERROR 
        }
      } 
    break; 
    case 59:                     // --94-- CAS[          59--( c == ',' )-->60 ]
      { 
        if  ( c == ',' )        // --94-- CAS[          59--( c == ',' )-->60 ]
        {
           
          state = 60; 
        }
                                        // --95--
        else {
          state = 256;                // --95-- ERROR 
        }
      } 
    break; 
    case 60:                     // --95-- CAS[          60--( isDigit(c) )-->61 ]
      { 
        if  ( isDigit(c) )        // --95-- CAS[          60--( isDigit(c) )-->61 ]
        {
          value = valueDecimal(c); 
          state = 61; 
        }
                                        // --96--
        else {
          state = 256;                // --96-- ERROR 
        }
      } 
    break; 
    case 61:                     // --96-- CAS[          61--( isDigit(c) )-->62 ]
      { 
        if  ( isDigit(c) )        // --96-- CAS[          61--( isDigit(c) )-->62 ]
        {
          value = value * 10 + valueDecimal(c); 
          state = 62; 
        }
                                        // --97--
        else if  ( c == '\n' )        // --97-- CAS[          61--( c == '\n' )-->255 ]
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
               
          state = 255; 
        }
                                        // --98--
        else {
          state = 256;                // --98-- ERROR 
        }
      } 
    break; 
    case 62:                     // --98-- CAS[          62--( isDigit(c) )-->63 ]
      { 
        if  ( isDigit(c) )        // --98-- CAS[          62--( isDigit(c) )-->63 ]
        {
          value = value * 10 + valueDecimal(c); 
          state = 63; 
        }
                                        // --99--
        else if  ( c == '\n' )        // --99-- CAS[          62--( c == '\n' )-->255 ]
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
               
          state = 255; 
        }
                                        // --100--
        else {
          state = 256;                // --100-- ERROR 
        }
      } 
    break; 
    case 63:                     // --100-- CAS[          63--( c == '\n' )-->255 ]
      { 
        if  ( c == '\n' )        // --100-- CAS[          63--( c == '\n' )-->255 ]
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
               
          state = 255; 
        }
                                        // --101--
        else {
          state = 256;                // --101-- ERROR 
        }
      } 
    break; 
    case 64:                     // --101-- CAS[          64--( isDigit(c) )-->65 ]
      { 
        if  ( isDigit(c) )        // --101-- CAS[          64--( isDigit(c) )-->65 ]
        {
          port = valueDecimal(c); 
          state = 65; 
        }
                                        // --102--
        else {
          state = 256;                // --102-- ERROR 
        }
      } 
    break; 
    case 65:                     // --102-- CAS[          65--( isDigit(c) )-->66 ]
      { 
        if  ( isDigit(c) )        // --102-- CAS[          65--( isDigit(c) )-->66 ]
        {
          port = port * 10 + valueDecimal(c); 
          state = 66; 
        }
                                        // --103--
        else if  ( c == ',' )        // --103-- CAS[          65--( c == ',' )-->67 ]
        {
           
          state = 67; 
        }
                                        // --104--
        else {
          state = 256;                // --104-- ERROR 
        }
      } 
    break; 
    case 66:                     // --104-- CAS[          66--( c == ',' )-->67 ]
      { 
        if  ( c == ',' )        // --104-- CAS[          66--( c == ',' )-->67 ]
        {
           
          state = 67; 
        }
                                        // --105--
        else {
          state = 256;                // --105-- ERROR 
        }
      } 
    break; 
    case 67:                     // --105-- CAS[          67--( isDigit(c) )-->68 ]
      { 
        if  ( isDigit(c) )        // --105-- CAS[          67--( isDigit(c) )-->68 ]
        {
          value = valueDecimal(c); 
          state = 68; 
        }
                                        // --106--
        else {
          state = 256;                // --106-- ERROR 
        }
      } 
    break; 
    case 68:                     // --106-- CAS[          68--( isDigit(c) )-->69 ]
      { 
        if  ( isDigit(c) )        // --106-- CAS[          68--( isDigit(c) )-->69 ]
        {
          value = value * 10 + valueDecimal(c); 
          state = 69; 
        }
                                        // --107--
        else if  ( c == '\n' )        // --107-- CAS[          68--( c == '\n' )-->255 ]
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
               
          state = 255; 
        }
                                        // --108--
        else {
          state = 256;                // --108-- ERROR 
        }
      } 
    break; 
    case 69:                     // --108-- CAS[          69--( isDigit(c) )-->70 ]
      { 
        if  ( isDigit(c) )        // --108-- CAS[          69--( isDigit(c) )-->70 ]
        {
          value = value * 10 + valueDecimal(c); 
          state = 70; 
        }
                                        // --109--
        else if  ( c == '\n' )        // --109-- CAS[          69--( c == '\n' )-->255 ]
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
               
          state = 255; 
        }
                                        // --110--
        else {
          state = 256;                // --110-- ERROR 
        }
      } 
    break; 
    case 70:                     // --110-- CAS[          70--( c == '\n' )-->255 ]
      { 
        if  ( c == '\n' )        // --110-- CAS[          70--( c == '\n' )-->255 ]
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
               
          state = 255; 
        }
                                        // --111--
        else {
          state = 256;                // --111-- ERROR 
        }
      } 
    break; 
    case 71:                     // --111-- CAS[          71--( isDigit(c) )-->72 ]
      { 
        if  ( isDigit(c) )        // --111-- CAS[          71--( isDigit(c) )-->72 ]
        {
          port = port * 10 + valueDecimal(c); 
          state = 72; 
        }
                                        // --112--
        else if  ( c == ',' )        // --112-- CAS[          71--( c == ',' )-->73 ]
        {
           
          state = 73; 
        }
                                        // --113--
        else {
          state = 256;                // --113-- ERROR 
        }
      } 
    break; 
    case 72:                     // --113-- CAS[          72--( c == ',' )-->73 ]
      { 
        if  ( c == ',' )        // --113-- CAS[          72--( c == ',' )-->73 ]
        {
           
          state = 73; 
        }
                                        // --114--
        else {
          state = 256;                // --114-- ERROR 
        }
      } 
    break; 
    case 73:                     // --114-- CAS[          73--( isDigit(c) )-->74 ]
      { 
        if  ( isDigit(c) )        // --114-- CAS[          73--( isDigit(c) )-->74 ]
        {
          value = valueDecimal(c); 
          state = 74; 
        }
                                        // --115--
        else {
          state = 256;                // --115-- ERROR 
        }
      } 
    break; 
    case 74:                     // --115-- CAS[          74--( isDigit(c) )-->75 ]
      { 
        if  ( isDigit(c) )        // --115-- CAS[          74--( isDigit(c) )-->75 ]
        {
          value = value * 10 + valueDecimal(c); 
          state = 75; 
        }
                                        // --116--
        else if  ( c == '\n' )        // --116-- CAS[          74--( c == '\n' )-->255 ]
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
           
          state = 255; 
        }
                                        // --117--
        else {
          state = 256;                // --117-- ERROR 
        }
      } 
    break; 
    case 75:                     // --117-- CAS[          75--( isDigit(c) )-->76 ]
      { 
        if  ( isDigit(c) )        // --117-- CAS[          75--( isDigit(c) )-->76 ]
        {
          value = value * 10 + valueDecimal(c); 
          state = 76; 
        }
                                        // --118--
        else if  ( c == '\n' )        // --118-- CAS[          75--( c == '\n' )-->255 ]
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
           
          state = 255; 
        }
                                        // --119--
        else {
          state = 256;                // --119-- ERROR 
        }
      } 
    break; 
    case 76:                     // --119-- CAS[          76--( c == '\n' )-->255 ]
      { 
        if  ( c == '\n' )        // --119-- CAS[          76--( c == '\n' )-->255 ]
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
           
          state = 255; 
        }
                                        // --120--
        else {
          state = 256;                // --120-- ERROR 
        }
      } 
    break; 
    case 77:                     // --120-- CAS[          77--( c == 'e' )-->78 ]
      { 
        if  ( c == 'e' )        // --120-- CAS[          77--( c == 'e' )-->78 ]
        {
           
          state = 78; 
        }
                                        // --121--
        else {
          state = 256;                // --121-- ERROR 
        }
      } 
    break; 
    case 78:                     // --121-- CAS[          78--( c == 'l' )-->79 ]
      { 
        if  ( c == 'l' )        // --121-- CAS[          78--( c == 'l' )-->79 ]
        {
           
          state = 79; 
        }
                                        // --122--
        else {
          state = 256;                // --122-- ERROR 
        }
      } 
    break; 
    case 79:                     // --122-- CAS[          79--( c == 'p' )-->80 ]
      { 
        if  ( c == 'p' )        // --122-- CAS[          79--( c == 'p' )-->80 ]
        {
           
          state = 80; 
        }
                                        // --123--
        else {
          state = 256;                // --123-- ERROR 
        }
      } 
    break; 
    case 80:                     // --123-- CAS[          80--( c == '\n' )-->255 ]
      { 
        if  ( c == '\n' )        // --123-- CAS[          80--( c == '\n' )-->255 ]
        {
          
          {
              for ( int k = 0; TRUE; k ++ ) {
                  char  c =  pgm_read_byte_near(helpText + k);
                  if ( c == 0 ) break;
                  Serial.print(c);
              }
          }
           
          state = 255; 
        }
                                        // --124--
        else {
          state = 256;                // --124-- ERROR 
        }
      } 
    break; 
    case 81:                     // --124-- CAS[          81--( c == 'i' )-->82 ]
      { 
        if  ( c == 'i' )        // --124-- CAS[          81--( c == 'i' )-->82 ]
        {
           
          state = 82; 
        }
                                        // --125--
        else if  ( c == 'd' )        // --125-- CAS[          81--( c == 'd' )-->105 ]
        {
           
          state = 105; 
        }
                                        // --126--
        else if  ( c == 'a' )        // --126-- CAS[          81--( c == 'a' )-->178 ]
        {
           
          state = 178; 
        }
                                        // --127--
        else if  ( c == 'v' )        // --127-- CAS[          81--( c == 'v' )-->243 ]
        {
           
          state = 243; 
        }
                                        // --128--
        else if  ( c == 'e' )        // --128-- CAS[          81--( c == 'e' )-->251 ]
        {
           
          state = 251; 
        }
                                        // --129--
        else {
          state = 256;                // --129-- ERROR 
        }
      } 
    break; 
    case 82:                     // --129-- CAS[          82--( c == 'd' )-->83 ]
      { 
        if  ( c == 'd' )        // --129-- CAS[          82--( c == 'd' )-->83 ]
        {
           
          state = 83; 
        }
                                        // --130--
        else {
          state = 256;                // --130-- ERROR 
        }
      } 
    break; 
    case 83:                     // --130-- CAS[          83--( c == 'e' )-->84 ]
      { 
        if  ( c == 'e' )        // --130-- CAS[          83--( c == 'e' )-->84 ]
        {
           
          state = 84; 
        }
                                        // --131--
        else {
          state = 256;                // --131-- ERROR 
        }
      } 
    break; 
    case 84:                     // --131-- CAS[          84--( c == 'n' )-->85 ]
      { 
        if  ( c == 'n' )        // --131-- CAS[          84--( c == 'n' )-->85 ]
        {
           
          state = 85; 
        }
                                        // --132--
        else {
          state = 256;                // --132-- ERROR 
        }
      } 
    break; 
    case 85:                     // --132-- CAS[          85--( c == 't' )-->86 ]
      { 
        if  ( c == 't' )        // --132-- CAS[          85--( c == 't' )-->86 ]
        {
           
          state = 86; 
        }
                                        // --133--
        else {
          state = 256;                // --133-- ERROR 
        }
      } 
    break; 
    case 86:                     // --133-- CAS[          86--( c == ':' )-->87 ]
      { 
        if  ( c == ':' )        // --133-- CAS[          86--( c == ':' )-->87 ]
        {
           
          state = 87; 
        }
                                        // --134--
        else if  ( c == '?' )        // --134-- CAS[          86--( c == '?' )-->104 ]
        {
           
          state = 104; 
        }
                                        // --135--
        else {
          state = 256;                // --135-- ERROR 
        }
      } 
    break; 
    case 87:                     // --135-- CAS[          87--( isChar(c) )-->88 ]
      { 
        if  ( isChar(c) )        // --135-- CAS[          87--( isChar(c) )-->88 ]
        {
          id[0] = c;id[0+1] = 0; 
          state = 88; 
        }
                                        // --136--
        else {
          state = 256;                // --136-- ERROR 
        }
      } 
    break; 
    case 88:                     // --136-- CAS[          88--( isLabel(c) )-->89 ]
      { 
        if  ( isLabel(c) )        // --136-- CAS[          88--( isLabel(c) )-->89 ]
        {
          id[1] = c;id[1+1] = 0; 
          state = 89; 
        }
                                        // --137--
        else if  ( c == '\n' )        // --137-- CAS[          88--( c == '\n' )-->255 ]
        {
          
          {
              setEEPROM();
              if ( debug & 1) {
                  Serial.print(F( "cident=") );
                  Serial.println(id);
              }
          }
           
          state = 255; 
        }
                                        // --138--
        else {
          state = 256;                // --138-- ERROR 
        }
      } 
    break; 
    case 89:                     // --138-- CAS[          89--( isLabel(c) )-->90 ]
      { 
        if  ( isLabel(c) )        // --138-- CAS[          89--( isLabel(c) )-->90 ]
        {
          id[2] = c;id[2+1] = 0; 
          state = 90; 
        }
                                        // --139--
        else if  ( c == '\n' )        // --139-- CAS[          89--( c == '\n' )-->255 ]
        {
          
          {
              setEEPROM();
              if ( debug & 1) {
                  Serial.print(F( "cident=") );
                  Serial.println(id);
              }
          }
           
          state = 255; 
        }
                                        // --140--
        else {
          state = 256;                // --140-- ERROR 
        }
      } 
    break; 
    case 90:                     // --140-- CAS[          90--( isLabel(c) )-->91 ]
      { 
        if  ( isLabel(c) )        // --140-- CAS[          90--( isLabel(c) )-->91 ]
        {
          id[3] = c;id[3+1] = 0; 
          state = 91; 
        }
                                        // --141--
        else if  ( c == '\n' )        // --141-- CAS[          90--( c == '\n' )-->255 ]
        {
          
          {
              setEEPROM();
              if ( debug & 1) {
                  Serial.print(F( "cident=") );
                  Serial.println(id);
              }
          }
           
          state = 255; 
        }
                                        // --142--
        else {
          state = 256;                // --142-- ERROR 
        }
      } 
    break; 
    case 91:                     // --142-- CAS[          91--( isLabel(c) )-->92 ]
      { 
        if  ( isLabel(c) )        // --142-- CAS[          91--( isLabel(c) )-->92 ]
        {
          id[4] = c;id[4+1] = 0; 
          state = 92; 
        }
                                        // --143--
        else if  ( c == '\n' )        // --143-- CAS[          91--( c == '\n' )-->255 ]
        {
          
          {
              setEEPROM();
              if ( debug & 1) {
                  Serial.print(F( "cident=") );
                  Serial.println(id);
              }
          }
           
          state = 255; 
        }
                                        // --144--
        else {
          state = 256;                // --144-- ERROR 
        }
      } 
    break; 
    case 92:                     // --144-- CAS[          92--( isLabel(c) )-->93 ]
      { 
        if  ( isLabel(c) )        // --144-- CAS[          92--( isLabel(c) )-->93 ]
        {
          id[5] = c;id[5+1] = 0; 
          state = 93; 
        }
                                        // --145--
        else if  ( c == '\n' )        // --145-- CAS[          92--( c == '\n' )-->255 ]
        {
          
          {
              setEEPROM();
              if ( debug & 1) {
                  Serial.print(F( "cident=") );
                  Serial.println(id);
              }
          }
           
          state = 255; 
        }
                                        // --146--
        else {
          state = 256;                // --146-- ERROR 
        }
      } 
    break; 
    case 93:                     // --146-- CAS[          93--( isLabel(c) )-->94 ]
      { 
        if  ( isLabel(c) )        // --146-- CAS[          93--( isLabel(c) )-->94 ]
        {
          id[6] = c;id[6+1] = 0; 
          state = 94; 
        }
                                        // --147--
        else if  ( c == '\n' )        // --147-- CAS[          93--( c == '\n' )-->255 ]
        {
          
          {
              setEEPROM();
              if ( debug & 1) {
                  Serial.print(F( "cident=") );
                  Serial.println(id);
              }
          }
           
          state = 255; 
        }
                                        // --148--
        else {
          state = 256;                // --148-- ERROR 
        }
      } 
    break; 
    case 94:                     // --148-- CAS[          94--( isLabel(c) )-->95 ]
      { 
        if  ( isLabel(c) )        // --148-- CAS[          94--( isLabel(c) )-->95 ]
        {
          id[7] = c;id[7+1] = 0; 
          state = 95; 
        }
                                        // --149--
        else if  ( c == '\n' )        // --149-- CAS[          94--( c == '\n' )-->255 ]
        {
          
          {
              setEEPROM();
              if ( debug & 1) {
                  Serial.print(F( "cident=") );
                  Serial.println(id);
              }
          }
           
          state = 255; 
        }
                                        // --150--
        else {
          state = 256;                // --150-- ERROR 
        }
      } 
    break; 
    case 95:                     // --150-- CAS[          95--( isLabel(c) )-->96 ]
      { 
        if  ( isLabel(c) )        // --150-- CAS[          95--( isLabel(c) )-->96 ]
        {
          id[8] = c;id[8+1] = 0; 
          state = 96; 
        }
                                        // --151--
        else if  ( c == '\n' )        // --151-- CAS[          95--( c == '\n' )-->255 ]
        {
          
          {
              setEEPROM();
              if ( debug & 1) {
                  Serial.print(F( "cident=") );
                  Serial.println(id);
              }
          }
           
          state = 255; 
        }
                                        // --152--
        else {
          state = 256;                // --152-- ERROR 
        }
      } 
    break; 
    case 96:                     // --152-- CAS[          96--( isLabel(c) )-->97 ]
      { 
        if  ( isLabel(c) )        // --152-- CAS[          96--( isLabel(c) )-->97 ]
        {
          id[9] = c;id[9+1] = 0; 
          state = 97; 
        }
                                        // --153--
        else if  ( c == '\n' )        // --153-- CAS[          96--( c == '\n' )-->255 ]
        {
          
          {
              setEEPROM();
              if ( debug & 1) {
                  Serial.print(F( "cident=") );
                  Serial.println(id);
              }
          }
           
          state = 255; 
        }
                                        // --154--
        else {
          state = 256;                // --154-- ERROR 
        }
      } 
    break; 
    case 97:                     // --154-- CAS[          97--( isLabel(c) )-->98 ]
      { 
        if  ( isLabel(c) )        // --154-- CAS[          97--( isLabel(c) )-->98 ]
        {
          id[10] = c;id[10+1] = 0; 
          state = 98; 
        }
                                        // --155--
        else if  ( c == '\n' )        // --155-- CAS[          97--( c == '\n' )-->255 ]
        {
          
          {
              setEEPROM();
              if ( debug & 1) {
                  Serial.print(F( "cident=") );
                  Serial.println(id);
              }
          }
           
          state = 255; 
        }
                                        // --156--
        else {
          state = 256;                // --156-- ERROR 
        }
      } 
    break; 
    case 98:                     // --156-- CAS[          98--( isLabel(c) )-->99 ]
      { 
        if  ( isLabel(c) )        // --156-- CAS[          98--( isLabel(c) )-->99 ]
        {
          id[11] = c;id[11+1] = 0; 
          state = 99; 
        }
                                        // --157--
        else if  ( c == '\n' )        // --157-- CAS[          98--( c == '\n' )-->255 ]
        {
          
          {
              setEEPROM();
              if ( debug & 1) {
                  Serial.print(F( "cident=") );
                  Serial.println(id);
              }
          }
           
          state = 255; 
        }
                                        // --158--
        else {
          state = 256;                // --158-- ERROR 
        }
      } 
    break; 
    case 99:                     // --158-- CAS[          99--( isLabel(c) )-->100 ]
      { 
        if  ( isLabel(c) )        // --158-- CAS[          99--( isLabel(c) )-->100 ]
        {
          id[12] = c;id[12+1] = 0; 
          state = 100; 
        }
                                        // --159--
        else if  ( c == '\n' )        // --159-- CAS[          99--( c == '\n' )-->255 ]
        {
          
          {
              setEEPROM();
              if ( debug & 1) {
                  Serial.print(F( "cident=") );
                  Serial.println(id);
              }
          }
           
          state = 255; 
        }
                                        // --160--
        else {
          state = 256;                // --160-- ERROR 
        }
      } 
    break; 
    case 100:                     // --160-- CAS[          100--( isLabel(c) )-->101 ]
      { 
        if  ( isLabel(c) )        // --160-- CAS[          100--( isLabel(c) )-->101 ]
        {
          id[13] = c;id[13+1] = 0; 
          state = 101; 
        }
                                        // --161--
        else if  ( c == '\n' )        // --161-- CAS[          100--( c == '\n' )-->255 ]
        {
          
          {
              setEEPROM();
              if ( debug & 1) {
                  Serial.print(F( "cident=") );
                  Serial.println(id);
              }
          }
           
          state = 255; 
        }
                                        // --162--
        else {
          state = 256;                // --162-- ERROR 
        }
      } 
    break; 
    case 101:                     // --162-- CAS[          101--( isLabel(c) )-->102 ]
      { 
        if  ( isLabel(c) )        // --162-- CAS[          101--( isLabel(c) )-->102 ]
        {
          id[14] = c;id[14+1] = 0; 
          state = 102; 
        }
                                        // --163--
        else if  ( c == '\n' )        // --163-- CAS[          101--( c == '\n' )-->255 ]
        {
          
          {
              setEEPROM();
              if ( debug & 1) {
                  Serial.print(F( "cident=") );
                  Serial.println(id);
              }
          }
           
          state = 255; 
        }
                                        // --164--
        else {
          state = 256;                // --164-- ERROR 
        }
      } 
    break; 
    case 102:                     // --164-- CAS[          102--( isLabel(c) )-->103 ]
      { 
        if  ( isLabel(c) )        // --164-- CAS[          102--( isLabel(c) )-->103 ]
        {
          id[15] = c;id[15+1] = 0; 
          state = 103; 
        }
                                        // --165--
        else if  ( c == '\n' )        // --165-- CAS[          102--( c == '\n' )-->255 ]
        {
          
          {
              setEEPROM();
              if ( debug & 1) {
                  Serial.print(F( "cident=") );
                  Serial.println(id);
              }
          }
           
          state = 255; 
        }
                                        // --166--
        else {
          state = 256;                // --166-- ERROR 
        }
      } 
    break; 
    case 103:                     // --166-- CAS[          103--( c == '\n' )-->255 ]
      { 
        if  ( c == '\n' )        // --166-- CAS[          103--( c == '\n' )-->255 ]
        {
          
          {
              setEEPROM();
              if ( debug & 1) {
                  Serial.print(F( "cident=") );
                  Serial.println(id);
              }
          }
           
          state = 255; 
        }
                                        // --167--
        else {
          state = 256;                // --167-- ERROR 
        }
      } 
    break; 
    case 104:                     // --167-- CAS[          104--( c == '\n' )-->255 ]
      { 
        if  ( c == '\n' )        // --167-- CAS[          104--( c == '\n' )-->255 ]
        {
          
          {
              getEEPROM();
          }
           
          state = 255; 
        }
                                        // --168--
        else {
          state = 256;                // --168-- ERROR 
        }
      } 
    break; 
    case 105:                     // --168-- CAS[          105--( c == 'i' )-->106 ]
      { 
        if  ( c == 'i' )        // --168-- CAS[          105--( c == 'i' )-->106 ]
        {
           
          state = 106; 
        }
                                        // --169--
        else if  ( c == 'e' )        // --169-- CAS[          105--( c == 'e' )-->117 ]
        {
           
          state = 117; 
        }
                                        // --170--
        else if  ( c == 'o' )        // --170-- CAS[          105--( c == 'o' )-->140 ]
        {
           
          state = 140; 
        }
                                        // --171--
        else if  ( c == 'p' )        // --171-- CAS[          105--( c == 'p' )-->152 ]
        {
           
          state = 152; 
        }
                                        // --172--
        else if  ( c == 's' )        // --172-- CAS[          105--( c == 's' )-->164 ]
        {
           
          state = 164; 
        }
                                        // --173--
        else {
          state = 256;                // --173-- ERROR 
        }
      } 
    break; 
    case 106:                     // --173-- CAS[          106--( c == 'n' )-->107 ]
      { 
        if  ( c == 'n' )        // --173-- CAS[          106--( c == 'n' )-->107 ]
        {
           
          state = 107; 
        }
                                        // --174--
        else {
          state = 256;                // --174-- ERROR 
        }
      } 
    break; 
    case 107:                     // --174-- CAS[          107--( c == ':' )-->108 ]
      { 
        if  ( c == ':' )        // --174-- CAS[          107--( c == ':' )-->108 ]
        {
           
          state = 108; 
        }
                                        // --175--
        else if  ( c == 'p' )        // --175-- CAS[          107--( c == 'p' )-->130 ]
        {
           
          state = 130; 
        }
                                        // --176--
        else {
          state = 256;                // --176-- ERROR 
        }
      } 
    break; 
    case 108:                     // --176-- CAS[          108--( isHex(c) )-->109 ]
      { 
        if  ( isHex(c) )        // --176-- CAS[          108--( isHex(c) )-->109 ]
        {
          data = valueHex(c); 
          state = 109; 
        }
                                        // --177--
        else {
          state = 256;                // --177-- ERROR 
        }
      } 
    break; 
    case 109:                     // --177-- CAS[          109--( isHex(c) )-->110 ]
      { 
        if  ( isHex(c) )        // --177-- CAS[          109--( isHex(c) )-->110 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 110; 
        }
                                        // --178--
        else if  ( c == '\n' )        // --178-- CAS[          109--( c == '\n' )-->255 ]
        {
          
          {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalInput(data);
              if ( debug & 1) {
                  printDebug_cdin();
              }
          }
           
          state = 255; 
        }
                                        // --179--
        else {
          state = 256;                // --179-- ERROR 
        }
      } 
    break; 
    case 110:                     // --179-- CAS[          110--( isHex(c) )-->111 ]
      { 
        if  ( isHex(c) )        // --179-- CAS[          110--( isHex(c) )-->111 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 111; 
        }
                                        // --180--
        else if  ( c == '\n' )        // --180-- CAS[          110--( c == '\n' )-->255 ]
        {
          
          {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalInput(data);
              if ( debug & 1) {
                  printDebug_cdin();
              }
          }
           
          state = 255; 
        }
                                        // --181--
        else {
          state = 256;                // --181-- ERROR 
        }
      } 
    break; 
    case 111:                     // --181-- CAS[          111--( isHex(c) )-->112 ]
      { 
        if  ( isHex(c) )        // --181-- CAS[          111--( isHex(c) )-->112 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 112; 
        }
                                        // --182--
        else if  ( c == '\n' )        // --182-- CAS[          111--( c == '\n' )-->255 ]
        {
          
          {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalInput(data);
              if ( debug & 1) {
                  printDebug_cdin();
              }
          }
           
          state = 255; 
        }
                                        // --183--
        else {
          state = 256;                // --183-- ERROR 
        }
      } 
    break; 
    case 112:                     // --183-- CAS[          112--( isHex(c) )-->113 ]
      { 
        if  ( isHex(c) )        // --183-- CAS[          112--( isHex(c) )-->113 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 113; 
        }
                                        // --184--
        else if  ( c == '\n' )        // --184-- CAS[          112--( c == '\n' )-->255 ]
        {
          
          {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalInput(data);
              if ( debug & 1) {
                  printDebug_cdin();
              }
          }
           
          state = 255; 
        }
                                        // --185--
        else {
          state = 256;                // --185-- ERROR 
        }
      } 
    break; 
    case 113:                     // --185-- CAS[          113--( isHex(c) )-->114 ]
      { 
        if  ( isHex(c) )        // --185-- CAS[          113--( isHex(c) )-->114 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 114; 
        }
                                        // --186--
        else if  ( c == '\n' )        // --186-- CAS[          113--( c == '\n' )-->255 ]
        {
          
          {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalInput(data);
              if ( debug & 1) {
                  printDebug_cdin();
              }
          }
           
          state = 255; 
        }
                                        // --187--
        else {
          state = 256;                // --187-- ERROR 
        }
      } 
    break; 
    case 114:                     // --187-- CAS[          114--( isHex(c) )-->115 ]
      { 
        if  ( isHex(c) )        // --187-- CAS[          114--( isHex(c) )-->115 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 115; 
        }
                                        // --188--
        else if  ( c == '\n' )        // --188-- CAS[          114--( c == '\n' )-->255 ]
        {
          
          {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalInput(data);
              if ( debug & 1) {
                  printDebug_cdin();
              }
          }
           
          state = 255; 
        }
                                        // --189--
        else {
          state = 256;                // --189-- ERROR 
        }
      } 
    break; 
    case 115:                     // --189-- CAS[          115--( isHex(c) )-->116 ]
      { 
        if  ( isHex(c) )        // --189-- CAS[          115--( isHex(c) )-->116 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 116; 
        }
                                        // --190--
        else if  ( c == '\n' )        // --190-- CAS[          115--( c == '\n' )-->255 ]
        {
          
          {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalInput(data);
              if ( debug & 1) {
                  printDebug_cdin();
              }
          }
           
          state = 255; 
        }
                                        // --191--
        else {
          state = 256;                // --191-- ERROR 
        }
      } 
    break; 
    case 116:                     // --191-- CAS[          116--( c == '\n' )-->255 ]
      { 
        if  ( c == '\n' )        // --191-- CAS[          116--( c == '\n' )-->255 ]
        {
          
          {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalInput(data);
              if ( debug & 1) {
                  printDebug_cdin();
              }
          }
           
          state = 255; 
        }
                                        // --192--
        else {
          state = 256;                // --192-- ERROR 
        }
      } 
    break; 
    case 117:                     // --192-- CAS[          117--( c == 'b' )-->118 ]
      { 
        if  ( c == 'b' )        // --192-- CAS[          117--( c == 'b' )-->118 ]
        {
           
          state = 118; 
        }
                                        // --193--
        else {
          state = 256;                // --193-- ERROR 
        }
      } 
    break; 
    case 118:                     // --193-- CAS[          118--( c == 'u' )-->119 ]
      { 
        if  ( c == 'u' )        // --193-- CAS[          118--( c == 'u' )-->119 ]
        {
           
          state = 119; 
        }
                                        // --194--
        else {
          state = 256;                // --194-- ERROR 
        }
      } 
    break; 
    case 119:                     // --194-- CAS[          119--( c == 'g' )-->120 ]
      { 
        if  ( c == 'g' )        // --194-- CAS[          119--( c == 'g' )-->120 ]
        {
           
          state = 120; 
        }
                                        // --195--
        else {
          state = 256;                // --195-- ERROR 
        }
      } 
    break; 
    case 120:                     // --195-- CAS[          120--( c == ':' )-->121 ]
      { 
        if  ( c == ':' )        // --195-- CAS[          120--( c == ':' )-->121 ]
        {
           
          state = 121; 
        }
                                        // --196--
        else {
          state = 256;                // --196-- ERROR 
        }
      } 
    break; 
    case 121:                     // --196-- CAS[          121--( isHex(c) )-->122 ]
      { 
        if  ( isHex(c) )        // --196-- CAS[          121--( isHex(c) )-->122 ]
        {
          data = valueHex(c); 
          state = 122; 
        }
                                        // --197--
        else {
          state = 256;                // --197-- ERROR 
        }
      } 
    break; 
    case 122:                     // --197-- CAS[          122--( isHex(c) )-->123 ]
      { 
        if  ( isHex(c) )        // --197-- CAS[          122--( isHex(c) )-->123 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 123; 
        }
                                        // --198--
        else if  ( c == '\n' )        // --198-- CAS[          122--( c == '\n' )-->255 ]
        {
           
          {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              debug =  data;
              if ( debug & 1) {
                  printDebug_cdebug();
              }
          
           }
           
          state = 255; 
        }
                                        // --199--
        else {
          state = 256;                // --199-- ERROR 
        }
      } 
    break; 
    case 123:                     // --199-- CAS[          123--( isHex(c) )-->124 ]
      { 
        if  ( isHex(c) )        // --199-- CAS[          123--( isHex(c) )-->124 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 124; 
        }
                                        // --200--
        else if  ( c == '\n' )        // --200-- CAS[          123--( c == '\n' )-->255 ]
        {
           
          {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              debug =  data;
              if ( debug & 1) {
                  printDebug_cdebug();
              }
          
           }
           
          state = 255; 
        }
                                        // --201--
        else {
          state = 256;                // --201-- ERROR 
        }
      } 
    break; 
    case 124:                     // --201-- CAS[          124--( isHex(c) )-->125 ]
      { 
        if  ( isHex(c) )        // --201-- CAS[          124--( isHex(c) )-->125 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 125; 
        }
                                        // --202--
        else if  ( c == '\n' )        // --202-- CAS[          124--( c == '\n' )-->255 ]
        {
           
          {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              debug =  data;
              if ( debug & 1) {
                  printDebug_cdebug();
              }
          
           }
           
          state = 255; 
        }
                                        // --203--
        else {
          state = 256;                // --203-- ERROR 
        }
      } 
    break; 
    case 125:                     // --203-- CAS[          125--( isHex(c) )-->126 ]
      { 
        if  ( isHex(c) )        // --203-- CAS[          125--( isHex(c) )-->126 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 126; 
        }
                                        // --204--
        else if  ( c == '\n' )        // --204-- CAS[          125--( c == '\n' )-->255 ]
        {
           
          {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              debug =  data;
              if ( debug & 1) {
                  printDebug_cdebug();
              }
          
           }
           
          state = 255; 
        }
                                        // --205--
        else {
          state = 256;                // --205-- ERROR 
        }
      } 
    break; 
    case 126:                     // --205-- CAS[          126--( isHex(c) )-->127 ]
      { 
        if  ( isHex(c) )        // --205-- CAS[          126--( isHex(c) )-->127 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 127; 
        }
                                        // --206--
        else if  ( c == '\n' )        // --206-- CAS[          126--( c == '\n' )-->255 ]
        {
           
          {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              debug =  data;
              if ( debug & 1) {
                  printDebug_cdebug();
              }
          
           }
           
          state = 255; 
        }
                                        // --207--
        else {
          state = 256;                // --207-- ERROR 
        }
      } 
    break; 
    case 127:                     // --207-- CAS[          127--( isHex(c) )-->128 ]
      { 
        if  ( isHex(c) )        // --207-- CAS[          127--( isHex(c) )-->128 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 128; 
        }
                                        // --208--
        else if  ( c == '\n' )        // --208-- CAS[          127--( c == '\n' )-->255 ]
        {
           
          {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              debug =  data;
              if ( debug & 1) {
                  printDebug_cdebug();
              }
          
           }
           
          state = 255; 
        }
                                        // --209--
        else {
          state = 256;                // --209-- ERROR 
        }
      } 
    break; 
    case 128:                     // --209-- CAS[          128--( isHex(c) )-->129 ]
      { 
        if  ( isHex(c) )        // --209-- CAS[          128--( isHex(c) )-->129 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 129; 
        }
                                        // --210--
        else if  ( c == '\n' )        // --210-- CAS[          128--( c == '\n' )-->255 ]
        {
           
          {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              debug =  data;
              if ( debug & 1) {
                  printDebug_cdebug();
              }
          
           }
           
          state = 255; 
        }
                                        // --211--
        else {
          state = 256;                // --211-- ERROR 
        }
      } 
    break; 
    case 129:                     // --211-- CAS[          129--( c == '\n' )-->255 ]
      { 
        if  ( c == '\n' )        // --211-- CAS[          129--( c == '\n' )-->255 ]
        {
           
          {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              debug =  data;
              if ( debug & 1) {
                  printDebug_cdebug();
              }
          
           }
           
          state = 255; 
        }
                                        // --212--
        else {
          state = 256;                // --212-- ERROR 
        }
      } 
    break; 
    case 130:                     // --212-- CAS[          130--( c == ':' )-->131 ]
      { 
        if  ( c == ':' )        // --212-- CAS[          130--( c == ':' )-->131 ]
        {
           
          state = 131; 
        }
                                        // --213--
        else {
          state = 256;                // --213-- ERROR 
        }
      } 
    break; 
    case 131:                     // --213-- CAS[          131--( isHex(c) )-->132 ]
      { 
        if  ( isHex(c) )        // --213-- CAS[          131--( isHex(c) )-->132 ]
        {
          data = valueHex(c); 
          state = 132; 
        }
                                        // --214--
        else {
          state = 256;                // --214-- ERROR 
        }
      } 
    break; 
    case 132:                     // --214-- CAS[          132--( isHex(c) )-->133 ]
      { 
        if  ( isHex(c) )        // --214-- CAS[          132--( isHex(c) )-->133 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 133; 
        }
                                        // --215--
        else if  ( c == '\n' )        // --215-- CAS[          132--( c == '\n' )-->255 ]
        {
           
          {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalInputPullup(data);
              if ( debug & 1) {
                  printDebug_cdinp();
                  
              }
          }
           
          state = 255; 
        }
                                        // --216--
        else {
          state = 256;                // --216-- ERROR 
        }
      } 
    break; 
    case 133:                     // --216-- CAS[          133--( isHex(c) )-->134 ]
      { 
        if  ( isHex(c) )        // --216-- CAS[          133--( isHex(c) )-->134 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 134; 
        }
                                        // --217--
        else if  ( c == '\n' )        // --217-- CAS[          133--( c == '\n' )-->255 ]
        {
           
          {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalInputPullup(data);
              if ( debug & 1) {
                  printDebug_cdinp();
                  
              }
          }
           
          state = 255; 
        }
                                        // --218--
        else {
          state = 256;                // --218-- ERROR 
        }
      } 
    break; 
    case 134:                     // --218-- CAS[          134--( isHex(c) )-->135 ]
      { 
        if  ( isHex(c) )        // --218-- CAS[          134--( isHex(c) )-->135 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 135; 
        }
                                        // --219--
        else if  ( c == '\n' )        // --219-- CAS[          134--( c == '\n' )-->255 ]
        {
           
          {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalInputPullup(data);
              if ( debug & 1) {
                  printDebug_cdinp();
                  
              }
          }
           
          state = 255; 
        }
                                        // --220--
        else {
          state = 256;                // --220-- ERROR 
        }
      } 
    break; 
    case 135:                     // --220-- CAS[          135--( isHex(c) )-->136 ]
      { 
        if  ( isHex(c) )        // --220-- CAS[          135--( isHex(c) )-->136 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 136; 
        }
                                        // --221--
        else if  ( c == '\n' )        // --221-- CAS[          135--( c == '\n' )-->255 ]
        {
           
          {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalInputPullup(data);
              if ( debug & 1) {
                  printDebug_cdinp();
                  
              }
          }
           
          state = 255; 
        }
                                        // --222--
        else {
          state = 256;                // --222-- ERROR 
        }
      } 
    break; 
    case 136:                     // --222-- CAS[          136--( isHex(c) )-->137 ]
      { 
        if  ( isHex(c) )        // --222-- CAS[          136--( isHex(c) )-->137 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 137; 
        }
                                        // --223--
        else if  ( c == '\n' )        // --223-- CAS[          136--( c == '\n' )-->255 ]
        {
           
          {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalInputPullup(data);
              if ( debug & 1) {
                  printDebug_cdinp();
                  
              }
          }
           
          state = 255; 
        }
                                        // --224--
        else {
          state = 256;                // --224-- ERROR 
        }
      } 
    break; 
    case 137:                     // --224-- CAS[          137--( isHex(c) )-->138 ]
      { 
        if  ( isHex(c) )        // --224-- CAS[          137--( isHex(c) )-->138 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 138; 
        }
                                        // --225--
        else if  ( c == '\n' )        // --225-- CAS[          137--( c == '\n' )-->255 ]
        {
           
          {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalInputPullup(data);
              if ( debug & 1) {
                  printDebug_cdinp();
                  
              }
          }
           
          state = 255; 
        }
                                        // --226--
        else {
          state = 256;                // --226-- ERROR 
        }
      } 
    break; 
    case 138:                     // --226-- CAS[          138--( isHex(c) )-->139 ]
      { 
        if  ( isHex(c) )        // --226-- CAS[          138--( isHex(c) )-->139 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 139; 
        }
                                        // --227--
        else if  ( c == '\n' )        // --227-- CAS[          138--( c == '\n' )-->255 ]
        {
           
          {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalInputPullup(data);
              if ( debug & 1) {
                  printDebug_cdinp();
                  
              }
          }
           
          state = 255; 
        }
                                        // --228--
        else {
          state = 256;                // --228-- ERROR 
        }
      } 
    break; 
    case 139:                     // --228-- CAS[          139--( c == '\n' )-->255 ]
      { 
        if  ( c == '\n' )        // --228-- CAS[          139--( c == '\n' )-->255 ]
        {
           
          {
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalInputPullup(data);
              if ( debug & 1) {
                  printDebug_cdinp();
                  
              }
          }
           
          state = 255; 
        }
                                        // --229--
        else {
          state = 256;                // --229-- ERROR 
        }
      } 
    break; 
    case 140:                     // --229-- CAS[          140--( c == 'u' )-->141 ]
      { 
        if  ( c == 'u' )        // --229-- CAS[          140--( c == 'u' )-->141 ]
        {
           
          state = 141; 
        }
                                        // --230--
        else {
          state = 256;                // --230-- ERROR 
        }
      } 
    break; 
    case 141:                     // --230-- CAS[          141--( c == 't' )-->142 ]
      { 
        if  ( c == 't' )        // --230-- CAS[          141--( c == 't' )-->142 ]
        {
           
          state = 142; 
        }
                                        // --231--
        else {
          state = 256;                // --231-- ERROR 
        }
      } 
    break; 
    case 142:                     // --231-- CAS[          142--( c == ':' )-->143 ]
      { 
        if  ( c == ':' )        // --231-- CAS[          142--( c == ':' )-->143 ]
        {
           
          state = 143; 
        }
                                        // --232--
        else {
          state = 256;                // --232-- ERROR 
        }
      } 
    break; 
    case 143:                     // --232-- CAS[          143--( isHex(c) )-->144 ]
      { 
        if  ( isHex(c) )        // --232-- CAS[          143--( isHex(c) )-->144 ]
        {
          data = valueHex(c); 
          state = 144; 
        }
                                        // --233--
        else {
          state = 256;                // --233-- ERROR 
        }
      } 
    break; 
    case 144:                     // --233-- CAS[          144--( isHex(c) )-->145 ]
      { 
        if  ( isHex(c) )        // --233-- CAS[          144--( isHex(c) )-->145 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 145; 
        }
                                        // --234--
        else if  ( c == '\n' )        // --234-- CAS[          144--( c == '\n' )-->255 ]
        {
          
          {
              setDigitalOutput(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cdout();
              }
          }
           
          state = 255; 
        }
                                        // --235--
        else {
          state = 256;                // --235-- ERROR 
        }
      } 
    break; 
    case 145:                     // --235-- CAS[          145--( isHex(c) )-->146 ]
      { 
        if  ( isHex(c) )        // --235-- CAS[          145--( isHex(c) )-->146 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 146; 
        }
                                        // --236--
        else if  ( c == '\n' )        // --236-- CAS[          145--( c == '\n' )-->255 ]
        {
          
          {
              setDigitalOutput(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cdout();
              }
          }
           
          state = 255; 
        }
                                        // --237--
        else {
          state = 256;                // --237-- ERROR 
        }
      } 
    break; 
    case 146:                     // --237-- CAS[          146--( isHex(c) )-->147 ]
      { 
        if  ( isHex(c) )        // --237-- CAS[          146--( isHex(c) )-->147 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 147; 
        }
                                        // --238--
        else if  ( c == '\n' )        // --238-- CAS[          146--( c == '\n' )-->255 ]
        {
          
          {
              setDigitalOutput(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cdout();
              }
          }
           
          state = 255; 
        }
                                        // --239--
        else {
          state = 256;                // --239-- ERROR 
        }
      } 
    break; 
    case 147:                     // --239-- CAS[          147--( isHex(c) )-->148 ]
      { 
        if  ( isHex(c) )        // --239-- CAS[          147--( isHex(c) )-->148 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 148; 
        }
                                        // --240--
        else if  ( c == '\n' )        // --240-- CAS[          147--( c == '\n' )-->255 ]
        {
          
          {
              setDigitalOutput(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cdout();
              }
          }
           
          state = 255; 
        }
                                        // --241--
        else {
          state = 256;                // --241-- ERROR 
        }
      } 
    break; 
    case 148:                     // --241-- CAS[          148--( isHex(c) )-->149 ]
      { 
        if  ( isHex(c) )        // --241-- CAS[          148--( isHex(c) )-->149 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 149; 
        }
                                        // --242--
        else if  ( c == '\n' )        // --242-- CAS[          148--( c == '\n' )-->255 ]
        {
          
          {
              setDigitalOutput(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cdout();
              }
          }
           
          state = 255; 
        }
                                        // --243--
        else {
          state = 256;                // --243-- ERROR 
        }
      } 
    break; 
    case 149:                     // --243-- CAS[          149--( isHex(c) )-->150 ]
      { 
        if  ( isHex(c) )        // --243-- CAS[          149--( isHex(c) )-->150 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 150; 
        }
                                        // --244--
        else if  ( c == '\n' )        // --244-- CAS[          149--( c == '\n' )-->255 ]
        {
          
          {
              setDigitalOutput(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cdout();
              }
          }
           
          state = 255; 
        }
                                        // --245--
        else {
          state = 256;                // --245-- ERROR 
        }
      } 
    break; 
    case 150:                     // --245-- CAS[          150--( isHex(c) )-->151 ]
      { 
        if  ( isHex(c) )        // --245-- CAS[          150--( isHex(c) )-->151 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 151; 
        }
                                        // --246--
        else if  ( c == '\n' )        // --246-- CAS[          150--( c == '\n' )-->255 ]
        {
          
          {
              setDigitalOutput(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cdout();
              }
          }
           
          state = 255; 
        }
                                        // --247--
        else {
          state = 256;                // --247-- ERROR 
        }
      } 
    break; 
    case 151:                     // --247-- CAS[          151--( c == '\n' )-->255 ]
      { 
        if  ( c == '\n' )        // --247-- CAS[          151--( c == '\n' )-->255 ]
        {
          
          {
              setDigitalOutput(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cdout();
              }
          }
           
          state = 255; 
        }
                                        // --248--
        else {
          state = 256;                // --248-- ERROR 
        }
      } 
    break; 
    case 152:                     // --248-- CAS[          152--( c == 'w' )-->153 ]
      { 
        if  ( c == 'w' )        // --248-- CAS[          152--( c == 'w' )-->153 ]
        {
           
          state = 153; 
        }
                                        // --249--
        else {
          state = 256;                // --249-- ERROR 
        }
      } 
    break; 
    case 153:                     // --249-- CAS[          153--( c == 'm' )-->154 ]
      { 
        if  ( c == 'm' )        // --249-- CAS[          153--( c == 'm' )-->154 ]
        {
           
          state = 154; 
        }
                                        // --250--
        else {
          state = 256;                // --250-- ERROR 
        }
      } 
    break; 
    case 154:                     // --250-- CAS[          154--( c == ':' )-->155 ]
      { 
        if  ( c == ':' )        // --250-- CAS[          154--( c == ':' )-->155 ]
        {
           
          state = 155; 
        }
                                        // --251--
        else {
          state = 256;                // --251-- ERROR 
        }
      } 
    break; 
    case 155:                     // --251-- CAS[          155--( isHex(c) )-->156 ]
      { 
        if  ( isHex(c) )        // --251-- CAS[          155--( isHex(c) )-->156 ]
        {
          data = valueHex(c); 
          state = 156; 
        }
                                        // --252--
        else {
          state = 256;                // --252-- ERROR 
        }
      } 
    break; 
    case 156:                     // --252-- CAS[          156--( isHex(c) )-->157 ]
      { 
        if  ( isHex(c) )        // --252-- CAS[          156--( isHex(c) )-->157 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 157; 
        }
                                        // --253--
        else if  ( c == '\n' )        // --253-- CAS[          156--( c == '\n' )-->255 ]
        {
          
          {
              pwms = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalPWMOutput(data);
              if ( debug & 1) {
                  printDebug_cdpwm();
              }
          }
           
          state = 255; 
        }
                                        // --254--
        else {
          state = 256;                // --254-- ERROR 
        }
      } 
    break; 
    case 157:                     // --254-- CAS[          157--( isHex(c) )-->158 ]
      { 
        if  ( isHex(c) )        // --254-- CAS[          157--( isHex(c) )-->158 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 158; 
        }
                                        // --255--
        else if  ( c == '\n' )        // --255-- CAS[          157--( c == '\n' )-->255 ]
        {
          
          {
              pwms = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalPWMOutput(data);
              if ( debug & 1) {
                  printDebug_cdpwm();
              }
          }
           
          state = 255; 
        }
                                        // --256--
        else {
          state = 256;                // --256-- ERROR 
        }
      } 
    break; 
    case 158:                     // --256-- CAS[          158--( isHex(c) )-->159 ]
      { 
        if  ( isHex(c) )        // --256-- CAS[          158--( isHex(c) )-->159 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 159; 
        }
                                        // --257--
        else if  ( c == '\n' )        // --257-- CAS[          158--( c == '\n' )-->255 ]
        {
          
          {
              pwms = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalPWMOutput(data);
              if ( debug & 1) {
                  printDebug_cdpwm();
              }
          }
           
          state = 255; 
        }
                                        // --258--
        else {
          state = 256;                // --258-- ERROR 
        }
      } 
    break; 
    case 159:                     // --258-- CAS[          159--( isHex(c) )-->160 ]
      { 
        if  ( isHex(c) )        // --258-- CAS[          159--( isHex(c) )-->160 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 160; 
        }
                                        // --259--
        else if  ( c == '\n' )        // --259-- CAS[          159--( c == '\n' )-->255 ]
        {
          
          {
              pwms = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalPWMOutput(data);
              if ( debug & 1) {
                  printDebug_cdpwm();
              }
          }
           
          state = 255; 
        }
                                        // --260--
        else {
          state = 256;                // --260-- ERROR 
        }
      } 
    break; 
    case 160:                     // --260-- CAS[          160--( isHex(c) )-->161 ]
      { 
        if  ( isHex(c) )        // --260-- CAS[          160--( isHex(c) )-->161 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 161; 
        }
                                        // --261--
        else if  ( c == '\n' )        // --261-- CAS[          160--( c == '\n' )-->255 ]
        {
          
          {
              pwms = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalPWMOutput(data);
              if ( debug & 1) {
                  printDebug_cdpwm();
              }
          }
           
          state = 255; 
        }
                                        // --262--
        else {
          state = 256;                // --262-- ERROR 
        }
      } 
    break; 
    case 161:                     // --262-- CAS[          161--( isHex(c) )-->162 ]
      { 
        if  ( isHex(c) )        // --262-- CAS[          161--( isHex(c) )-->162 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 162; 
        }
                                        // --263--
        else if  ( c == '\n' )        // --263-- CAS[          161--( c == '\n' )-->255 ]
        {
          
          {
              pwms = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalPWMOutput(data);
              if ( debug & 1) {
                  printDebug_cdpwm();
              }
          }
           
          state = 255; 
        }
                                        // --264--
        else {
          state = 256;                // --264-- ERROR 
        }
      } 
    break; 
    case 162:                     // --264-- CAS[          162--( isHex(c) )-->163 ]
      { 
        if  ( isHex(c) )        // --264-- CAS[          162--( isHex(c) )-->163 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 163; 
        }
                                        // --265--
        else if  ( c == '\n' )        // --265-- CAS[          162--( c == '\n' )-->255 ]
        {
          
          {
              pwms = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalPWMOutput(data);
              if ( debug & 1) {
                  printDebug_cdpwm();
              }
          }
           
          state = 255; 
        }
                                        // --266--
        else {
          state = 256;                // --266-- ERROR 
        }
      } 
    break; 
    case 163:                     // --266-- CAS[          163--( c == '\n' )-->255 ]
      { 
        if  ( c == '\n' )        // --266-- CAS[          163--( c == '\n' )-->255 ]
        {
          
          {
              pwms = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalPWMOutput(data);
              if ( debug & 1) {
                  printDebug_cdpwm();
              }
          }
           
          state = 255; 
        }
                                        // --267--
        else {
          state = 256;                // --267-- ERROR 
        }
      } 
    break; 
    case 164:                     // --267-- CAS[          164--( c == 'e' )-->165 ]
      { 
        if  ( c == 'e' )        // --267-- CAS[          164--( c == 'e' )-->165 ]
        {
           
          state = 165; 
        }
                                        // --268--
        else {
          state = 256;                // --268-- ERROR 
        }
      } 
    break; 
    case 165:                     // --268-- CAS[          165--( c == 'r' )-->166 ]
      { 
        if  ( c == 'r' )        // --268-- CAS[          165--( c == 'r' )-->166 ]
        {
           
          state = 166; 
        }
                                        // --269--
        else {
          state = 256;                // --269-- ERROR 
        }
      } 
    break; 
    case 166:                     // --269-- CAS[          166--( c == 'v' )-->167 ]
      { 
        if  ( c == 'v' )        // --269-- CAS[          166--( c == 'v' )-->167 ]
        {
           
          state = 167; 
        }
                                        // --270--
        else {
          state = 256;                // --270-- ERROR 
        }
      } 
    break; 
    case 167:                     // --270-- CAS[          167--( c == 'o' )-->168 ]
      { 
        if  ( c == 'o' )        // --270-- CAS[          167--( c == 'o' )-->168 ]
        {
           
          state = 168; 
        }
                                        // --271--
        else {
          state = 256;                // --271-- ERROR 
        }
      } 
    break; 
    case 168:                     // --271-- CAS[          168--( c == ':' )-->169 ]
      { 
        if  ( c == ':' )        // --271-- CAS[          168--( c == ':' )-->169 ]
        {
           
          state = 169; 
        }
                                        // --272--
        else {
          state = 256;                // --272-- ERROR 
        }
      } 
    break; 
    case 169:                     // --272-- CAS[          169--( isHex(c) )-->170 ]
      { 
        if  ( isHex(c) )        // --272-- CAS[          169--( isHex(c) )-->170 ]
        {
          data = valueHex(c); 
          state = 170; 
        }
                                        // --273--
        else {
          state = 256;                // --273-- ERROR 
        }
      } 
    break; 
    case 170:                     // --273-- CAS[          170--( isHex(c) )-->171 ]
      { 
        if  ( isHex(c) )        // --273-- CAS[          170--( isHex(c) )-->171 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 171; 
        }
                                        // --274--
        else if  ( c == '\n' )        // --274-- CAS[          170--( c == '\n' )-->255 ]
        {
           
          {
              servos = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalServoOutput(data);
              if ( debug & 1) {
                  printDebug_cdservo();
                  
              }
          }
           
          state = 255; 
        }
                                        // --275--
        else {
          state = 256;                // --275-- ERROR 
        }
      } 
    break; 
    case 171:                     // --275-- CAS[          171--( isHex(c) )-->172 ]
      { 
        if  ( isHex(c) )        // --275-- CAS[          171--( isHex(c) )-->172 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 172; 
        }
                                        // --276--
        else if  ( c == '\n' )        // --276-- CAS[          171--( c == '\n' )-->255 ]
        {
           
          {
              servos = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalServoOutput(data);
              if ( debug & 1) {
                  printDebug_cdservo();
                  
              }
          }
           
          state = 255; 
        }
                                        // --277--
        else {
          state = 256;                // --277-- ERROR 
        }
      } 
    break; 
    case 172:                     // --277-- CAS[          172--( isHex(c) )-->173 ]
      { 
        if  ( isHex(c) )        // --277-- CAS[          172--( isHex(c) )-->173 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 173; 
        }
                                        // --278--
        else if  ( c == '\n' )        // --278-- CAS[          172--( c == '\n' )-->255 ]
        {
           
          {
              servos = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalServoOutput(data);
              if ( debug & 1) {
                  printDebug_cdservo();
                  
              }
          }
           
          state = 255; 
        }
                                        // --279--
        else {
          state = 256;                // --279-- ERROR 
        }
      } 
    break; 
    case 173:                     // --279-- CAS[          173--( isHex(c) )-->174 ]
      { 
        if  ( isHex(c) )        // --279-- CAS[          173--( isHex(c) )-->174 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 174; 
        }
                                        // --280--
        else if  ( c == '\n' )        // --280-- CAS[          173--( c == '\n' )-->255 ]
        {
           
          {
              servos = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalServoOutput(data);
              if ( debug & 1) {
                  printDebug_cdservo();
                  
              }
          }
           
          state = 255; 
        }
                                        // --281--
        else {
          state = 256;                // --281-- ERROR 
        }
      } 
    break; 
    case 174:                     // --281-- CAS[          174--( isHex(c) )-->175 ]
      { 
        if  ( isHex(c) )        // --281-- CAS[          174--( isHex(c) )-->175 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 175; 
        }
                                        // --282--
        else if  ( c == '\n' )        // --282-- CAS[          174--( c == '\n' )-->255 ]
        {
           
          {
              servos = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalServoOutput(data);
              if ( debug & 1) {
                  printDebug_cdservo();
                  
              }
          }
           
          state = 255; 
        }
                                        // --283--
        else {
          state = 256;                // --283-- ERROR 
        }
      } 
    break; 
    case 175:                     // --283-- CAS[          175--( isHex(c) )-->176 ]
      { 
        if  ( isHex(c) )        // --283-- CAS[          175--( isHex(c) )-->176 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 176; 
        }
                                        // --284--
        else if  ( c == '\n' )        // --284-- CAS[          175--( c == '\n' )-->255 ]
        {
           
          {
              servos = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalServoOutput(data);
              if ( debug & 1) {
                  printDebug_cdservo();
                  
              }
          }
           
          state = 255; 
        }
                                        // --285--
        else {
          state = 256;                // --285-- ERROR 
        }
      } 
    break; 
    case 176:                     // --285-- CAS[          176--( isHex(c) )-->177 ]
      { 
        if  ( isHex(c) )        // --285-- CAS[          176--( isHex(c) )-->177 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 177; 
        }
                                        // --286--
        else if  ( c == '\n' )        // --286-- CAS[          176--( c == '\n' )-->255 ]
        {
           
          {
              servos = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalServoOutput(data);
              if ( debug & 1) {
                  printDebug_cdservo();
                  
              }
          }
           
          state = 255; 
        }
                                        // --287--
        else {
          state = 256;                // --287-- ERROR 
        }
      } 
    break; 
    case 177:                     // --287-- CAS[          177--( c == '\n' )-->255 ]
      { 
        if  ( c == '\n' )        // --287-- CAS[          177--( c == '\n' )-->255 ]
        {
           
          {
              servos = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              setDigitalServoOutput(data);
              if ( debug & 1) {
                  printDebug_cdservo();
                  
              }
          }
           
          state = 255; 
        }
                                        // --288--
        else {
          state = 256;                // --288-- ERROR 
        }
      } 
    break; 
    case 178:                     // --288-- CAS[          178--( c == 'a' )-->179 ]
      { 
        if  ( c == 'a' )        // --288-- CAS[          178--( c == 'a' )-->179 ]
        {
           
          state = 179; 
        }
                                        // --289--
        else if  ( c == 'd' )        // --289-- CAS[          178--( c == 'd' )-->191 ]
        {
           
          state = 191; 
        }
                                        // --290--
        else {
          state = 256;                // --290-- ERROR 
        }
      } 
    break; 
    case 179:                     // --290-- CAS[          179--( c == 'i' )-->180 ]
      { 
        if  ( c == 'i' )        // --290-- CAS[          179--( c == 'i' )-->180 ]
        {
           
          state = 180; 
        }
                                        // --291--
        else {
          state = 256;                // --291-- ERROR 
        }
      } 
    break; 
    case 180:                     // --291-- CAS[          180--( c == 'n' )-->181 ]
      { 
        if  ( c == 'n' )        // --291-- CAS[          180--( c == 'n' )-->181 ]
        {
           
          state = 181; 
        }
                                        // --292--
        else {
          state = 256;                // --292-- ERROR 
        }
      } 
    break; 
    case 181:                     // --292-- CAS[          181--( c == ':' )-->182 ]
      { 
        if  ( c == ':' )        // --292-- CAS[          181--( c == ':' )-->182 ]
        {
           
          state = 182; 
        }
                                        // --293--
        else {
          state = 256;                // --293-- ERROR 
        }
      } 
    break; 
    case 182:                     // --293-- CAS[          182--( isHex(c) )-->183 ]
      { 
        if  ( isHex(c) )        // --293-- CAS[          182--( isHex(c) )-->183 ]
        {
          data = valueHex(c); 
          state = 183; 
        }
                                        // --294--
        else {
          state = 256;                // --294-- ERROR 
        }
      } 
    break; 
    case 183:                     // --294-- CAS[          183--( isHex(c) )-->184 ]
      { 
        if  ( isHex(c) )        // --294-- CAS[          183--( isHex(c) )-->184 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 184; 
        }
                                        // --295--
        else if  ( c == '\n' )        // --295-- CAS[          183--( c == '\n' )-->255 ]
        {
          
          { 
              analogAnalogInputs = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_caain();
              }
          }
           
          state = 255; 
        }
                                        // --296--
        else {
          state = 256;                // --296-- ERROR 
        }
      } 
    break; 
    case 184:                     // --296-- CAS[          184--( isHex(c) )-->185 ]
      { 
        if  ( isHex(c) )        // --296-- CAS[          184--( isHex(c) )-->185 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 185; 
        }
                                        // --297--
        else if  ( c == '\n' )        // --297-- CAS[          184--( c == '\n' )-->255 ]
        {
          
          { 
              analogAnalogInputs = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_caain();
              }
          }
           
          state = 255; 
        }
                                        // --298--
        else {
          state = 256;                // --298-- ERROR 
        }
      } 
    break; 
    case 185:                     // --298-- CAS[          185--( isHex(c) )-->186 ]
      { 
        if  ( isHex(c) )        // --298-- CAS[          185--( isHex(c) )-->186 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 186; 
        }
                                        // --299--
        else if  ( c == '\n' )        // --299-- CAS[          185--( c == '\n' )-->255 ]
        {
          
          { 
              analogAnalogInputs = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_caain();
              }
          }
           
          state = 255; 
        }
                                        // --300--
        else {
          state = 256;                // --300-- ERROR 
        }
      } 
    break; 
    case 186:                     // --300-- CAS[          186--( isHex(c) )-->187 ]
      { 
        if  ( isHex(c) )        // --300-- CAS[          186--( isHex(c) )-->187 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 187; 
        }
                                        // --301--
        else if  ( c == '\n' )        // --301-- CAS[          186--( c == '\n' )-->255 ]
        {
          
          { 
              analogAnalogInputs = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_caain();
              }
          }
           
          state = 255; 
        }
                                        // --302--
        else {
          state = 256;                // --302-- ERROR 
        }
      } 
    break; 
    case 187:                     // --302-- CAS[          187--( isHex(c) )-->188 ]
      { 
        if  ( isHex(c) )        // --302-- CAS[          187--( isHex(c) )-->188 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 188; 
        }
                                        // --303--
        else if  ( c == '\n' )        // --303-- CAS[          187--( c == '\n' )-->255 ]
        {
          
          { 
              analogAnalogInputs = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_caain();
              }
          }
           
          state = 255; 
        }
                                        // --304--
        else {
          state = 256;                // --304-- ERROR 
        }
      } 
    break; 
    case 188:                     // --304-- CAS[          188--( isHex(c) )-->189 ]
      { 
        if  ( isHex(c) )        // --304-- CAS[          188--( isHex(c) )-->189 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 189; 
        }
                                        // --305--
        else if  ( c == '\n' )        // --305-- CAS[          188--( c == '\n' )-->255 ]
        {
          
          { 
              analogAnalogInputs = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_caain();
              }
          }
           
          state = 255; 
        }
                                        // --306--
        else {
          state = 256;                // --306-- ERROR 
        }
      } 
    break; 
    case 189:                     // --306-- CAS[          189--( isHex(c) )-->190 ]
      { 
        if  ( isHex(c) )        // --306-- CAS[          189--( isHex(c) )-->190 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 190; 
        }
                                        // --307--
        else if  ( c == '\n' )        // --307-- CAS[          189--( c == '\n' )-->255 ]
        {
          
          { 
              analogAnalogInputs = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_caain();
              }
          }
           
          state = 255; 
        }
                                        // --308--
        else {
          state = 256;                // --308-- ERROR 
        }
      } 
    break; 
    case 190:                     // --308-- CAS[          190--( c == '\n' )-->255 ]
      { 
        if  ( c == '\n' )        // --308-- CAS[          190--( c == '\n' )-->255 ]
        {
          
          { 
              analogAnalogInputs = data;
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_caain();
              }
          }
           
          state = 255; 
        }
                                        // --309--
        else {
          state = 256;                // --309-- ERROR 
        }
      } 
    break; 
    case 191:                     // --309-- CAS[          191--( c == 'i' )-->192 ]
      { 
        if  ( c == 'i' )        // --309-- CAS[          191--( c == 'i' )-->192 ]
        {
           
          state = 192; 
        }
                                        // --310--
        else if  ( c == 'o' )        // --310-- CAS[          191--( c == 'o' )-->203 ]
        {
           
          state = 203; 
        }
                                        // --311--
        else {
          state = 256;                // --311-- ERROR 
        }
      } 
    break; 
    case 192:                     // --311-- CAS[          192--( c == 'n' )-->193 ]
      { 
        if  ( c == 'n' )        // --311-- CAS[          192--( c == 'n' )-->193 ]
        {
           
          state = 193; 
        }
                                        // --312--
        else {
          state = 256;                // --312-- ERROR 
        }
      } 
    break; 
    case 193:                     // --312-- CAS[          193--( c == ':' )-->194 ]
      { 
        if  ( c == ':' )        // --312-- CAS[          193--( c == ':' )-->194 ]
        {
           
          state = 194; 
        }
                                        // --313--
        else if  ( c == 'p' )        // --313-- CAS[          193--( c == 'p' )-->215 ]
        {
           
          state = 215; 
        }
                                        // --314--
        else {
          state = 256;                // --314-- ERROR 
        }
      } 
    break; 
    case 194:                     // --314-- CAS[          194--( isHex(c) )-->195 ]
      { 
        if  ( isHex(c) )        // --314-- CAS[          194--( isHex(c) )-->195 ]
        {
          data = valueHex(c); 
          state = 195; 
        }
                                        // --315--
        else {
          state = 256;                // --315-- ERROR 
        }
      } 
    break; 
    case 195:                     // --315-- CAS[          195--( isHex(c) )-->196 ]
      { 
        if  ( isHex(c) )        // --315-- CAS[          195--( isHex(c) )-->196 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 196; 
        }
                                        // --316--
        else if  ( c == '\n' )        // --316-- CAS[          195--( c == '\n' )-->255 ]
        {
          
          { 
              setAnalogDigitalInput(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cadin();
              }
          }
           
          state = 255; 
        }
                                        // --317--
        else {
          state = 256;                // --317-- ERROR 
        }
      } 
    break; 
    case 196:                     // --317-- CAS[          196--( isHex(c) )-->197 ]
      { 
        if  ( isHex(c) )        // --317-- CAS[          196--( isHex(c) )-->197 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 197; 
        }
                                        // --318--
        else if  ( c == '\n' )        // --318-- CAS[          196--( c == '\n' )-->255 ]
        {
          
          { 
              setAnalogDigitalInput(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cadin();
              }
          }
           
          state = 255; 
        }
                                        // --319--
        else {
          state = 256;                // --319-- ERROR 
        }
      } 
    break; 
    case 197:                     // --319-- CAS[          197--( isHex(c) )-->198 ]
      { 
        if  ( isHex(c) )        // --319-- CAS[          197--( isHex(c) )-->198 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 198; 
        }
                                        // --320--
        else if  ( c == '\n' )        // --320-- CAS[          197--( c == '\n' )-->255 ]
        {
          
          { 
              setAnalogDigitalInput(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cadin();
              }
          }
           
          state = 255; 
        }
                                        // --321--
        else {
          state = 256;                // --321-- ERROR 
        }
      } 
    break; 
    case 198:                     // --321-- CAS[          198--( isHex(c) )-->199 ]
      { 
        if  ( isHex(c) )        // --321-- CAS[          198--( isHex(c) )-->199 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 199; 
        }
                                        // --322--
        else if  ( c == '\n' )        // --322-- CAS[          198--( c == '\n' )-->255 ]
        {
          
          { 
              setAnalogDigitalInput(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cadin();
              }
          }
           
          state = 255; 
        }
                                        // --323--
        else {
          state = 256;                // --323-- ERROR 
        }
      } 
    break; 
    case 199:                     // --323-- CAS[          199--( isHex(c) )-->200 ]
      { 
        if  ( isHex(c) )        // --323-- CAS[          199--( isHex(c) )-->200 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 200; 
        }
                                        // --324--
        else if  ( c == '\n' )        // --324-- CAS[          199--( c == '\n' )-->255 ]
        {
          
          { 
              setAnalogDigitalInput(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cadin();
              }
          }
           
          state = 255; 
        }
                                        // --325--
        else {
          state = 256;                // --325-- ERROR 
        }
      } 
    break; 
    case 200:                     // --325-- CAS[          200--( isHex(c) )-->201 ]
      { 
        if  ( isHex(c) )        // --325-- CAS[          200--( isHex(c) )-->201 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 201; 
        }
                                        // --326--
        else if  ( c == '\n' )        // --326-- CAS[          200--( c == '\n' )-->255 ]
        {
          
          { 
              setAnalogDigitalInput(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cadin();
              }
          }
           
          state = 255; 
        }
                                        // --327--
        else {
          state = 256;                // --327-- ERROR 
        }
      } 
    break; 
    case 201:                     // --327-- CAS[          201--( isHex(c) )-->202 ]
      { 
        if  ( isHex(c) )        // --327-- CAS[          201--( isHex(c) )-->202 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 202; 
        }
                                        // --328--
        else if  ( c == '\n' )        // --328-- CAS[          201--( c == '\n' )-->255 ]
        {
          
          { 
              setAnalogDigitalInput(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cadin();
              }
          }
           
          state = 255; 
        }
                                        // --329--
        else {
          state = 256;                // --329-- ERROR 
        }
      } 
    break; 
    case 202:                     // --329-- CAS[          202--( c == '\n' )-->255 ]
      { 
        if  ( c == '\n' )        // --329-- CAS[          202--( c == '\n' )-->255 ]
        {
          
          { 
              setAnalogDigitalInput(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cadin();
              }
          }
           
          state = 255; 
        }
                                        // --330--
        else {
          state = 256;                // --330-- ERROR 
        }
      } 
    break; 
    case 203:                     // --330-- CAS[          203--( c == 'u' )-->204 ]
      { 
        if  ( c == 'u' )        // --330-- CAS[          203--( c == 'u' )-->204 ]
        {
           
          state = 204; 
        }
                                        // --331--
        else {
          state = 256;                // --331-- ERROR 
        }
      } 
    break; 
    case 204:                     // --331-- CAS[          204--( c == 't' )-->205 ]
      { 
        if  ( c == 't' )        // --331-- CAS[          204--( c == 't' )-->205 ]
        {
           
          state = 205; 
        }
                                        // --332--
        else {
          state = 256;                // --332-- ERROR 
        }
      } 
    break; 
    case 205:                     // --332-- CAS[          205--( c == ':' )-->206 ]
      { 
        if  ( c == ':' )        // --332-- CAS[          205--( c == ':' )-->206 ]
        {
           
          state = 206; 
        }
                                        // --333--
        else {
          state = 256;                // --333-- ERROR 
        }
      } 
    break; 
    case 206:                     // --333-- CAS[          206--( isHex(c) )-->207 ]
      { 
        if  ( isHex(c) )        // --333-- CAS[          206--( isHex(c) )-->207 ]
        {
          data = valueHex(c); 
          state = 207; 
        }
                                        // --334--
        else {
          state = 256;                // --334-- ERROR 
        }
      } 
    break; 
    case 207:                     // --334-- CAS[          207--( isHex(c) )-->208 ]
      { 
        if  ( isHex(c) )        // --334-- CAS[          207--( isHex(c) )-->208 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 208; 
        }
                                        // --335--
        else if  ( c == '\n' )        // --335-- CAS[          207--( c == '\n' )-->255 ]
        {
          
          { 
              setAnalogDigitalOutput(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cadout();
              }
          }
           
          state = 255; 
        }
                                        // --336--
        else {
          state = 256;                // --336-- ERROR 
        }
      } 
    break; 
    case 208:                     // --336-- CAS[          208--( isHex(c) )-->209 ]
      { 
        if  ( isHex(c) )        // --336-- CAS[          208--( isHex(c) )-->209 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 209; 
        }
                                        // --337--
        else if  ( c == '\n' )        // --337-- CAS[          208--( c == '\n' )-->255 ]
        {
          
          { 
              setAnalogDigitalOutput(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cadout();
              }
          }
           
          state = 255; 
        }
                                        // --338--
        else {
          state = 256;                // --338-- ERROR 
        }
      } 
    break; 
    case 209:                     // --338-- CAS[          209--( isHex(c) )-->210 ]
      { 
        if  ( isHex(c) )        // --338-- CAS[          209--( isHex(c) )-->210 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 210; 
        }
                                        // --339--
        else if  ( c == '\n' )        // --339-- CAS[          209--( c == '\n' )-->255 ]
        {
          
          { 
              setAnalogDigitalOutput(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cadout();
              }
          }
           
          state = 255; 
        }
                                        // --340--
        else {
          state = 256;                // --340-- ERROR 
        }
      } 
    break; 
    case 210:                     // --340-- CAS[          210--( isHex(c) )-->211 ]
      { 
        if  ( isHex(c) )        // --340-- CAS[          210--( isHex(c) )-->211 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 211; 
        }
                                        // --341--
        else if  ( c == '\n' )        // --341-- CAS[          210--( c == '\n' )-->255 ]
        {
          
          { 
              setAnalogDigitalOutput(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cadout();
              }
          }
           
          state = 255; 
        }
                                        // --342--
        else {
          state = 256;                // --342-- ERROR 
        }
      } 
    break; 
    case 211:                     // --342-- CAS[          211--( isHex(c) )-->212 ]
      { 
        if  ( isHex(c) )        // --342-- CAS[          211--( isHex(c) )-->212 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 212; 
        }
                                        // --343--
        else if  ( c == '\n' )        // --343-- CAS[          211--( c == '\n' )-->255 ]
        {
          
          { 
              setAnalogDigitalOutput(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cadout();
              }
          }
           
          state = 255; 
        }
                                        // --344--
        else {
          state = 256;                // --344-- ERROR 
        }
      } 
    break; 
    case 212:                     // --344-- CAS[          212--( isHex(c) )-->213 ]
      { 
        if  ( isHex(c) )        // --344-- CAS[          212--( isHex(c) )-->213 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 213; 
        }
                                        // --345--
        else if  ( c == '\n' )        // --345-- CAS[          212--( c == '\n' )-->255 ]
        {
          
          { 
              setAnalogDigitalOutput(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cadout();
              }
          }
           
          state = 255; 
        }
                                        // --346--
        else {
          state = 256;                // --346-- ERROR 
        }
      } 
    break; 
    case 213:                     // --346-- CAS[          213--( isHex(c) )-->214 ]
      { 
        if  ( isHex(c) )        // --346-- CAS[          213--( isHex(c) )-->214 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 214; 
        }
                                        // --347--
        else if  ( c == '\n' )        // --347-- CAS[          213--( c == '\n' )-->255 ]
        {
          
          { 
              setAnalogDigitalOutput(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cadout();
              }
          }
           
          state = 255; 
        }
                                        // --348--
        else {
          state = 256;                // --348-- ERROR 
        }
      } 
    break; 
    case 214:                     // --348-- CAS[          214--( c == '\n' )-->255 ]
      { 
        if  ( c == '\n' )        // --348-- CAS[          214--( c == '\n' )-->255 ]
        {
          
          { 
              setAnalogDigitalOutput(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cadout();
              }
          }
           
          state = 255; 
        }
                                        // --349--
        else {
          state = 256;                // --349-- ERROR 
        }
      } 
    break; 
    case 215:                     // --349-- CAS[          215--( c == ':' )-->216 ]
      { 
        if  ( c == ':' )        // --349-- CAS[          215--( c == ':' )-->216 ]
        {
           
          state = 216; 
        }
                                        // --350--
        else {
          state = 256;                // --350-- ERROR 
        }
      } 
    break; 
    case 216:                     // --350-- CAS[          216--( isHex(c) )-->217 ]
      { 
        if  ( isHex(c) )        // --350-- CAS[          216--( isHex(c) )-->217 ]
        {
          data = valueHex(c); 
          state = 217; 
        }
                                        // --351--
        else {
          state = 256;                // --351-- ERROR 
        }
      } 
    break; 
    case 217:                     // --351-- CAS[          217--( isHex(c) )-->218 ]
      { 
        if  ( isHex(c) )        // --351-- CAS[          217--( isHex(c) )-->218 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 218; 
        }
                                        // --352--
        else if  ( c == '\n' )        // --352-- CAS[          217--( c == '\n' )-->255 ]
        {
          
          { 
              setAnalogDigitalInputPullup(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cadinp();
              }
          }
           
          state = 255; 
        }
                                        // --353--
        else {
          state = 256;                // --353-- ERROR 
        }
      } 
    break; 
    case 218:                     // --353-- CAS[          218--( isHex(c) )-->219 ]
      { 
        if  ( isHex(c) )        // --353-- CAS[          218--( isHex(c) )-->219 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 219; 
        }
                                        // --354--
        else if  ( c == '\n' )        // --354-- CAS[          218--( c == '\n' )-->255 ]
        {
          
          { 
              setAnalogDigitalInputPullup(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cadinp();
              }
          }
           
          state = 255; 
        }
                                        // --355--
        else {
          state = 256;                // --355-- ERROR 
        }
      } 
    break; 
    case 219:                     // --355-- CAS[          219--( isHex(c) )-->220 ]
      { 
        if  ( isHex(c) )        // --355-- CAS[          219--( isHex(c) )-->220 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 220; 
        }
                                        // --356--
        else if  ( c == '\n' )        // --356-- CAS[          219--( c == '\n' )-->255 ]
        {
          
          { 
              setAnalogDigitalInputPullup(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cadinp();
              }
          }
           
          state = 255; 
        }
                                        // --357--
        else {
          state = 256;                // --357-- ERROR 
        }
      } 
    break; 
    case 220:                     // --357-- CAS[          220--( isHex(c) )-->221 ]
      { 
        if  ( isHex(c) )        // --357-- CAS[          220--( isHex(c) )-->221 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 221; 
        }
                                        // --358--
        else if  ( c == '\n' )        // --358-- CAS[          220--( c == '\n' )-->255 ]
        {
          
          { 
              setAnalogDigitalInputPullup(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cadinp();
              }
          }
           
          state = 255; 
        }
                                        // --359--
        else {
          state = 256;                // --359-- ERROR 
        }
      } 
    break; 
    case 221:                     // --359-- CAS[          221--( isHex(c) )-->222 ]
      { 
        if  ( isHex(c) )        // --359-- CAS[          221--( isHex(c) )-->222 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 222; 
        }
                                        // --360--
        else if  ( c == '\n' )        // --360-- CAS[          221--( c == '\n' )-->255 ]
        {
          
          { 
              setAnalogDigitalInputPullup(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cadinp();
              }
          }
           
          state = 255; 
        }
                                        // --361--
        else {
          state = 256;                // --361-- ERROR 
        }
      } 
    break; 
    case 222:                     // --361-- CAS[          222--( isHex(c) )-->223 ]
      { 
        if  ( isHex(c) )        // --361-- CAS[          222--( isHex(c) )-->223 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 223; 
        }
                                        // --362--
        else if  ( c == '\n' )        // --362-- CAS[          222--( c == '\n' )-->255 ]
        {
          
          { 
              setAnalogDigitalInputPullup(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cadinp();
              }
          }
           
          state = 255; 
        }
                                        // --363--
        else {
          state = 256;                // --363-- ERROR 
        }
      } 
    break; 
    case 223:                     // --363-- CAS[          223--( isHex(c) )-->224 ]
      { 
        if  ( isHex(c) )        // --363-- CAS[          223--( isHex(c) )-->224 ]
        {
          data = (data<<4) | valueHex(c); 
          state = 224; 
        }
                                        // --364--
        else if  ( c == '\n' )        // --364-- CAS[          223--( c == '\n' )-->255 ]
        {
          
          { 
              setAnalogDigitalInputPullup(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cadinp();
              }
          }
           
          state = 255; 
        }
                                        // --365--
        else {
          state = 256;                // --365-- ERROR 
        }
      } 
    break; 
    case 224:                     // --365-- CAS[          224--( c == '\n' )-->255 ]
      { 
        if  ( c == '\n' )        // --365-- CAS[          224--( c == '\n' )-->255 ]
        {
          
          { 
              setAnalogDigitalInputPullup(data);
              stateMachine(STATEMACHINE_EVENT_CONFIG);
              if ( debug & 1) {
                  printDebug_cadinp();
              }
          }
           
          state = 255; 
        }
                                        // --366--
        else {
          state = 256;                // --366-- ERROR 
        }
      } 
    break; 
    case 225:                     // --366-- CAS[          225--( c == 'i' )-->226 ]
      { 
        if  ( c == 'i' )        // --366-- CAS[          225--( c == 'i' )-->226 ]
        {
           
          state = 226; 
        }
                                        // --367--
        else {
          state = 256;                // --367-- ERROR 
        }
      } 
    break; 
    case 226:                     // --367-- CAS[          226--( c == 's' )-->227 ]
      { 
        if  ( c == 's' )        // --367-- CAS[          226--( c == 's' )-->227 ]
        {
           
          state = 227; 
        }
                                        // --368--
        else {
          state = 256;                // --368-- ERROR 
        }
      } 
    break; 
    case 227:                     // --368-- CAS[          227--( c == 'c' )-->228 ]
      { 
        if  ( c == 'c' )        // --368-- CAS[          227--( c == 'c' )-->228 ]
        {
           
          state = 228; 
        }
                                        // --369--
        else {
          state = 256;                // --369-- ERROR 
        }
      } 
    break; 
    case 228:                     // --369-- CAS[          228--( c == 'o' )-->229 ]
      { 
        if  ( c == 'o' )        // --369-- CAS[          228--( c == 'o' )-->229 ]
        {
           
          state = 229; 
        }
                                        // --370--
        else {
          state = 256;                // --370-- ERROR 
        }
      } 
    break; 
    case 229:                     // --370-- CAS[          229--( c == 'n' )-->230 ]
      { 
        if  ( c == 'n' )        // --370-- CAS[          229--( c == 'n' )-->230 ]
        {
           
          state = 230; 
        }
                                        // --371--
        else {
          state = 256;                // --371-- ERROR 
        }
      } 
    break; 
    case 230:                     // --371-- CAS[          230--( c == 'n' )-->231 ]
      { 
        if  ( c == 'n' )        // --371-- CAS[          230--( c == 'n' )-->231 ]
        {
           
          state = 231; 
        }
                                        // --372--
        else {
          state = 256;                // --372-- ERROR 
        }
      } 
    break; 
    case 231:                     // --372-- CAS[          231--( c == 'e' )-->232 ]
      { 
        if  ( c == 'e' )        // --372-- CAS[          231--( c == 'e' )-->232 ]
        {
           
          state = 232; 
        }
                                        // --373--
        else {
          state = 256;                // --373-- ERROR 
        }
      } 
    break; 
    case 232:                     // --373-- CAS[          232--( c == 'c' )-->233 ]
      { 
        if  ( c == 'c' )        // --373-- CAS[          232--( c == 'c' )-->233 ]
        {
           
          state = 233; 
        }
                                        // --374--
        else {
          state = 256;                // --374-- ERROR 
        }
      } 
    break; 
    case 233:                     // --374-- CAS[          233--( c == 't' )-->234 ]
      { 
        if  ( c == 't' )        // --374-- CAS[          233--( c == 't' )-->234 ]
        {
           
          state = 234; 
        }
                                        // --375--
        else {
          state = 256;                // --375-- ERROR 
        }
      } 
    break; 
    case 234:                     // --375-- CAS[          234--( c == '\n' )-->255 ]
      { 
        if  ( c == '\n' )        // --375-- CAS[          234--( c == '\n' )-->255 ]
        {
          
          { 
              stateMachine(STATEMACHINE_EVENT_DISCONNECT);
              Serial.println( F("disconnect"));
          }
           
          state = 255; 
        }
                                        // --376--
        else {
          state = 256;                // --376-- ERROR 
        }
      } 
    break; 
    case 235:                     // --376-- CAS[          235--( c == 'e' )-->236 ]
      { 
        if  ( c == 'e' )        // --376-- CAS[          235--( c == 'e' )-->236 ]
        {
           
          state = 236; 
        }
                                        // --377--
        else {
          state = 256;                // --377-- ERROR 
        }
      } 
    break; 
    case 236:                     // --377-- CAS[          236--( c == 'r' )-->237 ]
      { 
        if  ( c == 'r' )        // --377-- CAS[          236--( c == 'r' )-->237 ]
        {
           
          state = 237; 
        }
                                        // --378--
        else {
          state = 256;                // --378-- ERROR 
        }
      } 
    break; 
    case 237:                     // --378-- CAS[          237--( c == 's' )-->238 ]
      { 
        if  ( c == 's' )        // --378-- CAS[          237--( c == 's' )-->238 ]
        {
           
          state = 238; 
        }
                                        // --379--
        else {
          state = 256;                // --379-- ERROR 
        }
      } 
    break; 
    case 238:                     // --379-- CAS[          238--( c == 'i' )-->239 ]
      { 
        if  ( c == 'i' )        // --379-- CAS[          238--( c == 'i' )-->239 ]
        {
           
          state = 239; 
        }
                                        // --380--
        else {
          state = 256;                // --380-- ERROR 
        }
      } 
    break; 
    case 239:                     // --380-- CAS[          239--( c == 'o' )-->240 ]
      { 
        if  ( c == 'o' )        // --380-- CAS[          239--( c == 'o' )-->240 ]
        {
           
          state = 240; 
        }
                                        // --381--
        else {
          state = 256;                // --381-- ERROR 
        }
      } 
    break; 
    case 240:                     // --381-- CAS[          240--( c == 'n' )-->241 ]
      { 
        if  ( c == 'n' )        // --381-- CAS[          240--( c == 'n' )-->241 ]
        {
           
          state = 241; 
        }
                                        // --382--
        else {
          state = 256;                // --382-- ERROR 
        }
      } 
    break; 
    case 241:                     // --382-- CAS[          241--( c == ':' )-->242 ]
      { 
        if  ( c == ':' )        // --382-- CAS[          241--( c == ':' )-->242 ]
        {
           
          state = 242; 
        }
                                        // --383--
        else {
          state = 256;                // --383-- ERROR 
        }
      } 
    break; 
    case 242:                     // --383-- CAS[          242--( c == '\n' )-->255 ]
      { 
        if  ( c == '\n' )        // --383-- CAS[          242--( c == '\n' )-->255 ]
        {
          
          {
              Serial.print( F("v:"));
              Serial.println(version);
          }
           
          state = 255; 
        }
                                        // --384--
        else {
          state = 256;                // --384-- ERROR 
        }
      } 
    break; 
    case 243:                     // --384-- CAS[          243--( c == 'e' )-->244 ]
      { 
        if  ( c == 'e' )        // --384-- CAS[          243--( c == 'e' )-->244 ]
        {
           
          state = 244; 
        }
                                        // --385--
        else {
          state = 256;                // --385-- ERROR 
        }
      } 
    break; 
    case 244:                     // --385-- CAS[          244--( c == 'r' )-->245 ]
      { 
        if  ( c == 'r' )        // --385-- CAS[          244--( c == 'r' )-->245 ]
        {
           
          state = 245; 
        }
                                        // --386--
        else {
          state = 256;                // --386-- ERROR 
        }
      } 
    break; 
    case 245:                     // --386-- CAS[          245--( c == 's' )-->246 ]
      { 
        if  ( c == 's' )        // --386-- CAS[          245--( c == 's' )-->246 ]
        {
           
          state = 246; 
        }
                                        // --387--
        else {
          state = 256;                // --387-- ERROR 
        }
      } 
    break; 
    case 246:                     // --387-- CAS[          246--( c == 'i' )-->247 ]
      { 
        if  ( c == 'i' )        // --387-- CAS[          246--( c == 'i' )-->247 ]
        {
           
          state = 247; 
        }
                                        // --388--
        else {
          state = 256;                // --388-- ERROR 
        }
      } 
    break; 
    case 247:                     // --388-- CAS[          247--( c == 'o' )-->248 ]
      { 
        if  ( c == 'o' )        // --388-- CAS[          247--( c == 'o' )-->248 ]
        {
           
          state = 248; 
        }
                                        // --389--
        else {
          state = 256;                // --389-- ERROR 
        }
      } 
    break; 
    case 248:                     // --389-- CAS[          248--( c == 'n' )-->249 ]
      { 
        if  ( c == 'n' )        // --389-- CAS[          248--( c == 'n' )-->249 ]
        {
           
          state = 249; 
        }
                                        // --390--
        else {
          state = 256;                // --390-- ERROR 
        }
      } 
    break; 
    case 249:                     // --390-- CAS[          249--( c == '?' )-->250 ]
      { 
        if  ( c == '?' )        // --390-- CAS[          249--( c == '?' )-->250 ]
        {
           
          state = 250; 
        }
                                        // --391--
        else {
          state = 256;                // --391-- ERROR 
        }
      } 
    break; 
    case 250:                     // --391-- CAS[          250--( c == '\n' )-->255 ]
      { 
        if  ( c == '\n' )        // --391-- CAS[          250--( c == '\n' )-->255 ]
        {
          
          {
              Serial.print( F("v:"));
              Serial.println(version);
          }
           
          state = 255; 
        }
                                        // --392--
        else {
          state = 256;                // --392-- ERROR 
        }
      } 
    break; 
    case 251:                     // --392-- CAS[          251--( c == 'r' )-->252 ]
      { 
        if  ( c == 'r' )        // --392-- CAS[          251--( c == 'r' )-->252 ]
        {
           
          state = 252; 
        }
                                        // --393--
        else {
          state = 256;                // --393-- ERROR 
        }
      } 
    break; 
    case 252:                     // --393-- CAS[          252--( c == 'r' )-->253 ]
      { 
        if  ( c == 'r' )        // --393-- CAS[          252--( c == 'r' )-->253 ]
        {
           
          state = 253; 
        }
                                        // --394--
        else {
          state = 256;                // --394-- ERROR 
        }
      } 
    break; 
    case 253:                     // --394-- CAS[          253--( c == '?' )-->254 ]
      { 
        if  ( c == '?' )        // --394-- CAS[          253--( c == '?' )-->254 ]
        {
           
          state = 254; 
        }
                                        // --395--
        else {
          state = 256;                // --395-- ERROR 
        }
      } 
    break; 
    case 254:                     // --395-- CAS[          254--( c == '\n' )-->255 ]
      { 
        if  ( c == '\n' )        // --395-- CAS[          254--( c == '\n' )-->255 ]
        {
          
          {
              Serial.print( F("e:"));
              Serial.println(errorCount);
          }
           
          state = 255; 
        }
        else {
          state = 256; // ERROR 
        }
      } 
    break; 
} // end switch state
  if ( state == 255) 
    {
      state = 0;
    }
    if ( state == 256) 
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

