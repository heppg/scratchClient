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

import configuration
#import eventHandler
import publishSubscribe
import inspect
import threading
import time
import re
# from spi.manager import SPIManager
import logging

logger = logging.getLogger(__name__)

try:
    import spidev
except ImportError as e:
    logger.warn(e)        

debug   = False
verbose = False
        
class Adapter (configuration.AdapterSetting):
    """base functionality for adapters"""
    
    description = None
    
    active = None
    #
    # parameter dict
    #        
    parameters = None

    #    
    # InputSetting, connects scratch names with method names
    #
    inputs = None
    input_values = None
    #
    # OutputSettings, connects output names to scratch names
    #
    outputs = None
    output_values = None
    #
    scratchInputMethod = None
    #
    mandatoryParameters = None
    mandatoryAlias = None

    # state = None

    thread = None
    
    def __init__(self):
        # 
        # self.state = self.STATE_NORMAL
        self.active = False
        # threading.Thread.__init__(self)
        self._stop = threading.Event()
        
        self.parameters = {}
        
        self.inputs = []
        self.outputs = []
        self.input_values = []
        self.output_values = []
        self.scratchInputValueMethod = []
        self.mandatoryAlias = []
        self.scratchInputMethod = []
        
    def setName(self, name): 
        self.name = name       
        #eventHandler.register("adapter", self.name, self)
    
    def setOutputs(self, outputs):
        """ inputs is configuration.OutputSetting"""
        self.outputs = outputs
    def addOutputs(self, outputs):
        """ inputs is configuration.OutputSetting"""
        self.outputs.extend( outputs )

    def setInputs(self, inputs):
        """ inputs is configuration.InputSetting"""
        self.inputs = inputs
        self._setInputs(inputs)
        
    def _setInputs(self, inputs):
        
        moduleMethodDict = {} # dict( inspect.getmembers(self, inspect.ismethod ) )
        for c in inspect.getmembers(self, inspect.ismethod ):
            moduleMethodDict[c[0]] = c[1]
         
        for inp in inputs:
            for sn in inp.scratchNames:
                topic = "scratch.input.command.{name:s}".format(name=sn)
                publishSubscribe.Pub.subscribe(topic, self.resolveCommand)
               
        for inp in inputs:
            for sn in inp.scratchNames:
                f = moduleMethodDict[inp.name]
                self.scratchInputMethod.append( { 'name': sn, 'method' : f} )

    def addInputs(self, inputs):
        """ inputs is configuration.InputSetting"""
        self.inputs.extend( inputs)
        self._setInputs(inputs)
        
        
    def setOutputValues(self, outputs):
        """ inputs is configuration.InputSetting"""
        self.output_values = outputs

    def addOutputValues(self, outputs):
        """ inputs is configuration.InputSetting"""
        self.output_values.extend( outputs)

    def setInputValues(self, inputs):
        """ inputs is configuration.InputSetting"""
        self.input_values = inputs
        self._setInputValues(inputs)
        
    def addInputValues(self, inputs):
        """ inputs is configuration.InputSetting"""
        self.input_values.extend( inputs)
        self._setInputValues(inputs)
        
    def _setInputValues(self, inputs):    
        moduleMethodDict = {} # dict( inspect.getmembers(self, inspect.ismethod ) )
        for c in inspect.getmembers(self, inspect.ismethod ):
            moduleMethodDict[c[0]] = c[1]

        for inp in inputs:
            for sn in inp.scratchNames:
                topic = "scratch.input.value.{name:s}".format(name=sn)
                publishSubscribe.Pub.subscribe(topic, self.resolveValue)
         
        for inp in inputs:
            for sn in inp.scratchNames:
                f = moduleMethodDict[inp.name]
                self.scratchInputValueMethod.append( { 'name': sn, 'method' : f} )        
            
    def start(self):
        """Start adapter Thread"""
        if debug:
            print("Start adapter Thread")
        self.thread = threading.Thread(target=self.run)
        self.thread.setName(self.name)
        self._stop.clear()
        self.thread.start()
        
    def stop(self):
        """stop adapter thread. It is the thread's responsibility to timely shut down"""
        self._stop.set()
        if self.thread != None:
            self.thread.join(1)
            if self.thread.isAlive():
                logger.debug(self.name +  " no timely join in adapter")

    def stopped(self):
        """helper method for the thread's run method to find out whether a stop is pending"""
        return self._stop.isSet()

    def run(self):
        """default, empty implementation für a run method"""
        logger.debug(self.name + " adapter.run()")
        pass
    
    # def registerOutput(self, outputSender):
    #     """used by configurationManager to implement binding to the send-Methods to scratch"""
    #     self.outputSender = outputSender


    def setActive (self, active):
        """default implementation for setActive, needs to be overwritten in derived code
        if more complex operations are needed."""
        logger.debug("Adapter, {name:s} setActive({act:s}) ".format(name=self.name, act=str(active)))
        
        self.active = active
        
        if active:
            self.start()
        else:
            self.stop() 
        
    def isActive (self):
        return self.active

    
    def sendCommandAlias(self, alias):
        publishSubscribe.Pub.publish( "scratch.output.command.{name:s}".format( name=alias ), { 'name':alias } )
        #eventHandler.resolveCommand(self, self.name, alias)
    
    def send(self):
        """send a broadcast event to scratch"""
        callerName = inspect.stack()[1][3]
        
        cnt = 0
        for ov in self.outputs:
            if ov.name == callerName:
                alias = ov.scratchNames[0]
                publishSubscribe.Pub.publish( "scratch.output.command.{name:s}".format( name=alias ), { 'name':alias } )
                #eventHandler.resolveCommand(self, self.name, alias)
                cnt += 1
        if cnt > 1:
            logger.error("MULTIPLE SEND, BAD, REALLY BAD")  
                  
    def sendValue(self, value):
        """send a numeric or string value"""
        callerName = inspect.stack()[1][3]
        
        for ov in self.output_values:
            if ov.name == callerName:
                alias = ov.scratchNames[0]
                topic = "scratch.output.value.{name:s}".format(name=alias)
                if isinstance(value, str):
                    publishSubscribe.Pub.publish( topic, { 'name':alias, 'value':value } )
                elif isinstance(value, unicode):
                    publishSubscribe.Pub.publish( topic , { 'name':alias, 'value':value } )
                else:
                    publishSubscribe.Pub.publish( topic , { 'name':alias, 'value':str(value) } )

    def sendValueByName(self, name,  value):
        """send a numeric or string value"""
        callerName = name
        
        for ov in self.output_values:
            if ov.name == callerName:
                alias = ov.scratchNames[0]
                topic = "scratch.output.value.{name:s}".format(name=alias)
                if isinstance(value, str):
                    publishSubscribe.Pub.publish( topic, { 'name':alias, 'value':value } )
                elif isinstance(value, unicode):
                    publishSubscribe.Pub.publish( topic, { 'name':alias, 'value':value } )
                else:
                    publishSubscribe.Pub.publish( topic, { 'name':alias, 'value':str(value) } )

    def resolveCommand(self, message):
        """events of the adapters entry point"""
        # print("Adapter, resolveCommand", adapter_name, message)
        for x in self.scratchInputMethod:
            if x['name'] == message['name']:
                f = x['method']
                f()
                
    def resolveValue(self, message):
        """value events of the adapters entry point"""
        # print("Adapter, resolveValue", adapter_name, message, value)
        
        for x in self.scratchInputValueMethod:
            if debug:
                print("resolveValue", message, x)
                
            if x['name'] == message['name']:
                f = x['method']
                f(message['value'])
    
    def configureCommandResolver (self, commandResolver):
        for _input in self.inputs:
            for command in _input.scratchNames:
                publishSubscribe.Pub.subscribe('scratch.input.command.{name:s}'.format(name=command), self.resolveCommand ) 
                
        for value in self.input_values:
            for command in value.scratchNames:
                publishSubscribe.Pub.subscribe('scratch.input.value.{name:s}'.format(name=command), self.resolveValue )
    
    def delay(self, t):
        """delay a specific time. break it into time slots, so a stop of adapter almost 
        immediately breaks these loops.
        to be used inside adapter thread run method"""
        t0 = 0
        while t0 + 0.1 < t:
            #print("t0=", t0, "t=", t)
            if self.stopped():
                return
            time.sleep(0.1) 
            t0 += 0.1
        tx = t - t0
        if tx > 0:
            time.sleep(tx)
            
    def isTrue(self, value):
        """ helper method to convert strings to true/false"""
        v = value.upper()
        if v == '1':
            return True
        if v == 'TRUE':
            return True
        if v == 'Y':
            return True
        if v == 'YES':
            return True
        if v == 'HIGH':
            return True
        
        return False 
    
    def isFalse(self, value):
        """ helper method to convert strings to true/false"""
        v = value.upper()
        if v == '0':
            return True
        if v == 'FALSE':
            return True
        if v == 'N':
            return True
        if v == 'NO':
            return True
        if v == 'LOW':
            return True
        
        return False 

    conversion = {
              'red':        [0xff, 0x00, 0x00 ],
              'green':      [0x00, 0xff, 0x00 ],
              'blue':       [0x00, 0x00, 0xff ],
              
              'darkred':    [0x40, 0x00, 0x00 ],
              'darkgreen':  [0x00, 0x40, 0x00 ],
              'darkblue':   [0xff, 0x00, 0x40 ],
              
              'yellow':     [0xff, 0xff, 0x00 ],
              'pink':       [0xfc, 0x0f, 0xc0 ],
              'magenta':    [0xff, 0x00, 0xff ],
              
              'off':        [0x00, 0x00, 0x00 ],
              'black':      [0x00, 0x00, 0x00 ],
              'white':      [0xff, 0xff, 0xff ],
              
              'default':    [0x03, 0x03, 0x03 ],
              }
    
    def getRGBFromString(self, color):
        c = {}
        if color in self.conversion:
            c['red'] = self.conversion[color][0]
            c['green'] = self.conversion[color][1]
            c['blue'] = self.conversion[color][2]
            return c
        
        m =  re.match("(#?)([0-9A-Fa-f]{6})", color)
        if m:
            value = m.group(2)    
            r = value[0:2]
            g = value[2:4]
            b = value[4:6]
            if debug:
                print(r, g, b)
            c['red'] = int(r, 16)
            c['green'] = int(g, 16)
            c['blue'] = int(b, 16) 
            return c
        
        c['red'] = self.conversion['default'][0]
        c['green'] = self.conversion['default'][1]
        c['blue'] = self.conversion['default'][2]
            
        return c                       
           
    def string2int(self, string):
        try:
            n = int(string)
            return n
        except ValueError:
            # second try: get hex
            n = int(string, 16)
            return n
    
    def getOptionalParameter(self, name, default):
        """return a property. as this is optional, a default value needs to be provided"""
        if self.parameters.has_key(name):
            return self.parameters[ name]
        return default
              
