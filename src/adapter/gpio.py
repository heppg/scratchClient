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
logger = logging.getLogger(__name__)

debug = False

# --------------------------------------------------------------------------------------
#
# This is a special implementation, which outputs signals on adapter state.
# DO NOT use this adapter as a sample for new adapters, as lifecycle is
# different from regular adapters.
#
class GpioStateOutput (adapter.adapters.GPIOAdapter):
    """state based output. Needed for display of connection state"""
    mandatoryParameters = {}
    
    def __init__(self):
        if debug:
            print("STATE INIT")
        adapter.adapters.GPIOAdapter.__init__(self)
        self.start()
        pass

    def setGpioManager(self, gpioManager):
        adapter.adapters.GPIOAdapter.setGpioManager(self, gpioManager)
        for gpio in self.gpios:
            self.gpioManager.setGPIOActive( gpio, True)
            pass 

        
    def setActive (self, active):
        logger.debug("Adapter, {name:s} setActive({act:s}) ".format(name=self.name, act=str(active)))
        self.active = active
    
    def stop(self):
        adapter.adapters.GPIOAdapter.stop(self)
        
        for gpio in self.gpios:
            self.gpioManager.setGPIOActive( gpio, False)
            pass 
        
    def run(self):
        # _del = float(self.parameters['poll.interval'])
        if True:
            _del_high = float('0.06')
            _del_low = float('0.6')
        else:
            _del_high = float('5')
            _del_low = float('5')
                
        while not self.stopped():

            self.delay(_del_low)
            
            if self.gpioManager:
                if self.active == False:
                    self.gpioManager.high(self.gpios[0])
                if self.active == True:
                        
                    self.gpioManager.high(self.gpios[0])
                    self.delay(_del_high)
                    self.gpioManager.low(self.gpios[0])
                    
# --------------------------------------------------------------------------------------
        
class GpioOutput (adapter.adapters.GPIOAdapter):
    
    mandatoryParameters = {}
    
    def __init__(self):
        adapter.adapters.GPIOAdapter.__init__(self)
        pass
        
    def low(self):
        if debug:
            logger.debug("%s %s", self.name, 'low')
        if self.active:
            if self.gpioManager == None:
                logger.error("gpioManager == None !!")
            self.gpioManager.low(self.gpios[0])
    
    def high(self):
        if debug:
            logger.debug("executing %s %s", self.name, 'high')
        if self.active:
            if self.gpioManager == None:
                logger.debug("gpioManager == None !!")
            self.gpioManager.high(self.gpios[0])
        
# --------------------------------------------------------------------------------------

