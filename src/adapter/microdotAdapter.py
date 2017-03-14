# -*- coding: utf-8 -*-

# Adapter based on code from pimoroni.com,
# microdot- version 0.1.3
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

debug = False

        


class NanoMatrix:
    '''        
    _BUF_MATRIX_1 = [ # Green
#Col   1 2 3 4 5
    0b00000000, # Row 1
    0b00000000, # Row 2
    0b00000000, # Row 3
    0b00000000, # Row 4
    0b00000000, # Row 5
    0b00000000, # Row 6
    0b10000000, # Row 7, bit 8 =  decimal place
    0b00000000
]

    _BUF_MATRIX_2 = [ # Red
#Row 8 7 6 5 4 3 2 1
    0b01111111, # Col 1, bottom to top
    0b01111111, # Col 2
    0b01111111, # Col 3
    0b01111111, # Col 4
    0b01111111, # Col 5
    0b00000000,
    0b00000000,
    0b01000000  # bit 7, decimal place
]

    _BUF_MATRIX_1 = [0] * 8
    _BUF_MATRIX_2 = [0] * 8
'''
    MODE = 0b00011000
    OPTS = 0b00001110 # 1110 = 35mA, 0000 = 40mA
    
    CMD_BRIGHTNESS = 0x19
    CMD_MODE = 0x00
    CMD_UPDATE = 0x0C
    CMD_OPTIONS = 0x0D
    
    CMD_MATRIX_1 = 0x01
    CMD_MATRIX_2 = 0x0E
    
    MATRIX_1 = 0
    MATRIX_2 = 1

    def __init__(self, address):
        self.address = address
        self._brightness = 127

        self.bus = smbus.SMBus(1)

        self.bus.write_byte_data(self.address, self.CMD_MODE, self.MODE)
        self.bus.write_byte_data(self.address, self.CMD_OPTIONS, self.OPTS)
        self.bus.write_byte_data(self.address, self.CMD_BRIGHTNESS, self._brightness)

        self._BUF_MATRIX_1 = [0] * 8
        self._BUF_MATRIX_2 = [0] * 8

    def set_brightness(self, brightness):
        self._brightness = int(brightness * 127)
        if self._brightness > 127: self._brightness = 127

        self.bus.write_byte_data(self.address, self.CMD_BRIGHTNESS, self._brightness)

    def set_decimal(self, m, c):

        if m == self.MATRIX_1:
            if c == 1:
                self._BUF_MATRIX_1[6] |= 0b10000000    
            else:
                self._BUF_MATRIX_1[6] &= 0b01111111

        elif m == self.MATRIX_2:

            if c == 1:
                self._BUF_MATRIX_2[7] |= 0b01000000
            else:
                self._BUF_MATRIX_2[7] &= 0b10111111

        #self.update()

    def set(self, m, data):
        for y in range(7):
            self.set_row(m, y, data[y])

    def set_row(self, m, r, data):
        for x in range(5):
            self.set_pixel(m, x, r, (data & (1 << (4-x))) > 0)

    def set_col(self, m, c, data):
        for y in range(7):
            self.set_pixel(m, c, y, (data & (1 << y)) > 0)

    def set_pixel(self, m, x, y, c):

        if m == self.MATRIX_1:
            if c == 1:
                self._BUF_MATRIX_1[y] |= (0b1 << x)
            else:
                self._BUF_MATRIX_1[y] &= ~(0b1 << x)
        elif m == self.MATRIX_2:
            if c == 1:
                self._BUF_MATRIX_2[x] |= (0b1 << y)
            else:
                self._BUF_MATRIX_2[x] &= ~(0b1 << y)

        #self.update()

    def clear(self, m):
        if m == self.MATRIX_1:
            self._BUF_MATRIX_1 = [0] * 8
        elif m == self.MATRIX_2:
            self._BUF_MATRIX_2 = [0] * 8

        self.update()

    def update(self):
        for x in range(10):
            try:
                self.bus.write_i2c_block_data(self.address, self.CMD_MATRIX_1, self._BUF_MATRIX_1)
                self.bus.write_i2c_block_data(self.address, self.CMD_MATRIX_2, self._BUF_MATRIX_2)

                self.bus.write_byte_data(self.address, self.CMD_UPDATE, 0x01)
                break
            except IOError:
                print("IO Error")