class GPIOAdapter (Adapter):
    """base functionality for GPIO based adapters"""
    gpios = None
    gpioManager = None
    
    def __init__(self):
        Adapter.__init__(self)
        
        self.gpios = []

    def setActive (self, active):
        """default implementation for setActive, needs to be overwritten in derived code
        if more complex operations are needed."""
              
        if debug:
            print("GPIOAdapter, setActive", active)
        #
        if active:
            for gpio in self.gpios:
                self.gpioManager.setGPIOActive(gpio, active)
                pass 
            Adapter.setActive(self, active)  
        else:
            Adapter.setActive(self, active)  
            for gpio in self.gpios:
                self.gpioManager.setGPIOActive(gpio, active)
                pass 
        
    def setGpioManager(self, gpioManager):
        logger.debug("setGpioManager() {name:s} {gm:s}".format( name=self.name, gm=str(gpioManager) ))
        self.gpioManager = gpioManager

    def getChannelByAlias(self, alias):
        """read a channel aka gpio number by alias"""
        for gpio in self.gpios:
            if gpio.alias != None:
                if gpio.alias == alias:
                    return gpio 
        return None

class DMAAdapter (Adapter):
    """base functionality for DMA based adapters"""
    gpios = None
    dmaManager = None
    
    def __init__(self):
        Adapter.__init__(self)
        
        self.gpios = []

    def setActive (self, active):
        """default implementation for setActive, needs to be overwritten in derived code
        if more complex operations are needed."""
              
        if debug:
            print("DMAAdapter, setActive", active)
        #
        if active:
            for gpio in self.gpios:
                #self.dmaManager.setGPIOActive(gpio, active)
                pass 
            Adapter.setActive(self, active)  
        else:
            Adapter.setActive(self, active)  
            for gpio in self.gpios:
                #self.dmaManager.setGPIOActive(gpio, active)
                pass
             
    def startPWM(self, gpio,frequency,value):
        self.dmaManager.startPWM(gpio, frequency, value/100)
        
    def resetPWM(self, gpio):
        self.dmaManager. clear_channel_gpio( gpio)
        
    def set_pwm(self, gpio, value):
        """value is 0..100"""
        self.dmaManager.set_pwm(gpio, value/100.0)
           
    def setDMAManager(self, dmaManager):
        logger.debug("setDMAManager() {name:s} {gm:s}".format( name=self.name, gm=str(dmaManager) ))
        self.dmaManager = dmaManager

    def getChannelByAlias(self, alias):
        """read a channel aka gpio number by alias"""
        for gpio in self.gpios:
            if gpio.alias != None:
                if gpio.alias == alias:
                    return gpio 
        return None
        
