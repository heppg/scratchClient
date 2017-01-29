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
from spi.manager import SPIManager

debug = False

# --------------------------------------------------------------------------------------

class WS2801_Adapter (adapter.adapters.SPIAdapter):
    """Interface to WS2801_Adapter SPI """
    
    mandatoryParameters = { 'led.length': '25', 
                           'spi.bus' : '0', 
                           'spi.device' :'0',
                          }

    
    def __init__(self):
        adapter.adapters.SPIAdapter.__init__(self)

    def setActive (self, state):
        if debug:
            print(self.name, "setActive", state)
        #
        # use default implementation to start up SPI
        #
        adapter.adapters.SPIAdapter.setActive(self, state)

               
    def led(self, value):
        if debug:
            print("led", value)
        
        # in the conversion table, r, g, b values are defined
        # in the chipset, it is not clear where r,g,b is wired
        # the mapping-values give the possibility to remap 
        
        mapping_0 = 'red'    # 0 is accessing the 'red
        mapping_1 = 'green'  # 1 is accessing the 'green
        mapping_2 = 'blue'   # 2 for the blue
          
        try:
            # sequence of r, g, b-Values 
            _bytes = []
            values = value.split(' ')
            
            if len(values) > int(self.parameters['led.length']):
                value = value[0:int(self.parameters['led.length'])]
                
            for k in values:
                color = self.getRGBFromString(k)

                _bytes.append ( color [  mapping_0] )
                _bytes.append ( color [  mapping_1] )
                _bytes.append ( color [  mapping_2] )

            if debug:
                print(_bytes)
            
            self.spi.max_speed_hz = 2000000
            self.spi.xfer2(_bytes)    
            
        except Exception as e:
            print(e)
            pass        


class MAX31855_Adapter (adapter.adapters.SPIAdapter):
    """Interface to MAX31855_Adapter SPI """
    
    mandatoryParameters = {  
                           'spi.bus' : '0', 
                           'spi.device' :'0',
                           'poll.interval': 0.1
                          }

    def __init__(self):
        adapter.adapters.SPIAdapter.__init__(self, SPIManager.type_MAX31855)

    def setActive (self, state):
        if debug:
            print(self.name, "setActive", state)
        #
        # use default implementation to start up SPI
        #
        adapter.adapters.SPIAdapter.setActive(self, state);

    def run(self):
        _del = float(self.parameters['poll.interval'])
            
        last = self.spiManager.getValue(
                                self.int_spi_bus, 
                                self.int_spi_device, 
                                0
                                )
        self.temp_wire(  last['temp_ext']  )
        self.temp_intern(  last['temp_int']  )
        self.temp_error(  last['error']  )
   
        while not self.stopped():
            #
            # delay 5 sec, but break time in small junks to allow stopping fast
            #
            self.delay(_del)

            current = self.spiManager.getValue(
                                self.int_spi_bus, 
                                self.int_spi_device, 
                                0
                   )
            
            if last['temp_ext'] != current['temp_ext']:
                self.temp_wire(current['temp_ext'])
                last['temp_ext'] = current['temp_ext']
                
            if last['temp_int'] != current['temp_int']:
                self.temp_intern(current['temp_int'])
                last['temp_int'] = current['temp_int']
                
            if last['error'] != current['error']:
                self.temp_error(current['error'])
                last['error'] = current['error']
                

    def temp_wire(self, value):
        self.sendValue(str(value))
     
    def temp_intern(self, value):
        self.sendValue(str(value))
    
    def temp_error(self, value):
        self.sendValue('"' + str(value) + '"')

