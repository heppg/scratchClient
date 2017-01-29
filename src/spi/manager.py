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

# 
#
#
import time
import logging
logger = logging.getLogger(__name__)

try:
    import spidev
except ImportError as e:
    logger.warn(e)        

class _SPIData:
    spi = None
    spiHandler = None
    
    def __init__(self, spi, spiHandler):
        self.spi = spi
        self.spiHandler = spiHandler
        
class _SPIRegistry:
    
    spiBusDeviceHandler = None
    
    def __init__(self):
        self.spiBusDeviceHandler = {}
    
    def getBusDeviceHandler(self, bus, device):
        k = str(bus) + '$' + str(device)
        try:
            return self.spiBusDeviceHandler[k]
        except:
            return None
    
    def addBusDeviceHandler(self, bus, device, spi, spiHandler):
        k = str(bus) + '$' + str(device)
        self.spiBusDeviceHandler[k] = _SPIData(spi, spiHandler)
        
    def closeAll(self):
        for k in self.spiBusDeviceHandler.keys():
            spiData = self.spiBusDeviceHandler[k]
            spiData.spi.close()
        self.spiBusDeviceHandler = {}

            
class _Handler_WS2801:
    def __init__(self):
        pass
    
    def getValue(self, spi, channel):
        pass
    
    def writeValue(self, spi, channel, data):

        spi.max_speed_hz = 2000000
        # print(data)
        r = spi.xfer2(data)
        return r 
    
    def writeRawData(self, spi, data):

        spi.max_speed_hz = 2000000
        # print(data)
        r = spi.xfer2( list( data) )
        return r 

class _Handler_MCP3202_10bit:
    def __init__(self):
        pass
    
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
        # print(" i {r0:08b} {r1:08b} {r2:08b} {r3:08b} ".format(r0=i[0], r1=i[1], r2=i[2], r3=i[3]))
        
        r = spi.xfer2(  i  )  # these two lines are explained in more detail at the bottom
        
        # print(" r {r0:08b} {r1:08b} {r2:08b} {r3:08b} ".format(r0=r[0], r1=r[1], r2=r[2], r3=r[3]))
        
        ret = ((r[1]&31) << 6) + (r[2] >> 2)
        return ret 
    
    def writeValue(self, spi, channel, data):
        pass
    def writeRawData(self, spi, data):
        pass
    
class _Handler_MCP3202_12bit:
    def __init__(self):
        pass
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
        # We must then parse out the correct 12-bit byte from the 24 bits returned. The following line discards
        # all bits but the 12 data bits from the center of the last 2 bytes: XXXX XXXX - XXXX DDDD - DDDD DDDD 

        spi.max_speed_hz = 800000
        r = spi.xfer2([1, (2+channel)<<6, 0])  # these two lines are explained in more detail at the bottom
        ret = ((r[1]&31) << 8) + (r[2] )
        return ret 
    
    def writeValue(self, spi, channel, data):
        pass
    def writeRawData(self, spi, data):
        pass

class _Handler_MAX31855:
    def __init__(self):
        pass
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
        # We must then parse out the correct 12-bit byte from the 24 bits returned. The following line discards
        # all bits but the 12 data bits from the center of the last 2 bytes: XXXX XXXX - XXXX DDDD - DDDD DDDD 

        spi.max_speed_hz = 5000000
        r = spi.xfer2([0,0,0,0])  # these two lines are explained in more detail at the bottom
        # print(" r {r0:08b} {r1:08b} {r2:08b} {r3:08b} ".format(r0=r[0], r1=r[1], r2=r[2], r3=r[3]))

        #
        # 14 bit number, signed
        #
        temp_ext = ((r[0] & 0b11111111) << 6 ) + ((r[1] & 0b11111100) >> 2 )
        if r[0] & 0b10000000 != 0:
            temp_ext = -( (~temp_ext & 0b11111111111111) +1)
        #
        # 12 bit number, signed
        #   
        temp_int = ((r[2] & 0b11111111) << 4 ) + ((r[3] & 0b11110000) >> 4 )
        if r[2] & 0b10000000 != 0:
            temp_int = -( (~temp_int & 0b111111111111) +1)
        
        ret = { 'temp_ext': float( temp_ext * 0.25) , 'temp_int': float( temp_int * 0.0625)}
        ret['error'] = ''
        if r[3] & 0b00000100 > 0 :
            ret['error'] = 'SCV: short to Vcc'
        if r[3] & 0b00000010 > 0 :
            ret['error'] = 'SCG: short to GND'
        if r[3] & 0b00000001 > 0 :
            ret['error'] = 'OC: open circuit'
            
        # print ret
        return ret 
    
    def writeValue(self, spi, channel, data):
        pass
    def writeRawData(self, spi, data):
        pass
    

