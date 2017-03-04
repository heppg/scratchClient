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

    version 2017-03-04 remove redundant commands.
    version 2017-02-27 added counter function. Refactoring of command parser, sending of values.
    version 2017-02-18 small comment changes, added 'empty' ident set command.
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

#if not (defined(__AVR_ATmega328P__) )
#error This program works only on ATmega328, UNO or NANO boards
#endif
// ------------------------------------------------------------------------
// Customizing
//
// the code allows patching with custom initializer
// look for
//    - CUSTOM_VERSION
//    - CUSTOM_INITIALIZING
// replace code between START and END-tags.
//
// ------------------------------------------------------------------------

#include <avr/pgmspace.h>
#include <Servo.h>
#include <EEPROM.h>

// ----------------------------------------------------------------
// define HELP_COMPLETE to have complete command help available.
// comment out to reduce code size.
//
#define HELP_COMPLETE
// ----------------------------------------------------------------

char version[] = "arduinoUno, version 2017-03-27"
                 // CUSTOM_VERSION_START
                 ""
                 // CUSTOM_VERSION_END
                 ;

const char helpText[] PROGMEM =
#ifdef HELP_COMPLETE
  "arduino requests configuration with 'config?' on reset\n"
  "\n"
  "Configuration commands\n"
  " cdebug:<data>      debug settings, data are hex (0,1,2,3)\n"
  " cr:                dummy request, just get a newline and clean buffer\n"
  " cversion?          request version string\n"
  " cerr?              request error count for parser\n"
  " cident?            request idcode\n"
  " cident:<char16>    write idcode\n"
  " cident:            reset idcode\n"
  "\n"
  " char16 = [A-Za-z][A-Za-z0-9-_.]{1,15}\n"
  "\n"

  " cdin:<data>        digital inputs, data are hex\n"
  " cdinp:<data>       digital inputs, pullup enabled\n"
  " cdcnt:<data>       digital inputs for a counter\n"
  " cdcntp:<data>      digital inputs for a counter, pullup enabled\n"
  " cdout:<data>       digital outputs\n"
  " cdpwm:<data>       digital pwm\n"
  " cdservo:<data>     digital servo\n"

  " caain:<data>       analog line, analog input [a0..a5]\n"
  " cadin:<data>       analog line, digital input [a0..a5]\n"
  " cadinp:<data>      analog line, digital input, pullup [a0..a5]\n"
  " cadout:<data>      analog line, digital output\n"
  "\n"
  " data give bit patterns for IO pins, hex coded\n"
  "\n"
  "Commands to set values in arduino\n"
  " o<port>,<value>    write output\n"
  " oa<port>,<value>   write output analog line\n"
  " p<port>,<value>    write pwm\n"
  " s<port>,<value>    write servo\n"
  "\n"
  "Values reported from arduino\n"
  " v:<version>        report version\n"
  " ident:<char16>     report ident from EEPROM\n"
  " e:<errors>         report number of errors (decimal)\n"
  " a<port>,<value>    report analog input\n"
  " i<port>,<value>    report digital input\n"
  " c<port>,<hex>      report counter value\n"
  " ai<port>,<value>   report digital input on analog line\n"
  "\n"
  "Disconnect\n"
  " disconnect         stop processing, start request configuration\n"
#else
  "help text not available, see docs in scratchClient documentation"
#endif
  "\n";

#define FALSE (1==0)
#define TRUE (1==1)


// -------------------------------------------
// debug settings
// bit 0: command debug out  (debug)
// bit 1: input char out (slow, verbose)
// bit 2: switch off blink LED13, but use for toggle on each main loop (runtime analysis)
//
// debug is limited to 8Bit so far.
//
uint32_t debug = 0L;
//

// -------------------------------------------
// id command variables
#define N_ID 16

// add one char for terminating zero '\0'
char id[N_ID + 1];

// -------------------------------------------
//  EEPROM storage
typedef struct
{
  char id[N_ID + 1];

} EEPROM_STRUCTURE;

class Persist {
  private:

  public:
    Persist() {}  //constructor

    void writeId(char * id) {
      for ( uint8_t i = 0; i < N_ID + 1; i ++ ) {
        EEPROM.write(  offsetof( EEPROM_STRUCTURE, id) +  i, id[i]);
      }
    }
    char readId ( char * id) {

      for ( uint8_t i = 0; i < N_ID; i ++ )
      {
        id[i] = 0;
        id[i] = EEPROM.read( offsetof( EEPROM_STRUCTURE, id) + i );
      }
    }
};

// make an instance of the managing class called eeprom (in lower case!)
Persist eeprom;

// -------------------------------------------
// data aquisition control variables
uint8_t digital_input_count = 0;
uint8_t analog_input_count = 0;
uint16_t aval = 0;

// -------------------------------------------
//

#define COUNTER_STATE_START 0

#define COUNTER_STATE_LOW_0   1
#define COUNTER_STATE_LOW_1   2
#define COUNTER_STATE_LOW_2   3
#define COUNTER_STATE_LOW_3   4

#define COUNTER_STATE_HIGH_0  5
#define COUNTER_STATE_HIGH_1  6
#define COUNTER_STATE_HIGH_2  7
#define COUNTER_STATE_HIGH_3  8
#define COUNTER_STATE_HIGH_4  9

// -------------------------------------------

#define N_LASTRESULT 32
int lastResult[N_LASTRESULT];
unsigned int lastAnalogAnalogResult[N_LASTRESULT];
unsigned int lastAnalogDigitalResult[N_LASTRESULT];

uint32_t counter_cnt_prev [N_LASTRESULT];
uint32_t counter_cnt [N_LASTRESULT];
uint8_t state_cnt  [N_LASTRESULT];

// -------------------------------------------------

void setEEPROM() {
  eeprom.writeId( id);
}

void getEEPROM() {
  eeprom.readId( id);

  Serial.print( F("ident:") );
  Serial.println(id);
}

// -------------------------------------------------
// BLINK LED 13
//
uint16_t blinkCount = 0;
uint8_t blinkState = 10;

void blinkModeFast() {
  blinkState = 10;
}

void blinkModeSlow() {
  blinkState = 20;
}
// --------------------------------------
//
// is called each millisecond
//
void _blinking() {
  blinkCount ++;

  switch (blinkState) {
    case 10:
      {
        blinkState = 11;
        blinkCount = 0;
      }
      break;
    case 11:
      if ( blinkCount  >= 120 ) {
        blinkCount = 0;
        digitalWrite(13, LOW);
        blinkState = 12;
      }
      break;
    case 12:
      if ( blinkCount  >= 60 ) {
        blinkCount = 0;
        digitalWrite(13, HIGH);
        blinkState = 11;
      }
      break;

    case 20:  {
        blinkState = 21;
        blinkCount = 0;
      }
      break;

    case 21:
      if ( blinkCount  >= 1000 ) {
        blinkCount = 0;
        digitalWrite(13, LOW);
        blinkState = 22;
      }
      break;

    case 22:
      if ( blinkCount  >= 1000 ) {
        blinkCount = 0;
        digitalWrite(13, HIGH);
        blinkState = 21;
      }
      break;
  }
}

// -------------------------------------------------

Servo* servoObject [16];

// -------------------------------------------------
// Configuration State machine Events
#define STATEMACHINE_EVENT_START  10
#define STATEMACHINE_EVENT_CONFIG  20
#define STATEMACHINE_EVENT_DISCONNECT 30
#define STATEMACHINE_EVENT_TIMEOUT 40


// -- State machine States
// START
#define S0000    0

// User state
#define S0001    1

// request config
#define S1000    3

// operational
#define S2000    4

// -------------------------------------------------
/** Statemachine state handling */
uint8_t STATEMACHINE_waitState = S0000;

uint16_t nextTimeoutEvent = 0L;
boolean  timeoutEnable = FALSE;

uint16_t timeoutMillis = 0L;

void _timeout() {
  if ( ! timeoutEnable)
    return;

  timeoutMillis ++;
  if ( timeoutMillis > nextTimeoutEvent ) {
    timeoutEnable = FALSE;
    stateMachine(STATEMACHINE_EVENT_TIMEOUT);
  }
}

void setTimeout(uint16_t t) {
  timeoutMillis = 0;
  nextTimeoutEvent = t;
  timeoutEnable = TRUE;
}

// ---------------------------------------------------
// state entry and exit methods
//

// --- executed once after reset
//
void statemachine_S0000_exit() {
  timeoutEnable = FALSE;
}

void statemachine_S0001_entry() {
  //
  // the timeout is needed to leave this state by a timeout.
  //
  setTimeout( 500);
  //
  // ------ leave these tags CUSTOM_INITIALIZING in code
  // CUSTOM_INITIALIZING_START

  // if it is needed to set servo to a defined position after reset,
  // then for servo pin N
  // servoObject [N] = new Servo();
  // servoObject[N]->attach(N);
  // sample write with value 23
  // servoObject[N]->write( 23);

  // CUSTOM_INITIALIZING_END

}

void statemachine_S0001_exit() {
  timeoutEnable = FALSE;
}

// --- executed each 2000 ms until config commads are arriving

void statemachine_S1000_entry() {
  Serial.println( F("config?") );
  setTimeout(2000);
}
void statemachine_S1000_exit() {
  timeoutEnable = FALSE;
}

// --- executed once after a config command
void statemachine_S2000_entry() {
  timeoutEnable = FALSE;
  blinkModeSlow();
}
void statemachine_S2000_exit() {
  timeoutEnable = FALSE;
  blinkModeFast();
}

void stateMachine(uint8_t event) {

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
    case S0000:
      switch (event) {
        case STATEMACHINE_EVENT_START:

          STATEMACHINE_waitState = S0001;
          statemachine_S0000_exit();
          statemachine_S0001_entry();
          break;
      }
      break;

    // State 1 is an intermediate state which is usually kept for 0.5 sec.
    // This can be used to set up additional resources, e.g, servo
    //
    case S0001:
      switch (event) {
        case STATEMACHINE_EVENT_TIMEOUT:

          STATEMACHINE_waitState = S1000;
          statemachine_S0001_exit();
          statemachine_S1000_entry();
          break;

        case STATEMACHINE_EVENT_CONFIG:
          STATEMACHINE_waitState = S2000;
          statemachine_S0001_exit();
          statemachine_S2000_entry();

          break;
      }
      break;

    case S1000:
      switch (event) {
        case STATEMACHINE_EVENT_CONFIG:
          STATEMACHINE_waitState = S2000;
          statemachine_S1000_exit();
          statemachine_S2000_entry();

          break;

        case STATEMACHINE_EVENT_TIMEOUT:
          STATEMACHINE_waitState = S1000;
          statemachine_S1000_exit();
          statemachine_S1000_entry();
          break;
      }
      break;

    // Configuration data are available
    case S2000:
      switch (event) {
        case STATEMACHINE_EVENT_DISCONNECT:
          STATEMACHINE_waitState = S1000;
          statemachine_S2000_exit();
          statemachine_S1000_entry();
          break;
      }
      break;
  }
}

//
// these variables are an overlay of e.g. digitalINput with digitalPullupInput

unsigned long digitalInputs = 0L;
unsigned long digitalCounters = 0L;
unsigned long analogAnalogInputs = 0L;
unsigned long analogDigitalInputs = 0L;
unsigned long analogDigitalOutputs = 0L;

unsigned long pwms = 0L;
unsigned long servos = 0L;


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