class SPIAdapter (Adapter):
    """base functionality for SPI based adapters"""
    spiManager = None
    handlerType = None
        
    # cached parameters, converted to int from the hashtable provided.
    int_spi_bus = 0 
    int_spi_device = 0 

    def __init__(self):
        
        Adapter.__init__(self)
        
    def setActive (self, state):
        if debug:
            print(self.name, "setActive", state)

        # cache the properties for faster access.
        self.int_spi_bus =      int(self.parameters['spi.bus'])
        self.int_spi_device =   int(self.parameters['spi.device']) 

        if state == True:
            logger.debug("spi open bus:{bus:d} device:{device:d}".format(bus=self.int_spi_bus, device=self.int_spi_device))   
                                  
            self.spi = spidev.SpiDev()
            self.spi.open(self.int_spi_bus, self.int_spi_device)
            time.sleep(0.05)
            
            Adapter.setActive(self, state);
        else:
            Adapter.setActive(self, state);
            self.spi.close()

        
    def setSPIManager(self, spiManager):
        if debug:
            print("setSPIManager()", self.name, spiManager)
        self.spiManager = spiManager
        
            
        
class I2CAdapter (Adapter):
    """base functionality for I2C based adapters"""
    i2cManager = None
    
    # cached parameters, converted to int from the hashtable provided.
    int_i2c_bus = 0 
    int_i2c_address = 0 

    def __init__(self):
       
        Adapter.__init__(self)
        
    def setActive (self, state):
        if debug:
            print(self.name, "setActive", state)

        # cache the properties for faster access.
        self.int_i2c_bus =       int(self.parameters['i2c.bus'], 0)
        self.int_i2c_address =   int(self.parameters['i2c.address'], 0) 

        if state == True:
            logger.debug("i2c open bus:{bus:d} address:{device:2x}".format(bus=self.int_i2c_bus, device=self.int_i2c_address))   
                                  
            self.bus = self.i2cManager.open(self.int_i2c_bus,      
                                            self.int_i2c_address                                   
                                )
            Adapter.setActive(self, state);
        else:
            Adapter.setActive(self, state);
            self.i2cManager.close()
            self.bus = None
            
        
    def setI2CManager(self, i2cManager):
        if debug:
            print("setI2CManager()", self.name, i2cManager)
        self.i2cManager = i2cManager

    def write8(self, reg, value):
        "Writes an 8-bit value to the specified register/address"
        try:
            self.bus.write_byte_data(self.int_i2c_address, reg, value)
            if debug:
                print("I2C: Wrote 0x%02X to register 0x%02X" % (value, reg))
        except IOError as err:
            print ("Error accessing 0x%02X: Check your I2C address" % self.int_i2c_address)
            return -1

    def writeList(self, reg, list):
        "Writes an array of bytes using I2C format"
        try:
            self.bus.write_i2c_block_data(self.int_i2c_address, reg, list)
        except IOError as err:
            print ("Error accessing 0x%02X: Check your I2C address" % self.int_i2c_address)
            return -1

    def readU8(self, reg):
        "Read an unsigned byte from the I2C device"
        try:
            result = self.bus.read_byte_data(self.int_i2c_address, reg)
            if debug:
                print ("I2C: Device 0x%02X returned 0x%02X from reg 0x%02X" % (self.int_i2c_address, result & 0xFF, reg))
            return result
        except IOError as err:
            print ("Error accessing 0x%02X: Check your I2C address" % self.int_i2c_address)
            return -1

    def readS8(self, reg):
        "Reads a signed byte from the I2C device"
        try:
            result = self.bus.read_byte_data(self.int_i2c_address, reg)
            if debug:
                print ("I2C: Device 0x%02X returned 0x%02X from reg 0x%02X" % (self.int_i2c_address, result & 0xFF, reg))
            if (result > 127):
                return result - 256
            else:
                return result
        except IOError as err:
            print ("Error accessing 0x%02X: Check your I2C address" % self.int_i2c_address)
            return -1

    def readU16(self, reg):
        "Reads an unsigned 16-bit value from the I2C device"
        try:
            hibyte = self.bus.read_byte_data(self.int_i2c_address, reg)
            result = (hibyte << 8) + self.bus.read_byte_data(self.int_i2c_address, reg+1)
            if debug:
                print ("I2C: Device 0x%02X returned 0x%04X from reg 0x%02X" % (self.int_i2c_address, result & 0xFFFF, reg))
            return result
        except IOError as err:
            print ("Error accessing 0x%02X: Check your I2C address" % self.int_i2c_address)
            return -1

    def readS16(self, reg):
        "Reads a signed 16-bit value from the I2C device"
        try:
            hibyte = self.bus.read_byte_data(self.int_i2c_address, reg)
            if (hibyte > 127):
                hibyte -= 256
            result = (hibyte << 8) + self.bus.read_byte_data(self.int_i2c_address, reg+1)
            if debug:
                print ("I2C: Device 0x%02X returned 0x%04X from reg 0x%02X" % (self.int_i2c_address, result & 0xFFFF, reg))
            return result
        except IOError as err:
            print ("Error accessing 0x%02X: Check your I2C address" % self.int_i2c_address)
            return -1

    def readU16BE(self, reg):
        return self.readU16(reg, False)
  
    def readS16BE(self, reg, little_endian=True):
        return self.readS16(reg, False)
  
    def readList(self, reg, length):
        "Read a list of bytes from the I2C device"
        try:
            results = self.bus.read_i2c_block_data(self.int_i2c_address, reg, length)
            if debug:
                print ("I2C: Device 0x%02X returned the following from reg 0x%02X" % 
                            (self.int_i2c_address, reg))
                print (results)
            return results
        except IOError as err:
            logger.error("readList " + str(err))
            return None
