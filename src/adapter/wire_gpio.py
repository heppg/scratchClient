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
import time
import logging
import math

logger = logging.getLogger(__name__)
debug = False

# --------------------------------------------------------------------------------------
#
# Two wire protocol (not I2C).
#
class Wire_SHTx (adapter.adapters.GPIOAdapter):
    """SHT11, 15 humidity and temperature sensor"""
    mandatoryParameters = {'poll.interval'}
    
    def __init__(self):
        adapter.adapters.GPIOAdapter.__init__(self)
        # self.start()
        pass

    def setGpioManager(self, gpioManager):
        adapter.adapters.GPIOAdapter.setGpioManager(self, gpioManager)
#         for gpio in self.gpios:
#             self.gpioManager.setGPIOActive( gpio, True)
#             pass 

        
    def setActive (self, active):
        logger.debug("Adapter, {name:s} setActive({act:s}) ".format(name=self.name, act=str(active)))
        for gpio in self.gpios:
            self.gpioManager.setGPIOActive( gpio, True)
            pass 
        adapter.adapters.GPIOAdapter.start(self)
        
    def stop(self):
        adapter.adapters.GPIOAdapter.stop(self)
        
        for gpio in self.gpios:
            self.gpioManager.setGPIOActive( gpio, False)
            pass 
        
        
    #   Conversion coefficients from SHT15 datasheet
    # D1 = -40.0  # for 14 Bit @ 5V
    D1 = -39.7  # for 14 Bit @ 3.3V
    D2 =  0.01 # for 14 Bit DEGC

    C1 = -2.0468       # for 12 Bit
    C2 =  0.0367       # for 12 Bit
    C3 = -0.0000015955 # for 12 Bit
    T1 =  0.01      # for 14 Bit @ 5V
    T2 =  0.00008   # for 14 Bit @ 5V

    TEMPERATURE_COMMAND = 0b00000011
    HUMIDITY_COMMAND =  0b00000101
    
    def read_temperature_C(self):
        try:
            self.__sendCommand(self.TEMPERATURE_COMMAND)
            self.__waitForResult()
        except SystemError:
            logger.error("SystemError: No return value from sensor ")
            return None
        
        rawTemperature = self.__getData16Bit()
        self.__skipCrc()
        self.__cleanup()

        return rawTemperature * self.D2 + self.D1
        
    def read_humidity(self):
#        Get current temperature for humidity correction
        temperature = self.read_temperature_C()
        if temperature == None:
            return None
        return self._read_humidity(temperature)
    
    def _read_humidity(self, temperature):
        try:
            self.__sendCommand( self.HUMIDITY_COMMAND )
            self.__waitForResult()
        except SystemError:
            logger.error("SystemError: No return value from sensor ")
            return None
        
        rawHumidity = self.__getData16Bit()
        self.__skipCrc()
        self.__cleanup()
#        Apply linear conversion to raw value
        linearHumidity = self.C1 + self.C2 * rawHumidity + self.C3 * rawHumidity * rawHumidity
#        Correct humidity value for current temperature
        return (temperature - 25.0 ) * (self.T1 + self.T2 * rawHumidity) + linearHumidity            

    def calculate_dew_point(self, temperature, humidity):
        if temperature > 0:
            tn = 243.12
            m = 17.62
        else:
            tn = 272.62
            m = 22.46
        return tn * (math.log(humidity / 100.0) + (m * temperature) / (tn + temperature)) / (m - math.log(humidity / 100.0) - m * temperature / (tn + temperature))

    def __sendCommand(self, command):
        if debug:
            logger.info("__sendCommand %x", command)
        #Transmission start
        self.gpioManager.direction_out(self.dataPort)
        self.gpioManager.direction_out(self.clockPort)
        
        self.gpioManager.high(self.dataPort)
        
        self.__clockTick_high()
        self.gpioManager.low(self.dataPort)
        self.__clockTick_low()
        self.__clockTick_high()
        self.gpioManager.high(self.dataPort)
        self.__clockTick_low()

        for i in range(8):
            if  command & (1 << 7 - i):
                self.gpioManager.high(self.dataPort)
            else:
                self.gpioManager.low(self.dataPort)
                
            self.__clockTick_high ()
            self.__clockTick_low ()     
        
        self.__clockTick_high ()
        
        self.gpioManager.direction_in(self.dataPort)
        
        ack = self.gpioManager.get(self.dataPort)
        if logger.isEnabledFor(logging.DEBUG):
            logger.debug("ack1: %s", ack)
        if ack != 0:
            logger.error("nack1")
        
        self.__clockTick_low()
        
        ack = self.gpioManager.get(self.dataPort)
        if logger.isEnabledFor(logging.DEBUG):
            logger.debug("ack2: %s", ack)
        if ack != 1:
            logger.error("nack2")        
            
    def __clockTick_high(self ):
        if debug:
            logger.info("__clockTick_high")
        self.gpioManager.high (self.clockPort)