void servoInit() {
  for ( uint8_t i = 0; i < 16; i ++ ) {
    servoObject [i] = NULL;
  }
}
void setDigitalServoOutput(long data) {

  for ( uint8_t i = 0; i < 16; i ++ ) {
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
void setDigitalCount(long data) {
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
  digitalCounters |= data;
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
void setDigitalCountPullup(long data) {
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
  digitalCounters |= data;

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

// ---------------------------------------------------
void handle_counter_debouncing( uint8_t pin) {
  uint8_t di = digitalRead( pin );
  uint8_t _state_cnt = state_cnt[pin];

  switch ( _state_cnt) {
    case COUNTER_STATE_START:
      if (di == 0)
        _state_cnt = COUNTER_STATE_LOW_0;
      break;

    case COUNTER_STATE_LOW_0:
      if (di == 0)
        _state_cnt = COUNTER_STATE_LOW_1;
      else
        _state_cnt = COUNTER_STATE_LOW_0;
      break;
    case COUNTER_STATE_LOW_1:
      if (di == 0)
        _state_cnt = COUNTER_STATE_LOW_2;
      else
        _state_cnt = COUNTER_STATE_LOW_0;
      break;
    case COUNTER_STATE_LOW_2:
      if (di == 0)
        _state_cnt = COUNTER_STATE_LOW_3;
      else
        _state_cnt = COUNTER_STATE_LOW_0;
      break;
    case COUNTER_STATE_LOW_3:
      if (di == 0)
        _state_cnt = COUNTER_STATE_LOW_3;
      else
        _state_cnt = COUNTER_STATE_HIGH_0;
      break;

    case COUNTER_STATE_HIGH_0:
      if (di == 0)
        _state_cnt = COUNTER_STATE_HIGH_0;
      else
        _state_cnt = COUNTER_STATE_HIGH_1;
      break;
    case COUNTER_STATE_HIGH_1:
      if (di == 0)
        _state_cnt = COUNTER_STATE_HIGH_0;
      else
        _state_cnt = COUNTER_STATE_HIGH_2;
      break;
    case COUNTER_STATE_HIGH_2:
      if (di == 0)
        _state_cnt = COUNTER_STATE_HIGH_0;
      else
        _state_cnt = COUNTER_STATE_HIGH_3;
      break;
    case COUNTER_STATE_HIGH_3:
      if (di == 0)
        _state_cnt = COUNTER_STATE_LOW_0;
      else {
        _state_cnt = COUNTER_STATE_HIGH_4;
        // -----------
        counter_cnt[pin] ++;
        // -----------
      }
      break;
    case COUNTER_STATE_HIGH_4:
      if (di == 0)
        _state_cnt = COUNTER_STATE_LOW_0;
      else
        _state_cnt = COUNTER_STATE_HIGH_4;
      break;
  } // end switch
  state_cnt[pin] = _state_cnt;
}

// ---------------------------------------------------
unsigned long errorCount = 0;

unsigned int value;
unsigned int port;
unsigned long data;
uint16_t state = 0;

void printDebug_cdebug() {
  Serial.print( F("cdebug=") );
  Serial.println(data, HEX);
}
void printDebug_cdin() {
  Serial.print( F("cdin=") );
  Serial.println(data, HEX);
}
void printDebug_cdinp() {
  Serial.print( F("cdinp=") );
  Serial.println(data, HEX);
}
void printDebug_cdcnt() {
  Serial.print( F("cdcnt=") );
  Serial.println(data, HEX);
}
void printDebug_cdcntp() {
  Serial.print( F("cdcntp=") );
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
// ---------------------------------------------------------
void setup() {
  for ( uint8_t i = 0; i < N_LASTRESULT; i ++ ) {
    lastResult[i] = 2;
    lastAnalogAnalogResult[i] = 0xffff;
    lastAnalogDigitalResult[i] = 0xffff;

    state_cnt  [i] = COUNTER_STATE_START;
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

// ---------------------------------------------------------
uint8_t fastblink = TRUE;

void _fastblink() {
  if (fastblink == HIGH) {
    digitalWrite(13, HIGH);
    fastblink = LOW;
  } else {
    digitalWrite(13, LOW);
    fastblink = HIGH;
  }
}

boolean milliSecond = FALSE;
unsigned long digitalPreviousMicros = 0L;
unsigned long currentMicros = 0L;

void loop() {
  //
  // there are quite a few things which need to be updated in millisecond range
  //
  milliSecond = FALSE;

  currentMicros = micros();
  //
  // generate 'events' each ms
  //
  if ((unsigned long)(currentMicros - digitalPreviousMicros) >= 1000L) {
    digitalPreviousMicros = currentMicros;
    milliSecond = TRUE;
  }

  if ( debug & ( 1 << 2) ) {
    _fastblink();
  }
  else {
    if ( milliSecond)
      _blinking();
  }
  
  if ( milliSecond)
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
  // generated code 2017-03-04 12:54:44
#define STATE_START    0
#define STATE_0123    1
#define STATE_0124    2
#define STATE_0125    3
#define STATE_0126    4
#define STATE_0127    5
#define STATE_0128    6
#define STATE_0129    7
#define STATE_0131    8
#define STATE_0132    9
#define STATE_0133   10
#define STATE_0134   11
#define STATE_0135   12
#define STATE_0136   13
#define STATE_0137   14
#define STATE_0138   15
#define STATE_0139   16
#define STATE_0140   17
#define STATE_0141   18
#define STATE_0142   19
#define STATE_0143   20
#define STATE_0144   21
#define STATE_0145   22
#define STATE_0146   23
#define STATE_0147   24
#define STATE_0148   25
#define STATE_0149   26
#define STATE_0150   27
#define STATE_0151   28
#define STATE_0152   29
#define STATE_0153   30
#define STATE_0154   31
#define STATE_0155   32
#define STATE_0156   33
#define STATE_0157   34
#define STATE_0158   35
#define STATE_0159   36
#define STATE_0160   37
#define STATE_0161   38
#define STATE_0162   39
#define STATE_0163   40
#define STATE_0164   41
#define STATE_0165   42
#define STATE_0166   43
#define STATE_0167   44
#define STATE_0168   45
#define STATE_0169   46
#define STATE_0170   47
#define STATE_0171   48
#define STATE_0172   49
#define STATE_0173   50
#define STATE_0174   51
#define STATE_0175   52
#define STATE_0176   53
#define STATE_0177   54
#define STATE_0178   55
#define STATE_0192   56
#define STATE_0194   57
#define STATE_0195   58
#define STATE_0196   59
#define STATE_0197   60
#define STATE_0198   61
#define STATE_0199   62
#define STATE_0200   63
#define STATE_0201   64
#define STATE_0202   65
#define STATE_0203   66
#define STATE_0204   67
#define STATE_0205   68
#define STATE_0206   69
#define STATE_0207   70
#define STATE_0210   71
#define STATE_0211   72
#define STATE_0212   73
#define STATE_0213   74
#define STATE_0214   75
#define STATE_0215   76
#define STATE_0216   77
#define STATE_0217   78
#define STATE_0218   79
#define STATE_0219   80
#define STATE_0220   81
#define STATE_0225   82
#define STATE_0226   83
#define STATE_0227   84
#define STATE_0228   85
#define STATE_0229   86
#define STATE_0230   87
#define STATE_0231   88
#define STATE_0232   89
#define STATE_0233   90
#define STATE_0234   91
#define STATE_0237   92
#define STATE_0238   93
#define STATE_0239   94
#define STATE_0240   95
#define STATE_0241   96
#define STATE_0242   97
#define STATE_0243   98
#define STATE_0244   99
#define STATE_0245  100
#define STATE_0246  101
#define STATE_0247  102
#define STATE_0248  103
#define STATE_0254  104
#define STATE_0255  105
#define STATE_0256  106
#define STATE_0257  107
#define STATE_0258  108
#define STATE_0259  109
#define STATE_0260  110
#define STATE_0261  111
#define STATE_0262  112
#define STATE_0263  113
#define STATE_0266  114
#define STATE_0267  115
#define STATE_0268  116
#define STATE_0269  117
#define STATE_0270  118
#define STATE_0271  119
#define STATE_0272  120
#define STATE_0273  121
#define STATE_0274  122
#define STATE_0275  123
#define STATE_0276  124
#define STATE_0277  125
#define STATE_0280  126
#define STATE_0281  127
#define STATE_0282  128
#define STATE_0283  129
#define STATE_0284  130
#define STATE_0285  131
#define STATE_0286  132
#define STATE_0287  133
#define STATE_0288  134
#define STATE_0289  135
#define STATE_0290  136
#define STATE_0291  137
#define STATE_0294  138
#define STATE_0295  139
#define STATE_0296  140
#define STATE_0297  141
#define STATE_0298  142
#define STATE_0299  143
#define STATE_0300  144
#define STATE_0301  145
#define STATE_0302  146
#define STATE_0303  147
#define STATE_0304  148
#define STATE_0305  149
#define STATE_0306  150
#define STATE_0307  151
#define STATE_0309  152
#define STATE_0310  153
#define STATE_0311  154
#define STATE_0312  155
#define STATE_0313  156
#define STATE_0314  157
#define STATE_0315  158
#define STATE_0316  159
#define STATE_0317  160
#define STATE_0318  161
#define STATE_0319  162
#define STATE_0320  163
#define STATE_0321  164
#define STATE_0324  165
#define STATE_0325  166
#define STATE_0326  167
#define STATE_0327  168
#define STATE_0328  169
#define STATE_0329  170
#define STATE_0330  171
#define STATE_0331  172
#define STATE_0332  173
#define STATE_0333  174
#define STATE_0334  175
#define STATE_0335  176
#define STATE_0339  177
#define STATE_0340  178
#define STATE_0341  179
#define STATE_0342  180
#define STATE_0343  181
#define STATE_0344  182
#define STATE_0345  183
#define STATE_0346  184
#define STATE_0347  185
#define STATE_0348  186
#define STATE_0349  187
#define STATE_0350  188
#define STATE_0356  189
#define STATE_0357  190
#define STATE_0358  191
#define STATE_0359  192
#define STATE_0360  193
#define STATE_0361  194
#define STATE_0362  195
#define STATE_0363  196
#define STATE_0364  197
#define STATE_0365  198
#define STATE_0366  199
#define STATE_0367  200
#define STATE_0368  201
#define STATE_0369  202
#define STATE_0370  203
#define STATE_0371  204
#define STATE_0372  205
#define STATE_0373  206
#define STATE_0374  207
#define STATE_0375  208
#define STATE_0377  209
#define STATE_0378  210
#define STATE_0379  211
#define STATE_0380  212
#define STATE_0381  213
#define STATE_0382  214
#define STATE_0383  215
#define STATE_0384  216
#define STATE_0386  217
#define STATE_0387  218
#define STATE_0388  219
#define STATE_0389  220
#define STATE_TERMINAL  221
#define STATE_ERROR  222
#define ACTION_0100  223
#define ACTION_0101  224
#define ACTION_0102  225
#define ACTION_0103  226
#define ACTION_0104  227
#define ACTION_0105  228
#define ACTION_0106  229
#define ACTION_0107  230
#define ACTION_0108  231
#define ACTION_0109  232
#define ACTION_0110  233
#define ACTION_0111  234
#define ACTION_0112  235
#define ACTION_0113  236
#define ACTION_0114  237
#define ACTION_0115  238
#define ACTION_0116  239
#define ACTION_0117  240
#define ACTION_0118  241
#define ACTION_0119  242
#define ACTION_0120  243
#define ACTION_0121  244
#define ACTION_0122  245
  switch( state) { 
    case STATE_START:                     // --0-- CAS[          STATE_START--( c == 'o' )-->STATE_0123 ]
      { 
        if  ( c == 'o' )        // --0-- CAS[          STATE_START--( c == 'o' )-->STATE_0123 ]
        {
          state = STATE_0123;   // nextState 
        }
        else if  ( c == 'p' )        // --1-- CAS[          STATE_START--( c == 'p' )-->STATE_0138 ]
        {
          state = STATE_0138;   // nextState 
        }
        else if  ( c == 's' )        // --2-- CAS[          STATE_START--( c == 's' )-->STATE_0145 ]
        {
          state = STATE_0145;   // nextState 
        }
        else if  ( c == 'h' )        // --3-- CAS[          STATE_START--( c == 'h' )-->STATE_0152 ]
        {
          state = STATE_0152;   // nextState 
        }
        else if  ( c == 'c' )        // --4-- CAS[          STATE_START--( c == 'c' )-->STATE_0156 ]
        {
          state = STATE_0156;   // nextState 
        }
        else if  ( c == 'd' )        // --5-- CAS[          STATE_START--( c == 'd' )-->STATE_0366 ]
        {
          state = STATE_0366;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --6-- ERROR 
        }
      } 
    break; 
    case STATE_0123:                     // --6-- CAS[          STATE_0123--( isDigit(c) )-->STATE_0124 ]
      { 
        if  ( isDigit(c) )        // --6-- CAS[          STATE_0123--( isDigit(c) )-->STATE_0124 ]
        {
          port = valueDecimal(c);;    // action 
          state = STATE_0124;   // nextState 
        }
        else if  ( c == 'a' )        // --7-- CAS[          STATE_0123--( c == 'a' )-->STATE_0131 ]
        {
          state = STATE_0131;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --8-- ERROR 
        }
      } 
    break; 
    case STATE_0124:                     // --8-- CAS[          STATE_0124--( isDigit(c) )-->STATE_0125 ]
      { 
        if  ( isDigit(c) )        // --8-- CAS[          STATE_0124--( isDigit(c) )-->STATE_0125 ]
        {
          port = port * 10 + valueDecimal(c);;    // action 
          state = STATE_0125;   // nextState 
        }
        else if  ( c == ',' )        // --9-- CAS[          STATE_0124--( c == ',' )-->STATE_0126 ]
        {
          state = STATE_0126;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --10-- ERROR 
        }
      } 
    break; 
    case STATE_0125:                     // --10-- CAS[          STATE_0125--( c == ',' )-->STATE_0126 ]
      { 
        if  ( c == ',' )        // --10-- CAS[          STATE_0125--( c == ',' )-->STATE_0126 ]
        {
          state = STATE_0126;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --11-- ERROR 
        }
      } 
    break; 
    case STATE_0126:                     // --11-- CAS[          STATE_0126--( isDigit(c) )-->STATE_0127 ]
      { 
        if  ( isDigit(c) )        // --11-- CAS[          STATE_0126--( isDigit(c) )-->STATE_0127 ]
        {
          value = valueDecimal(c);;    // action 
          state = STATE_0127;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --12-- ERROR 
        }
      } 
    break; 
    case STATE_0127:                     // --12-- CAS[          STATE_0127--( isDigit(c) )-->STATE_0128 ]
      { 
        if  ( isDigit(c) )        // --12-- CAS[          STATE_0127--( isDigit(c) )-->STATE_0128 ]
        {
          value = value * 10 + valueDecimal(c);;    // action 
          state = STATE_0128;   // nextState 
        }
        else if  ( c == '\n' )        // --13-- CAS[          STATE_0127--( c == '\n' )-->ACTION_0100 ]
        {
          state = ACTION_0100;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --14-- ERROR 
        }
      } 
    break; 
    case STATE_0128:                     // --14-- CAS[          STATE_0128--( isDigit(c) )-->STATE_0129 ]
      { 
        if  ( isDigit(c) )        // --14-- CAS[          STATE_0128--( isDigit(c) )-->STATE_0129 ]
        {
          value = value * 10 + valueDecimal(c);;    // action 
          state = STATE_0129;   // nextState 
        }
        else if  ( c == '\n' )        // --15-- CAS[          STATE_0128--( c == '\n' )-->ACTION_0100 ]
        {
          state = ACTION_0100;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --16-- ERROR 
        }
      } 
    break; 
    case STATE_0129:                     // --16-- CAS[          STATE_0129--( c == '\n' )-->ACTION_0100 ]
      { 
        if  ( c == '\n' )        // --16-- CAS[          STATE_0129--( c == '\n' )-->ACTION_0100 ]
        {
          state = ACTION_0100;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --17-- ERROR 
        }
      } 
    break; 
    case STATE_0131:                     // --17-- CAS[          STATE_0131--( isDigit(c) )-->STATE_0132 ]
      { 
        if  ( isDigit(c) )        // --17-- CAS[          STATE_0131--( isDigit(c) )-->STATE_0132 ]
        {
          port = valueDecimal(c);;    // action 
          state = STATE_0132;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --18-- ERROR 
        }
      } 
    break; 
    case STATE_0132:                     // --18-- CAS[          STATE_0132--( isDigit(c) )-->STATE_0133 ]
      { 
        if  ( isDigit(c) )        // --18-- CAS[          STATE_0132--( isDigit(c) )-->STATE_0133 ]
        {
          port = port * 10 + valueDecimal(c);;    // action 
          state = STATE_0133;   // nextState 
        }
        else if  ( c == ',' )        // --19-- CAS[          STATE_0132--( c == ',' )-->STATE_0134 ]
        {
          state = STATE_0134;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --20-- ERROR 
        }
      } 
    break; 
    case STATE_0133:                     // --20-- CAS[          STATE_0133--( c == ',' )-->STATE_0134 ]
      { 
        if  ( c == ',' )        // --20-- CAS[          STATE_0133--( c == ',' )-->STATE_0134 ]
        {
          state = STATE_0134;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --21-- ERROR 
        }
      } 
    break; 
    case STATE_0134:                     // --21-- CAS[          STATE_0134--( isDigit(c) )-->STATE_0135 ]
      { 
        if  ( isDigit(c) )        // --21-- CAS[          STATE_0134--( isDigit(c) )-->STATE_0135 ]
        {
          value = valueDecimal(c);;    // action 
          state = STATE_0135;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --22-- ERROR 
        }
      } 
    break; 
    case STATE_0135:                     // --22-- CAS[          STATE_0135--( isDigit(c) )-->STATE_0136 ]
      { 
        if  ( isDigit(c) )        // --22-- CAS[          STATE_0135--( isDigit(c) )-->STATE_0136 ]
        {
          value = value * 10 + valueDecimal(c);;    // action 
          state = STATE_0136;   // nextState 
        }
        else if  ( c == '\n' )        // --23-- CAS[          STATE_0135--( c == '\n' )-->ACTION_0101 ]
        {
          state = ACTION_0101;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --24-- ERROR 
        }
      } 
    break; 
    case STATE_0136:                     // --24-- CAS[          STATE_0136--( isDigit(c) )-->STATE_0137 ]
      { 
        if  ( isDigit(c) )        // --24-- CAS[          STATE_0136--( isDigit(c) )-->STATE_0137 ]
        {
          value = value * 10 + valueDecimal(c);;    // action 
          state = STATE_0137;   // nextState 
        }
        else if  ( c == '\n' )        // --25-- CAS[          STATE_0136--( c == '\n' )-->ACTION_0101 ]
        {
          state = ACTION_0101;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --26-- ERROR 
        }
      } 
    break; 
    case STATE_0137:                     // --26-- CAS[          STATE_0137--( c == '\n' )-->ACTION_0101 ]
      { 
        if  ( c == '\n' )        // --26-- CAS[          STATE_0137--( c == '\n' )-->ACTION_0101 ]
        {
          state = ACTION_0101;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --27-- ERROR 
        }
      } 
    break; 
    case STATE_0138:                     // --27-- CAS[          STATE_0138--( isDigit(c) )-->STATE_0139 ]
      { 
        if  ( isDigit(c) )        // --27-- CAS[          STATE_0138--( isDigit(c) )-->STATE_0139 ]
        {
          port = valueDecimal(c);;    // action 
          state = STATE_0139;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --28-- ERROR 
        }
      } 
    break; 
    case STATE_0139:                     // --28-- CAS[          STATE_0139--( isDigit(c) )-->STATE_0140 ]
      { 
        if  ( isDigit(c) )        // --28-- CAS[          STATE_0139--( isDigit(c) )-->STATE_0140 ]
        {
          port = port * 10 + valueDecimal(c);;    // action 
          state = STATE_0140;   // nextState 
        }
        else if  ( c == ',' )        // --29-- CAS[          STATE_0139--( c == ',' )-->STATE_0141 ]
        {
          state = STATE_0141;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --30-- ERROR 
        }
      } 
    break; 
    case STATE_0140:                     // --30-- CAS[          STATE_0140--( c == ',' )-->STATE_0141 ]
      { 
        if  ( c == ',' )        // --30-- CAS[          STATE_0140--( c == ',' )-->STATE_0141 ]
        {
          state = STATE_0141;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --31-- ERROR 
        }
      } 
    break; 
    case STATE_0141:                     // --31-- CAS[          STATE_0141--( isDigit(c) )-->STATE_0142 ]
      { 
        if  ( isDigit(c) )        // --31-- CAS[          STATE_0141--( isDigit(c) )-->STATE_0142 ]
        {
          value = valueDecimal(c);;    // action 
          state = STATE_0142;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --32-- ERROR 
        }
      } 
    break; 
    case STATE_0142:                     // --32-- CAS[          STATE_0142--( isDigit(c) )-->STATE_0143 ]
      { 
        if  ( isDigit(c) )        // --32-- CAS[          STATE_0142--( isDigit(c) )-->STATE_0143 ]
        {
          value = value * 10 + valueDecimal(c);;    // action 
          state = STATE_0143;   // nextState 
        }
        else if  ( c == '\n' )        // --33-- CAS[          STATE_0142--( c == '\n' )-->ACTION_0102 ]
        {
          state = ACTION_0102;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --34-- ERROR 
        }
      } 
    break; 
    case STATE_0143:                     // --34-- CAS[          STATE_0143--( isDigit(c) )-->STATE_0144 ]
      { 
        if  ( isDigit(c) )        // --34-- CAS[          STATE_0143--( isDigit(c) )-->STATE_0144 ]
        {
          value = value * 10 + valueDecimal(c);;    // action 
          state = STATE_0144;   // nextState 
        }
        else if  ( c == '\n' )        // --35-- CAS[          STATE_0143--( c == '\n' )-->ACTION_0102 ]
        {
          state = ACTION_0102;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --36-- ERROR 
        }
      } 
    break; 
    case STATE_0144:                     // --36-- CAS[          STATE_0144--( c == '\n' )-->ACTION_0102 ]
      { 
        if  ( c == '\n' )        // --36-- CAS[          STATE_0144--( c == '\n' )-->ACTION_0102 ]
        {
          state = ACTION_0102;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --37-- ERROR 
        }
      } 
    break; 
    case STATE_0145:                     // --37-- CAS[          STATE_0145--( isDigit(c) )-->STATE_0146 ]
      { 
        if  ( isDigit(c) )        // --37-- CAS[          STATE_0145--( isDigit(c) )-->STATE_0146 ]
        {
          port = valueDecimal(c);;    // action 
          state = STATE_0146;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --38-- ERROR 
        }
      } 
    break; 
    case STATE_0146:                     // --38-- CAS[          STATE_0146--( isDigit(c) )-->STATE_0147 ]
      { 
        if  ( isDigit(c) )        // --38-- CAS[          STATE_0146--( isDigit(c) )-->STATE_0147 ]
        {
          port = port * 10 + valueDecimal(c);;    // action 
          state = STATE_0147;   // nextState 
        }
        else if  ( c == ',' )        // --39-- CAS[          STATE_0146--( c == ',' )-->STATE_0148 ]
        {
          state = STATE_0148;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --40-- ERROR 
        }
      } 
    break; 
    case STATE_0147:                     // --40-- CAS[          STATE_0147--( c == ',' )-->STATE_0148 ]
      { 
        if  ( c == ',' )        // --40-- CAS[          STATE_0147--( c == ',' )-->STATE_0148 ]
        {
          state = STATE_0148;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --41-- ERROR 
        }
      } 
    break; 
    case STATE_0148:                     // --41-- CAS[          STATE_0148--( isDigit(c) )-->STATE_0149 ]
      { 
        if  ( isDigit(c) )        // --41-- CAS[          STATE_0148--( isDigit(c) )-->STATE_0149 ]
        {
          value = valueDecimal(c);;    // action 
          state = STATE_0149;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --42-- ERROR 
        }
      } 
    break; 
    case STATE_0149:                     // --42-- CAS[          STATE_0149--( isDigit(c) )-->STATE_0150 ]
      { 
        if  ( isDigit(c) )        // --42-- CAS[          STATE_0149--( isDigit(c) )-->STATE_0150 ]
        {
          value = value * 10 + valueDecimal(c);;    // action 
          state = STATE_0150;   // nextState 
        }
        else if  ( c == '\n' )        // --43-- CAS[          STATE_0149--( c == '\n' )-->ACTION_0103 ]
        {
          state = ACTION_0103;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --44-- ERROR 
        }
      } 
    break; 
    case STATE_0150:                     // --44-- CAS[          STATE_0150--( isDigit(c) )-->STATE_0151 ]
      { 
        if  ( isDigit(c) )        // --44-- CAS[          STATE_0150--( isDigit(c) )-->STATE_0151 ]
        {
          value = value * 10 + valueDecimal(c);;    // action 
          state = STATE_0151;   // nextState 
        }
        else if  ( c == '\n' )        // --45-- CAS[          STATE_0150--( c == '\n' )-->ACTION_0103 ]
        {
          state = ACTION_0103;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --46-- ERROR 
        }
      } 
    break; 
    case STATE_0151:                     // --46-- CAS[          STATE_0151--( c == '\n' )-->ACTION_0103 ]
      { 
        if  ( c == '\n' )        // --46-- CAS[          STATE_0151--( c == '\n' )-->ACTION_0103 ]
        {
          state = ACTION_0103;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --47-- ERROR 
        }
      } 
    break; 
    case STATE_0152:                     // --47-- CAS[          STATE_0152--( c == 'e' )-->STATE_0153 ]
      { 
        if  ( c == 'e' )        // --47-- CAS[          STATE_0152--( c == 'e' )-->STATE_0153 ]
        {
          state = STATE_0153;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --48-- ERROR 
        }
      } 
    break; 
    case STATE_0153:                     // --48-- CAS[          STATE_0153--( c == 'l' )-->STATE_0154 ]
      { 
        if  ( c == 'l' )        // --48-- CAS[          STATE_0153--( c == 'l' )-->STATE_0154 ]
        {
          state = STATE_0154;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --49-- ERROR 
        }
      } 
    break; 
    case STATE_0154:                     // --49-- CAS[          STATE_0154--( c == 'p' )-->STATE_0155 ]
      { 
        if  ( c == 'p' )        // --49-- CAS[          STATE_0154--( c == 'p' )-->STATE_0155 ]
        {
          state = STATE_0155;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --50-- ERROR 
        }
      } 
    break; 
    case STATE_0155:                     // --50-- CAS[          STATE_0155--( c == '\n' )-->ACTION_0104 ]
      { 
        if  ( c == '\n' )        // --50-- CAS[          STATE_0155--( c == '\n' )-->ACTION_0104 ]
        {
          state = ACTION_0104;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --51-- ERROR 
        }
      } 
    break; 
    case STATE_0156:                     // --51-- CAS[          STATE_0156--( c == 'i' )-->STATE_0157 ]
      { 
        if  ( c == 'i' )        // --51-- CAS[          STATE_0156--( c == 'i' )-->STATE_0157 ]
        {
          state = STATE_0157;   // nextState 
        }
        else if  ( c == 'd' )        // --52-- CAS[          STATE_0156--( c == 'd' )-->STATE_0194 ]
        {
          state = STATE_0194;   // nextState 
        }
        else if  ( c == 'a' )        // --53-- CAS[          STATE_0156--( c == 'a' )-->STATE_0309 ]
        {
          state = STATE_0309;   // nextState 
        }
        else if  ( c == 'v' )        // --54-- CAS[          STATE_0156--( c == 'v' )-->STATE_0377 ]
        {
          state = STATE_0377;   // nextState 
        }
        else if  ( c == 'e' )        // --55-- CAS[          STATE_0156--( c == 'e' )-->STATE_0386 ]
        {
          state = STATE_0386;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --56-- ERROR 
        }
      } 
    break; 
    case STATE_0157:                     // --56-- CAS[          STATE_0157--( c == 'd' )-->STATE_0158 ]
      { 
        if  ( c == 'd' )        // --56-- CAS[          STATE_0157--( c == 'd' )-->STATE_0158 ]
        {
          state = STATE_0158;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --57-- ERROR 
        }
      } 
    break; 
    case STATE_0158:                     // --57-- CAS[          STATE_0158--( c == 'e' )-->STATE_0159 ]
      { 
        if  ( c == 'e' )        // --57-- CAS[          STATE_0158--( c == 'e' )-->STATE_0159 ]
        {
          state = STATE_0159;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --58-- ERROR 
        }
      } 
    break; 
    case STATE_0159:                     // --58-- CAS[          STATE_0159--( c == 'n' )-->STATE_0160 ]
      { 
        if  ( c == 'n' )        // --58-- CAS[          STATE_0159--( c == 'n' )-->STATE_0160 ]
        {
          state = STATE_0160;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --59-- ERROR 
        }
      } 
    break; 
    case STATE_0160:                     // --59-- CAS[          STATE_0160--( c == 't' )-->STATE_0161 ]
      { 
        if  ( c == 't' )        // --59-- CAS[          STATE_0160--( c == 't' )-->STATE_0161 ]
        {
          state = STATE_0161;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --60-- ERROR 
        }
      } 
    break; 
    case STATE_0161:                     // --60-- CAS[          STATE_0161--( c == ':' )-->STATE_0162 ]
      { 
        if  ( c == ':' )        // --60-- CAS[          STATE_0161--( c == ':' )-->STATE_0162 ]
        {
          state = STATE_0162;   // nextState 
        }
        else if  ( c == '?' )        // --61-- CAS[          STATE_0161--( c == '?' )-->STATE_0192 ]
        {
          state = STATE_0192;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --62-- ERROR 
        }
      } 
    break; 
    case STATE_0162:                     // --62-- CAS[          STATE_0162--( isChar(c) )-->STATE_0163 ]
      { 
        if  ( isChar(c) )        // --62-- CAS[          STATE_0162--( isChar(c) )-->STATE_0163 ]
        {
          id[0] = c; id[0+1] = 0;;    // action 
          state = STATE_0163;   // nextState 
        }
        else if  ( c == '\n' )        // --63-- CAS[          STATE_0162--( c == '\n' )-->ACTION_0106 ]
        {
          state = ACTION_0106;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --64-- ERROR 
        }
      } 
    break; 
    case STATE_0163:                     // --64-- CAS[          STATE_0163--( isLabel(c) )-->STATE_0164 ]
      { 
        if  ( isLabel(c) )        // --64-- CAS[          STATE_0163--( isLabel(c) )-->STATE_0164 ]
        {
          id[1] = c; id[1+1] = 0;;    // action 
          state = STATE_0164;   // nextState 
        }
        else if  ( c == '\n' )        // --65-- CAS[          STATE_0163--( c == '\n' )-->ACTION_0105 ]
        {
          state = ACTION_0105;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --66-- ERROR 
        }
      } 
    break; 
    case STATE_0164:                     // --66-- CAS[          STATE_0164--( isLabel(c) )-->STATE_0165 ]
      { 
        if  ( isLabel(c) )        // --66-- CAS[          STATE_0164--( isLabel(c) )-->STATE_0165 ]
        {
          id[2] = c; id[2+1] = 0;;    // action 
          state = STATE_0165;   // nextState 
        }
        else if  ( c == '\n' )        // --67-- CAS[          STATE_0164--( c == '\n' )-->ACTION_0105 ]
        {
          state = ACTION_0105;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --68-- ERROR 
        }
      } 
    break; 
    case STATE_0165:                     // --68-- CAS[          STATE_0165--( isLabel(c) )-->STATE_0166 ]
      { 
        if  ( isLabel(c) )        // --68-- CAS[          STATE_0165--( isLabel(c) )-->STATE_0166 ]
        {
          id[3] = c; id[3+1] = 0;;    // action 
          state = STATE_0166;   // nextState 
        }
        else if  ( c == '\n' )        // --69-- CAS[          STATE_0165--( c == '\n' )-->ACTION_0105 ]
        {
          state = ACTION_0105;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --70-- ERROR 
        }
      } 
    break; 
    case STATE_0166:                     // --70-- CAS[          STATE_0166--( isLabel(c) )-->STATE_0167 ]
      { 
        if  ( isLabel(c) )        // --70-- CAS[          STATE_0166--( isLabel(c) )-->STATE_0167 ]
        {
          id[4] = c; id[4+1] = 0;;    // action 
          state = STATE_0167;   // nextState 
        }
        else if  ( c == '\n' )        // --71-- CAS[          STATE_0166--( c == '\n' )-->ACTION_0105 ]
        {
          state = ACTION_0105;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --72-- ERROR 
        }
      } 
    break; 
    case STATE_0167:                     // --72-- CAS[          STATE_0167--( isLabel(c) )-->STATE_0168 ]
      { 
        if  ( isLabel(c) )        // --72-- CAS[          STATE_0167--( isLabel(c) )-->STATE_0168 ]
        {
          id[5] = c; id[5+1] = 0;;    // action 
          state = STATE_0168;   // nextState 
        }
        else if  ( c == '\n' )        // --73-- CAS[          STATE_0167--( c == '\n' )-->ACTION_0105 ]
        {
          state = ACTION_0105;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --74-- ERROR 
        }
      } 
    break; 
    case STATE_0168:                     // --74-- CAS[          STATE_0168--( isLabel(c) )-->STATE_0169 ]
      { 
        if  ( isLabel(c) )        // --74-- CAS[          STATE_0168--( isLabel(c) )-->STATE_0169 ]
        {
          id[6] = c; id[6+1] = 0;;    // action 
          state = STATE_0169;   // nextState 
        }
        else if  ( c == '\n' )        // --75-- CAS[          STATE_0168--( c == '\n' )-->ACTION_0105 ]
        {
          state = ACTION_0105;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --76-- ERROR 
        }
      } 
    break; 
    case STATE_0169:                     // --76-- CAS[          STATE_0169--( isLabel(c) )-->STATE_0170 ]
      { 
        if  ( isLabel(c) )        // --76-- CAS[          STATE_0169--( isLabel(c) )-->STATE_0170 ]
        {
          id[7] = c; id[7+1] = 0;;    // action 
          state = STATE_0170;   // nextState 
        }
        else if  ( c == '\n' )        // --77-- CAS[          STATE_0169--( c == '\n' )-->ACTION_0105 ]
        {
          state = ACTION_0105;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --78-- ERROR 
        }
      } 
    break; 
    case STATE_0170:                     // --78-- CAS[          STATE_0170--( isLabel(c) )-->STATE_0171 ]
      { 
        if  ( isLabel(c) )        // --78-- CAS[          STATE_0170--( isLabel(c) )-->STATE_0171 ]
        {
          id[8] = c; id[8+1] = 0;;    // action 
          state = STATE_0171;   // nextState 
        }
        else if  ( c == '\n' )        // --79-- CAS[          STATE_0170--( c == '\n' )-->ACTION_0105 ]
        {
          state = ACTION_0105;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --80-- ERROR 
        }
      } 
    break; 
    case STATE_0171:                     // --80-- CAS[          STATE_0171--( isLabel(c) )-->STATE_0172 ]
      { 
        if  ( isLabel(c) )        // --80-- CAS[          STATE_0171--( isLabel(c) )-->STATE_0172 ]
        {
          id[9] = c; id[9+1] = 0;;    // action 
          state = STATE_0172;   // nextState 
        }
        else if  ( c == '\n' )        // --81-- CAS[          STATE_0171--( c == '\n' )-->ACTION_0105 ]
        {
          state = ACTION_0105;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --82-- ERROR 
        }
      } 
    break; 
    case STATE_0172:                     // --82-- CAS[          STATE_0172--( isLabel(c) )-->STATE_0173 ]
      { 
        if  ( isLabel(c) )        // --82-- CAS[          STATE_0172--( isLabel(c) )-->STATE_0173 ]
        {
          id[10] = c; id[10+1] = 0;;    // action 
          state = STATE_0173;   // nextState 
        }
        else if  ( c == '\n' )        // --83-- CAS[          STATE_0172--( c == '\n' )-->ACTION_0105 ]
        {
          state = ACTION_0105;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --84-- ERROR 
        }
      } 
    break; 
    case STATE_0173:                     // --84-- CAS[          STATE_0173--( isLabel(c) )-->STATE_0174 ]
      { 
        if  ( isLabel(c) )        // --84-- CAS[          STATE_0173--( isLabel(c) )-->STATE_0174 ]
        {
          id[11] = c; id[11+1] = 0;;    // action 
          state = STATE_0174;   // nextState 
        }
        else if  ( c == '\n' )        // --85-- CAS[          STATE_0173--( c == '\n' )-->ACTION_0105 ]
        {
          state = ACTION_0105;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --86-- ERROR 
        }
      } 
    break; 
    case STATE_0174:                     // --86-- CAS[          STATE_0174--( isLabel(c) )-->STATE_0175 ]
      { 
        if  ( isLabel(c) )        // --86-- CAS[          STATE_0174--( isLabel(c) )-->STATE_0175 ]
        {
          id[12] = c; id[12+1] = 0;;    // action 
          state = STATE_0175;   // nextState 
        }
        else if  ( c == '\n' )        // --87-- CAS[          STATE_0174--( c == '\n' )-->ACTION_0105 ]
        {
          state = ACTION_0105;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --88-- ERROR 
        }
      } 
    break; 
    case STATE_0175:                     // --88-- CAS[          STATE_0175--( isLabel(c) )-->STATE_0176 ]
      { 
        if  ( isLabel(c) )        // --88-- CAS[          STATE_0175--( isLabel(c) )-->STATE_0176 ]
        {
          id[13] = c; id[13+1] = 0;;    // action 
          state = STATE_0176;   // nextState 
        }
        else if  ( c == '\n' )        // --89-- CAS[          STATE_0175--( c == '\n' )-->ACTION_0105 ]
        {
          state = ACTION_0105;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --90-- ERROR 
        }
      } 
    break; 
    case STATE_0176:                     // --90-- CAS[          STATE_0176--( isLabel(c) )-->STATE_0177 ]
      { 
        if  ( isLabel(c) )        // --90-- CAS[          STATE_0176--( isLabel(c) )-->STATE_0177 ]
        {
          id[14] = c; id[14+1] = 0;;    // action 
          state = STATE_0177;   // nextState 
        }
        else if  ( c == '\n' )        // --91-- CAS[          STATE_0176--( c == '\n' )-->ACTION_0105 ]
        {
          state = ACTION_0105;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --92-- ERROR 
        }
      } 
    break; 
    case STATE_0177:                     // --92-- CAS[          STATE_0177--( isLabel(c) )-->STATE_0178 ]
      { 
        if  ( isLabel(c) )        // --92-- CAS[          STATE_0177--( isLabel(c) )-->STATE_0178 ]
        {
          id[15] = c; id[15+1] = 0;;    // action 
          state = STATE_0178;   // nextState 
        }
        else if  ( c == '\n' )        // --93-- CAS[          STATE_0177--( c == '\n' )-->ACTION_0105 ]
        {
          state = ACTION_0105;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --94-- ERROR 
        }
      } 
    break; 
    case STATE_0178:                     // --94-- CAS[          STATE_0178--( c == '\n' )-->ACTION_0105 ]
      { 
        if  ( c == '\n' )        // --94-- CAS[          STATE_0178--( c == '\n' )-->ACTION_0105 ]
        {
          state = ACTION_0105;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --95-- ERROR 
        }
      } 
    break; 
    case STATE_0192:                     // --95-- CAS[          STATE_0192--( c == '\n' )-->ACTION_0107 ]
      { 
        if  ( c == '\n' )        // --95-- CAS[          STATE_0192--( c == '\n' )-->ACTION_0107 ]
        {
          state = ACTION_0107;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --96-- ERROR 
        }
      } 
    break; 
    case STATE_0194:                     // --96-- CAS[          STATE_0194--( c == 'e' )-->STATE_0195 ]
      { 
        if  ( c == 'e' )        // --96-- CAS[          STATE_0194--( c == 'e' )-->STATE_0195 ]
        {
          state = STATE_0195;   // nextState 
        }
        else if  ( c == 'i' )        // --97-- CAS[          STATE_0194--( c == 'i' )-->STATE_0210 ]
        {
          state = STATE_0210;   // nextState 
        }
        else if  ( c == 'c' )        // --98-- CAS[          STATE_0194--( c == 'c' )-->STATE_0237 ]
        {
          state = STATE_0237;   // nextState 
        }
        else if  ( c == 'o' )        // --99-- CAS[          STATE_0194--( c == 'o' )-->STATE_0266 ]
        {
          state = STATE_0266;   // nextState 
        }
        else if  ( c == 'p' )        // --100-- CAS[          STATE_0194--( c == 'p' )-->STATE_0280 ]
        {
          state = STATE_0280;   // nextState 
        }
        else if  ( c == 's' )        // --101-- CAS[          STATE_0194--( c == 's' )-->STATE_0294 ]
        {
          state = STATE_0294;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --102-- ERROR 
        }
      } 
    break; 
    case STATE_0195:                     // --102-- CAS[          STATE_0195--( c == 'b' )-->STATE_0196 ]
      { 
        if  ( c == 'b' )        // --102-- CAS[          STATE_0195--( c == 'b' )-->STATE_0196 ]
        {
          state = STATE_0196;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --103-- ERROR 
        }
      } 
    break; 
    case STATE_0196:                     // --103-- CAS[          STATE_0196--( c == 'u' )-->STATE_0197 ]
      { 
        if  ( c == 'u' )        // --103-- CAS[          STATE_0196--( c == 'u' )-->STATE_0197 ]
        {
          state = STATE_0197;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --104-- ERROR 
        }
      } 
    break; 
    case STATE_0197:                     // --104-- CAS[          STATE_0197--( c == 'g' )-->STATE_0198 ]
      { 
        if  ( c == 'g' )        // --104-- CAS[          STATE_0197--( c == 'g' )-->STATE_0198 ]
        {
          state = STATE_0198;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --105-- ERROR 
        }
      } 
    break; 
    case STATE_0198:                     // --105-- CAS[          STATE_0198--( c == ':' )-->STATE_0199 ]
      { 
        if  ( c == ':' )        // --105-- CAS[          STATE_0198--( c == ':' )-->STATE_0199 ]
        {
          state = STATE_0199;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --106-- ERROR 
        }
      } 
    break; 
    case STATE_0199:                     // --106-- CAS[          STATE_0199--( isHex(c) )-->STATE_0200 ]
      { 
        if  ( isHex(c) )        // --106-- CAS[          STATE_0199--( isHex(c) )-->STATE_0200 ]
        {
          data = valueHex(c);;    // action 
          state = STATE_0200;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --107-- ERROR 
        }
      } 
    break; 
    case STATE_0200:                     // --107-- CAS[          STATE_0200--( isHex(c) )-->STATE_0201 ]
      { 
        if  ( isHex(c) )        // --107-- CAS[          STATE_0200--( isHex(c) )-->STATE_0201 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0201;   // nextState 
        }
        else if  ( c == '\n' )        // --108-- CAS[          STATE_0200--( c == '\n' )-->ACTION_0108 ]
        {
          state = ACTION_0108;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --109-- ERROR 
        }
      } 
    break; 
    case STATE_0201:                     // --109-- CAS[          STATE_0201--( isHex(c) )-->STATE_0202 ]
      { 
        if  ( isHex(c) )        // --109-- CAS[          STATE_0201--( isHex(c) )-->STATE_0202 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0202;   // nextState 
        }
        else if  ( c == '\n' )        // --110-- CAS[          STATE_0201--( c == '\n' )-->ACTION_0108 ]
        {
          state = ACTION_0108;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --111-- ERROR 
        }
      } 
    break; 
    case STATE_0202:                     // --111-- CAS[          STATE_0202--( isHex(c) )-->STATE_0203 ]
      { 
        if  ( isHex(c) )        // --111-- CAS[          STATE_0202--( isHex(c) )-->STATE_0203 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0203;   // nextState 
        }
        else if  ( c == '\n' )        // --112-- CAS[          STATE_0202--( c == '\n' )-->ACTION_0108 ]
        {
          state = ACTION_0108;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --113-- ERROR 
        }
      } 
    break; 
    case STATE_0203:                     // --113-- CAS[          STATE_0203--( isHex(c) )-->STATE_0204 ]
      { 
        if  ( isHex(c) )        // --113-- CAS[          STATE_0203--( isHex(c) )-->STATE_0204 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0204;   // nextState 
        }
        else if  ( c == '\n' )        // --114-- CAS[          STATE_0203--( c == '\n' )-->ACTION_0108 ]
        {
          state = ACTION_0108;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --115-- ERROR 
        }
      } 
    break; 
    case STATE_0204:                     // --115-- CAS[          STATE_0204--( isHex(c) )-->STATE_0205 ]
      { 
        if  ( isHex(c) )        // --115-- CAS[          STATE_0204--( isHex(c) )-->STATE_0205 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0205;   // nextState 
        }
        else if  ( c == '\n' )        // --116-- CAS[          STATE_0204--( c == '\n' )-->ACTION_0108 ]
        {
          state = ACTION_0108;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --117-- ERROR 
        }
      } 
    break; 
    case STATE_0205:                     // --117-- CAS[          STATE_0205--( isHex(c) )-->STATE_0206 ]
      { 
        if  ( isHex(c) )        // --117-- CAS[          STATE_0205--( isHex(c) )-->STATE_0206 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0206;   // nextState 
        }
        else if  ( c == '\n' )        // --118-- CAS[          STATE_0205--( c == '\n' )-->ACTION_0108 ]
        {
          state = ACTION_0108;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --119-- ERROR 
        }
      } 
    break; 
    case STATE_0206:                     // --119-- CAS[          STATE_0206--( isHex(c) )-->STATE_0207 ]
      { 
        if  ( isHex(c) )        // --119-- CAS[          STATE_0206--( isHex(c) )-->STATE_0207 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0207;   // nextState 
        }
        else if  ( c == '\n' )        // --120-- CAS[          STATE_0206--( c == '\n' )-->ACTION_0108 ]
        {
          state = ACTION_0108;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --121-- ERROR 
        }
      } 
    break; 
    case STATE_0207:                     // --121-- CAS[          STATE_0207--( c == '\n' )-->ACTION_0108 ]
      { 
        if  ( c == '\n' )        // --121-- CAS[          STATE_0207--( c == '\n' )-->ACTION_0108 ]
        {
          state = ACTION_0108;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --122-- ERROR 
        }
      } 
    break; 
    case STATE_0210:                     // --122-- CAS[          STATE_0210--( c == 'n' )-->STATE_0211 ]
      { 
        if  ( c == 'n' )        // --122-- CAS[          STATE_0210--( c == 'n' )-->STATE_0211 ]
        {
          state = STATE_0211;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --123-- ERROR 
        }
      } 
    break; 
    case STATE_0211:                     // --123-- CAS[          STATE_0211--( c == ':' )-->STATE_0212 ]
      { 
        if  ( c == ':' )        // --123-- CAS[          STATE_0211--( c == ':' )-->STATE_0212 ]
        {
          state = STATE_0212;   // nextState 
        }
        else if  ( c == 'p' )        // --124-- CAS[          STATE_0211--( c == 'p' )-->STATE_0225 ]
        {
          state = STATE_0225;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --125-- ERROR 
        }
      } 
    break; 
    case STATE_0212:                     // --125-- CAS[          STATE_0212--( isHex(c) )-->STATE_0213 ]
      { 
        if  ( isHex(c) )        // --125-- CAS[          STATE_0212--( isHex(c) )-->STATE_0213 ]
        {
          data = valueHex(c);;    // action 
          state = STATE_0213;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --126-- ERROR 
        }
      } 
    break; 
    case STATE_0213:                     // --126-- CAS[          STATE_0213--( isHex(c) )-->STATE_0214 ]
      { 
        if  ( isHex(c) )        // --126-- CAS[          STATE_0213--( isHex(c) )-->STATE_0214 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0214;   // nextState 
        }
        else if  ( c == '\n' )        // --127-- CAS[          STATE_0213--( c == '\n' )-->ACTION_0109 ]
        {
          state = ACTION_0109;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --128-- ERROR 
        }
      } 
    break; 
    case STATE_0214:                     // --128-- CAS[          STATE_0214--( isHex(c) )-->STATE_0215 ]
      { 
        if  ( isHex(c) )        // --128-- CAS[          STATE_0214--( isHex(c) )-->STATE_0215 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0215;   // nextState 
        }
        else if  ( c == '\n' )        // --129-- CAS[          STATE_0214--( c == '\n' )-->ACTION_0109 ]
        {
          state = ACTION_0109;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --130-- ERROR 
        }
      } 
    break; 
    case STATE_0215:                     // --130-- CAS[          STATE_0215--( isHex(c) )-->STATE_0216 ]
      { 
        if  ( isHex(c) )        // --130-- CAS[          STATE_0215--( isHex(c) )-->STATE_0216 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0216;   // nextState 
        }
        else if  ( c == '\n' )        // --131-- CAS[          STATE_0215--( c == '\n' )-->ACTION_0109 ]
        {
          state = ACTION_0109;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --132-- ERROR 
        }
      } 
    break; 
    case STATE_0216:                     // --132-- CAS[          STATE_0216--( isHex(c) )-->STATE_0217 ]
      { 
        if  ( isHex(c) )        // --132-- CAS[          STATE_0216--( isHex(c) )-->STATE_0217 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0217;   // nextState 
        }
        else if  ( c == '\n' )        // --133-- CAS[          STATE_0216--( c == '\n' )-->ACTION_0109 ]
        {
          state = ACTION_0109;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --134-- ERROR 
        }
      } 
    break; 
    case STATE_0217:                     // --134-- CAS[          STATE_0217--( isHex(c) )-->STATE_0218 ]
      { 
        if  ( isHex(c) )        // --134-- CAS[          STATE_0217--( isHex(c) )-->STATE_0218 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0218;   // nextState 
        }
        else if  ( c == '\n' )        // --135-- CAS[          STATE_0217--( c == '\n' )-->ACTION_0109 ]
        {
          state = ACTION_0109;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --136-- ERROR 
        }
      } 
    break; 
    case STATE_0218:                     // --136-- CAS[          STATE_0218--( isHex(c) )-->STATE_0219 ]
      { 
        if  ( isHex(c) )        // --136-- CAS[          STATE_0218--( isHex(c) )-->STATE_0219 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0219;   // nextState 
        }
        else if  ( c == '\n' )        // --137-- CAS[          STATE_0218--( c == '\n' )-->ACTION_0109 ]
        {
          state = ACTION_0109;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --138-- ERROR 
        }
      } 
    break; 
    case STATE_0219:                     // --138-- CAS[          STATE_0219--( isHex(c) )-->STATE_0220 ]
      { 
        if  ( isHex(c) )        // --138-- CAS[          STATE_0219--( isHex(c) )-->STATE_0220 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0220;   // nextState 
        }
        else if  ( c == '\n' )        // --139-- CAS[          STATE_0219--( c == '\n' )-->ACTION_0109 ]
        {
          state = ACTION_0109;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --140-- ERROR 
        }
      } 
    break; 
    case STATE_0220:                     // --140-- CAS[          STATE_0220--( c == '\n' )-->ACTION_0109 ]
      { 
        if  ( c == '\n' )        // --140-- CAS[          STATE_0220--( c == '\n' )-->ACTION_0109 ]
        {
          state = ACTION_0109;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --141-- ERROR 
        }
      } 
    break; 
    case STATE_0225:                     // --141-- CAS[          STATE_0225--( c == ':' )-->STATE_0226 ]
      { 
        if  ( c == ':' )        // --141-- CAS[          STATE_0225--( c == ':' )-->STATE_0226 ]
        {
          state = STATE_0226;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --142-- ERROR 
        }
      } 
    break; 
    case STATE_0226:                     // --142-- CAS[          STATE_0226--( isHex(c) )-->STATE_0227 ]
      { 
        if  ( isHex(c) )        // --142-- CAS[          STATE_0226--( isHex(c) )-->STATE_0227 ]
        {
          data = valueHex(c);;    // action 
          state = STATE_0227;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --143-- ERROR 
        }
      } 
    break; 
    case STATE_0227:                     // --143-- CAS[          STATE_0227--( isHex(c) )-->STATE_0228 ]
      { 
        if  ( isHex(c) )        // --143-- CAS[          STATE_0227--( isHex(c) )-->STATE_0228 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0228;   // nextState 
        }
        else if  ( c == '\n' )        // --144-- CAS[          STATE_0227--( c == '\n' )-->ACTION_0110 ]
        {
          state = ACTION_0110;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --145-- ERROR 
        }
      } 
    break; 
    case STATE_0228:                     // --145-- CAS[          STATE_0228--( isHex(c) )-->STATE_0229 ]
      { 
        if  ( isHex(c) )        // --145-- CAS[          STATE_0228--( isHex(c) )-->STATE_0229 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0229;   // nextState 
        }
        else if  ( c == '\n' )        // --146-- CAS[          STATE_0228--( c == '\n' )-->ACTION_0110 ]
        {
          state = ACTION_0110;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --147-- ERROR 
        }
      } 
    break; 
    case STATE_0229:                     // --147-- CAS[          STATE_0229--( isHex(c) )-->STATE_0230 ]
      { 
        if  ( isHex(c) )        // --147-- CAS[          STATE_0229--( isHex(c) )-->STATE_0230 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0230;   // nextState 
        }
        else if  ( c == '\n' )        // --148-- CAS[          STATE_0229--( c == '\n' )-->ACTION_0110 ]
        {
          state = ACTION_0110;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --149-- ERROR 
        }
      } 
    break; 
    case STATE_0230:                     // --149-- CAS[          STATE_0230--( isHex(c) )-->STATE_0231 ]
      { 
        if  ( isHex(c) )        // --149-- CAS[          STATE_0230--( isHex(c) )-->STATE_0231 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0231;   // nextState 
        }
        else if  ( c == '\n' )        // --150-- CAS[          STATE_0230--( c == '\n' )-->ACTION_0110 ]
        {
          state = ACTION_0110;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --151-- ERROR 
        }
      } 
    break; 
    case STATE_0231:                     // --151-- CAS[          STATE_0231--( isHex(c) )-->STATE_0232 ]
      { 
        if  ( isHex(c) )        // --151-- CAS[          STATE_0231--( isHex(c) )-->STATE_0232 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0232;   // nextState 
        }
        else if  ( c == '\n' )        // --152-- CAS[          STATE_0231--( c == '\n' )-->ACTION_0110 ]
        {
          state = ACTION_0110;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --153-- ERROR 
        }
      } 
    break; 
    case STATE_0232:                     // --153-- CAS[          STATE_0232--( isHex(c) )-->STATE_0233 ]
      { 
        if  ( isHex(c) )        // --153-- CAS[          STATE_0232--( isHex(c) )-->STATE_0233 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0233;   // nextState 
        }
        else if  ( c == '\n' )        // --154-- CAS[          STATE_0232--( c == '\n' )-->ACTION_0110 ]
        {
          state = ACTION_0110;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --155-- ERROR 
        }
      } 
    break; 
    case STATE_0233:                     // --155-- CAS[          STATE_0233--( isHex(c) )-->STATE_0234 ]
      { 
        if  ( isHex(c) )        // --155-- CAS[          STATE_0233--( isHex(c) )-->STATE_0234 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0234;   // nextState 
        }
        else if  ( c == '\n' )        // --156-- CAS[          STATE_0233--( c == '\n' )-->ACTION_0110 ]
        {
          state = ACTION_0110;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --157-- ERROR 
        }
      } 
    break; 
    case STATE_0234:                     // --157-- CAS[          STATE_0234--( c == '\n' )-->ACTION_0110 ]
      { 
        if  ( c == '\n' )        // --157-- CAS[          STATE_0234--( c == '\n' )-->ACTION_0110 ]
        {
          state = ACTION_0110;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --158-- ERROR 
        }
      } 
    break; 
    case STATE_0237:                     // --158-- CAS[          STATE_0237--( c == 'n' )-->STATE_0238 ]
      { 
        if  ( c == 'n' )        // --158-- CAS[          STATE_0237--( c == 'n' )-->STATE_0238 ]
        {
          state = STATE_0238;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --159-- ERROR 
        }
      } 
    break; 
    case STATE_0238:                     // --159-- CAS[          STATE_0238--( c == 't' )-->STATE_0239 ]
      { 
        if  ( c == 't' )        // --159-- CAS[          STATE_0238--( c == 't' )-->STATE_0239 ]
        {
          state = STATE_0239;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --160-- ERROR 
        }
      } 
    break; 
    case STATE_0239:                     // --160-- CAS[          STATE_0239--( c == ':' )-->STATE_0240 ]
      { 
        if  ( c == ':' )        // --160-- CAS[          STATE_0239--( c == ':' )-->STATE_0240 ]
        {
          state = STATE_0240;   // nextState 
        }
        else if  ( c == 'p' )        // --161-- CAS[          STATE_0239--( c == 'p' )-->STATE_0254 ]
        {
          state = STATE_0254;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --162-- ERROR 
        }
      } 
    break; 
    case STATE_0240:                     // --162-- CAS[          STATE_0240--( isHex(c) )-->STATE_0241 ]
      { 
        if  ( isHex(c) )        // --162-- CAS[          STATE_0240--( isHex(c) )-->STATE_0241 ]
        {
          data = valueHex(c);;    // action 
          state = STATE_0241;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --163-- ERROR 
        }
      } 
    break; 
    case STATE_0241:                     // --163-- CAS[          STATE_0241--( isHex(c) )-->STATE_0242 ]
      { 
        if  ( isHex(c) )        // --163-- CAS[          STATE_0241--( isHex(c) )-->STATE_0242 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0242;   // nextState 
        }
        else if  ( c == '\n' )        // --164-- CAS[          STATE_0241--( c == '\n' )-->ACTION_0111 ]
        {
          state = ACTION_0111;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --165-- ERROR 
        }
      } 
    break; 
    case STATE_0242:                     // --165-- CAS[          STATE_0242--( isHex(c) )-->STATE_0243 ]
      { 
        if  ( isHex(c) )        // --165-- CAS[          STATE_0242--( isHex(c) )-->STATE_0243 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0243;   // nextState 
        }
        else if  ( c == '\n' )        // --166-- CAS[          STATE_0242--( c == '\n' )-->ACTION_0111 ]
        {
          state = ACTION_0111;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --167-- ERROR 
        }
      } 
    break; 
    case STATE_0243:                     // --167-- CAS[          STATE_0243--( isHex(c) )-->STATE_0244 ]
      { 
        if  ( isHex(c) )        // --167-- CAS[          STATE_0243--( isHex(c) )-->STATE_0244 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0244;   // nextState 
        }
        else if  ( c == '\n' )        // --168-- CAS[          STATE_0243--( c == '\n' )-->ACTION_0111 ]
        {
          state = ACTION_0111;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --169-- ERROR 
        }
      } 
    break; 
    case STATE_0244:                     // --169-- CAS[          STATE_0244--( isHex(c) )-->STATE_0245 ]
      { 
        if  ( isHex(c) )        // --169-- CAS[          STATE_0244--( isHex(c) )-->STATE_0245 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0245;   // nextState 
        }
        else if  ( c == '\n' )        // --170-- CAS[          STATE_0244--( c == '\n' )-->ACTION_0111 ]
        {
          state = ACTION_0111;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --171-- ERROR 
        }
      } 
    break; 
    case STATE_0245:                     // --171-- CAS[          STATE_0245--( isHex(c) )-->STATE_0246 ]
      { 
        if  ( isHex(c) )        // --171-- CAS[          STATE_0245--( isHex(c) )-->STATE_0246 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0246;   // nextState 
        }
        else if  ( c == '\n' )        // --172-- CAS[          STATE_0245--( c == '\n' )-->ACTION_0111 ]
        {
          state = ACTION_0111;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --173-- ERROR 
        }
      } 
    break; 
    case STATE_0246:                     // --173-- CAS[          STATE_0246--( isHex(c) )-->STATE_0247 ]
      { 
        if  ( isHex(c) )        // --173-- CAS[          STATE_0246--( isHex(c) )-->STATE_0247 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0247;   // nextState 
        }
        else if  ( c == '\n' )        // --174-- CAS[          STATE_0246--( c == '\n' )-->ACTION_0111 ]
        {
          state = ACTION_0111;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --175-- ERROR 
        }
      } 
    break; 
    case STATE_0247:                     // --175-- CAS[          STATE_0247--( isHex(c) )-->STATE_0248 ]
      { 
        if  ( isHex(c) )        // --175-- CAS[          STATE_0247--( isHex(c) )-->STATE_0248 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0248;   // nextState 
        }
        else if  ( c == '\n' )        // --176-- CAS[          STATE_0247--( c == '\n' )-->ACTION_0111 ]
        {
          state = ACTION_0111;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --177-- ERROR 
        }
      } 
    break; 
    case STATE_0248:                     // --177-- CAS[          STATE_0248--( c == '\n' )-->ACTION_0111 ]
      { 
        if  ( c == '\n' )        // --177-- CAS[          STATE_0248--( c == '\n' )-->ACTION_0111 ]
        {
          state = ACTION_0111;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --178-- ERROR 
        }
      } 
    break; 
    case STATE_0254:                     // --178-- CAS[          STATE_0254--( c == ':' )-->STATE_0255 ]
      { 
        if  ( c == ':' )        // --178-- CAS[          STATE_0254--( c == ':' )-->STATE_0255 ]
        {
          state = STATE_0255;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --179-- ERROR 
        }
      } 
    break; 
    case STATE_0255:                     // --179-- CAS[          STATE_0255--( isHex(c) )-->STATE_0256 ]
      { 
        if  ( isHex(c) )        // --179-- CAS[          STATE_0255--( isHex(c) )-->STATE_0256 ]
        {
          data = valueHex(c);;    // action 
          state = STATE_0256;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --180-- ERROR 
        }
      } 
    break; 
    case STATE_0256:                     // --180-- CAS[          STATE_0256--( isHex(c) )-->STATE_0257 ]
      { 
        if  ( isHex(c) )        // --180-- CAS[          STATE_0256--( isHex(c) )-->STATE_0257 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0257;   // nextState 
        }
        else if  ( c == '\n' )        // --181-- CAS[          STATE_0256--( c == '\n' )-->ACTION_0112 ]
        {
          state = ACTION_0112;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --182-- ERROR 
        }
      } 
    break; 
    case STATE_0257:                     // --182-- CAS[          STATE_0257--( isHex(c) )-->STATE_0258 ]
      { 
        if  ( isHex(c) )        // --182-- CAS[          STATE_0257--( isHex(c) )-->STATE_0258 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0258;   // nextState 
        }
        else if  ( c == '\n' )        // --183-- CAS[          STATE_0257--( c == '\n' )-->ACTION_0112 ]
        {
          state = ACTION_0112;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --184-- ERROR 
        }
      } 
    break; 
    case STATE_0258:                     // --184-- CAS[          STATE_0258--( isHex(c) )-->STATE_0259 ]
      { 
        if  ( isHex(c) )        // --184-- CAS[          STATE_0258--( isHex(c) )-->STATE_0259 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0259;   // nextState 
        }
        else if  ( c == '\n' )        // --185-- CAS[          STATE_0258--( c == '\n' )-->ACTION_0112 ]
        {
          state = ACTION_0112;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --186-- ERROR 
        }
      } 
    break; 
    case STATE_0259:                     // --186-- CAS[          STATE_0259--( isHex(c) )-->STATE_0260 ]
      { 
        if  ( isHex(c) )        // --186-- CAS[          STATE_0259--( isHex(c) )-->STATE_0260 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0260;   // nextState 
        }
        else if  ( c == '\n' )        // --187-- CAS[          STATE_0259--( c == '\n' )-->ACTION_0112 ]
        {
          state = ACTION_0112;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --188-- ERROR 
        }
      } 
    break; 
    case STATE_0260:                     // --188-- CAS[          STATE_0260--( isHex(c) )-->STATE_0261 ]
      { 
        if  ( isHex(c) )        // --188-- CAS[          STATE_0260--( isHex(c) )-->STATE_0261 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0261;   // nextState 
        }
        else if  ( c == '\n' )        // --189-- CAS[          STATE_0260--( c == '\n' )-->ACTION_0112 ]
        {
          state = ACTION_0112;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --190-- ERROR 
        }
      } 
    break; 
    case STATE_0261:                     // --190-- CAS[          STATE_0261--( isHex(c) )-->STATE_0262 ]
      { 
        if  ( isHex(c) )        // --190-- CAS[          STATE_0261--( isHex(c) )-->STATE_0262 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0262;   // nextState 
        }
        else if  ( c == '\n' )        // --191-- CAS[          STATE_0261--( c == '\n' )-->ACTION_0112 ]
        {
          state = ACTION_0112;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --192-- ERROR 
        }
      } 
    break; 
    case STATE_0262:                     // --192-- CAS[          STATE_0262--( isHex(c) )-->STATE_0263 ]
      { 
        if  ( isHex(c) )        // --192-- CAS[          STATE_0262--( isHex(c) )-->STATE_0263 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0263;   // nextState 
        }
        else if  ( c == '\n' )        // --193-- CAS[          STATE_0262--( c == '\n' )-->ACTION_0112 ]
        {
          state = ACTION_0112;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --194-- ERROR 
        }
      } 
    break; 
    case STATE_0263:                     // --194-- CAS[          STATE_0263--( c == '\n' )-->ACTION_0112 ]
      { 
        if  ( c == '\n' )        // --194-- CAS[          STATE_0263--( c == '\n' )-->ACTION_0112 ]
        {
          state = ACTION_0112;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --195-- ERROR 
        }
      } 
    break; 
    case STATE_0266:                     // --195-- CAS[          STATE_0266--( c == 'u' )-->STATE_0267 ]
      { 
        if  ( c == 'u' )        // --195-- CAS[          STATE_0266--( c == 'u' )-->STATE_0267 ]
        {
          state = STATE_0267;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --196-- ERROR 
        }
      } 
    break; 
    case STATE_0267:                     // --196-- CAS[          STATE_0267--( c == 't' )-->STATE_0268 ]
      { 
        if  ( c == 't' )        // --196-- CAS[          STATE_0267--( c == 't' )-->STATE_0268 ]
        {
          state = STATE_0268;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --197-- ERROR 
        }
      } 
    break; 
    case STATE_0268:                     // --197-- CAS[          STATE_0268--( c == ':' )-->STATE_0269 ]
      { 
        if  ( c == ':' )        // --197-- CAS[          STATE_0268--( c == ':' )-->STATE_0269 ]
        {
          state = STATE_0269;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --198-- ERROR 
        }
      } 
    break; 
    case STATE_0269:                     // --198-- CAS[          STATE_0269--( isHex(c) )-->STATE_0270 ]
      { 
        if  ( isHex(c) )        // --198-- CAS[          STATE_0269--( isHex(c) )-->STATE_0270 ]
        {
          data = valueHex(c);;    // action 
          state = STATE_0270;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --199-- ERROR 
        }
      } 
    break; 
    case STATE_0270:                     // --199-- CAS[          STATE_0270--( isHex(c) )-->STATE_0271 ]
      { 
        if  ( isHex(c) )        // --199-- CAS[          STATE_0270--( isHex(c) )-->STATE_0271 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0271;   // nextState 
        }
        else if  ( c == '\n' )        // --200-- CAS[          STATE_0270--( c == '\n' )-->ACTION_0113 ]
        {
          state = ACTION_0113;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --201-- ERROR 
        }
      } 
    break; 
    case STATE_0271:                     // --201-- CAS[          STATE_0271--( isHex(c) )-->STATE_0272 ]
      { 
        if  ( isHex(c) )        // --201-- CAS[          STATE_0271--( isHex(c) )-->STATE_0272 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0272;   // nextState 
        }
        else if  ( c == '\n' )        // --202-- CAS[          STATE_0271--( c == '\n' )-->ACTION_0113 ]
        {
          state = ACTION_0113;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --203-- ERROR 
        }
      } 
    break; 
    case STATE_0272:                     // --203-- CAS[          STATE_0272--( isHex(c) )-->STATE_0273 ]
      { 
        if  ( isHex(c) )        // --203-- CAS[          STATE_0272--( isHex(c) )-->STATE_0273 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0273;   // nextState 
        }
        else if  ( c == '\n' )        // --204-- CAS[          STATE_0272--( c == '\n' )-->ACTION_0113 ]
        {
          state = ACTION_0113;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --205-- ERROR 
        }
      } 
    break; 
    case STATE_0273:                     // --205-- CAS[          STATE_0273--( isHex(c) )-->STATE_0274 ]
      { 
        if  ( isHex(c) )        // --205-- CAS[          STATE_0273--( isHex(c) )-->STATE_0274 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0274;   // nextState 
        }
        else if  ( c == '\n' )        // --206-- CAS[          STATE_0273--( c == '\n' )-->ACTION_0113 ]
        {
          state = ACTION_0113;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --207-- ERROR 
        }
      } 
    break; 
    case STATE_0274:                     // --207-- CAS[          STATE_0274--( isHex(c) )-->STATE_0275 ]
      { 
        if  ( isHex(c) )        // --207-- CAS[          STATE_0274--( isHex(c) )-->STATE_0275 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0275;   // nextState 
        }
        else if  ( c == '\n' )        // --208-- CAS[          STATE_0274--( c == '\n' )-->ACTION_0113 ]
        {
          state = ACTION_0113;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --209-- ERROR 
        }
      } 
    break; 
    case STATE_0275:                     // --209-- CAS[          STATE_0275--( isHex(c) )-->STATE_0276 ]
      { 
        if  ( isHex(c) )        // --209-- CAS[          STATE_0275--( isHex(c) )-->STATE_0276 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0276;   // nextState 
        }
        else if  ( c == '\n' )        // --210-- CAS[          STATE_0275--( c == '\n' )-->ACTION_0113 ]
        {
          state = ACTION_0113;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --211-- ERROR 
        }
      } 
    break; 
    case STATE_0276:                     // --211-- CAS[          STATE_0276--( isHex(c) )-->STATE_0277 ]
      { 
        if  ( isHex(c) )        // --211-- CAS[          STATE_0276--( isHex(c) )-->STATE_0277 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0277;   // nextState 
        }
        else if  ( c == '\n' )        // --212-- CAS[          STATE_0276--( c == '\n' )-->ACTION_0113 ]
        {
          state = ACTION_0113;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --213-- ERROR 
        }
      } 
    break; 
    case STATE_0277:                     // --213-- CAS[          STATE_0277--( c == '\n' )-->ACTION_0113 ]
      { 
        if  ( c == '\n' )        // --213-- CAS[          STATE_0277--( c == '\n' )-->ACTION_0113 ]
        {
          state = ACTION_0113;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --214-- ERROR 
        }
      } 
    break; 
    case STATE_0280:                     // --214-- CAS[          STATE_0280--( c == 'w' )-->STATE_0281 ]
      { 
        if  ( c == 'w' )        // --214-- CAS[          STATE_0280--( c == 'w' )-->STATE_0281 ]
        {
          state = STATE_0281;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --215-- ERROR 
        }
      } 
    break; 
    case STATE_0281:                     // --215-- CAS[          STATE_0281--( c == 'm' )-->STATE_0282 ]
      { 
        if  ( c == 'm' )        // --215-- CAS[          STATE_0281--( c == 'm' )-->STATE_0282 ]
        {
          state = STATE_0282;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --216-- ERROR 
        }
      } 
    break; 
    case STATE_0282:                     // --216-- CAS[          STATE_0282--( c == ':' )-->STATE_0283 ]
      { 
        if  ( c == ':' )        // --216-- CAS[          STATE_0282--( c == ':' )-->STATE_0283 ]
        {
          state = STATE_0283;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --217-- ERROR 
        }
      } 
    break; 
    case STATE_0283:                     // --217-- CAS[          STATE_0283--( isHex(c) )-->STATE_0284 ]
      { 
        if  ( isHex(c) )        // --217-- CAS[          STATE_0283--( isHex(c) )-->STATE_0284 ]
        {
          data = valueHex(c);;    // action 
          state = STATE_0284;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --218-- ERROR 
        }
      } 
    break; 
    case STATE_0284:                     // --218-- CAS[          STATE_0284--( isHex(c) )-->STATE_0285 ]
      { 
        if  ( isHex(c) )        // --218-- CAS[          STATE_0284--( isHex(c) )-->STATE_0285 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0285;   // nextState 
        }
        else if  ( c == '\n' )        // --219-- CAS[          STATE_0284--( c == '\n' )-->ACTION_0114 ]
        {
          state = ACTION_0114;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --220-- ERROR 
        }
      } 
    break; 
    case STATE_0285:                     // --220-- CAS[          STATE_0285--( isHex(c) )-->STATE_0286 ]
      { 
        if  ( isHex(c) )        // --220-- CAS[          STATE_0285--( isHex(c) )-->STATE_0286 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0286;   // nextState 
        }
        else if  ( c == '\n' )        // --221-- CAS[          STATE_0285--( c == '\n' )-->ACTION_0114 ]
        {
          state = ACTION_0114;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --222-- ERROR 
        }
      } 
    break; 
    case STATE_0286:                     // --222-- CAS[          STATE_0286--( isHex(c) )-->STATE_0287 ]
      { 
        if  ( isHex(c) )        // --222-- CAS[          STATE_0286--( isHex(c) )-->STATE_0287 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0287;   // nextState 
        }
        else if  ( c == '\n' )        // --223-- CAS[          STATE_0286--( c == '\n' )-->ACTION_0114 ]
        {
          state = ACTION_0114;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --224-- ERROR 
        }
      } 
    break; 
    case STATE_0287:                     // --224-- CAS[          STATE_0287--( isHex(c) )-->STATE_0288 ]
      { 
        if  ( isHex(c) )        // --224-- CAS[          STATE_0287--( isHex(c) )-->STATE_0288 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0288;   // nextState 
        }
        else if  ( c == '\n' )        // --225-- CAS[          STATE_0287--( c == '\n' )-->ACTION_0114 ]
        {
          state = ACTION_0114;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --226-- ERROR 
        }
      } 
    break; 
    case STATE_0288:                     // --226-- CAS[          STATE_0288--( isHex(c) )-->STATE_0289 ]
      { 
        if  ( isHex(c) )        // --226-- CAS[          STATE_0288--( isHex(c) )-->STATE_0289 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0289;   // nextState 
        }
        else if  ( c == '\n' )        // --227-- CAS[          STATE_0288--( c == '\n' )-->ACTION_0114 ]
        {
          state = ACTION_0114;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --228-- ERROR 
        }
      } 
    break; 
    case STATE_0289:                     // --228-- CAS[          STATE_0289--( isHex(c) )-->STATE_0290 ]
      { 
        if  ( isHex(c) )        // --228-- CAS[          STATE_0289--( isHex(c) )-->STATE_0290 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0290;   // nextState 
        }
        else if  ( c == '\n' )        // --229-- CAS[          STATE_0289--( c == '\n' )-->ACTION_0114 ]
        {
          state = ACTION_0114;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --230-- ERROR 
        }
      } 
    break; 
    case STATE_0290:                     // --230-- CAS[          STATE_0290--( isHex(c) )-->STATE_0291 ]
      { 
        if  ( isHex(c) )        // --230-- CAS[          STATE_0290--( isHex(c) )-->STATE_0291 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0291;   // nextState 
        }
        else if  ( c == '\n' )        // --231-- CAS[          STATE_0290--( c == '\n' )-->ACTION_0114 ]
        {
          state = ACTION_0114;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --232-- ERROR 
        }
      } 
    break; 
    case STATE_0291:                     // --232-- CAS[          STATE_0291--( c == '\n' )-->ACTION_0114 ]
      { 
        if  ( c == '\n' )        // --232-- CAS[          STATE_0291--( c == '\n' )-->ACTION_0114 ]
        {
          state = ACTION_0114;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --233-- ERROR 
        }
      } 
    break; 
    case STATE_0294:                     // --233-- CAS[          STATE_0294--( c == 'e' )-->STATE_0295 ]
      { 
        if  ( c == 'e' )        // --233-- CAS[          STATE_0294--( c == 'e' )-->STATE_0295 ]
        {
          state = STATE_0295;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --234-- ERROR 
        }
      } 
    break; 
    case STATE_0295:                     // --234-- CAS[          STATE_0295--( c == 'r' )-->STATE_0296 ]
      { 
        if  ( c == 'r' )        // --234-- CAS[          STATE_0295--( c == 'r' )-->STATE_0296 ]
        {
          state = STATE_0296;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --235-- ERROR 
        }
      } 
    break; 
    case STATE_0296:                     // --235-- CAS[          STATE_0296--( c == 'v' )-->STATE_0297 ]
      { 
        if  ( c == 'v' )        // --235-- CAS[          STATE_0296--( c == 'v' )-->STATE_0297 ]
        {
          state = STATE_0297;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --236-- ERROR 
        }
      } 
    break; 
    case STATE_0297:                     // --236-- CAS[          STATE_0297--( c == 'o' )-->STATE_0298 ]
      { 
        if  ( c == 'o' )        // --236-- CAS[          STATE_0297--( c == 'o' )-->STATE_0298 ]
        {
          state = STATE_0298;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --237-- ERROR 
        }
      } 
    break; 
    case STATE_0298:                     // --237-- CAS[          STATE_0298--( c == ':' )-->STATE_0299 ]
      { 
        if  ( c == ':' )        // --237-- CAS[          STATE_0298--( c == ':' )-->STATE_0299 ]
        {
          state = STATE_0299;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --238-- ERROR 
        }
      } 
    break; 
    case STATE_0299:                     // --238-- CAS[          STATE_0299--( isHex(c) )-->STATE_0300 ]
      { 
        if  ( isHex(c) )        // --238-- CAS[          STATE_0299--( isHex(c) )-->STATE_0300 ]
        {
          data = valueHex(c);;    // action 
          state = STATE_0300;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --239-- ERROR 
        }
      } 
    break; 
    case STATE_0300:                     // --239-- CAS[          STATE_0300--( isHex(c) )-->STATE_0301 ]
      { 
        if  ( isHex(c) )        // --239-- CAS[          STATE_0300--( isHex(c) )-->STATE_0301 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0301;   // nextState 
        }
        else if  ( c == '\n' )        // --240-- CAS[          STATE_0300--( c == '\n' )-->ACTION_0115 ]
        {
          state = ACTION_0115;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --241-- ERROR 
        }
      } 
    break; 
    case STATE_0301:                     // --241-- CAS[          STATE_0301--( isHex(c) )-->STATE_0302 ]
      { 
        if  ( isHex(c) )        // --241-- CAS[          STATE_0301--( isHex(c) )-->STATE_0302 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0302;   // nextState 
        }
        else if  ( c == '\n' )        // --242-- CAS[          STATE_0301--( c == '\n' )-->ACTION_0115 ]
        {
          state = ACTION_0115;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --243-- ERROR 
        }
      } 
    break; 
    case STATE_0302:                     // --243-- CAS[          STATE_0302--( isHex(c) )-->STATE_0303 ]
      { 
        if  ( isHex(c) )        // --243-- CAS[          STATE_0302--( isHex(c) )-->STATE_0303 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0303;   // nextState 
        }
        else if  ( c == '\n' )        // --244-- CAS[          STATE_0302--( c == '\n' )-->ACTION_0115 ]
        {
          state = ACTION_0115;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --245-- ERROR 
        }
      } 
    break; 
    case STATE_0303:                     // --245-- CAS[          STATE_0303--( isHex(c) )-->STATE_0304 ]
      { 
        if  ( isHex(c) )        // --245-- CAS[          STATE_0303--( isHex(c) )-->STATE_0304 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0304;   // nextState 
        }
        else if  ( c == '\n' )        // --246-- CAS[          STATE_0303--( c == '\n' )-->ACTION_0115 ]
        {
          state = ACTION_0115;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --247-- ERROR 
        }
      } 
    break; 
    case STATE_0304:                     // --247-- CAS[          STATE_0304--( isHex(c) )-->STATE_0305 ]
      { 
        if  ( isHex(c) )        // --247-- CAS[          STATE_0304--( isHex(c) )-->STATE_0305 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0305;   // nextState 
        }
        else if  ( c == '\n' )        // --248-- CAS[          STATE_0304--( c == '\n' )-->ACTION_0115 ]
        {
          state = ACTION_0115;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --249-- ERROR 
        }
      } 
    break; 
    case STATE_0305:                     // --249-- CAS[          STATE_0305--( isHex(c) )-->STATE_0306 ]
      { 
        if  ( isHex(c) )        // --249-- CAS[          STATE_0305--( isHex(c) )-->STATE_0306 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0306;   // nextState 
        }
        else if  ( c == '\n' )        // --250-- CAS[          STATE_0305--( c == '\n' )-->ACTION_0115 ]
        {
          state = ACTION_0115;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --251-- ERROR 
        }
      } 
    break; 
    case STATE_0306:                     // --251-- CAS[          STATE_0306--( isHex(c) )-->STATE_0307 ]
      { 
        if  ( isHex(c) )        // --251-- CAS[          STATE_0306--( isHex(c) )-->STATE_0307 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0307;   // nextState 
        }
        else if  ( c == '\n' )        // --252-- CAS[          STATE_0306--( c == '\n' )-->ACTION_0115 ]
        {
          state = ACTION_0115;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --253-- ERROR 
        }
      } 
    break; 
    case STATE_0307:                     // --253-- CAS[          STATE_0307--( c == '\n' )-->ACTION_0115 ]
      { 
        if  ( c == '\n' )        // --253-- CAS[          STATE_0307--( c == '\n' )-->ACTION_0115 ]
        {
          state = ACTION_0115;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --254-- ERROR 
        }
      } 
    break; 
    case STATE_0309:                     // --254-- CAS[          STATE_0309--( c == 'a' )-->STATE_0310 ]
      { 
        if  ( c == 'a' )        // --254-- CAS[          STATE_0309--( c == 'a' )-->STATE_0310 ]
        {
          state = STATE_0310;   // nextState 
        }
        else if  ( c == 'd' )        // --255-- CAS[          STATE_0309--( c == 'd' )-->STATE_0324 ]
        {
          state = STATE_0324;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --256-- ERROR 
        }
      } 
    break; 
    case STATE_0310:                     // --256-- CAS[          STATE_0310--( c == 'i' )-->STATE_0311 ]
      { 
        if  ( c == 'i' )        // --256-- CAS[          STATE_0310--( c == 'i' )-->STATE_0311 ]
        {
          state = STATE_0311;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --257-- ERROR 
        }
      } 
    break; 
    case STATE_0311:                     // --257-- CAS[          STATE_0311--( c == 'n' )-->STATE_0312 ]
      { 
        if  ( c == 'n' )        // --257-- CAS[          STATE_0311--( c == 'n' )-->STATE_0312 ]
        {
          state = STATE_0312;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --258-- ERROR 
        }
      } 
    break; 
    case STATE_0312:                     // --258-- CAS[          STATE_0312--( c == ':' )-->STATE_0313 ]
      { 
        if  ( c == ':' )        // --258-- CAS[          STATE_0312--( c == ':' )-->STATE_0313 ]
        {
          state = STATE_0313;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --259-- ERROR 
        }
      } 
    break; 
    case STATE_0313:                     // --259-- CAS[          STATE_0313--( isHex(c) )-->STATE_0314 ]
      { 
        if  ( isHex(c) )        // --259-- CAS[          STATE_0313--( isHex(c) )-->STATE_0314 ]
        {
          data = valueHex(c);;    // action 
          state = STATE_0314;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --260-- ERROR 
        }
      } 
    break; 
    case STATE_0314:                     // --260-- CAS[          STATE_0314--( isHex(c) )-->STATE_0315 ]
      { 
        if  ( isHex(c) )        // --260-- CAS[          STATE_0314--( isHex(c) )-->STATE_0315 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0315;   // nextState 
        }
        else if  ( c == '\n' )        // --261-- CAS[          STATE_0314--( c == '\n' )-->ACTION_0116 ]
        {
          state = ACTION_0116;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --262-- ERROR 
        }
      } 
    break; 
    case STATE_0315:                     // --262-- CAS[          STATE_0315--( isHex(c) )-->STATE_0316 ]
      { 
        if  ( isHex(c) )        // --262-- CAS[          STATE_0315--( isHex(c) )-->STATE_0316 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0316;   // nextState 
        }
        else if  ( c == '\n' )        // --263-- CAS[          STATE_0315--( c == '\n' )-->ACTION_0116 ]
        {
          state = ACTION_0116;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --264-- ERROR 
        }
      } 
    break; 
    case STATE_0316:                     // --264-- CAS[          STATE_0316--( isHex(c) )-->STATE_0317 ]
      { 
        if  ( isHex(c) )        // --264-- CAS[          STATE_0316--( isHex(c) )-->STATE_0317 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0317;   // nextState 
        }
        else if  ( c == '\n' )        // --265-- CAS[          STATE_0316--( c == '\n' )-->ACTION_0116 ]
        {
          state = ACTION_0116;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --266-- ERROR 
        }
      } 
    break; 
    case STATE_0317:                     // --266-- CAS[          STATE_0317--( isHex(c) )-->STATE_0318 ]
      { 
        if  ( isHex(c) )        // --266-- CAS[          STATE_0317--( isHex(c) )-->STATE_0318 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0318;   // nextState 
        }
        else if  ( c == '\n' )        // --267-- CAS[          STATE_0317--( c == '\n' )-->ACTION_0116 ]
        {
          state = ACTION_0116;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --268-- ERROR 
        }
      } 
    break; 
    case STATE_0318:                     // --268-- CAS[          STATE_0318--( isHex(c) )-->STATE_0319 ]
      { 
        if  ( isHex(c) )        // --268-- CAS[          STATE_0318--( isHex(c) )-->STATE_0319 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0319;   // nextState 
        }
        else if  ( c == '\n' )        // --269-- CAS[          STATE_0318--( c == '\n' )-->ACTION_0116 ]
        {
          state = ACTION_0116;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --270-- ERROR 
        }
      } 
    break; 
    case STATE_0319:                     // --270-- CAS[          STATE_0319--( isHex(c) )-->STATE_0320 ]
      { 
        if  ( isHex(c) )        // --270-- CAS[          STATE_0319--( isHex(c) )-->STATE_0320 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0320;   // nextState 
        }
        else if  ( c == '\n' )        // --271-- CAS[          STATE_0319--( c == '\n' )-->ACTION_0116 ]
        {
          state = ACTION_0116;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --272-- ERROR 
        }
      } 
    break; 
    case STATE_0320:                     // --272-- CAS[          STATE_0320--( isHex(c) )-->STATE_0321 ]
      { 
        if  ( isHex(c) )        // --272-- CAS[          STATE_0320--( isHex(c) )-->STATE_0321 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0321;   // nextState 
        }
        else if  ( c == '\n' )        // --273-- CAS[          STATE_0320--( c == '\n' )-->ACTION_0116 ]
        {
          state = ACTION_0116;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --274-- ERROR 
        }
      } 
    break; 
    case STATE_0321:                     // --274-- CAS[          STATE_0321--( c == '\n' )-->ACTION_0116 ]
      { 
        if  ( c == '\n' )        // --274-- CAS[          STATE_0321--( c == '\n' )-->ACTION_0116 ]
        {
          state = ACTION_0116;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --275-- ERROR 
        }
      } 
    break; 
    case STATE_0324:                     // --275-- CAS[          STATE_0324--( c == 'i' )-->STATE_0325 ]
      { 
        if  ( c == 'i' )        // --275-- CAS[          STATE_0324--( c == 'i' )-->STATE_0325 ]
        {
          state = STATE_0325;   // nextState 
        }
        else if  ( c == 'o' )        // --276-- CAS[          STATE_0324--( c == 'o' )-->STATE_0339 ]
        {
          state = STATE_0339;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --277-- ERROR 
        }
      } 
    break; 
    case STATE_0325:                     // --277-- CAS[          STATE_0325--( c == 'n' )-->STATE_0326 ]
      { 
        if  ( c == 'n' )        // --277-- CAS[          STATE_0325--( c == 'n' )-->STATE_0326 ]
        {
          state = STATE_0326;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --278-- ERROR 
        }
      } 
    break; 
    case STATE_0326:                     // --278-- CAS[          STATE_0326--( c == ':' )-->STATE_0327 ]
      { 
        if  ( c == ':' )        // --278-- CAS[          STATE_0326--( c == ':' )-->STATE_0327 ]
        {
          state = STATE_0327;   // nextState 
        }
        else if  ( c == 'p' )        // --279-- CAS[          STATE_0326--( c == 'p' )-->STATE_0356 ]
        {
          state = STATE_0356;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --280-- ERROR 
        }
      } 
    break; 
    case STATE_0327:                     // --280-- CAS[          STATE_0327--( isHex(c) )-->STATE_0328 ]
      { 
        if  ( isHex(c) )        // --280-- CAS[          STATE_0327--( isHex(c) )-->STATE_0328 ]
        {
          data = valueHex(c);;    // action 
          state = STATE_0328;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --281-- ERROR 
        }
      } 
    break; 
    case STATE_0328:                     // --281-- CAS[          STATE_0328--( isHex(c) )-->STATE_0329 ]
      { 
        if  ( isHex(c) )        // --281-- CAS[          STATE_0328--( isHex(c) )-->STATE_0329 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0329;   // nextState 
        }
        else if  ( c == '\n' )        // --282-- CAS[          STATE_0328--( c == '\n' )-->ACTION_0117 ]
        {
          state = ACTION_0117;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --283-- ERROR 
        }
      } 
    break; 
    case STATE_0329:                     // --283-- CAS[          STATE_0329--( isHex(c) )-->STATE_0330 ]
      { 
        if  ( isHex(c) )        // --283-- CAS[          STATE_0329--( isHex(c) )-->STATE_0330 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0330;   // nextState 
        }
        else if  ( c == '\n' )        // --284-- CAS[          STATE_0329--( c == '\n' )-->ACTION_0117 ]
        {
          state = ACTION_0117;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --285-- ERROR 
        }
      } 
    break; 
    case STATE_0330:                     // --285-- CAS[          STATE_0330--( isHex(c) )-->STATE_0331 ]
      { 
        if  ( isHex(c) )        // --285-- CAS[          STATE_0330--( isHex(c) )-->STATE_0331 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0331;   // nextState 
        }
        else if  ( c == '\n' )        // --286-- CAS[          STATE_0330--( c == '\n' )-->ACTION_0117 ]
        {
          state = ACTION_0117;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --287-- ERROR 
        }
      } 
    break; 
    case STATE_0331:                     // --287-- CAS[          STATE_0331--( isHex(c) )-->STATE_0332 ]
      { 
        if  ( isHex(c) )        // --287-- CAS[          STATE_0331--( isHex(c) )-->STATE_0332 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0332;   // nextState 
        }
        else if  ( c == '\n' )        // --288-- CAS[          STATE_0331--( c == '\n' )-->ACTION_0117 ]
        {
          state = ACTION_0117;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --289-- ERROR 
        }
      } 
    break; 
    case STATE_0332:                     // --289-- CAS[          STATE_0332--( isHex(c) )-->STATE_0333 ]
      { 
        if  ( isHex(c) )        // --289-- CAS[          STATE_0332--( isHex(c) )-->STATE_0333 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0333;   // nextState 
        }
        else if  ( c == '\n' )        // --290-- CAS[          STATE_0332--( c == '\n' )-->ACTION_0117 ]
        {
          state = ACTION_0117;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --291-- ERROR 
        }
      } 
    break; 
    case STATE_0333:                     // --291-- CAS[          STATE_0333--( isHex(c) )-->STATE_0334 ]
      { 
        if  ( isHex(c) )        // --291-- CAS[          STATE_0333--( isHex(c) )-->STATE_0334 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0334;   // nextState 
        }
        else if  ( c == '\n' )        // --292-- CAS[          STATE_0333--( c == '\n' )-->ACTION_0117 ]
        {
          state = ACTION_0117;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --293-- ERROR 
        }
      } 
    break; 
    case STATE_0334:                     // --293-- CAS[          STATE_0334--( isHex(c) )-->STATE_0335 ]
      { 
        if  ( isHex(c) )        // --293-- CAS[          STATE_0334--( isHex(c) )-->STATE_0335 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0335;   // nextState 
        }
        else if  ( c == '\n' )        // --294-- CAS[          STATE_0334--( c == '\n' )-->ACTION_0117 ]
        {
          state = ACTION_0117;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --295-- ERROR 
        }
      } 
    break; 
    case STATE_0335:                     // --295-- CAS[          STATE_0335--( c == '\n' )-->ACTION_0117 ]
      { 
        if  ( c == '\n' )        // --295-- CAS[          STATE_0335--( c == '\n' )-->ACTION_0117 ]
        {
          state = ACTION_0117;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --296-- ERROR 
        }
      } 
    break; 
    case STATE_0339:                     // --296-- CAS[          STATE_0339--( c == 'u' )-->STATE_0340 ]
      { 
        if  ( c == 'u' )        // --296-- CAS[          STATE_0339--( c == 'u' )-->STATE_0340 ]
        {
          state = STATE_0340;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --297-- ERROR 
        }
      } 
    break; 
    case STATE_0340:                     // --297-- CAS[          STATE_0340--( c == 't' )-->STATE_0341 ]
      { 
        if  ( c == 't' )        // --297-- CAS[          STATE_0340--( c == 't' )-->STATE_0341 ]
        {
          state = STATE_0341;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --298-- ERROR 
        }
      } 
    break; 
    case STATE_0341:                     // --298-- CAS[          STATE_0341--( c == ':' )-->STATE_0342 ]
      { 
        if  ( c == ':' )        // --298-- CAS[          STATE_0341--( c == ':' )-->STATE_0342 ]
        {
          state = STATE_0342;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --299-- ERROR 
        }
      } 
    break; 
    case STATE_0342:                     // --299-- CAS[          STATE_0342--( isHex(c) )-->STATE_0343 ]
      { 
        if  ( isHex(c) )        // --299-- CAS[          STATE_0342--( isHex(c) )-->STATE_0343 ]
        {
          data = valueHex(c);;    // action 
          state = STATE_0343;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --300-- ERROR 
        }
      } 
    break; 
    case STATE_0343:                     // --300-- CAS[          STATE_0343--( isHex(c) )-->STATE_0344 ]
      { 
        if  ( isHex(c) )        // --300-- CAS[          STATE_0343--( isHex(c) )-->STATE_0344 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0344;   // nextState 
        }
        else if  ( c == '\n' )        // --301-- CAS[          STATE_0343--( c == '\n' )-->ACTION_0118 ]
        {
          state = ACTION_0118;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --302-- ERROR 
        }
      } 
    break; 
    case STATE_0344:                     // --302-- CAS[          STATE_0344--( isHex(c) )-->STATE_0345 ]
      { 
        if  ( isHex(c) )        // --302-- CAS[          STATE_0344--( isHex(c) )-->STATE_0345 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0345;   // nextState 
        }
        else if  ( c == '\n' )        // --303-- CAS[          STATE_0344--( c == '\n' )-->ACTION_0118 ]
        {
          state = ACTION_0118;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --304-- ERROR 
        }
      } 
    break; 
    case STATE_0345:                     // --304-- CAS[          STATE_0345--( isHex(c) )-->STATE_0346 ]
      { 
        if  ( isHex(c) )        // --304-- CAS[          STATE_0345--( isHex(c) )-->STATE_0346 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0346;   // nextState 
        }
        else if  ( c == '\n' )        // --305-- CAS[          STATE_0345--( c == '\n' )-->ACTION_0118 ]
        {
          state = ACTION_0118;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --306-- ERROR 
        }
      } 
    break; 
    case STATE_0346:                     // --306-- CAS[          STATE_0346--( isHex(c) )-->STATE_0347 ]
      { 
        if  ( isHex(c) )        // --306-- CAS[          STATE_0346--( isHex(c) )-->STATE_0347 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0347;   // nextState 
        }
        else if  ( c == '\n' )        // --307-- CAS[          STATE_0346--( c == '\n' )-->ACTION_0118 ]
        {
          state = ACTION_0118;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --308-- ERROR 
        }
      } 
    break; 
    case STATE_0347:                     // --308-- CAS[          STATE_0347--( isHex(c) )-->STATE_0348 ]
      { 
        if  ( isHex(c) )        // --308-- CAS[          STATE_0347--( isHex(c) )-->STATE_0348 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0348;   // nextState 
        }
        else if  ( c == '\n' )        // --309-- CAS[          STATE_0347--( c == '\n' )-->ACTION_0118 ]
        {
          state = ACTION_0118;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --310-- ERROR 
        }
      } 
    break; 
    case STATE_0348:                     // --310-- CAS[          STATE_0348--( isHex(c) )-->STATE_0349 ]
      { 
        if  ( isHex(c) )        // --310-- CAS[          STATE_0348--( isHex(c) )-->STATE_0349 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0349;   // nextState 
        }
        else if  ( c == '\n' )        // --311-- CAS[          STATE_0348--( c == '\n' )-->ACTION_0118 ]
        {
          state = ACTION_0118;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --312-- ERROR 
        }
      } 
    break; 
    case STATE_0349:                     // --312-- CAS[          STATE_0349--( isHex(c) )-->STATE_0350 ]
      { 
        if  ( isHex(c) )        // --312-- CAS[          STATE_0349--( isHex(c) )-->STATE_0350 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0350;   // nextState 
        }
        else if  ( c == '\n' )        // --313-- CAS[          STATE_0349--( c == '\n' )-->ACTION_0118 ]
        {
          state = ACTION_0118;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --314-- ERROR 
        }
      } 
    break; 
    case STATE_0350:                     // --314-- CAS[          STATE_0350--( c == '\n' )-->ACTION_0118 ]
      { 
        if  ( c == '\n' )        // --314-- CAS[          STATE_0350--( c == '\n' )-->ACTION_0118 ]
        {
          state = ACTION_0118;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --315-- ERROR 
        }
      } 
    break; 
    case STATE_0356:                     // --315-- CAS[          STATE_0356--( c == ':' )-->STATE_0357 ]
      { 
        if  ( c == ':' )        // --315-- CAS[          STATE_0356--( c == ':' )-->STATE_0357 ]
        {
          state = STATE_0357;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --316-- ERROR 
        }
      } 
    break; 
    case STATE_0357:                     // --316-- CAS[          STATE_0357--( isHex(c) )-->STATE_0358 ]
      { 
        if  ( isHex(c) )        // --316-- CAS[          STATE_0357--( isHex(c) )-->STATE_0358 ]
        {
          data = valueHex(c);;    // action 
          state = STATE_0358;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --317-- ERROR 
        }
      } 
    break; 
    case STATE_0358:                     // --317-- CAS[          STATE_0358--( isHex(c) )-->STATE_0359 ]
      { 
        if  ( isHex(c) )        // --317-- CAS[          STATE_0358--( isHex(c) )-->STATE_0359 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0359;   // nextState 
        }
        else if  ( c == '\n' )        // --318-- CAS[          STATE_0358--( c == '\n' )-->ACTION_0119 ]
        {
          state = ACTION_0119;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --319-- ERROR 
        }
      } 
    break; 
    case STATE_0359:                     // --319-- CAS[          STATE_0359--( isHex(c) )-->STATE_0360 ]
      { 
        if  ( isHex(c) )        // --319-- CAS[          STATE_0359--( isHex(c) )-->STATE_0360 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0360;   // nextState 
        }
        else if  ( c == '\n' )        // --320-- CAS[          STATE_0359--( c == '\n' )-->ACTION_0119 ]
        {
          state = ACTION_0119;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --321-- ERROR 
        }
      } 
    break; 
    case STATE_0360:                     // --321-- CAS[          STATE_0360--( isHex(c) )-->STATE_0361 ]
      { 
        if  ( isHex(c) )        // --321-- CAS[          STATE_0360--( isHex(c) )-->STATE_0361 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0361;   // nextState 
        }
        else if  ( c == '\n' )        // --322-- CAS[          STATE_0360--( c == '\n' )-->ACTION_0119 ]
        {
          state = ACTION_0119;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --323-- ERROR 
        }
      } 
    break; 
    case STATE_0361:                     // --323-- CAS[          STATE_0361--( isHex(c) )-->STATE_0362 ]
      { 
        if  ( isHex(c) )        // --323-- CAS[          STATE_0361--( isHex(c) )-->STATE_0362 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0362;   // nextState 
        }
        else if  ( c == '\n' )        // --324-- CAS[          STATE_0361--( c == '\n' )-->ACTION_0119 ]
        {
          state = ACTION_0119;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --325-- ERROR 
        }
      } 
    break; 
    case STATE_0362:                     // --325-- CAS[          STATE_0362--( isHex(c) )-->STATE_0363 ]
      { 
        if  ( isHex(c) )        // --325-- CAS[          STATE_0362--( isHex(c) )-->STATE_0363 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0363;   // nextState 
        }
        else if  ( c == '\n' )        // --326-- CAS[          STATE_0362--( c == '\n' )-->ACTION_0119 ]
        {
          state = ACTION_0119;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --327-- ERROR 
        }
      } 
    break; 
    case STATE_0363:                     // --327-- CAS[          STATE_0363--( isHex(c) )-->STATE_0364 ]
      { 
        if  ( isHex(c) )        // --327-- CAS[          STATE_0363--( isHex(c) )-->STATE_0364 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0364;   // nextState 
        }
        else if  ( c == '\n' )        // --328-- CAS[          STATE_0363--( c == '\n' )-->ACTION_0119 ]
        {
          state = ACTION_0119;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --329-- ERROR 
        }
      } 
    break; 
    case STATE_0364:                     // --329-- CAS[          STATE_0364--( isHex(c) )-->STATE_0365 ]
      { 
        if  ( isHex(c) )        // --329-- CAS[          STATE_0364--( isHex(c) )-->STATE_0365 ]
        {
          data = (data<<4) | valueHex(c);;    // action 
          state = STATE_0365;   // nextState 
        }
        else if  ( c == '\n' )        // --330-- CAS[          STATE_0364--( c == '\n' )-->ACTION_0119 ]
        {
          state = ACTION_0119;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --331-- ERROR 
        }
      } 
    break; 
    case STATE_0365:                     // --331-- CAS[          STATE_0365--( c == '\n' )-->ACTION_0119 ]
      { 
        if  ( c == '\n' )        // --331-- CAS[          STATE_0365--( c == '\n' )-->ACTION_0119 ]
        {
          state = ACTION_0119;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --332-- ERROR 
        }
      } 
    break; 
    case STATE_0366:                     // --332-- CAS[          STATE_0366--( c == 'i' )-->STATE_0367 ]
      { 
        if  ( c == 'i' )        // --332-- CAS[          STATE_0366--( c == 'i' )-->STATE_0367 ]
        {
          state = STATE_0367;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --333-- ERROR 
        }
      } 
    break; 
    case STATE_0367:                     // --333-- CAS[          STATE_0367--( c == 's' )-->STATE_0368 ]
      { 
        if  ( c == 's' )        // --333-- CAS[          STATE_0367--( c == 's' )-->STATE_0368 ]
        {
          state = STATE_0368;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --334-- ERROR 
        }
      } 
    break; 
    case STATE_0368:                     // --334-- CAS[          STATE_0368--( c == 'c' )-->STATE_0369 ]
      { 
        if  ( c == 'c' )        // --334-- CAS[          STATE_0368--( c == 'c' )-->STATE_0369 ]
        {
          state = STATE_0369;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --335-- ERROR 
        }
      } 
    break; 
    case STATE_0369:                     // --335-- CAS[          STATE_0369--( c == 'o' )-->STATE_0370 ]
      { 
        if  ( c == 'o' )        // --335-- CAS[          STATE_0369--( c == 'o' )-->STATE_0370 ]
        {
          state = STATE_0370;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --336-- ERROR 
        }
      } 
    break; 
    case STATE_0370:                     // --336-- CAS[          STATE_0370--( c == 'n' )-->STATE_0371 ]
      { 
        if  ( c == 'n' )        // --336-- CAS[          STATE_0370--( c == 'n' )-->STATE_0371 ]
        {
          state = STATE_0371;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --337-- ERROR 
        }
      } 
    break; 
    case STATE_0371:                     // --337-- CAS[          STATE_0371--( c == 'n' )-->STATE_0372 ]
      { 
        if  ( c == 'n' )        // --337-- CAS[          STATE_0371--( c == 'n' )-->STATE_0372 ]
        {
          state = STATE_0372;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --338-- ERROR 
        }
      } 
    break; 
    case STATE_0372:                     // --338-- CAS[          STATE_0372--( c == 'e' )-->STATE_0373 ]
      { 
        if  ( c == 'e' )        // --338-- CAS[          STATE_0372--( c == 'e' )-->STATE_0373 ]
        {
          state = STATE_0373;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --339-- ERROR 
        }
      } 
    break; 
    case STATE_0373:                     // --339-- CAS[          STATE_0373--( c == 'c' )-->STATE_0374 ]
      { 
        if  ( c == 'c' )        // --339-- CAS[          STATE_0373--( c == 'c' )-->STATE_0374 ]
        {
          state = STATE_0374;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --340-- ERROR 
        }
      } 
    break; 
    case STATE_0374:                     // --340-- CAS[          STATE_0374--( c == 't' )-->STATE_0375 ]
      { 
        if  ( c == 't' )        // --340-- CAS[          STATE_0374--( c == 't' )-->STATE_0375 ]
        {
          state = STATE_0375;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --341-- ERROR 
        }
      } 
    break; 
    case STATE_0375:                     // --341-- CAS[          STATE_0375--( c == '\n' )-->ACTION_0120 ]
      { 
        if  ( c == '\n' )        // --341-- CAS[          STATE_0375--( c == '\n' )-->ACTION_0120 ]
        {
          state = ACTION_0120;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --342-- ERROR 
        }
      } 
    break; 
    case STATE_0377:                     // --342-- CAS[          STATE_0377--( c == 'e' )-->STATE_0378 ]
      { 
        if  ( c == 'e' )        // --342-- CAS[          STATE_0377--( c == 'e' )-->STATE_0378 ]
        {
          state = STATE_0378;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --343-- ERROR 
        }
      } 
    break; 
    case STATE_0378:                     // --343-- CAS[          STATE_0378--( c == 'r' )-->STATE_0379 ]
      { 
        if  ( c == 'r' )        // --343-- CAS[          STATE_0378--( c == 'r' )-->STATE_0379 ]
        {
          state = STATE_0379;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --344-- ERROR 
        }
      } 
    break; 
    case STATE_0379:                     // --344-- CAS[          STATE_0379--( c == 's' )-->STATE_0380 ]
      { 
        if  ( c == 's' )        // --344-- CAS[          STATE_0379--( c == 's' )-->STATE_0380 ]
        {
          state = STATE_0380;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --345-- ERROR 
        }
      } 
    break; 
    case STATE_0380:                     // --345-- CAS[          STATE_0380--( c == 'i' )-->STATE_0381 ]
      { 
        if  ( c == 'i' )        // --345-- CAS[          STATE_0380--( c == 'i' )-->STATE_0381 ]
        {
          state = STATE_0381;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --346-- ERROR 
        }
      } 
    break; 
    case STATE_0381:                     // --346-- CAS[          STATE_0381--( c == 'o' )-->STATE_0382 ]
      { 
        if  ( c == 'o' )        // --346-- CAS[          STATE_0381--( c == 'o' )-->STATE_0382 ]
        {
          state = STATE_0382;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --347-- ERROR 
        }
      } 
    break; 
    case STATE_0382:                     // --347-- CAS[          STATE_0382--( c == 'n' )-->STATE_0383 ]
      { 
        if  ( c == 'n' )        // --347-- CAS[          STATE_0382--( c == 'n' )-->STATE_0383 ]
        {
          state = STATE_0383;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --348-- ERROR 
        }
      } 
    break; 
    case STATE_0383:                     // --348-- CAS[          STATE_0383--( c == '?' )-->STATE_0384 ]
      { 
        if  ( c == '?' )        // --348-- CAS[          STATE_0383--( c == '?' )-->STATE_0384 ]
        {
          state = STATE_0384;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --349-- ERROR 
        }
      } 
    break; 
    case STATE_0384:                     // --349-- CAS[          STATE_0384--( c == '\n' )-->ACTION_0121 ]
      { 
        if  ( c == '\n' )        // --349-- CAS[          STATE_0384--( c == '\n' )-->ACTION_0121 ]
        {
          state = ACTION_0121;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --350-- ERROR 
        }
      } 
    break; 
    case STATE_0386:                     // --350-- CAS[          STATE_0386--( c == 'r' )-->STATE_0387 ]
      { 
        if  ( c == 'r' )        // --350-- CAS[          STATE_0386--( c == 'r' )-->STATE_0387 ]
        {
          state = STATE_0387;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --351-- ERROR 
        }
      } 
    break; 
    case STATE_0387:                     // --351-- CAS[          STATE_0387--( c == 'r' )-->STATE_0388 ]
      { 
        if  ( c == 'r' )        // --351-- CAS[          STATE_0387--( c == 'r' )-->STATE_0388 ]
        {
          state = STATE_0388;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --352-- ERROR 
        }
      } 
    break; 
    case STATE_0388:                     // --352-- CAS[          STATE_0388--( c == '?' )-->STATE_0389 ]
      { 
        if  ( c == '?' )        // --352-- CAS[          STATE_0388--( c == '?' )-->STATE_0389 ]
        {
          state = STATE_0389;   // nextState 
        }
        else {
          state = STATE_ERROR;                // --353-- ERROR 
        }
      } 
    break; 
    case STATE_0389:                     // --353-- CAS[          STATE_0389--( c == '\n' )-->ACTION_0122 ]
      { 
        if  ( c == '\n' )        // --353-- CAS[          STATE_0389--( c == '\n' )-->ACTION_0122 ]
        {
          state = ACTION_0122;   // nextState 
        }
        else {
          state = STATE_ERROR; // ERROR 
        }
      } 
    break; 
} // end switch state

