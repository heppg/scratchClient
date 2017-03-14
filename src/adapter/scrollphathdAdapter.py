# -*- coding: utf-8 -*-

# Adapter based on code from pimoroni.com,
# scrollphat-hd - version 0.1.3
#

import re
import sys
import adapter
import font

try:
    import numpy
except ImportError:
    exit("""
    This library requires the numpy module
      Install with: 
          sudo pip install numpy
          sudo pip3 install numpy
    """)

try:
    import smbus
except ImportError:
    if sys.version_info[0] < 3:
        exit("This library requires python-smbus\nInstall with: sudo apt-get install python-smbus")
    elif sys.version_info[0] == 3:
        exit("This library requires python3-smbus\nInstall with: sudo apt-get install python3-smbus")

import logging
logger = logging.getLogger(__name__)


debug = False

        
import math
import time



class IS31FL3731:
    _MODE_REGISTER = 0x00
    _FRAME_REGISTER = 0x01
    _AUTOPLAY1_REGISTER = 0x02
    _AUTOPLAY2_REGISTER = 0x03
    _BLINK_REGISTER = 0x05
    _AUDIOSYNC_REGISTER = 0x06
    _BREATH1_REGISTER = 0x08
    _BREATH2_REGISTER = 0x09
    _SHUTDOWN_REGISTER = 0x0a
    _GAIN_REGISTER = 0x0b
    _ADC_REGISTER = 0x0c
    
    _CONFIG_BANK = 0x0b
    _BANK_ADDRESS = 0xfd
    
    _PICTURE_MODE = 0x00
    _AUTOPLAY_MODE = 0x08
    _AUDIOPLAY_MODE = 0x18
    
    _ENABLE_OFFSET = 0x00
    _BLINK_OFFSET = 0x12
    _COLOR_OFFSET = 0x24

    def __init__(self, i2c, address=0x74):
        self.address = address
        self.i2c = i2c
        
    def display_initialization(self):
        # Switch to configuration bank
        self._bank(self._CONFIG_BANK)

        # Switch to Picture Mode
        self.i2c.write_i2c_block_data(self.address, self._MODE_REGISTER, [self._PICTURE_MODE])

        # Disable audio sync
        self.i2c.write_i2c_block_data(self.address, self._AUDIOSYNC_REGISTER, [0])

        self._bank(1)
        self.i2c.write_i2c_block_data(self.address, 0, [255] * 17)

        # Switch to bank 0 ( frame 0 )
        self._bank(0)

        # Enable all LEDs
        self.i2c.write_i2c_block_data(self.address, 0, [255] * 17)
        
    def _reset(self):
        self._sleep(True)
        time.sleep(0.00001)
        self._sleep(False)

    def _sleep(self, value):
        return self._register(self._CONFIG_BANK, self._SHUTDOWN_REGISTER, not value)

    def _frame(self, frame=None, show=True):
        if frame is None:
            return self._current_frame

        if not 0 <= frame <= 8:
            raise ValueError("Frame out of range: 0-8")

        self._current_frame = frame

        if show:
            self._register(self._CONFIG_BANK, self._FRAME_REGISTER, frame);

    def _bank(self, bank=None):
        """Switch display driver memory bank"""

        if bank is None:
            return self.i2c.readfrom_mem(self.address, self._BANK_ADDRESS, 1)[0]

        self.i2c.write_i2c_block_data(self.address, self._BANK_ADDRESS, [bank])

    def _register(self, bank, register, value=None):
        """Write display driver register"""

        self._bank(bank)

        if value is None:
            return self.i2c.readfrom_mem(self.address, register, 1)[0]

        #print "reg", value

        self.i2c.write_i2c_block_data(self.address, register, [value])
        
    def displayOutput(self, output):
        if debug:
            print("displayOutput")
            # print(output)
            
            # Belegung x=0,y=0 = 1
            # Belegung x=0,y=1 = 2
            # ..
            # Belegung x=0,y=6 = 6
            # Belegung x=1,y=0 = 7
            # ..
            # Belegung x=16,y=6 = 119
            
            # dann ist die Ausgabe wie folgt:
            #
            #  0   0..  7    63,  62,  61,  60,  59,  58,  57,   0,   x == 8
            #  1   8.. 15    64,  65,  66,  67,  68,  69,  70,   0,           x == 9
            #  2  16.. 31    56,  55,  54,  53,  52,  51,  50,   0,   x == 7
            #  3  32.. 39    71,  72,  73,  74,  75,  76,  77,   0,           x == 10
            #     
            #  4  40.. 47    49,  48,  47,  46,  45,  44,  43,   0,   x == 6
            #  5  48.. 55    78,  79,  80,  81,  82,  83,  84,   0,           x == 11
            #  6  56.. 63    42,  41,  40,  39,  38,  37,  36,   0,   x == 5
            #  7  64.. 71    85,  86,  87,  88,  89,  90,  91,   0,           x == 12
            #     
            #  8  72.. 79    35,  34,  33,  32,  31,  30,  29,   0,   x == 4
            #  9  80.. 87    92,  93,  94,  95,  96,  97,  98,   0,           x == 13 
            # 10  88.. 95    28,  27,  26,  25,  24,  23,  22,   0,   x == 3
            # 11  96..103    99, 100, 101, 102, 103, 104, 105,   0,           x == 14 
            #     
            # 12 104..111    21,  20,  19,  18,  17,  16,  15,   0,   x == 2
            # 13 112..119   106, 107, 108, 109, 110, 111, 112,   0,           x == 15 
            # 14 120..127    14,  13,  12,  11,  10,   9,   8,   0,   x == 1
            # 15 128..135   113, 114, 115, 116, 117, 118, 119,   0,           x == 16 
            #     
            # 16 136..143     7,   6,   5,   4,   3,   2,   1,   0,   x == 0
            # 17 144..151     0,   0,   0,   0,   0,   0,   0,   0
            # 
            #  y= 6    5    4    3    2    1    0            
            
            d = "      "
            for x in range(0,17):
                ds = " {x:2d}|".format(x = x ) 
                d += ds
            print (d)
            for y in range (0,7):
                d = "[{y:2d} ]".format(y=y)
                for x in range (0, 17):
                    if x <= 8:
                        p = (16 - 2 * x ) * 8  + (  6 - y)
                    else:
                        p = 8 + 2 * (x - 9 ) * 8 + y 
                    ds = " {x:3d}".format(x = output[ p])
                    d += ds
                print(d)
                    
        next_frame = 0 if self._current_frame == 1 else 0
        self._bank(next_frame)

        offset = 0
        for chunk in self._chunk(output, 32):
            #print(chunk)
            self.i2c.write_i2c_block_data(self.address, self._COLOR_OFFSET + offset, chunk)
            offset += 32

        self._frame(next_frame)
        
    def _chunk(self, l, n):
        for i in range(0, len(l)+1, n):
            yield l[i:i + n]

