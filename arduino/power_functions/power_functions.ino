# --------------------------------------------------------------------------------------------
    # Copyright (C) 2016  Gerhard Hepp
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
#include <Servo.h>
#include "PowerFunctions.h"
char version[] = "powerfunction, version 2016-03-25";

char help[] =
  "Control powerfunctions from LEGO\n" //
  "P<address><channel><bbbb><aaaa> PWM Command\n" //
  "  address 0, 1 \n" //
  "  channel 1..4 \n" //
  "  bbbb,aaaa 0..9 A-F" //
  "D<d> set debug\n" //
  "  d 0,1\n" //
  "H help\n";


#define TRUE (1==1)
#define FALSE (1==0)

int CHANNEL = 4;

PowerFunctions pf(12, 4);

int ledPin = 13;
int output = 12;
uint8_t debug = 0;

void setup() {
  Serial.begin(115200);

  Serial.println("arduino sending@115200 Bd");
  Serial.println(version);

  pinMode(10, OUTPUT);
  pinMode(13, OUTPUT);
}
int state = 0;
uint8_t mState = 0;
uint8_t transfer = 0;
//
// channel delay table
//
uint8_t channelStep = 0;

uint8_t channelTODO[42] = {
  4, // 0
  3, // 1
  2, // 2
  1, // 3
  0, // 4
  4, // 5
  3, // 6
  2, // 7
  1, // 8
  0, // 9
  4, // 10
  3, // 11
  2, // 12
  1, // 13
  0, // 14
  0, // 15
  0, // 16
  0, // 17
  0, // 18
  0, // 19
  0, // 20
  1, // 21
  2, // 22
  3, // 23
  4, // 24
  0, // 25
  0, // 26
  0, // 27
  0, // 28
  1, // 29
  0, // 30
  0, // 31
  2, // 32
  0, // 33
  0, // 34
  3, // 35
  0, // 36
  0, // 37
  4, // 38
  0, // 39
  0, // 40
  99
};

int address;
int channel;
int bbbb;
int aaaa;

uint16_t channelCommands[4] = { 0, 0, 0, 0};
uint16_t channelSend [4] = { 0, 0, 0, 0};

#define PWM_FLOAT 0
#define PWM_F_1 1
#define PWM_F_2 2
#define PWM_F_3 3
#define PWM_F_4 4
#define PWM_F_5 5
#define PWM_F_6 6
#define PWM_F_7 7
#define PWM_BRAKE_THEN_FLOAT 8
#define PWM_B_7 9
#define PWM_B_6 10
#define PWM_B_5 11
#define PWM_B_4 12
#define PWM_B_3 13
#define PWM_B_2 14
#define PWM_B_1 15

uint8_t pwmCommand( uint8_t c) {
  switch (c) {
    case '0':
      return PWM_FLOAT;
    case '1':
      return PWM_F_1;
    case '2':
      return PWM_F_2;
    case '3':
      return PWM_F_3;
    case '4':
      return PWM_F_4;
    case '5':
      return PWM_F_5;
    case '6':
      return PWM_F_6;
    case '7':
      return PWM_F_7;

    case '8':
      return PWM_BRAKE_THEN_FLOAT;
    
    case '9':
      return PWM_B_7;
    case 'A':
      return PWM_B_6;
    case 'B':
      return PWM_B_5;
    case 'C':
      return PWM_B_4;
    case 'D':
      return PWM_B_3;
    case 'E':
      return PWM_B_2;
    case 'F':
      return PWM_B_1;
  }
}

#define CHECKSUM() (0xf ^ _nib1 ^ _nib2 ^ _nib3)