class _Handler_MCP23S17:
    
    def __init__(self):
        self.max_speed_hz = 10000000
        pass
    
    def getValue(self, spi, channel):
        ret = { 'temp_ext': float( 0.25) , 'temp_int': float( 0.0625) }
        ret['error'] = ''
        return ret 
    
    def writeValue(self, spi, channel, data):
        pass
    
    def writeRawData(self, spi, data):
        spi.max_speed_hz = self.max_speed_hz
        r = spi.xfer2( list(data)  ) 
        return r
    
class _Handler_Atmel328:
    """handler for 'raw' commands. Sets speed very low, as the controller
    is running only 8MHz"""
    
    # 120000 --> 60 kHz, byte gap 24.5 us
    # 200000 --> 125kHz, byte gap 12.5 us
    # 240000 --> 250kHz, byte gap 6 us
    
    max_speed_hz = 120000
    
    def __init__(self):
        pass
    
    def getValue(self, spi, channel):
        ret = [0]
        return ret 
    
    def writeValue(self, spi, channel, data):
        pass
       
    def writeRawData(self, spi, dataList):
        spi.max_speed_hz = self.max_speed_hz
        r = spi.xfer2( list(dataList)  ) 
        return r
            
    
class SPIManager:
    """SPIManager for spi access"""
    
    spiRegistry = None

    # The spi bus is generic, but devices are not.
    # for each device, a handler is needed to build commands and retrieve values.
    # For one bus, device, the handler is fixed during runtime (no hot swapping of devices allowed).
    # Channels are an additional feature within a handler, e,g, a multichannel ADC.
    #
    type_MCP3202_10bit = 'MCP3202_10'
    type_MCP3202_12bit = 'MCP3202_12'
    type_WS2801        = 'WS2801'
    type_Atmel328      = 'Atmel328'
    type_MAX31855      = 'MCP31855'
    type_MCP23S17      = 'MCP23S17'
    
    handlers = {
                type_MCP3202_10bit: _Handler_MCP3202_10bit() ,
                type_MCP3202_12bit: _Handler_MCP3202_12bit() ,
                type_WS2801       : _Handler_WS2801()        ,
                type_Atmel328     : _Handler_Atmel328()      ,
                type_MAX31855     : _Handler_MAX31855()      ,
                type_MCP23S17     : _Handler_MCP23S17()      ,
                }
        
    def __init__(self):
        self.spiRegistry = _SPIRegistry()

    def setActive(self, state):
        """called from main module, but has no function for this manager"""
        pass
    
    def open(self, bus, device, deviceType ):
        #
        # avoid multiple open
        #
        if None == self.spiRegistry.getBusDeviceHandler(bus, device):
            
            # print("spi.open()")
            spi_0 = spidev.SpiDev()
            spi_0.open(bus, device)
            time.sleep(0.1)
            
            handler = self.handlers[deviceType]
            if handler == None:
                raise Exception("no handler defined")
            
            self.spiRegistry.addBusDeviceHandler(
                                                 bus,
                                                 device,
                                                 spi_0,
                                                 handler )

    def getValue(self, bus, device, channel):
        spiData = self.spiRegistry.getBusDeviceHandler(bus, device)
        return spiData.spiHandler.getValue(spiData.spi, channel)

    def writeValue(self, bus, device, channel, data):
        spiData = self.spiRegistry.getBusDeviceHandler(bus, device)
        return spiData.spiHandler.writeValue(spiData.spi, channel, data)
    
    def writeRawData ( self, bus, device, data ):
        # print("writeRawData", bus, device, data)
        spiData = self.spiRegistry.getBusDeviceHandler(bus, device)
        r =  spiData.spiHandler.writeRawData (spiData.spi, data )
        return r
    
    def close(self):
        """closes all connections, selective shutdowns are not implemented"""
        self.spiRegistry.closeAll()