class Gpio7segment (adapter.adapters.GPIOAdapter):
    
    mandatoryParameters = {}
    
    pattern = {
               #      a  b  c  d  e  f  g
               '0': [ 1, 1, 1, 1, 1, 1, 0 ],
               '1': [ 0, 1, 1, 0, 0, 0, 0 ],
               '2': [ 1, 1, 0, 1, 1, 0, 1 ],
               '3': [ 1, 1, 1, 1, 0, 0, 1 ],
               '4': [ 0, 1, 1, 0, 0, 1, 1 ],
               '5': [ 1, 0, 1, 1, 0, 1, 1 ],
               '6': [ 1, 0, 1, 1, 1, 1, 1 ],
               '7': [ 1, 1, 1, 0, 0, 0, 0 ],
               '8': [ 1, 1, 1, 1, 1, 1, 1 ],
               '9': [ 1, 1, 1, 1, 0, 1, 1 ],
               
               '-': [ 0, 0, 0, 0, 0, 0, 1 ],
               
               'A': [ 1, 1, 1, 0, 1, 1, 1 ],
               'b': [ 0, 0, 1, 1, 1, 1, 1 ],
               'C': [ 1, 0, 0, 1, 1, 1, 0 ],
               'c': [ 0, 0, 0, 1, 1, 0, 1 ],
               'd': [ 0, 1, 1, 1, 1, 0, 1 ],
               'E': [ 1, 0, 0, 1, 1, 1, 1 ],
               'F': [ 1, 0, 0, 0, 1, 1, 1 ],
               'r': [ 0, 0, 0, 0, 1, 0, 1 ],
 
               }
    
    def __init__(self):
        adapter.adapters.GPIOAdapter.__init__(self)
        pass
        
    def value(self, val):
        if debug:
            logger.debug("%s %s", self.name, 'val')
        
        if self.active:
            if self.gpioManager == None:
                logger.error("gpioManager == None !!")
            
            gpio_0 = self.getChannelByAlias('a')    
            gpio_1 = self.getChannelByAlias('b')    
            gpio_2 = self.getChannelByAlias('c')    
            gpio_3 = self.getChannelByAlias('d')    
            gpio_4 = self.getChannelByAlias('e')    
            gpio_5 = self.getChannelByAlias('f')    
            gpio_6 = self.getChannelByAlias('g')  
                
            gpios = [gpio_0, gpio_1, gpio_2, gpio_3, gpio_4, gpio_5, gpio_6 ]
            
            if not self.pattern.has_key(val):
                val = '-'
                
            segs = self.pattern[val]
            if debug:
                print(gpios)            
                print(segs)
                            
            for i in range(7):
                self.gpioManager.low (gpios[i])
                
            for i in range(7):
                if segs[i] == 1:
                    self.gpioManager.high (gpios[i])
                    
    def seg_a(self, val):
        if debug:
            logger.debug("%s %s", self.name, val)
        
        if self.gpioManager == None:
            logger.error("gpioManager == None !!")
            
        gpio_0 = self.getChannelByAlias('a')    
        if self.isTrue(val):
            self.gpioManager.high (gpio_0)
        else:
            self.gpioManager.low (gpio_0)

    def seg_b(self, val):
        if debug:
            logger.debug("%s %s", self.name, val)
        
        if self.gpioManager == None:
            logger.error("gpioManager == None !!")
            
        gpio_0 = self.getChannelByAlias('b')    
        if self.isTrue(val):
            self.gpioManager.high (gpio_0)
        else:
            self.gpioManager.low (gpio_0)

    def seg_c(self, val):
        if debug:
            logger.debug("%s %s", self.name, val)
        
        if self.gpioManager == None:
            logger.error("gpioManager == None !!")
            
        gpio_0 = self.getChannelByAlias('c')    
        if self.isTrue(val):
            self.gpioManager.high (gpio_0)
        else:
            self.gpioManager.low (gpio_0)

    def seg_d(self, val):
        if debug:
            logger.debug("%s %s", self.name, val)
        
        if self.gpioManager == None:
            logger.error("gpioManager == None !!")
            
        gpio_0 = self.getChannelByAlias('d')    
        if self.isTrue(val):
            self.gpioManager.high (gpio_0)
        else:
            self.gpioManager.low (gpio_0)

    def seg_e(self, val):
        if debug:
            logger.debug("%s %s", self.name, val)
        
        if self.gpioManager == None:
            logger.error("gpioManager == None !!")
            
        gpio_0 = self.getChannelByAlias('e')    
        if self.isTrue(val):
            self.gpioManager.high (gpio_0)
        else:
            self.gpioManager.low (gpio_0)

    def seg_f(self, val):
        if debug:
            logger.debug("%s %s", self.name, val)
        
        if self.gpioManager == None:
            logger.error("gpioManager == None !!")
            
        gpio_0 = self.getChannelByAlias('f')    
        if self.isTrue(val):
            self.gpioManager.high (gpio_0)
        else:
            self.gpioManager.low (gpio_0)

    def seg_g(self, val):
        if debug:
            logger.debug("%s %s", self.name, val)
        
        if self.gpioManager == None:
            logger.error("gpioManager == None !!")
            
        gpio_0 = self.getChannelByAlias('g')    
        if self.isTrue(val):
            self.gpioManager.high (gpio_0)
        else:
            self.gpioManager.low (gpio_0)

                
            
        
# --------------------------------------------------------------------------------------