// -----------------------------
// final action for the commands
  switch ( state ) {
  case ACTION_0100: 
    {
      // o%p,%v
       
{
    if (debug & 1){
        printDebug_o_port_value();
    }
    // LOW == 0, HIGH == 1
    digitalWrite(port, value);
} 

       state = STATE_START;
    }
  break; 
  case ACTION_0101: 
    {
      // oa%p,%v
       
{
    if (debug & 1){
        printDebug_oa_port_value();
    }
    // LOW == 0, HIGH == 1
    analogDigitalWrite(port, value);
} 

       state = STATE_START;
    }
  break; 
  case ACTION_0102: 
    {
      // p%p,%v
      
{
    if (debug & 1){
        printDebug_p_port_value();
    }
    if ( pwms & (1 << port)){
        analogWrite(port, value);
    }
}

       state = STATE_START;
    }
  break; 
  case ACTION_0103: 
    {
      // s%p,%v
       
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

       state = STATE_START;
    }
  break; 
  case ACTION_0104: 
    {
      // help
      
{
    for ( int k = 0; TRUE; k ++ ) {
        char  c =  pgm_read_byte_near(helpText + k);
        if ( c == 0 ) break;
        Serial.print(c);
    }
}

       state = STATE_START;
    }
  break; 
  case ACTION_0105: 
    {
      // cident:%s
      
{
    setEEPROM();
    if ( debug & 1) {
        Serial.print(F( "cident=") );
        Serial.println(id);
    }
}

       state = STATE_START;
    }
  break; 
  case ACTION_0106: 
    {
      // cident:
      
{
    // reset ident
    id[0] = 0;
    setEEPROM();
    if ( debug & 1) {
        Serial.print(F( "cident=") );
        Serial.println(id);
    }
}

       state = STATE_START;
    }
  break; 
  case ACTION_0107: 
    {
      // cident?
      
{
    getEEPROM();
}

       state = STATE_START;
    }
  break; 
  case ACTION_0108: 
    {
      // cdebug:%x
       
{
    stateMachine(STATEMACHINE_EVENT_CONFIG);
    debug =  data ;
    if ( debug & 1) {
        printDebug_cdebug();
    }

 }

       state = STATE_START;
    }
  break; 
  case ACTION_0109: 
    {
      // cdin:%x
      
{
    stateMachine(STATEMACHINE_EVENT_CONFIG);
    setDigitalInput(data);
    if ( debug & 1) {
        printDebug_cdin();
    }
}

       state = STATE_START;
    }
  break; 
  case ACTION_0110: 
    {
      // cdinp:%x
       
{
    stateMachine(STATEMACHINE_EVENT_CONFIG);
    setDigitalInputPullup(data);
    if ( debug & 1) {
        printDebug_cdinp();
        
    }
}

       state = STATE_START;
    }
  break; 
  case ACTION_0111: 
    {
      // cdcnt:%x
      
{
    stateMachine(STATEMACHINE_EVENT_CONFIG);
    setDigitalCount(data);
    if ( debug & 1) {
        printDebug_cdcnt();
    }
}

       state = STATE_START;
    }
  break; 
  case ACTION_0112: 
    {
      // cdcntp:%x
       
{
    stateMachine(STATEMACHINE_EVENT_CONFIG);
    setDigitalCountPullup(data);
    if ( debug & 1) {
        printDebug_cdcntp();
        
    }
}

       state = STATE_START;
    }
  break; 
  case ACTION_0113: 
    {
      // cdout:%x
      
{
    setDigitalOutput(data);
    stateMachine(STATEMACHINE_EVENT_CONFIG);
    if ( debug & 1) {
        printDebug_cdout();
    }
}

       state = STATE_START;
    }
  break; 
  case ACTION_0114: 
    {
      // cdpwm:%x
      
{
    pwms = data;
    stateMachine(STATEMACHINE_EVENT_CONFIG);
    setDigitalPWMOutput(data);
    if ( debug & 1) {
        printDebug_cdpwm();
    }
}

       state = STATE_START;
    }
  break; 
  case ACTION_0115: 
    {
      // cdservo:%x
       
{
    servos = data;
    stateMachine(STATEMACHINE_EVENT_CONFIG);
    setDigitalServoOutput(data);
    if ( debug & 1) {
        printDebug_cdservo();
        
    }
}

       state = STATE_START;
    }
  break; 
  case ACTION_0116: 
    {
      // caain:%x
      
{ 
    analogAnalogInputs = data;
    stateMachine(STATEMACHINE_EVENT_CONFIG);
    if ( debug & 1) {
        printDebug_caain();
    }
}

       state = STATE_START;
    }
  break; 
  case ACTION_0117: 
    {
      // cadin:%x
      
{ 
    setAnalogDigitalInput(data);
    stateMachine(STATEMACHINE_EVENT_CONFIG);
    if ( debug & 1) {
        printDebug_cadin();
    }
}

       state = STATE_START;
    }
  break; 
  case ACTION_0118: 
    {
      // cadout:%x
      
{ 
    setAnalogDigitalOutput(data);
    stateMachine(STATEMACHINE_EVENT_CONFIG);
    if ( debug & 1) {
        printDebug_cadout();
    }
}

       state = STATE_START;
    }
  break; 
  case ACTION_0119: 
    {
      // cadinp:%x
      
{ 
    setAnalogDigitalInputPullup(data);
    stateMachine(STATEMACHINE_EVENT_CONFIG);
    if ( debug & 1) {
        printDebug_cadinp();
    }
}

       state = STATE_START;
    }
  break; 
  case ACTION_0120: 
    {
      // disconnect
      
{ 
    stateMachine(STATEMACHINE_EVENT_DISCONNECT);
    Serial.println( F("disconnect"));
}

       state = STATE_START;
    }
  break; 
  case ACTION_0121: 
    {
      // cversion?
      
{
    Serial.print( F("v:"));
    Serial.println(version);
}

       state = STATE_START;
    }
  break; 
  case ACTION_0122: 
    {
      // cerr?
      
{
    Serial.print( F("e:"));
    Serial.println(errorCount);
}

       state = STATE_START;
    }
  break; 
  // -----------------------------
  // Terminal State
  case STATE_TERMINAL: 
    {
      state = STATE_START;
    }
  break; 
  // -----------------------------
  // Error counter
  case STATE_ERROR: 
    {
      errorCount ++;
       state = STATE_START;
    }
  break; 
  }
    //--END

  } // is Serial.available()

  if ( STATEMACHINE_waitState == S2000 ) {
    if (milliSecond) {

#define HANDLE_COUNTER_INPUT(PIN)              \
  if ( digitalCounters & ( 1 <<  PIN) ) {      \
    handle_counter_debouncing( PIN);           \
  }

      // counters are run each millisecond because debouncing is needed

      HANDLE_COUNTER_INPUT( 2)
      HANDLE_COUNTER_INPUT( 3)
      HANDLE_COUNTER_INPUT( 4)
      HANDLE_COUNTER_INPUT( 5)
      HANDLE_COUNTER_INPUT( 6)
      HANDLE_COUNTER_INPUT( 7)
      HANDLE_COUNTER_INPUT( 8)
      HANDLE_COUNTER_INPUT( 9)
      HANDLE_COUNTER_INPUT(10)
      HANDLE_COUNTER_INPUT(11)
      HANDLE_COUNTER_INPUT(12)

#define HANDLE_DIGITAL_INPUT(CNT,PIN)          \
  if ( digital_input_count == CNT )            \
    if ( digitalInputs & ( 1 <<  PIN) )        \
      handleInput( PIN, digitalRead( PIN ) );

      HANDLE_DIGITAL_INPUT( 0,  2)
      HANDLE_DIGITAL_INPUT( 1,  3)
      HANDLE_DIGITAL_INPUT( 2,  4)
      HANDLE_DIGITAL_INPUT( 3,  5)
      HANDLE_DIGITAL_INPUT( 4,  6)
      HANDLE_DIGITAL_INPUT( 5,  7)
      HANDLE_DIGITAL_INPUT( 6,  8)
      HANDLE_DIGITAL_INPUT( 7,  9)
      HANDLE_DIGITAL_INPUT( 8, 10)
      HANDLE_DIGITAL_INPUT( 9, 11)
      HANDLE_DIGITAL_INPUT(10, 12)

#define HANDLE_ANALOG_DIGITAL_INPUT(CNT,OFFSET,PIN) \
  if ( digital_input_count == CNT ) \
    if ( analogDigitalInputs & ( 1 <<  OFFSET) ) handleAnalogDigitalInput( OFFSET, digitalRead( PIN ) );

      HANDLE_ANALOG_DIGITAL_INPUT ( 11, 0, A0)
      HANDLE_ANALOG_DIGITAL_INPUT ( 12, 1, A1)
      HANDLE_ANALOG_DIGITAL_INPUT ( 13, 2, A2)
      HANDLE_ANALOG_DIGITAL_INPUT ( 14, 3, A3)
      HANDLE_ANALOG_DIGITAL_INPUT ( 15, 4, A4)
      HANDLE_ANALOG_DIGITAL_INPUT ( 16, 5, A5)
      // no digital input on A6, A7

#define HANDLE_ANALOG_ANALOG_INPUT(ACNT,OFFSET,PIN) \
  if ( analogAnalogInputs & ( 1 <<  OFFSET) ) {        \
    if (analog_input_count == ACNT + 0) {  aval  = analogRead( PIN ); } \
    if (analog_input_count == ACNT + 1) {  aval += analogRead( PIN ); } \
    if (analog_input_count == ACNT + 2) {  aval += analogRead( PIN );   \
      handleAnalogAnalogInput( OFFSET, aval / 3);  } \
  }

      HANDLE_ANALOG_ANALOG_INPUT( 17, 0, A0)
      HANDLE_ANALOG_ANALOG_INPUT( 20, 1, A1)
      HANDLE_ANALOG_ANALOG_INPUT( 23, 2, A2)
      HANDLE_ANALOG_ANALOG_INPUT( 26, 3, A3)
      HANDLE_ANALOG_ANALOG_INPUT( 29, 4, A4)
      HANDLE_ANALOG_ANALOG_INPUT( 32, 5, A5)
      HANDLE_ANALOG_ANALOG_INPUT( 35, 6, A6)
      HANDLE_ANALOG_ANALOG_INPUT( 38, 7, A7)
      //

#define REPORT_COUNTER(ACNT,PIN)                        \
  if (analog_input_count == ACNT){                      \
    if ( digitalCounters & ( 1 <<  PIN) ) {             \
      if ( counter_cnt_prev[PIN] != counter_cnt[PIN]) { \
        counter_cnt_prev[PIN] = counter_cnt[PIN];       \
        Serial.print(F("c"));                           \
        Serial.print(F( #PIN ) );                       \
        Serial.print(F(","));                           \
        Serial.println(counter_cnt[PIN] , HEX);         \
      }                                                 \
    }                                                   \
  }

      REPORT_COUNTER( 41,  2)
      REPORT_COUNTER( 42,  3)
      REPORT_COUNTER( 43,  4)
      REPORT_COUNTER( 44,  5)
      REPORT_COUNTER( 45,  6)
      REPORT_COUNTER( 46,  7)
      REPORT_COUNTER( 47,  8)
      REPORT_COUNTER( 48,  9)
      REPORT_COUNTER( 49, 10)
      REPORT_COUNTER( 50, 11)
      REPORT_COUNTER( 51, 12)

      analog_input_count ++;

      if (analog_input_count > 100)
        analog_input_count = 0;
      digital_input_count = analog_input_count % 50;

    }
  }
}