#       100 nanoseconds 
        time.sleep(.0000001)
        
    def __clockTick_low(self ):
        if debug:
            logger.info("__clockTick_low")
        self.gpioManager.low (self.clockPort)
#       100 nanoseconds 
        time.sleep(.0000001)
    
    def __cleanup(self):
        if debug:
            logger.info("__cleanup")
        # data pin first, da sonst ggf bei Freigeben 
        # der clk-oin ein Signal ausgelöst wird
        self.gpioManager.direction_in(self.dataPort)
        self.gpioManager.direction_in(self.clockPort)
             
    def __waitForResult(self):
        if debug:
            logger.info("__waitForResult begin")
            
        self.gpioManager.direction_in(self.dataPort)

        for _ in range(100):
#            10 milliseconds
            time.sleep(.01)
            ack = self.gpioManager.get(self.dataPort)
            if ack == 0:
                break
        if ack == 1:
            raise SystemError
        if debug:
            logger.info("__waitForResult end")
            
    def __getData16Bit(self):
        if debug:
            logger.info("__getData16Bit begin")
        self.gpioManager.direction_in(self.dataPort)
        self.gpioManager.direction_out(self.clockPort)
        # Get the most significant bits
        value = self.__shiftIn(8)
        value *= 256
#        Send the required ack
        self.gpioManager.direction_out(self.dataPort)
        self.gpioManager.high(self.dataPort)
        self.gpioManager.low(self.dataPort)
        self.__clockTick_high()
        self.__clockTick_low()
#        Get the least significant bits
        self.gpioManager.direction_in(self.dataPort)
        value |= self.__shiftIn(8)
        if debug:
            logger.info("__getData16Bit end  %x", value)
        
        return value
    
    def __shiftIn(self, bitNum):
        if debug:
            logger.info("__shiftIn begin %d", bitNum)
        value = 0
        for i in range(bitNum):
            self.__clockTick_high()
            value = value * 2 + self.gpioManager.get(self.dataPort)
            self.__clockTick_low()
        if debug:
            logger.info("__shiftIn end %x", value)
        return value
     
    def __skipCrc(self):
#        Skip acknowledge to end trans (no CRC)
        self.gpioManager.direction_out(self.dataPort)
        self.gpioManager.direction_out(self.clockPort)
        
        self.gpioManager.high(self.dataPort)
        self.__clockTick_high()
        self.__clockTick_low()
    
    def __connectionReset(self):
        if debug:
            logger.info("__connectionReset begin")
        
        self.gpioManager.direction_out(self.dataPort)
        self.gpioManager.direction_out(self.clockPort)
        
        self.gpioManager.high(self.dataPort)
        for _ in range(10):
            self.__clockTick_high()
            self.__clockTick_low()
            
        if debug:
            logger.info("__connectionReset end")
        
        
    def run(self):
        _del = float(self.parameters['poll.interval'])
        
        self.dataPort = self.getChannelByAlias('data')    
        self.clockPort = self.getChannelByAlias('clock')    
        
        lastT = self.read_temperature_C()
        lastH = self.read_humidity()
        
        self.temperature( lastT )
        self.humidity( lastH )
                                
        while not self.stopped():

            self.delay(_del)
            
            T = self.read_temperature_C()
            H = self.read_humidity()
            
            if T != lastT:
                if T == None:
                    self.temperature("")
                else:
                    self.temperature(T)
                lastT = T
                                   
            if H != lastH:
                if H == None:
                    self.humidity("")
                else:
                    self.humidity(H)
                lastH = H
                
    def temperature(self, value):
        """output from adapter to scratch."""
        self.sendValue(str(value))
    def humidity(self, value):
        """output from adapter to scratch."""
        self.sendValue(str(value))
                                   
# --------------------------------------------------------------------------------------