class GpioOutputPWM (adapter.adapters.GPIOAdapter):
    """outputs a pwm signal to a pin.
    Input          'rate'     : float, 0.0 to 100.0
    Configuration  'frequency': float, [Hz] 
    
    uses pwm-feature of RPi.GPIO-Library."""
    
    mandatoryParameters = {'frequency': 50.0, 'rate': 50.0}
    
    def __init__(self):
        adapter.adapters.GPIOAdapter.__init__(self)
        pass
        
    def rate(self, value):
        if debug:
            print(self.name, 'rate', value)
        if self.active:
            v = 0.0
            try:
                v = float(value)
            except:
                logger.error('{name:s}: invalid value: {value:s}'.format(name=self.name, value=str( value)))
                return
            self.gpioManager.setPWMDutyCycle(self.gpios[0], v )
            
    def setActive(self, state):
        adapter.adapters.GPIOAdapter.setActive(self, state);

        if state == True:
            # initially send data
            self.gpioManager.startPWM(self.gpios[0], 
                                    frequency = float( self.parameters['frequency']), 
                                    value=float( self.parameters['rate']))
        else:
            self.gpioManager.resetPWM(self.gpios[0])
        # self.gpios[0].setActive(state)
# --------------------------------------------------------------------------------------

class GpioMotorPWM (adapter.adapters.GPIOAdapter):
    """controls a motor connected to  two half-H-Bridges.
    Input          'speed'     : float, -100..0.0 .. 100.0
    Configuration  'frequency': float, [Hz] 
    
    Needs two GPIO port pins.
    
    uses pwm-feature of RPi.GPIO-Library."""
    
    mandatoryParameters = {'frequency': 50.0, 'speed': 0.0}
    
    def __init__(self):
        adapter.adapters.GPIOAdapter.__init__(self)
        pass
        
    def speed(self, value):
        if debug:
            print(self.name, 'speed', value)
        if self.active:
            try:
                speed = float(value)
            except ValueError:
                return 
            gpio_0 = self.getChannelByAlias('a')    
            gpio_1 = self.getChannelByAlias('b')    
            
            if speed > 100.0:
                self.gpioManager.setPWMDutyCycle(gpio_0, 1 )
                self.gpioManager.setPWMDutyCycle(gpio_1, 0 )
            elif speed > 0.001:
                self.gpioManager.setPWMDutyCycle(gpio_0, speed )
                self.gpioManager.setPWMDutyCycle(gpio_1, 0 )
            elif speed > -0.001:
                self.gpioManager.setPWMDutyCycle(gpio_0, 0 )
                self.gpioManager.setPWMDutyCycle(gpio_1, 0 )
            elif speed >= -100.0:
                self.gpioManager.setPWMDutyCycle(gpio_0, 0 )
                self.gpioManager.setPWMDutyCycle(gpio_1, abs( speed ) )
            else:
                self.gpioManager.setPWMDutyCycle(gpio_0, 0 )
                self.gpioManager.setPWMDutyCycle(gpio_1, 1 )
            
    def setActive(self, state):
        adapter.adapters.GPIOAdapter.setActive(self, state);

        if state == True:
            # initially send data
            self.gpioManager.startPWM(self.gpios[0], 
                                    frequency = float( self.parameters['frequency']), 
                                    value=float( self.parameters['speed']))
            self.gpioManager.startPWM(self.gpios[1], 
                                    frequency = float( self.parameters['frequency']), 
                                    value=float( self.parameters['speed']))
        else:
            self.gpioManager.resetPWM(self.gpios[0])
            self.gpioManager.resetPWM(self.gpios[1])

# --------------------------------------------------------------------------------------