class Matrix ( IS31FL3731):
    width = 17
    height = 7

    def __init__(self, i2c, address=0x74):
        IS31FL3731.__init__(self, i2c, address)
        self.buf = numpy.zeros((self.width, self.height))
        
        self._reset()

        self._font = font.Font_5_7()
        self._current_frame = 0
        self._scroll = [0,0]
        self._rotate = 0 # Increments of 90 degrees
        self._flipx = False
        self._flipy = False
        self._brightness = 1.0

        # Display initialization
        self.display_initialization()
        

    def scroll(self, x=0, y=0):
        """Offset the buffer by x/y pixels

        Scroll pHAT HD displays an 17x7 pixel window into the bufer,
        which starts at the left offset and wraps around.

        The x and y values are added to the internal scroll offset.

        If called with no arguments, a horizontal right to left scroll is used.

        :param x: Amount to scroll on x-axis
        :param y: Amount to scroll on y-axis

        """

        if x == 0 and y == 0:
            x = 1

        self._scroll[0] += x
        self._scroll[1] += y

    def scroll_to(self, x=0, y=0):
        """Scroll the buffer to a specific location.

        Scroll pHAT HD displays a 17x7 pixel window into the buffer,
        which starts at the left offset and wraps around.

        The x and y values set the internal scroll offset.

        If called with no arguments, the scroll offset is reset to 0,0

        :param x: Position to scroll to on x-axis
        :param y: Position to scroll to on y-axis

        """

        self._scroll = [x,y]

    def rotate(self, degrees=0):
        """Rotate the buffer 0, 90, 180 or 270 degrees before dislaying.


        :param degrees: Amount to rotate- will snap to the nearest 90 degrees

        """

        self._rotate = int(round(degrees/90.0))

    def flip(self, x=False, y=False):
        """Flip the buffer horizontally and/or vertically before displaying.

        :param x: Flip horizontally left to right
        :param y: Flip vertically up to down

        """

        self._flipx = x
        self._flipy = y

    def clear(self):
        """Clear the buffer

        You must call `show` after clearing the buffer to update the display.

        """

        del self.buf
        self.buf = numpy.zeros((self.width, self.height))

    def draw_char(self, x, y, char, font=None, brightness=1.0):
        """Draw a single character to the buffer.

        :param o_x: Offset x - distance of the char from the left of the buffer
        :param o_y: Offset y - distance of the char from the top of the buffer
        :param char: Char to display- either an integer ordinal or a single letter
        :param font: Font to use, default is to use one specified with `set_font`
        :param brightness: Brightness of the pixels that compromise the char, from 0.0 to 1.0

        """

        if font is None:
            if self._font is not None:
                font = self._font
            else:
                return (x, y)

        charP = font.getPattern(char)
        sizeX = font.getSizeX()
        sizeY = font.getSizeY()
        if debug: 
            print ("charP", char, charP, sizeX, sizeY)
            
        for px in range(sizeX):
            for py in range(sizeY):
                if charP[px] & (1 << py) > 0:
                    pix = 1
                else:
                    pix = 0 
                self.set_pixel(x + px, y + py, pix * brightness  )

        return (x + sizeX, y +  sizeY)

    def write_string(self, string, x=0, y=0, font=None, letter_spacing=1, brightness=1.0):
        """Write a string to the buffer. Calls draw_char for each character.

        :param string: The string to display
        :param x: Offset x - distance of the string from the left of the buffer
        :param y: Offset y - distance of the string from the top of the buffer
        :param font: Font to use, default is to use the one specified with `set_font`
        :param brightness: Brightness of the pixels that compromise the text, from 0.0 to 1.0

        """

        o_x = x

        for char in string:
            x, n = self.draw_char(x, y, char, font=font, brightness=brightness)
            x += 1 + letter_spacing

        return x - o_x

    def fill(self, brightness, x=0, y=0, width=0, height=0):
        """Fill an area of the display.

        :param brightness: Brightness of pixels
        :param x: Offset x - distance of the area from the left of the buffer
        :param y: Offset y - distance of the area from the top of the buffer
        :param width: Width of the area (default is 17)
        :param height: Height of the area (default is 7)

        """

        if width == 0:
            width = self.width

        if height == 0:
            height = self.height

        for px in range(width):
            for py in range(height):
                self.set_pixel(x+px, y+py,  brightness)

    def clear_rect(self, x, y, width, height):
        """Clear a rectangle.

        :param x: Offset x - distance of the area from the left of the buffer
        :param y: Offset y - distance of the area from the top of the buffer
        :param width: Width of the area (default is 17)
        :param height: Height of the area (default is 7)

        """

        self.fill(0, x, y, width, height)

    def set_graph(self, values, low=None, high=None, brightness=1.0, x=0, y=0, width=None, height=None):
        """Plot a series of values into the display buffer.

        :param values: A list of numerical values to display
        :param low: The lowest possible value (default min(values))
        :param high:  The highest possible value (default max(values))
        :param brightness:  Maximum graph brightness (from 0.0 to 1.0)
        :param x: x position of graph in display buffer (default 0)
        :param y: y position of graph in display buffer (default 0)
        :param width: width of graph in display buffer (default 17)
        :param height: height of graph in display buffer (default 7)
        :return: None

        """
        if width is None:
            width = self.width

        if height is None:
            height = self.height

        if low is None:
            low = min(values)

        if high is None:
            high = max(values)

        span = high - low

        for p_x in range(width):
            try:
                value = values[p_x]
                value -= low
                value /= float(span)
                value *= height * 10.0

                value = min(value, height * 10)
                value = max(value, 0)

                for p_y in range(height):
                    self.set_pixel(x+p_x, y+(height-p_y), brightness if value > 10 else (value / 10.0) * brightness)
                    value -= 10
                    if value < 0:
                        value = 0

            except IndexError:
                return

    def set_brightness(self, brightness):
        """Set a global brightness value.

        :param brightness: Brightness value from 0.0 to 1.0

        """

        self._brightness = brightness
    def get_brightness(self):
        """Get the global brightness value.
        """
        return self._brightness

    def set_pixel(self, x, y, brightness):
        """Set a single pixel in the buffer.

        :param x: Position of pixel from left of buffer
        :param y: Position of pixel from top of buffer
        :param brightness: Intensity of the pixel, from 0.0 to 1.0 or 0 to 255.

        """
        if debug:
            print("set_pixel (", x, y, brightness,  ")")
            
        brightness = int(255.0 * brightness)

        if brightness > 255:
            brightness = 255
        if brightness < 0:
            brightness = 0

        try:
            self.buf[x][y] = brightness

        except IndexError:
            if y >= self.buf.shape[1]:
                self.buf = numpy.pad(self.buf, ((0,0),(0,y - self.buf.shape[1] + 1)), mode='constant')

            if x >= self.buf.shape[0]:
                self.buf = numpy.pad(self.buf, ((0,x - self.buf.shape[0] + 1),(0,0)), mode='constant')

            self.buf[x][y] = brightness

    def show(self):
        """Show the buffer contents on the display.

        The buffer is copied, then  scrolling, rotation and flip y/x
        transforms applied before taking a 17x7 slice and displaying.

        """


        display_buffer = numpy.copy(self.buf)

        for axis in [0,1]:
            if not self._scroll[axis] == 0:
                display_buffer = numpy.roll(display_buffer, -self._scroll[axis], axis=axis)

        # Chop a width * height window out of the display buffer
        display_buffer = display_buffer[:self.width, :self.height]

        if self._rotate:
            display_buffer = numpy.rot90(display_buffer, self._rotate)

        if self._flipy:
            display_buffer = numpy.flipud(display_buffer)

        if self._flipx:
            display_buffer = numpy.fliplr(display_buffer)

        output = [0 for x in range(144)]

        for x in range(self.width):
            for y in range(self.height):
                idx = self._pixel_addr(x, 6-y)

                try:
                    output[idx] = int(display_buffer[x][y] * self._brightness)

                except IndexError:
                    output[idx] = 0

        self.displayOutput( output)
        del display_buffer

    def _pixel_addr(self, x, y):
        return x + y * 16