class MicroDotPhat:
    _n1 = NanoMatrix(address=0x63)
    _n2 = NanoMatrix(address=0x62)
    _n3 = NanoMatrix(address=0x61)
    
    _mat = [(_n1, 1), (_n1, 0), (_n2, 1), (_n2, 0), (_n3, 1), (_n3, 0)]
    
    WIDTH = 45
    HEIGHT = 7
    
    _buf = numpy.zeros(( HEIGHT, WIDTH))
    _decimal = [0] * 6
    
    _scroll_x = 0
    _scroll_y = 0
    
    _clear_on_exit = True
    _rotate180 = False
    _mirror = False
    
    
    def __init__(self):
        self._font = font.Font_5_7()
    
    def clear(self):
        """Clear the buffer"""
            
        self._decimal = [0] * 6
        self._buf.fill(0)
    
    def fill(self, c):
        """Fill the buffer either lit or unlit
    
        :param c: Colour that should be filled onto the display: 1=lit or 0=unlit
        
        """
    
        self._buf.fill(c)
    
     
    def set_rotate180(self, value):
        """Set whether the display should be rotated 180 degrees
    
        :param value: Whether the display should be rotated 180 degrees: True/False
    
        """
    
        self._rotate180 = (value == True)
    
    def set_mirror(self, value):
        """Set whether the display should be flipped left to right (mirrored)
    
        :param value: Whether the display should be flipped left to right: True/False
    
        """
    
        self._mirror = (value == True)
        
    def set_col(self, x, col):
        """Set a whole column of the buffer
    
        Only useful when not scrolling vertically
    
        :param x: Specify which column to set
        :param col: An 8-bit integer, the 7 least significant bits correspond to each row
    
        """
    
        for y in range(7):
            self.set_pixel(x, y, (col & (1 << y)) > 0)
    
    def set_pixel(self, x, y, c):
        """Set the state of a single pixel in the buffer
    
        If the pixel falls outside the current buffer size, it will be grown auto_matically
    
        :param x: The x position of the pixel to set
        :param y: The y position of the pixel to set
        :param c: The colour to set: 1=lit or 0=unlit
    
        """
    
        try:
            self._buf[y][x] = c
        except IndexError:
            if y >= self._buf.shape[0]:
                self.self._buf = numpy.pad(self._buf, ((0,y - self._buf.shape[0] + 1),(0,0)), mode='constant')
            if x >= self._buf.shape[1]:
                self._buf = numpy.pad(self._buf, ((0,0),(0,x - self._buf.shape[1] + 1)), mode='constant')
            self._buf[y][x] = c
    
    def write_char(self, char, offset_x=0, offset_y=0):
        """Write a single character to the buffer
    
        :param char: The ASCII char to write
        :param offset_x: Position the character along x (default 0)
        :param offset_y: Position the character along y (default 0)
    
        """
    
        char = self._get_char(char)
    
        for x in range(5):
            for y in range(7):
                p = (char[x] & (1 << y)) > 0
                self.set_pixel(offset_x + x, offset_y + y, p)
    
    def _get_char(self, char):
        
        return self._font.getPattern(char)
    
    def set_decimal(self, index, state):
        """Set the state of a _decimal point
    
        :param index: Index of _decimal from 0 to 5
        :param state: State to set: 1=lit or 0=unlit
    
        """
    
       
        if index in range(6):
            self._decimal[index] = 1 if state else 0
    
    def write_string(self, string, offset_x=0, offset_y=0, kerning=True):
        """Write a string to the buffer
    
        :returns: The length, in pixels, of the written string.
    
        :param string: The text string to write
    
        :param offset_x: Position the text along x (default 0)
        :param offset_y: Position the text along y (default 0)
        :param kerning: Whether to kern the characters closely together or display one per matrix (default True)
    
        :Examples:
    
        Write a string to the buffer, aligning one character per dislay, This is
        ideal for displaying still messages up to 6 characters long::
    
            microdotphat.write_string("Bilge!", kerning=False)
    
        Write a string to buffer, with the characters as close together as possible.
        This is ideal for writing text which you intend to scroll::
    
            microdotphat.write_string("Hello World!")
        
        """
     
        str_buf = []
    
        space = [0x00] * 5
        gap = [0x00] * 3
    
        if kerning:
            space = [0x00] * 2
            gap = [0x00]
    
        for char in string:
            if char == ' ':
                str_buf += space
            else:
                char_data = numpy.array(self._get_char(char))
                if kerning:
                    char_data = numpy.trim_zeros(char_data)
                str_buf += list(char_data)
            str_buf += gap # Gap between chars
    
        for x in range(len(str_buf)):
            for y in range(7):
                p = (str_buf[x] & (1 << y)) > 0
                self.set_pixel(offset_x + x, offset_y + y, p)
    
        l = len(str_buf)
        del str_buf
        return l
    
    def scroll(self, amount_x=0, amount_y=0):
        """Scroll the buffer
    
        Will scroll by 1 pixel horizontall if no arguments are supplied.
    
        :param amount_x: Amount to scroll along x axis (default 0)
        :param amount_y: Amount to scroll along y axis (default 0)
    
        :Examples:
    
        Scroll vertically::
    
           microdotphat.scroll(amount_y=1)
    
        Scroll diagonally::
    
           microdotphat.scroll(amount_x=1,amount_y=1)
    
        """
    
        
        if amount_x == 0 and amount_y == 0:
            amount_x = 1
    
        self._scroll_x += amount_x
        self._scroll_y += amount_y
        self._scroll_x %= self._buf.shape[1]
        self._scroll_y %= self._buf.shape[0]
    
    def scroll_to(self, position_x=0, position_y=0):
        """Scroll to a specific position
    
        :param position_x: Desired position along x axis (default 0)
        :param position_y: Desired position along y axis (default 0)
        
        """
    
        
        self._scroll_x = position_x % self._buf.shape[1]
        self._scroll_y = position_y % self._buf.shape[0]
    
    def scroll_horizontal(self, amount=1):
        """Scroll horizontally (along x)
    
        Will scroll one pixel horizontally if no amount is supplied.
    
        :param amount: Amount to scroll along x axis (default 1)
    
        """
    
        self.scroll(amount_x=amount, amount_y=0)
    
    def scroll_vertical(self, amount=1):
        """Scroll vertically (along y)
    
        Will scroll one pixel vertically if no amount is supplied.
    
        :param amount: Amount to scroll along y axis (default 1)
    
        """
    
        self.scroll(amount_x=0, amount_y=amount)
    
    def set_brightness(self, brightness):
        """Set the display brightness
    
        :param brightness: Brightness to set, from 0.0 to 1.0
    
        """
    
        if brightness < 0:
            brightness = 0
        if brightness > 1:
            brightness = 1
    
        for m_x in range(6):
            self._mat[m_x][0].set_brightness(brightness)
    
    def show(self):
        """Output the buffer to the display
    
        A copy of the buffer will be scrolled and rotated according
        to settings before being drawn to the display.
    
        """
    
        scrolled_buffer = numpy.copy(self._buf)
        scrolled_buffer = numpy.roll(scrolled_buffer, -self._scroll_x, axis=1)
        scrolled_buffer = numpy.roll(scrolled_buffer, -self._scroll_y, axis=0)
    
        if self._rotate180:
            scrolled_buffer = numpy.rot90(scrolled_buffer[:7, :45], 2)
    
        if self._mirror:
            scrolled_buffer = numpy.fliplr(scrolled_buffer[:7, :45])
    
        for m_x in range(6):
            x = (m_x * 8)
            b = scrolled_buffer[0:7, x:x+5]
    
            self._mat[m_x][0].set_decimal(self._mat[m_x][1], self._decimal[m_x])
    
            for x in range(5):
                for y in range(7):
                    try:
                        self._mat[m_x][0].set_pixel( self._mat[m_x][1], x, y, b[y][x])
                    except IndexError:
                        pass # Buffer doesn't span this matrix yet
            del b
        for m_x in range(0,6,2):
            self._mat[m_x][0].update()


