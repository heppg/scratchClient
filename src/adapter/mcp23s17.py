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

import xml.etree.ElementTree as ET
from types import MethodType
import threading
import logging
logger = logging.getLogger(__name__)

import errorManager

import adapter
from spi.manager import SPIManager

debug = False

# --------------------------------------------------------------------------------------


class MCP23S17_Adapter (adapter.adapters.SPIAdapter):
    """Interface to MCP23S17_Adapter SPI 
    for pins configured as output, there are methods defined as inputGPA7(self, value)
    
    This adapter implements a setXMLConfig-method, which allows to read from the xml-config-file.
    
    The input- and output methods are dynamically attached based on configuration.
    """
    
    mandatoryParameters = {  
                           'spi.bus' : '0', 
                           'spi.device' :'0',
                           #
                           # 23S17, device address (0..7)
                           #
                           '23s17.addr': '0',
                           'poll.interval': 0.1
                          }
    IN = 'in'
    OUT = 'out'
    WEAK = 'weak'
    
    #
    # Function prototypes for input setters
    # these are added dynamically to the class if needed.
    #
    def _setBit_GPA0(self, value):
        self._set0(value, 0b00000001)
    def _setBit_GPA1(self, value):
        self._set0(value, 0b00000010)
    def _setBit_GPA2(self, value):
        self._set0(value, 0b00000100)
    def _setBit_GPA3(self, value):
        self._set0(value, 0b00001000)
    def _setBit_GPA4(self, value):
        self._set0(value, 0b00010000)
    def _setBit_GPA5(self, value):
        self._set0(value, 0b00100000)
    def _setBit_GPA6(self, value):
        self._set0(value, 0b01000000)
    def _setBit_GPA7(self, value):
        self._set0(value, 0b10000000)
        
    def _setBit_GPB0(self, value):
        self._set1(value,  0b00000001)
    def _setBit_GPB1(self, value):
        self._set1(value, 0b00000010)
    def _setBit_GPB2(self, value):
        self._set1(value, 0b00000100)
    def _setBit_GPB3(self, value):
        self._set1(value, 0b00001000)
    def _setBit_GPB4(self, value):
        self._set1(value, 0b00010000)
    def _setBit_GPB5(self, value):
        self._set1(value, 0b00100000)
    def _setBit_GPB6(self, value):
        self._set1(value, 0b01000000)
    def _setBit_GPB7(self, value):
        self._set1(value, 0b10000000)
        
    def _set0(self, value, bitvalue):
        """set PortA to a specific value"""
        bValue = self.isTrue(value)
        if bValue:
            self.port[0] |= bitvalue
            # print("set pin value port", 0, " bit", bitvalue ," to ", bitvalue)
            self.setPortA(self.port[0])
        
        bValue = self.isFalse(value)
        if bValue:
            self.port[0] &= ~bitvalue
            # print("set pin value port", 0, " bit", bitvalue ," to ", 0)
            # print( self.port[0] )
            self.setPortA(self.port[0])
            # print("called PORTA")

    def _set1(self, value, bitvalue):
        """set PortB to a specific value"""
        bValue = self.isTrue(value)
        if bValue:
            self.port[1] |= bitvalue
            self.setPortB(self.port[1])
            # print("set pin value port", 1, " bit", bitvalue ," to ", bitvalue)
        
        bValue = self.isFalse(value)
        if bValue:
            self.port[1] &= ~bitvalue
            # print("set pin value port", 0, " bit", bitvalue ," to ", 0)
            # print( self.port[0] )
            self.setPortB(self.port[1])

    def _sendValue(self, value):
        """Prototype for a send function"""
        self.sendValue('"' + value + '"')

    pins = { 
            'GPA0': { 'dir': None, 'ifunc' : _setBit_GPA0, 'port': 0, 'bitValue': 0b00000001, 'lastValue': None, 'pullup':None },
            'GPA1': { 'dir': None, 'ifunc' : _setBit_GPA1, 'port': 0, 'bitValue': 0b00000010, 'lastValue': None, 'pullup':None },
            'GPA2': { 'dir': None, 'ifunc' : _setBit_GPA2, 'port': 0, 'bitValue': 0b00000100, 'lastValue': None, 'pullup':None },
            'GPA3': { 'dir': None, 'ifunc' : _setBit_GPA3, 'port': 0, 'bitValue': 0b00001000, 'lastValue': None, 'pullup':None },
            'GPA4': { 'dir': None, 'ifunc' : _setBit_GPA4, 'port': 0, 'bitValue': 0b00010000, 'lastValue': None, 'pullup':None },
            'GPA5': { 'dir': None, 'ifunc' : _setBit_GPA5, 'port': 0, 'bitValue': 0b00100000, 'lastValue': None, 'pullup':None },
            'GPA6': { 'dir': None, 'ifunc' : _setBit_GPA6, 'port': 0, 'bitValue': 0b01000000, 'lastValue': None, 'pullup':None },
            'GPA7': { 'dir': None, 'ifunc' : _setBit_GPA7, 'port': 0, 'bitValue': 0b10000000, 'lastValue': None, 'pullup':None },
            
            'GPB0': { 'dir': None, 'ifunc' : _setBit_GPB0, 'port': 1, 'bitValue': 0b00000001, 'lastValue': None, 'pullup':None },
            'GPB1': { 'dir': None, 'ifunc' : _setBit_GPB1, 'port': 1, 'bitValue': 0b00000010, 'lastValue': None, 'pullup':None },
            'GPB2': { 'dir': None, 'ifunc' : _setBit_GPB2, 'port': 1, 'bitValue': 0b00000100, 'lastValue': None, 'pullup':None },
            'GPB3': { 'dir': None, 'ifunc' : _setBit_GPB3, 'port': 1, 'bitValue': 0b00001000, 'lastValue': None, 'pullup':None },
            'GPB4': { 'dir': None, 'ifunc' : _setBit_GPB4, 'port': 1, 'bitValue': 0b00010000, 'lastValue': None, 'pullup':None },
            'GPB5': { 'dir': None, 'ifunc' : _setBit_GPB5, 'port': 1, 'bitValue': 0b00100000, 'lastValue': None, 'pullup':None },
            'GPB6': { 'dir': None, 'ifunc' : _setBit_GPB6, 'port': 1, 'bitValue': 0b01000000, 'lastValue': None, 'pullup':None },
            'GPB7': { 'dir': None, 'ifunc' : _setBit_GPB7, 'port': 1, 'bitValue': 0b10000000, 'lastValue': None, 'pullup':None },
            }

    def __init__(self):
        adapter.adapters.SPIAdapter.__init__(self,  SPIManager.type_MCP23S17)
        self.port = [0,0]
        self._lock = threading.Lock() 
        
    def _setPinDir (self, _id, _dir):
        self.pins[_id]['dir'] = _dir
        
    def _setPinPullup (self, _id, _pullup):
        self.pins[_id]['pullup'] = _pullup
        
    def _isPin (self, _id):
        return _id in self.pins
    
    def _getPins (self):
        return self.pins.keys()
    
    def _getPin(self, pin):
        return self.pins[pin]
             
    def setXMLConfig(self, child):
        # print "setXMLConfig"
        #
        # read configuration from xml
        #
        loggingContext = "adapter '[{a:s}]'".format(a=self.name ) 
        
        # look for extension tag (new from 2017)
        for tle in child:
            if 'extension' == tle.tag:
                child= tle
                break
        
        for tle in child:
            if 'io' == tle.tag:
                if 'id' in  tle.attrib:
                    _id = tle.attrib['id']
                    if self._isPin(_id):
                        if 'dir' in tle.attrib:
                            _dir = tle.attrib['dir']
                            self._setPinDir( _id,  _dir ) 
                        else:
                            errorManager.append("{lc:s}: no dir in mcp23s17:io".format( lc=loggingContext ))
                        if 'pullup' in tle.attrib:
                            _pullup = tle.attrib['pullup']
                            self._setPinPullup( _id,  _pullup ) 
                    else:
                        errorManager.append("{lc:s}: id unknown".format( lc=loggingContext ))
                else:
                    errorManager.append("{lc:s}: no id in mcp23s17:io".format( lc=loggingContext ))
        #
        # print self.pins
        #
        # process data
        # dynamically add methods to self.
        #
        for pin in self._getPins():
            x = self._getPin(pin)
            # print pin, x
            if x['dir'] == self.OUT:
                #
                # bind function to class.
                # see http://www.ianlewis.org/en/dynamically-adding-method-classes-or-class-instanc 
                #
                # a input function corresponds to an output pin
                if debug:
                    print ( "define input function", "input" + pin, " func ", x['ifunc'] )
                method = MethodType(x['ifunc'], self, type(self))
                setattr(self, "input" + pin, method )         

            if x['dir'] == self.IN:
                # a output function corresponds to an input pin
                if debug:
                    print ( "define value output function", "output" + pin)
                #method = MethodType(self.pins[pin]['ifunc'], self, type(self))
                setattr(self, "output" + pin, self._sendValue )  
                # register        
        
    def setActive (self, state):
        if debug:
            print(self.name, "setActive", state)
        #
        #
        #
        if '23s17.addr' in self.parameters:
            self.addr =  int(self.parameters['23s17.addr'])
        else:
            logger.error("could not find parameter '23s17.addr' in " + str(self.parameters))
 
        #
        # use default implementation to start up SPI
        #
        adapter.adapters.SPIAdapter.setActive(self, state);
        #
        #
        if state == True:
            # Initialize port directions
            # a '1' is direction INPUT
            #
            portDir = [0, 0]   
            for pin in self._getPins():
                x = self._getPin(pin)
                #print pin, x
                if x['dir'] == self.IN:
                    portDir[ x['port']] |=  x ['bitValue']
            
            self.setPortA_dir ( portDir[0] )
            self.setPortB_dir ( portDir[1] )
            #
            # Initialize Pullup
            #
            portPullup = [0, 0]   
            for pin in self._getPins():
                x = self._getPin(pin)
                #print pin, x
                if x['pullup'] == self.WEAK:
                    portPullup[ x['port']] |=  x ['bitValue']
            
            self.setPortA_pullup ( portPullup[0] )
            self.setPortB_pullup ( portPullup[1] )

    def run(self):
        _del = float(self.parameters['poll.interval'])
            
        readA = False
        readB = False
        #
        # The pin data, organized based on ports and only used pins with direction IN
        #
        pinsA = {}
        pinsB = {}
        
        for pin in self._getPins():
            x = self._getPin(pin)
            # print pin, self.pins[pin]
            if x ['dir'] == self.IN:
                if x ['port'] == 0:
                    readA = True
                    pinsA[pin] = self._getPin(pin)
                if x ['port'] == 1:
                    readB = True
                    pinsB[pin] = self._getPin(pin)
                   
        lastA = None
        lastB = None
        
        while not self.stopped():
            #
            # delay 5 sec, but break time in small junks to allow stopping fast
            #
            self.delay(_del)

            if readA:
                p = self.getPortA()
                if debug:
                    print ('read A {data:08b}'.format(data=p))
                if lastA != p:
                    lastA = p
                    for pin in pinsA:
                        # print pin, self.pins[pin]
                        x = pinsA[pin]
                        if p &  x['bitValue']:
                            if 1 != x['lastValue']:
                                x['lastValue'] = 1
                                self.sendValueByName("outputGPA"+pin, '1')
                        else:
                            if 0 != x['lastValue']:
                                x['lastValue'] = 0
                                self.sendValueByName("outputGPA"+pin, '0')
                   
            if readB:
                
                p = self.getPortB()
                if debug:
                    print ('read B {data:08b}'.format(data=p))
                if lastB != p:
                    lastB = p
                    for pin in pinsB:
                        x = pinsB[pin]
                        if debug:
                            print (pin, self.pins[pin], x )
                        if p &  x['bitValue']:
                            if 1 != x['lastValue']:
                                x['lastValue'] = 1
                                if debug:
                                    print("send ", "output"+pin, 1)
                                self.sendValueByName("output"+pin, '1')
                        else:
                            if 0 != x['lastValue']:
                                x['lastValue'] = 0
                                if debug:
                                    print("send ", "output"+pin, 0)
                                self.sendValueByName("output"+pin, '0')
        
                
    READ = 1
    WRITE = 0
    # Bank 0 Addresses
    IODIRA = 0x00
    IODIRB = 0x01
    
    IODIR_BIT_READ = 1
    IODIR_BIT_WRITE = 0
    
    GPIOA = 0x12
    GPIOB = 0x13
 
    GPPUA = 0x0C
    GPPUB = 0x0D
    
    addr = 0
    
    def setPortA_dir(self, value):
        if debug:
            print("Port A DIR ", value)

        addr1 = 0x40 | self.addr << 1 | self.WRITE
        addr2 = self.IODIRA
        cmdValue = value;
        
        data = list([addr1, addr2, cmdValue])
        self.writeRawData (   data )
        
    def setPortA_pullup(self, value):
        if debug:
            print("Port A PULLUP ", value)

        addr1 = 0x40 | self.addr << 1 | self.WRITE
        addr2 = self.GPPUA
        cmdValue = value;
        
        data = list([addr1, addr2, cmdValue])
        self.writeRawData (  data )
    
    def setPortA(self,  value):
        if debug:
            print("Port A  ", value)
        
        addr1 = 0x40 | self.addr << 1 | self.WRITE
        addr2 = self.GPIOA
        
        data = list([addr1, addr2, value])
        self.writeRawData (   data )
    
    def setPortB_dir(self, value):
        if debug:
            print("Port B DIR ", value)
        
        addr1 = 0x40 | self.addr << 1 | self.WRITE
        addr2 = self.IODIRB
        cmdValue = value;
        
        data = list([addr1, addr2, cmdValue])
        self.writeRawData ( data )

    def setPortB_pullup(self, value):
        if debug:
            print("Port B PULLUP ", value)

        addr1 = 0x40 | self.addr << 1 | self.WRITE
        addr2 = self.GPPUB
        cmdValue = value;
        
        data = list([addr1, addr2, cmdValue])
        self.writeRawData (  data )

    def setPortB(self,  value):
        if debug:
            print("Port B  ", value)
        addr1 = 0x40 | self.addr << 1 | self.WRITE
        addr2 = self.GPIOB
        data = list([addr1, addr2, value])
        self.writeRawData (   data )

    def getPortA(self):
        addr1 = 0x40 | self.addr << 1 | self.READ
        addr2 = self.GPIOA
        r = self.writeRawData (list([addr1, addr2, 0]) )
        #print r
        return r[2]

    def getPortB(self):
        addr1 = 0x40 | self.addr << 1 | self.READ
        addr2 = self.GPIOB
        r = self.writeRawData ( list([addr1, addr2, 0]) )
        #print r
        return r[2]

    def writeRawData(self, spi, data):
        self._lock.acquire()
        try:
            self.spi.max_speed_hz = self.max_speed_hz
            r = self.spi.xfer2( list(data)  ) 
            return r
        finally:
            self._lock.release()
            