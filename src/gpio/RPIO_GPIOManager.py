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
# Use of RPIO gpio-Handling
# http://pythonhosted.org/RPIO/
#
import thread
import threading

import logging
logger = logging.getLogger(__name__)

#simulationFlag = False

try:
    import RPIO 
    from RPIO import PWM
    
except ImportError as e:
    #simulationFlag = True
    logger.error(e)
    logger.error("Error importing RPIO, RPIO.PWM !  This is probably because you need superuser privileges.  You can achieve this by using 'sudo' to run your script")

debug = False

class _PWM:
    """registers gpio to pwm relations"""
    
    gpio = None
    pwm = None
    dmaChannel = None
    subcycle = None
    
    def __init__(self, gpio, pwm, dmaChannel, subcycle_time):
        self.gpio = gpio
        self.pwm = pwm
        self.dmaChannel = dmaChannel
        self.subcycle=subcycle_time

class PWMRegistry:
    pwmRegistry = None
    
    def __init__(self):
        self.pwmRegistry = {}
    
    def get(self, gpio):
        if len(self.pwmRegistry) == 0:
            return None
        try:
            return self.pwmRegistry[gpio.getPort() ]
        except KeyError:
            # print("key error")
            # print(gpio)
            # print( self.pwmRegistry )
            return None
            
    def append(self, pwm):
        self.pwmRegistry[ pwm.gpio.getPort() ] = pwm
         
    def remove(self, gpio):
        del self.pwmRegistry[ gpio.getPort() ]
         
    def getFreeDmaChannel(self): 
        """per PWM ein Channel; channel 3 and 4 are reserved. At least do not work. """
          
        i = len( self.pwmRegistry )
        if i == 0:
            return 0
        if i == 1:
            return 1
        if i == 2:
            return 5
        if i == 3:
            return 6
        
        return i+3
    
