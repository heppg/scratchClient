# -*- coding: utf-8 -*-
    # --------------------------------------------------------------------------------------------
    # Copyright (C) 2014  Gerhard Hepp
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
# For this adapter, there are additional libraries needed.
#
# 
# sudo apt-get python-smbus
# sudo modprobe i2c-bcm2708
# sudo modprobe i2c-dev
#
#
import time
import threading

import logging
logger = logging.getLogger(__name__)

try:
    import smbus
except ImportError as e:
    logger.warn(e)        

debug=True

class _I2CData:
    i2cbus = None
    address = None
    i2cHandler = None
     
    def __init__(self, i2cbus, address):
        self.i2cbus = i2cbus
        self.address = address
        
         
class _I2CRegistry:
    spiBusDeviceHandler = None
    
    def __init__(self):
        self.spiBusDeviceHandler = {}
     
    def getBusDeviceHandler(self, bus, address):
        k = str(bus) + '$' + str(address)
        try:
            return self.spiBusDeviceHandler[k]
        except:
            return None
    
    def addBusDeviceHandler(self, bus, address, i2cbus):
        k = str(bus) + '$' + str(address)
        self.spiBusDeviceHandler[k] = _I2CData(i2cbus, address)
         
    def closeAll(self):
        for k in self.spiBusDeviceHandler.keys():
            i2cData = self.spiBusDeviceHandler[k]
            i2cData.i2cbus.close()
        self.spiBusDeviceHandler = {}
 
 
        
class I2CManager:
    """I2CManager for i2c access"""
    
    i2cRegistry = None

    def __init__(self):
        self.i2cRegistry = _I2CRegistry()

    def setActive(self, state):
        """called from main module, but has no function for this manager"""
        pass

    def open(self, bus, address ):
        i2cbus = self.i2cRegistry.getBusDeviceHandler(bus, address)
        if i2cbus == None:

            try:
                i2cbus = smbus.SMBus( bus)
            except IOError as e:
                print("error", bus, e)
                logger.error("i2c manager open, error %s bus %d", e, bus)
            
            self.i2cRegistry.addBusDeviceHandler(
                                                 bus,
                                                 address,
                                                 i2cbus )
        return i2cbus
    
    def getValue(self, bus, address, channel):
        i2cData = self.i2cRegistry.getBusDeviceHandler(bus, address)
        return i2cData.i2cHandler.getValue(i2cData, channel)
    
    def getValues(self, bus, address, channel):
        """if a dictionary is needed for multiple results"""
        i2cData = self.i2cRegistry.getBusDeviceHandler(bus, address)
        return i2cData.i2cHandler.getValues(i2cData, channel)

    def writeValue(self, bus, address, channel, data):
        i2cData = self.i2cRegistry.getBusDeviceHandler(bus, address)
        return i2cData.i2cHandler.writeValue(i2cData, channel, data)

    def close(self):
        """closes all connections, selective shutdowns are not implemented"""
        self.i2cRegistry.closeAll()
        