class GpioOutputPWM_ON_OFF (adapter.adapters.GPIOAdapter):
    """outputs a pwm signal to a pin.
    Input          low, high
    Configuration  'frequency': float, [Hz] 
    
    uses pwm-feature of RPi.GPIO-Library."""
    
    mandatoryParameters = {'frequency': 10.0, 'rate': 50.0}
    
    def __init__(self):
        adapter.adapters.GPIOAdapter.__init__(self)
        pass
        
            
    def setActive(self, state):
        adapter.adapters.GPIOAdapter.setActive(self, state);

        if state == True:
            # initially send data
            self.gpioManager.startPWM(self.gpios[0], 
                                    frequency = float( self.parameters['frequency']), 
                                    value=float( self.parameters['rate']))
        else:
            self.gpioManager.resetPWM(self.gpios[0])
        # self.gpios[0].setActive(state)

    def low(self):
        if debug:
            logger.debug("%s %s", self.name, 'low')
        if self.active:
            if self.gpioManager == None:
                logger.error("gpioManager == None !!")
            self.gpioManager.setPWMDutyCycle(self.gpios[0], float(0) )
            
    
    def high(self):
        if debug:
            logger.debug("executing %s %s", self.name, 'high')
        if self.active:
            if self.gpioManager == None:
                logger.debug("gpioManager == None !!")
            self.gpioManager.setPWMDutyCycle(self.gpios[0], float( self.parameters['rate']) )
        
# --------------------------------------------------------------------------------------

class GpioOutputPWMServo (adapter.adapters.GPIOAdapter):
    """outputs a pwm signal to a pin with attached servo.
    Output is inverse, as a transistor line driver with pullup is used.
    Input          'rate'     : float, 0.0 to 100.0
    Configuration  'frequency': float, [Hz] 
    
    uses pwm-feature of RPi.GPIO-Library."""
    
    mandatoryParameters = {'frequency': 50.0, 'rate': 50.0}
    
    def __init__(self):
        adapter.adapters.GPIOAdapter.__init__(self)
        pass
        
    def rate(self, value):
        if debug:
            logger.debug("%s %s %s", self.name, 'value', value)
        v = 0.0
        try:
            v = float(value)
        except:
            logger.error('{name:s}: invalid value: {value:s}'.format(name=self.name, value=str( value)))
            return

        if v < 0:
            v = 0.0
        if v > 100:
            v = 100.0
        # Inverse Ausgabe, und   0 --> 95%
        #                      100 --> 90%
        v = 95 -v/20.0
        
        if self.active:
            self.gpioManager.setPWMDutyCycle(self.gpios[0], float(v) )
            
    def setActive(self, state):
        logger.info("Adapter, setActive " + self.name + ' ' + str(state) )
        
        adapter.adapters.GPIOAdapter.setActive(self, state);

        if state == True:
            # initially send data
            self.gpioManager.startPWM(self.gpios[0], 
                                    frequency = float( self.parameters['frequency']), 
                                    value=float( self.parameters['rate']))
        else:
            self.gpioManager.resetPWM(self.gpios[0])
            # self.gpios[0].setActive(state)

        
# --------------------------------------------------------------------------------------

class GpioInput(adapter.adapters.GPIOAdapter):
    """if button pressed, send a '1'"""
    
    sensorName = None
    mandatoryParameters = { 
                           'poll.interval': '1', 
                           'value.inverse': 'false' }
    
    def __init__(self):
        adapter.adapters.GPIOAdapter.__init__(self)
        
    def setActive (self, state):
        if debug:
            print(self.name, "setActive", state)
        #
        # use default implementation to start up GPIO
        #
        self.value_inverse = self.isTrue( self.parameters['value.inverse'] )
        adapter.adapters.GPIOAdapter.setActive(self, state);
        
        if state == True:
            # initially send data
            if self.gpioManager.get(self.gpios[0]) == 0:
                self.button ( '0')
            else:
                self.button( '1')
               
    def run(self):
        _del = float(self.parameters['poll.interval'])
            
        last = self.gpioManager.get(self.gpios[0])
        
        while not self.stopped():
            #
            # delay 5 sec, but break time in small junks to allow stopping fast
            #
            self.delay(_del)
                           
            current = self.gpioManager.get(self.gpios[0])
            if current != last:
                if current == 0:
                    self.button ( '0')
                else:
                    self.button( '1')
                last = current

    def button(self, value):
        """output from adapter to scratch."""

        if self.value_inverse:
            if value == '1':
                value = '0'
            else:
                value = '1'
    
        self.sendValue(value)

# --------------------------------------------------------------------------------------