class GPIOManager:
    """GPIOManager for RPIO library"""
    
    pwmRegistry = None
    usageCount = 0
    
    def __init__(self):
        RPIO.PWM.setup()
        self.usageCount = 0
        
        if logger.isEnabledFor(logging.DEBUG):
            RPIO.PWM.set_loglevel(RPIO.PWM.LOG_LEVEL_DEBUG)
        else:
            RPIO.PWM.set_loglevel(RPIO.PWM.LOG_LEVEL_ERRORS)
            
        self.lockPWMAccess = threading.Lock()
        self.pwmRegistry = PWMRegistry()
        pass
    
    #def getSimulation(self):
    #    return simulationFlag

    def setActive(self, state):
        logger.info("GPIOManager %s, %s", "setActive", str(state))
        if state:
            self.usageCount += 1
            # print("activate RPIO system")
            # RPIO.setmode(RPIO.BCM)
            # RPIO.setwarnings(True)
            if debug:
                print(RPIO.version())
        else:
            self.usageCount -= 1
            if self.usageCount == 0:
                RPIO.cleanup()
            
    def low(self, gpio):
        if logger.isEnabledFor(logging.DEBUG):
            logger.debug( "RPIO.low() %s", str(gpio) )
        RPIO.output(gpio.getPort(), RPIO.LOW)    

    def high(self, gpio):
        if logger.isEnabledFor(logging.DEBUG):
            logger.debug( "RPIO.high() %s", str(gpio) )
        RPIO.output(gpio.getPort(), RPIO.HIGH)    

    def direction_in(self, gpio):
        RPIO.setup(gpio.getPort(), RPIO.IN)
        
    def direction_out(self, gpio):
        RPIO.setup(gpio.getPort(), RPIO.OUT)

    def get(self, gpio):
        #print("GPIOManager", "get", gpio.getPort())
        return RPIO.input(gpio.getPort())    


    def startPWM(self, gpio, frequency=50.0, rate=50.0):
        logger.info("startPWM %s %s %s", str(gpio), str(frequency), str(rate) )
        self.lockPWMAccess.acquire()
        # if gpio in self.pwmRegistry:
        #     raise RuntimeError("GPIO {gpio:d} already registered as PWM".format(gpio=gpio))
        # self.pwmRegistry.append(gpio)
        
        pwm =  self.pwmRegistry.get(gpio)
        if pwm != None:
            self.lockPWMAccess.release()
            raise RuntimeError("RPIO {gpio:d} already registered as PWM".format(gpio=gpio.getPort()))
            
        RPIO.setup(gpio.getPort(), RPIO.OUT)
        
        #
        # always use channel 1
        # with raspbian 2013-12-20-wheezy-raspbian, dma channel 0 is used for accelerating the
        # X-Server.
        # see also /sys/module/dma/parameters/dmachans
        # 0 is frame buffer
        # 2 is SD-card
        # 1,3,6,7 for GPU
        #
        #dmaChannel = self.pwmRegistry.getFreeDmaChannel()
        dmaChannel = 4
        
        subcycle = int( 1000000 / frequency )
            
        if RPIO.PWM.is_channel_initialized(dmaChannel):
            pass
        else:
            logger.info("dmaChannel %s gpio %s", str(dmaChannel), str(gpio) )
            RPIO.PWM.init_channel(dmaChannel, subcycle_time_us=subcycle )
        
        #
        # rate is in percent
        # width is in 'increment granularity' == 10 us
        #
        width = int( subcycle * float(rate) / 100.0 )
        if width >=  subcycle:
            width = subcycle-1
        width /= 10
        if width > 0:
            RPIO.PWM.add_channel_pulse(dmaChannel, gpio.getPort(), 0, width)
        
        self.pwmRegistry.append( _PWM(gpio, pwm, dmaChannel, subcycle))
        logger.info("startPWM complete")
        self.lockPWMAccess.release()
        
    def setPWMDutyCycle(self, gpio, rate):
        self.lockPWMAccess.acquire()
        logger.debug("setPWMDutyCycle(%f)", rate)
        try:
            if rate < 0 :
                rate = 0.0
            if rate > 100:
                rate = 100.0
            
            pwm = self.pwmRegistry.get(gpio)
            width = int( pwm.subcycle * float(rate) / 100  )
            if width >=  pwm.subcycle:
                width = pwm.subcycle-1
            width /= 10
            
            # RPIO.PWM.clear_channel( pwm.dmaChannel )
            RPIO.PWM.clear_channel_gpio( pwm.dmaChannel, pwm.gpio.getPort() )
            if width > 0:
                RPIO.PWM.add_channel_pulse(pwm.dmaChannel, gpio.getPort(), 0, width)
        except:
            pass
        
        self.lockPWMAccess.release()
        
    def resetPWM(self, gpio):
        pwm = self.pwmRegistry.get(gpio)
        RPIO.PWM.clear_channel(pwm.dmaChannel)
        self.pwmRegistry.remove(gpio)    
    
    def setGPIOActive(self, gpioConfiguration, state):
        logger.debug("RPIO, setGPIOActive %s, %s", 'state', str(state))
        if state:
            logger.info("activeSetting")
            self.setGpioState(gpioConfiguration, gpioConfiguration.active_setting)
        else:
            logger.info("defaultSetting ")
            self.setGpioState(gpioConfiguration, gpioConfiguration.default_setting)
            
    def setGpioState(self, gpioConfiguration, setting):
        logger.debug("setGpioState %s", str(setting))              
        if setting.dir == 'RESERVED':
            if debug:
                logger.debug("gpio reserved, do not touch")
        
        gPull = RPIO.PUD_OFF
        if setting.pull == None or setting.pull == 'PUD_OFF':    
            pass
        elif setting.pull == 'PUD_UP':
            gPull = RPIO.PUD_UP;
        elif setting.pull == 'PUD_DOWN':
            gPull = RPIO.PUD_DOWN;
        else:
            pass
        
        if setting.dir == 'OUT':
            if debug:
                print(gpioConfiguration.portNumber)
            RPIO.setup(gpioConfiguration.portNumber, RPIO.OUT)
        elif setting.dir == 'IN':
            RPIO.setup(gpioConfiguration.portNumber, RPIO.IN, gPull)
        else:
            pass

        if setting.dir == 'OUT':
            if setting.default == 'low':
                self.low( gpioConfiguration)
            elif setting.default == 'high':
                self.high( gpioConfiguration)
            else:
                pass
