/*
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

#include <avr/pgmspace.h>

#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
#include <avr/power.h>
#endif
#define TRUE (1==1)

// ----------------------------------------
// Adjust for your neopixel array

#define PIN 6
#define NUM_LEDS 64
#define BRIGHTNESS 255

Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, PIN, NEO_GRB + NEO_KHZ800);
// ----------------------------------------

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  // This is for Trinket 5V 16MHz, you can remove these three lines if you are not using a Trinket
#if defined (__AVR_ATtiny85__)
  if (F_CPU == 16000000) clock_prescale_set(clock_div_1);
#endif
  // End of trinket special code


  strip.setBrightness(BRIGHTNESS);
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'

  Serial.println("neopixel driver 2016-12-22");
}

const char helpText[] PROGMEM =
  "all red  : red  \n"
  "all green: green \n"
  "all blue : blue  \n"
  "\n"
  "set pixel: s,NNN,RRR,GGG,BBB in dezimals e.g. s,029,000,000,255\n"
  "show     : show  \n"
  "w        : show  \n"
  "\n"
  "clear    : clear \n"
  "help     : help  \n"
  "echo     : echo  \n"
  "";

char buffer[64 + 1];
int iBuffer = 0;

int getInteger( char * s) {
  int ret = (( (s[0] - '0') * 10 + (s[1] - '0') ) * 10 + (s[2] - '0'));
  return ret;
}
void loop() {
  if ( Serial.available() > 0) {
    int inByte = Serial.read();
    if (iBuffer < 64) {
      buffer[iBuffer++] = inByte;
      buffer[iBuffer] = '\0';

    }
    if ( inByte == '\n' ) {
      // have received a full command string

#if 0
      Serial.println("check command");
#endif
      // set command
      if ( 0 == strncmp( "s,", buffer, 2 )) {
#if 0
        int n = getInteger( buffer + 2 );
        int r = getInteger( buffer + 6 );
        int g = getInteger( buffer + 10 );
        int b = getInteger( buffer + 14 );

        if ( n < strip.numPixels() ) {
          strip.setPixelColor(n, strip.Color(r, g, b) );
        }
        Serial.println(buffer);
#else
        //
        // this parser allows for
        // s,[0-9]{0-3},([0-9]{0-3})?,([0-9]{0-3})?,([0-9]{0-3})?
        // which reduces number of bytes sent 
        //
        int n = 0;
        int r = 0;
        int g = 0;
        int b = 0;
        int state = 0;
        char * s = buffer + 2;
        while (state < 1000) {
          switch ( state) {
            case 0:
              switch ( *s) {
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
                  n = *s - '0';
                  state = 10;
                  break;
                case ',':
                  state = 100;
                  break;
                case 0x0a:
                case 0x0d:
                  state = 1000;
                  break;
                default:
                  state = 1001;
              }
              break;

            case 10:
              switch ( *s) {
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
                  n = n * 10 + *s - '0';
                  state = 20;
                  break;
                case ',':
                  state = 100;
                  break;
                case 0x0a:
                case 0x0d:
                  state = 1000;
                  break;
                default:
                  state = 1001;
              }
              break;
            case 20:
              switch ( *s) {
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
                  n = n * 10 + *s - '0';
                  state = 30;
                  break;
                case ',':
                  state = 100;
                  break;
                case 0x0a:
                case 0x0d:
                  state = 1000;
                  break;
                default:
                  state = 1001;
              }
              break;
            case 30:
              switch ( *s) {
                case ',':
                  state = 100;
                  break;
                case 0x0a:
                case 0x0d:
                  state = 1000;
                  break;
                default:
                  state = 1001;
              }
              break;
            // ------------ red
            case 100:
              switch ( *s) {
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
                  r = *s - '0';
                  state = 110;
                  break;
                case ',':
                  state = 200;
                  break;
                case 0x0a:
                case 0x0d:
                  state = 1000;
                  break;
                default:
                  state = 1001;
              }
              break;

            case 110:
              switch ( *s) {
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
                  r = r * 10 + *s - '0';
                  state = 120;
                  break;
                case ',':
                  state = 200;
                  break;
                case 0x0a:
                case 0x0d:
                  state = 1000;
                  break;
                default:
                  state = 1001;
              }
              break;
            case 120:
              switch ( *s) {
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
                  r = r * 10 + *s - '0';
                  state = 130;
                  break;
                case ',':
                  state = 200;
                  break;
                case 0x0a:
                case 0x0d:
                  state = 1000;
                  break;
                default:
                  state = 1001;
              }
              break;
            case 130:
              switch ( *s) {
                case ',':
                  state = 200;
                  break;
                case 0x0a:
                case 0x0d:
                  state = 1000;
                  break;
                default:
                  state = 1001;
              }
              break;

            // ------------ green
            case 200:
              switch ( *s) {
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
                  g = *s - '0';
                  state = 210;
                  break;
                case ',':
                  state = 300;
                  break;
                case 0x0a:
                case 0x0d:
                  state = 1000;
                  break;
                default:
                  state = 1001;
              }
              break;

            case 210:
              switch ( *s) {
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
                  g = g * 10 + *s - '0';
                  state = 220;
                  break;
                case ',':
                  state = 300;
                  break;
                case 0x0a:
                case 0x0d:
                  state = 1000;
                  break;
                default:
                  state = 1001;
              }
              break;
            case 220:
              switch ( *s) {
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
                  g = g * 10 + *s - '0';
                  state = 230;
                  break;
                case ',':
                  state = 300;
                  break;
                case 0x0a:
                case 0x0d:
                  state = 1000;
                  break;
                default:
                  state = 1001;
              }
              break;
            case 230:
              switch ( *s) {
                case ',':
                  state = 300;
                  break;
                case 0x0a:
                case 0x0d:
                  state = 1000;
                  break;
                default:
                  state = 1001;
              }
              break;



            // ------------ blue
            case 300:
              switch ( *s) {
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
                  b = *s - '0';
                  state = 310;
                  break;
                case ' ':
                case 0x0a:
                case 0x0d:
                  state = 1000;
                  break;
                default:
                  state = 1001;
              }
              break;

            case 310:
              switch ( *s) {
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
                  b = b * 10 + *s - '0';
                  state = 320;
                  break;
                case ' ':
                case 0x0a:
                case 0x0d:
                  state = 1000;
                  break;
                default:
                  state = 1001;
              }
              break;
            case 320:
              switch ( *s) {
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
                  b = b * 10 + *s - '0';
                  state = 1000;
                  break;
                case ' ':
                case 0x0a:
                case 0x0d:
                  state = 1000;
                  break;
                default:
                  state = 1001;
              }
              break;
          }

          s++;
        }
        if ( state == 1001 ) {
          Serial.println(buffer);
        }
        if ( state == 1000 ) {
          if ( n < strip.numPixels() ) {
            strip.setPixelColor(n, strip.Color(r, g, b) );
          }
          Serial.println(buffer);
        }
#endif
#if 0
        Serial.print("n = "); Serial.println(n);
        Serial.print("r = "); Serial.println(r);
        Serial.print("g = "); Serial.println(g);
        Serial.print("b = "); Serial.println(b);
#endif


      }

      if ( 0 == strncmp( "show", buffer, 4 )) {
        strip.show();
        Serial.println(buffer);
      }
     if ( 0 == strncmp( "w", buffer, 1 )) {
        strip.show();
        Serial.println(buffer);
      }

      if ( 0 == strncmp( "clear", buffer, 5 )) {
        strip.clear();
        strip.show();
        Serial.println(buffer);
      }

      if ( 0 == strncmp( "red", buffer, 3 )) {
        for ( int n = 0; n < strip.numPixels(); n ++ ) {
          strip.setPixelColor(n, strip.Color(255, 0, 0) );
        }
        strip.show();
        Serial.println(buffer);
      }

      if ( 0 == strncmp( "green", buffer, 5 )) {
        for ( int n = 0; n < strip.numPixels(); n ++ ) {
          strip.setPixelColor(n, strip.Color(0, 255, 0) );
        }
        strip.show();
        Serial.println(buffer);
      }

      if ( 0 == strncmp( "blue", buffer, 4 )) {
        for ( int n = 0; n < strip.numPixels(); n ++ ) {
          strip.setPixelColor(n, strip.Color(0, 0, 255) );
        }
        strip.show();
        Serial.println(buffer);
      }

      if ( 0 == strncmp( "echo", buffer, 4 )) {
        Serial.println(buffer);
      }

      if ( 0 == strncmp( "help", buffer, 4 )) {
        for ( int k = 0; TRUE; k ++ ) {
          char  c =  pgm_read_byte_near(helpText + k);
          if ( c == 0 ) break;
          Serial.print(c);
        }
        Serial.println(buffer);
      }
      iBuffer = 0;
    }
  }
}