class Microdot_Adapter(adapter.adapters.Adapter):
    
    # -----------------------------------------
    # fields for adapter
    queueThread = None
    
    # -----------------------------------------
   
    mandatoryParameters = { 
                'microdot.rotate180'  : 'false',
                'microdot.mirror': 'false',
    }
    # -----------------------------------------
    
    def __init__(self):
        # General Adapter
        adapter.adapters.Adapter.__init__(self)
        self.mda = MicroDotPhat()
        self._inactive()   
        
    def _inactive(self):
        self.mda.clear()
        self.mda.set_decimal(0, 1)
        self.mda.set_decimal(1, 1)
        self.mda.set_decimal(2, 1)
        self.mda.set_decimal(3, 1)
        self.mda.set_decimal(4, 1)
        self.mda.set_decimal(5, 1)
        self.mda.show()
                            
    def setActive (self, active):
        adapter.adapters.Adapter.setActive(self, active)
        if active:
            
            self.mda.set_rotate180( self.isTrue( self.parameters['microdot.rotate180'] ) )
            self.mda.set_mirror( self.isTrue( self.parameters['microdot.mirror'] ) )
            pass
        else:
            self._inactive()   
    def run(self):
        pass
                    
         
    def text(self, value):
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
            self.mda.write_string( sx, kerning=False)
            
        if sys.version_info.major == 3: 
            self.mda.write_string( value, kerning=False)
            
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
    
if __name__ == "__main__":
    import time
    
    mdp = MicroDotPhat()
    while True:
        mdp.clear()
        mdp.write_string("hello!", kerning=True)
        mdp.show()
        time.sleep(1)
        
        mdp.clear()
        mdp.write_string("äöüÄÖÜß", kerning=False)
        mdp.show()
        time.sleep(1)
    
    
    