class ScrollPhatHD(Matrix):
    width = 17
    height = 7

    def __init__(self, i2c):
        Matrix.__init__(self, i2c)
        
    def _pixel_addr(self, x, y):
        if x > 8:
            x = x - 8
            y = 6 - (y + 8)
        else:
            x = 8 - x

        return x * 16 + y

class ScrollPhatHd_Adapter(adapter.adapters.Adapter):
    
    # -----------------------------------------
    # fields for adapter
    queueThread = None
    
    # -----------------------------------------
   
    mandatoryParameters = { 
                'scrollphathd.rotate180'  : 'false',
                'scrollphathd.mirror': 'false',
    }    
                # -----------------------------------------
    
    def __init__(self):
        # General Adapter
        adapter.adapters.Adapter.__init__(self)
        
        self.font_5_7 = font.Font_5_7()
        self.font_3_5 = font.Font_3_5()
        self.mda = ScrollPhatHD( smbus.SMBus(1) )
        self._inactive()   
        
    def _inactive(self):
        self.mda.clear()
        if True:
            for x in range ( 0, 17, 3):
                self.mda.set_pixel(x, 4, 0.1)
                    
        self.mda.show()
                            
    def setActive (self, active):
        adapter.adapters.Adapter.setActive(self, active)
        if active:
            self.mda.clear()
            
            flipx = self.isTrue( self.parameters['scrollphathd.mirror'] ) 
            self.mda.flip(x = flipx, y= False)
            
            rotate = self.isTrue( self.parameters['scrollphathd.rotate180'] )
            if rotate: 
                self.mda.rotate(180)
            
            self.mda.show()
            pass
        else:
            self._inactive()
               
    def run(self):
        pass
                    
         
    def text_5_7(self, value):
        """input from scratch to adapter"""
        if debug:
            print("text, value   ", type(value), value)
        
        self.mda.clear()
        
        if sys.version_info.major == 2: 
            try:
                sx = unicode(value, 'utf-8',  'replace')
            except TypeError as e:
                if (debug): print(e)
                sx = value
            if (debug): print(sx)
            self.mda.write_string( sx, 0, 0, self.font_5_7, letter_spacing= 0)
            
        if sys.version_info.major == 3: 
            self.mda.write_string( sx, 0, 0, self.font_5_7, letter_spacing= 0)
            
        self.mda.show()
        
    def text_3_5(self, value):
        """input from scratch to adapter"""
        if debug:
            print("text, value   ", type(value), value)
        
        self.mda.clear()
        
        if sys.version_info.major == 2: 
            try:
                sx = unicode(value, 'utf-8',  'replace')
            except TypeError as e:
                if (debug): print(e)
                sx = value
            if (debug): print(sx)
            self.mda.write_string( sx, 0, 0, self.font_3_5, letter_spacing= 0)
            
        if sys.version_info.major == 3: 
            self.mda.write_string( sx, 0, 0, self.font_3_5, letter_spacing= 0)
            
        self.mda.show()

    def command(self, value):
        """command values
            clear
            clear,x,y 
            pixel,x,y [,bright]
            box,x,y,w,h[,bright]
        """    
        value = value.strip()
        values = re.split("[, ;:/]+", value)
        
        if values[0] == 'clear':
            
            if len(values) == 1:
                self.mda.clear()
                self.mda.show()     
            
            if len(values) == 3:
                xs = values[1]
                ys = values[2]
                
                try:
                    x = int(xs)
                    y = int(ys)
                except Exception:
                    logger.error("{n:s}: could not decode x,y as int values.".format(n=self.name))
                    return
                
                self.mda.set_pixel(x, y, brightness= 0.0)
                self.mda.show()
                
            if len(values) == 4:
                xs = values[1]
                ys = values[2]
                bs = values[3]
                
                try:
                    x = int(xs)
                    y = int(ys)
                except Exception:
                    logger.error("{n:s}: could not decode x,y as int values.".format(n=self.name))
                    return
                try:
                    b = float(bs)
                except Exception:
                    logger.error("{n:s}: could not decode bright as float value.".format(n=self.name))
                    return
                
                self.mda.set_pixel(x, y, brightness=b)
                self.mda.show()
                
        elif values[0] == 'pixel':
            
            if len(values) == 3:
                xs = values[1]
                ys = values[2]
                
                try:
                    x = int(xs)
                    y = int(ys)
                except Exception:
                    logger.error("{n:s}: could not decode x,y as int values.".format(n=self.name))
                    return
                
                self.mda.set_pixel(x, y)
                self.mda.show()
                
            if len(values) == 4:
                xs = values[1]
                ys = values[2]
                bs = values[3]
                
                try:
                    x = int(xs)
                    y = int(ys)
                except Exception:
                    logger.error("{n:s}: could not decode x,y as int values.".format(n=self.name))
                    return
                try:
                    b = float(bs)
                except Exception:
                    logger.error("{n:s}: could not decode bright as float value.".format(n=self.name))
                    return
                
                self.mda.set_pixel(x, y, brightness=b)
                self.mda.show()
                
        elif values[0] == 'box':
            if len(values) == 5:
                xs = values[1]
                ys = values[2]
                ws = values[3]
                hs = values[4]
                
                try:
                    x = int(xs)
                    y = int(ys)
                    h = int(hs)
                    w = int(ws)
                except Exception:
                    logger.error("{n:s}: could not decode x,y,w,h as int values.".format(n=self.name))
                    return
                
                self.mda.fill(self.mda.get_brightness(), x, y, w, h)
                self.mda.show()
                
            if len(values) == 6:
                xs = values[1]
                ys = values[2]
                ws = values[3]
                hs = values[4]
                bs = values[5]
                
                try:
                    x = int(xs)
                    y = int(ys)
                    h = int(hs)
                    w = int(ws)
                except Exception:
                    logger.error("{n:s}: could not decode x,y,w,h as int values.".format(n=self.name))
                    return
                try:
                    b = float(bs)
                except Exception:
                    logger.error("{n:s}: could not decode bright as float value.".format(n=self.name))
                    return

                self.mda.fill(b, x, y, w, h)
                self.mda.show()
            
    def clear(self):
        """broadcast from adapter to scratch"""
        self.mda.clear()
        self.mda.show()
    
    def brightness(self, value):
        """input from scratch to adapter"""
        try:
            value = float(value)
        except Exception:
            return
        
        if value < 0:
            value = 0
        if value > 1:
            value = 1
        self.mda.set_brightness(value)    
    