void loop() {
  if ( Serial.available()) {
    char c = Serial.read();

    switch ( state)
    {
      // ------------------------
      case 0: {
          switch (c) {
            case 'P': state = 1;
              break;
            case 'D': state = 11;
              break;
            case 'H': state = 21;
              break;
            default:
              state = 9999;
              break;
          }
        } break;
      // -------------------------
      // DEBUG setting '0', '1'
      case 11: {
          switch (c) {
            case '0':

              state = 12;
              break;
            case '1':
              debug = 1;
              state = 13;
              break;
            default: state = 9999;
              break;
          }
        } break;
      case 12: {
          switch (c) {
            case '\n':
              if (debug == 1 )
                Serial.println("D 0");
              debug = 0;
              state = 0;

              break;
            default: state = 9999;
              break;
          }
        } break;

      case 13: {
          switch (c) {
            case '\n':
              debug = 1;
              state = 0;
              Serial.println("D 1");
              break;
            default: state = 9999;
              break;
          }
        } break;
      // -------------------------
      // HELP
      case 21: {
          switch (c) {
            case '\n':

              state = 0;
              Serial.println(help);
              break;
            default: state = 9999;
              break;
          }
        } break;

      // -------------------------
      //
      case 1: {
          switch (c) {
            case '0':
              address = 0;
              state = 3;
              break;
            case '1':
              address = 1;
              state = 3;
              break;
            default: state = 9999;
              break;
          }
        } break;
      // -------------------------
      case 3: {
          switch (c) {
            case '1':
              channel = 1;
              state = 4;
              break;
            case '2':
              channel = 2;
              state = 4;
              break;
            case '3':
              channel = 3;
              state = 4;
              break;
            case '4':
              channel = 4;
              state = 4;
              break;
            default: state = 9999;
              break;
          }
        } break;
      // -------------------------
      case 4: {
          switch (c) {
            case '0':
              bbbb = 0;
              state = 5;
              break;
            case '1':
              bbbb = 1;
              state = 5;
              break;
            case '2':
              bbbb = 2;
              state = 5;
              break;
            case '3':
              bbbb = 3;
              state = 5;
              break;
            case '4':
              bbbb = 4;
              state = 5;
              break;
            case '5':
              bbbb = 5;
              state = 5;
              break;
            case '6':
              bbbb = 6;
              state = 5;
              break;
            case '7':
              bbbb = 7;
              state = 5;
              break;
            case '8':
              bbbb = 8;
              state = 5;
              break;
            case '9':
              bbbb = 9;
              state = 5;
              break;
            case 'A':
              bbbb = 10;
              state = 5;
              break;
            case 'B':
              bbbb = 11;
              state = 5;
              break;
            case 'C':
              bbbb = 12;
              state = 5;
              break;
            case 'D':
              bbbb = 13;
              state = 5;
              break;
            case 'E':
              bbbb = 14;
              state = 5;
              break;
            case 'F':
              bbbb = 15;
              state = 5;
              break;
            default: state = 9999;
              break;
          }
        } break;
      // -------------------------
      case 5: {
          switch (c) {
            case '0':
              aaaa = 0;
              state = 6;
              break;
            case '1':
              aaaa = 1;
              state = 6;
              break;
            case '2':
              aaaa = 2;
              state = 6;
              break;
            case '3':
              aaaa = 3;
              state = 6;
              break;
            case '4':
              aaaa = 4;
              state = 6;
              break;
            case '5':
              aaaa = 5;
              state = 6;
              break;
            case '6':
              aaaa = 6;
              state = 6;
              break;
            case '7':
              aaaa = 7;
              state = 6;
              break;
            case '8':
              aaaa = 8;
              state = 6;
              break;
            case '9':
              aaaa = 9;
              state = 6;
              break;
            case 'A':
              aaaa = 10;
              state = 6;
              break;
            case 'B':
              aaaa = 11;
              state = 6;
              break;
            case 'C':
              aaaa = 12;
              state = 6;
              break;
            case 'D':
              aaaa = 13;
              state = 6;
              break;
            case 'E':
              aaaa = 14;
              state = 6;
              break;
            case 'F':
              aaaa = 15;
              state = 6;
              break;
            default: state = 9999;
              break;
          }
        } break;
      // -------------------------
      case 6: {
          switch (c) {
            case 0x0a:
              {
                if (debug) {
                  char _buf[100];
                  sprintf(_buf, "P %d %d b %d a %d ", address, channel, bbbb, aaaa);
                  Serial.println(_buf);
                }
                uint8_t _nib1 = ESCAPE | (channel - 1);
                uint8_t _nib2 = pwmCommand( bbbb);
                uint8_t _nib3 = pwmCommand( aaaa);

                channelCommands[channel - 1] = (_nib1 << 12) | (_nib2 << 8) | (_nib3 << 4) | CHECKSUM();
                transfer = TRUE;
                state = 0;
              }
              break;
            default: {
                state = 9999;
              }
              break;
          }
        }
        break;
      case 9999: {
          if ( debug == 1 ) {
            Serial.println("wrong command");
          }
          state = 0;
        }
        break;
    }
  }


  digitalWrite(10, !digitalRead(10) );
  if (mState == 0) {

    if ( (millis () & 0x07 ) == 0 ) {
      mState = 1;

      if (transfer == TRUE) {
        transfer = FALSE;
        channelStep = 0;

          channelSend[0] = channelCommands[0];
          channelSend[1] = channelCommands[1];
          channelSend[2] = channelCommands[2];
          channelSend[3] = channelCommands[3];
      }
    }

    switch ( channelTODO[ channelStep ] ) {
      //
      // setting the PIN13 is for debug only
      //
      case 1: digitalWrite(13, HIGH); pf.sendData ( channelSend[0] ); digitalWrite(13, LOW); channelStep++; break;
      case 2: digitalWrite(13, HIGH); pf.sendData ( channelSend[1] ); digitalWrite(13, LOW); channelStep++; break;
      case 3: digitalWrite(13, HIGH); pf.sendData ( channelSend[2] ); digitalWrite(13, LOW); channelStep++; break;
      case 4: digitalWrite(13, HIGH); pf.sendData ( channelSend[3] ); digitalWrite(13, LOW); channelStep++; break;
      case 0: channelStep++; break;
      case 99: break;
      default:  break;
    }
  }
  else {
    if ( (millis () & 0x07 ) != 0 ) {
      mState = 0;
    }
  }
}