class GpioValueInput(adapter.adapters.GPIOAdapter):
    """send named values on low or high input values"""
    
    sensorName = None
    mandatoryParameters = { 
                           'poll.interval': '1', 
                           'value.inverse': 'true', 
                           'value.0': 'low' ,
                           'value.1': 'high' 
                          }
    
    def __init__(self):
        adapter.adapters.GPIOAdapter.__init__(self)
        
        
    def setActive (self, state):
        if debug:
            print(self.name, "setActive", state)
        #
        # use default implementation to start up GPIO
        #
        self.value_inverse = self.isTrue( self.parameters['value.inverse'] )
        
        adapter.adapters.GPIOAdapter.setActive(self, state);
        
               
    def run(self):
        _del = float(self.parameters['poll.interval'])
            
        last = self.gpioManager.get(self.gpios[0])
        if last == 0:
            self.value ( '0')
        else:
            self.value ( '1')
            
        while not self.stopped():
            #
            # delay 5 sec, but break time in small junks to allow stopping fast
            #
            self.delay(_del)
                           
            current = self.gpioManager.get(self.gpios[0])
            if current != last:
                if current == 0:
                    self.value ( '0')
                else:
                    self.value ( '1')
                last = current

    def value(self, _value):
        """output from adapter to scratch."""

        if self.value_inverse:
            if _value == '1':
                _value = '0'
            else:
                _value = '1'
                
        if _value == '0':
            _svalue = self.parameters['value.0']    
        if _value == '1':
            _svalue = self.parameters['value.1']    
            
        self.sendValue( _svalue )

# --------------------------------------------------------------------------------------


class GpioButtonInput(adapter.adapters.GPIOAdapter):
    """deprecated, use GpioEventInput"""
    def __init__(self):
        logger.warn("Adapter GpioButtonInput is deprecated, use GpioEventInput instead")
        adapter.adapters.GPIOAdapter.__init__(self)
       
                
    sensorName = None
    mandatoryParameters = { 
                           'poll.interval': '1', 
                           'value.inverse': 'false' }
    value = None
    
    def setActive (self, state):
        if debug:
            print(self.name, "setActive", state)
        #
        # use default implementation to start up GPIO
        #
        adapter.adapters.GPIOAdapter.setActive(self, state);

               
    def run(self):
        _del = float(self.parameters['poll.interval'])
        self.value = None
        reverse =  self.isTrue( self.parameters['value.inverse'] ) 
        while not self.stopped():
            #
            # delay 5 sec, but break time in small junks to allow stopping fast
            #
            self.delay(_del)
            
            current = self.gpioManager.get(self.gpios[0])
            if current != self.value:
                if reverse:
                    if current:
                        self.button_released()
                    else:
                        self.button_pressed()
                else:
                    if current:
                        self.button_pressed()
                    else:
                        self.button_released()
                
                    
                self.value = current

    def button_pressed(self):
        """output command from adapter to scratch."""
        self.send()
        
    def button_released(self):
        """output command from adapter to scratch."""
        self.send()
        
class GpioEventInput(adapter.adapters.GPIOAdapter):
    """if button pressed, send a broadcast once."""
    
    sensorName = None
    mandatoryParameters = { 
                           'poll.interval': '1', 
                           'value.inverse': 'false' }
    value = None
    
    def __init__(self):
        adapter.adapters.GPIOAdapter.__init__(self)
        
    def setActive (self, state):
        if debug:
            print(self.name, "setActive", state)
        #
        # use default implementation to start up GPIO
        #
        adapter.adapters.GPIOAdapter.setActive(self, state);

               
    def run(self):
        _del = float(self.parameters['poll.interval'])
        self.value = None
        reverse =  self.isTrue( self.parameters['value.inverse'] ) 
        while not self.stopped():
            #
            # delay 5 sec, but break time in small junks to allow stopping fast
            #
            self.delay(_del)
            
            current = self.gpioManager.get(self.gpios[0])
            if current != self.value:
                if reverse:
                    if current:
                        self.button_released()
                    else:
                        self.button_pressed()
                else:
                    if current:
                        self.button_pressed()
                    else:
                        self.button_released()
                
                    
                self.value = current

    def button_pressed(self):
        """output command from adapter to scratch."""
        self.send()
        
    def button_released(self):
        """output command from adapter to scratch."""
        self.send()
