# -*- coding: utf-8 -*-
    # --------------------------------------------------------------------------------------------
    # Copyright (C) 2015  Gerhard Hepp
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

import xml.etree.ElementTree as ET
import threading
import logging
logger = logging.getLogger(__name__)

import errorManager

import adapter
from spi.manager import SPIManager

debug = False

class ADC_MCP3202_10_Zone_Input (adapter.adapters.SPIAdapter):
    """ADC Interface; Calculate zones from xml-parameters
       Useful to produce values like low, medium, high from adc values."""
    
    int_adc_channel = 0

    mandatoryParameters = { 'poll.interval': '0.2', 
                           
                           'spi.bus' : '0', 
                           'spi.device' :'0',
                           'adc.channel' : '0' }

    def __init__(self):
        adapter.adapters.SPIAdapter.__init__(self)
        self.mapping = []
        for i in range(1024):
            self.mapping.append(i)
            
    def setXMLConfig(self, child):
        # print "setXMLConfig"
        #
        # read configuration from xml
        #
        loggingContext = "adapter '[{a:s}]'".format(a=self.name ) 
        
        for tle in child:
            if 'extension' == tle.tag:
                child= tle
                break

        for tle in child:
            if 'zone' == tle.tag:
                _from = None
                _to = None
                _value = None
                
                if 'from' in  tle.attrib:
                    _from = tle.attrib['from']
                else:
                    errorManager.append("{lc:s}: no 'from' in adc:zone".format( lc=loggingContext ))
                    
                if 'to' in  tle.attrib:
                    _to = tle.attrib['to']
                else:
                    errorManager.append("{lc:s}: no 'to' in adc:zone".format( lc=loggingContext ))
                    
                if 'value' in  tle.attrib:
                    _value = tle.attrib['value']
                else:
                    errorManager.append("{lc:s}: no 'value' in adc:zone".format( lc=loggingContext ))
                    
                if _from == None:
                    continue
                if _to == None:
                    continue
                if _value == None:
                    continue
                
                try:
                    _from = int( _from)
                    _to = int( _to)
                except Exception:
                    errorManager.append("{lc:s}: no 'from', 'to' must be int values in adc:zone".format( lc=loggingContext ))
                    continue
                
                if _to < _from:
                    errorManager.append("{lc:s}: no 'from' > 'to' adc:zone".format( lc=loggingContext ))
                    continue
                        
                for i in range(_from, _to+1):
                    self.mapping[i] = _value

    def setActive (self, state):
        if debug:
            print(self.name, "setActive", state)
        #
        # use default implementation to start up SPI
        #
        adapter.adapters.SPIAdapter.setActive(self, state);
        self.int_adc_channel=   int(self.parameters['adc.channel'])

    def run(self):
        if debug:
            print(self.name, "ADCInput, setActive")
        _del = float(self.parameters['poll.interval'])
            
        last = None
             
        while not self.stopped():
            #
            # delay 5 sec, but break time in small junks to allow stopping fast
            #
            self.delay(_del)
                           
            current = self.getValue( self.spi, self.int_adc_channel )
            
            # plus/minus ein Punkt ist noch 'identisch'
            current = self.mapping[current]
            
            if last != current :
                self.adc( current )
                last = current

    def adc(self, value):
        """output from adapter to scratch."""
        self.sendValue(str(value))

    def getValue(self, spi, channel):
        # EXPLANATION of 
        # r = spi.xfer2([1,(2+channel)<<6,0])
        # Send start bit, sgl/diff, odd/sign, MSBF 
        # channel = 0 sends 0000 0001 1000 0000 0000 0000
        # channel = 1 sends 0000 0001 1100 0000 0000 0000
        # sgl/diff = 1; odd/sign = channel; MSBF = 0 
        
        # EXPLANATION of 
        # ret = ((r[1]&31) << 6) + (r[2] >> 2)
        # spi.xfer2 returns same number of 8-bit bytes as sent. In this case, three 8-bit bytes are returned
        # We must then parse out the correct 10-bit byte from the 24 bits returned. The following line discards
        # all bits but the 10 data bits from the center of the last 2 bytes: XXXX XXXX - XXXX DDDD - DDDD DDXX 

        spi.max_speed_hz = 500000
        
        i = [ 1, (2+channel)<<6, 0  ]
        # print(" i {r0:08b} {r1:08b} {r2:08b}  ".format(r0=i[0], r1=i[1], r2=i[2] ))
        
        r = spi.xfer2( list( i)  )  
        
        # print(" r {r0:08b} {r1:08b} {r2:08b} {r3:08b} ".format(r0=r[0], r1=r[1], r2=r[2], r3=r[3]))
        
        ret = ((r[1]&31) << 6) + (r[2] >> 2)
        return ret 
    