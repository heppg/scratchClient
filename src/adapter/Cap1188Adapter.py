# -*- coding: utf-8 -*-
    # --------------------------------------------------------------------------------------------
    # Copyright (C) 2013  Gerhard Hepp
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

import adapter


import pianohat
import signal

import logging
logger = logging.getLogger(__name__)

debug = False

# --------------------------------------------------------------------------------------
class PianoHat_CAP1188_Adapter (adapter.adapters.Adapter):
    """Interface for PianoHat"""
    
   
    mandatoryParameters = { 'auto_leds': 'true' }

    def __init__(self):
        adapter.adapters.Adapter.__init__(self )


    def setActive (self, state):
        if debug:
            print(self.name, "setActive", state)
        #
        # use default implementation to start up SPI
        #
        adapter.adapters.Adapter.setActive(self, state);
        self.bool_auto_leds =   self.isTrue( self.parameters['auto_leds']  )
        
        pianohat.auto_leds( self.bool_auto_leds )
        pianohat.on_note(self.handle_touch)
        pianohat.on_octave_up(self.handle_touch)
        pianohat.on_octave_down(self.handle_touch)
        pianohat.on_instrument(self.handle_touch)
    
    def handle_touch(self, ch, evt):
        if debug:
            print('handle_touch', ch, evt)
        if evt:
            if ch == 0:
                self.broadcast_00_on()
            elif ch == 1:
                self.broadcast_01_on()    
            elif ch == 2:
                self.broadcast_02_on()    
            elif ch == 3:
                self.broadcast_03_on()    
            elif ch == 4:
                self.broadcast_04_on()    
            elif ch == 5:
                self.broadcast_05_on()    
            elif ch == 6:
                self.broadcast_06_on()    
            elif ch == 7:
                self.broadcast_07_on()    
            elif ch == 8:
                self.broadcast_08_on()    
            elif ch == 9:
                self.broadcast_09_on()    
            elif ch == 10:
                self.broadcast_10_on()    
            elif ch == 11:
                self.broadcast_11_on()    
            elif ch == 12:
                self.broadcast_12_on()    
            elif ch == 13:
                self.broadcast_13_on()    
            elif ch == 14:
                self.broadcast_14_on()    
            elif ch == 15:
                self.broadcast_15_on()     
        else:
            if ch == 0:
                self.broadcast_00_off()
            elif ch == 1:
                self.broadcast_01_off()    
            elif ch == 2:
                self.broadcast_02_off()    
            elif ch == 3:
                self.broadcast_03_off()    
            elif ch == 4:
                self.broadcast_04_off()    
            elif ch == 5:
                self.broadcast_05_off()    
            elif ch == 6:
                self.broadcast_06_off()    
            elif ch == 7:
                self.broadcast_07_off()    
            elif ch == 8:
                self.broadcast_08_off()    
            elif ch == 9:
                self.broadcast_09_off()    
            elif ch == 10:
                self.broadcast_10_off()    
            elif ch == 11:
                self.broadcast_11_off()    
            elif ch == 12:
                self.broadcast_12_off()    
            elif ch == 13:
                self.broadcast_13_off()    
            elif ch == 14:
                self.broadcast_14_off()    
            elif ch == 15:
                self.broadcast_15_off()
                  
    def broadcast_00_on(self):
        """output command from adapter to scratch."""
        self.send()
    def broadcast_01_on(self):
        """output command from adapter to scratch."""
        self.send()
    def broadcast_02_on(self):
        """output command from adapter to scratch."""
        self.send()
    def broadcast_03_on(self):
        """output command from adapter to scratch."""
        self.send()
    def broadcast_04_on(self):
        """output command from adapter to scratch."""
        self.send()
    def broadcast_05_on(self):
        """output command from adapter to scratch."""
        self.send()
    def broadcast_06_on(self):
        """output command from adapter to scratch."""
        self.send()
    def broadcast_07_on(self):
        """output command from adapter to scratch."""
        self.send()
    def broadcast_08_on(self):
        """output command from adapter to scratch."""
        self.send()
    def broadcast_09_on(self):
        """output command from adapter to scratch."""
        self.send()
    def broadcast_10_on(self):
        """output command from adapter to scratch."""
        self.send()
    def broadcast_11_on(self):
        """output command from adapter to scratch."""
        self.send()
    def broadcast_12_on(self):
        """output command from adapter to scratch."""
        self.send()
    def broadcast_13_on(self):
        """output command from adapter to scratch."""
        self.send()
    def broadcast_14_on(self):
        """output command from adapter to scratch."""
        self.send()
    def broadcast_15_on(self):
        """output command from adapter to scratch."""
        self.send()
        
    def broadcast_00_off(self):
        """output command from adapter to scratch."""
        self.send()
    def broadcast_01_off(self):
        """output command from adapter to scratch."""
        self.send()
    def broadcast_02_off(self):
        """output command from adapter to scratch."""
        self.send()
    def broadcast_03_off(self):
        """output command from adapter to scratch."""
        self.send()
    def broadcast_04_off(self):
        """output command from adapter to scratch."""
        self.send()
    def broadcast_05_off(self):
        """output command from adapter to scratch."""
        self.send()
    def broadcast_06_off(self):
        """output command from adapter to scratch."""
        self.send()
    def broadcast_07_off(self):
        """output command from adapter to scratch."""
        self.send()
    def broadcast_08_off(self):
        """output command from adapter to scratch."""
        self.send()
    def broadcast_09_off(self):
        """output command from adapter to scratch."""
        self.send()
    def broadcast_10_off(self):
        """output command from adapter to scratch."""
        self.send()
    def broadcast_11_off(self):
        """output command from adapter to scratch."""
        self.send()
    def broadcast_12_off(self):
        """output command from adapter to scratch."""
        self.send()
    def broadcast_13_off(self):
        """output command from adapter to scratch."""
        self.send()
    def broadcast_14_off(self):
        """output command from adapter to scratch."""
        self.send()
    def broadcast_15_off(self):
        """output command from adapter to scratch."""
        self.send()
  
  
