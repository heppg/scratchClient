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
import logging
logger = logging.getLogger(__name__)

#simulationFlag = False
debug = False

try:
    import RPi.GPIO as GPIO
except ImportError:
    #simulationFlag = True
    logger.error("Error importing RPi.GPIO!  This is probably because you need superuser privileges.  You can achieve this by using 'sudo' to run your script")


class PWM:
    """registers gpio to pwm relations"""
    
    gpio = None
    pwm = None
    def __init__(self, gpio, pwm):
        self.gpio = gpio
        self.pwm = pwm

class PWMRegistry:
    pwms = None
    
    def __init__(self):
        self.pwms = {}
    
    def get(self, gpio):
        return self.pwms[gpio]
    
    def append(self, pwm):
        self.pwms[pwm.gpio] = pwm    
        
class GPIOManager:
    """GPIOManager for RPi.GPIO library"""
    
    pwms = None
    usageCount = 0
    
    def __init__(self):
        GPIO.setmode(GPIO.BCM)
        self.pwms = PWMRegistry()
        self.usageCount = 0
        pass
    
    #def getSimulation(self):
    #    return simulationFlag

    def setActive(self, state):
        logger.info("GPIOManager %s, %s", "setActive", str(state))
        if state:
            self.usageCount += 1
            # print("activate GPIO system")
            GPIO.setwarnings(True)
            logger.info(GPIO.VERSION)
        else:
            self.usageCount -= 1
            if self.usageCount == 0:
                GPIO.cleanup()
            
    def low(self, gpio):
        if debug:
            logger.debug("low %s", gpio)
            
        GPIO.output(gpio.getPort(), GPIO.LOW)    

    def high(self, gpio):
        if debug:
            logger.debug("high %s", gpio)
            
        GPIO.output(gpio.getPort(), GPIO.HIGH)    

    def get(self, gpio):
        return GPIO.input(gpio.getPort())    

    def direction_in(self, gpio):
        if debug: 
            logger.debug("direction_in %s", gpio)
        GPIO.setup(gpio.getPort(), GPIO.IN)
        
    def direction_out(self, gpio):
        if debug: 
            logger.debug("direction_out %s", gpio)
        GPIO.setup(gpio.getPort(), GPIO.OUT)

    def startPWM(self, gpio, frequency=20.0, rate=50.0):
        # if gpio in self.pwms:
        #     raise RuntimeError("GPIO {gpio:d} already registered as PWM".format(gpio=gpio))
        # self.pwms.append(gpio)
        GPIO.setup(gpio.getPort(), GPIO.OUT)
        pwm = GPIO.PWM( gpio.getPort(), frequency)
        pwm.start(rate)
        self.pwms.append( PWM(gpio, pwm))
    
    def setPWMDutyCycle(self, gpio, value):
        pwm = self.pwms.get(gpio)
        
        if value < 0:
            value = 0
        if value > 100:
            value = 100
            
        pwm.pwm.ChangeDutyCycle(value)
    
    
    def resetPWM(self, gpio):
        pwm = self.pwms.get(gpio)
        pwm.pwm.stop()
            
    
    def setGPIOActive(self, gpioConfiguration, state):
        logger.debug("RPi_GPIO, setGPIOActive %s, %s", 'state', str(state))
        if state:
            logger.info("activeSetting")
            if debug:
                print(gpioConfiguration)
            self.setGpioState(gpioConfiguration, gpioConfiguration.active_setting)
        else:
            logger.info("defaultSetting")
            self.setGpioState(gpioConfiguration, gpioConfiguration.default_setting)
            
    def setGpioState(self, gpioConfiguration, setting):
        logger.debug("setGpioState %s", str(setting))              
        if setting.dir == 'RESERVED':
            if debug:
                logger.debug("gpio reserved, do not touch")
        
        gPull = GPIO.PUD_OFF
        if setting.pull == None or setting.pull == 'PUD_OFF':    
            pass
        elif setting.pull == 'PUD_UP':
            gPull = GPIO.PUD_UP;
        elif setting.pull == 'PUD_DOWN':
            gPull = GPIO.PUD_DOWN;
        else:
            pass
        
        if setting.dir == 'OUT':
            if debug:
                print('set direction OUT', gpioConfiguration.portNumber)
            GPIO.setup(gpioConfiguration.portNumber, GPIO.OUT)
        elif setting.dir == 'IN':
            GPIO.setup(gpioConfiguration.portNumber, GPIO.IN, gPull)
        else:
            pass

        if setting.dir == 'OUT':
            if setting.default == 'low':
                if debug:
                    print('low')
                self.low( gpioConfiguration)
            elif setting.default == 'high':
                if debug:
                    print('high')
                self.high( gpioConfiguration)
            else:
                pass
