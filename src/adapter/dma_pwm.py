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

import RPIO2.PWM

import logging
logger = logging.getLogger(__name__)

debug = True

        
# --------------------------------------------------------------------------------------
# uses a RPI2 modified RPIO PWM module

class DMA_PWM (adapter.adapters.DMAAdapter):
    """outputs a pwm signal to a pin.
    Input          'rate'     : float, 0.0 to 100.0
    Configuration  'frequency': float, [Hz] 
    
    uses DMA controlled PWM."""
    
    mandatoryParameters = {'frequency': 50.0, 'rate': 50.0}
    
    def __init__(self):
        adapter.adapters.DMAAdapter.__init__(self)
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
            self.set_pwm (self.gpios[0], float(v))
            
    def setActive(self, state):
        logger.info("Adapter, setActive " + self.name + ' ' + str(state) )
        
        adapter.adapters.DMAAdapter.setActive(self, state)

        if state == True:
            # initially send data
            self.startPWM(self.gpios[0], 
                                    frequency = float( self.parameters['frequency']), 
                                    value=float( self.parameters['rate']))
        else:
            self.resetPWM(self.gpios[0])
            # self.gpios[0].setActive(state)

class DMA_PWM_ON_OFF (adapter.adapters.DMAAdapter):
    """outputs a pwm signal to a pin.
    Input          low, high
    Configuration  'frequency': float, [Hz] 
    
    uses pwm-feature of RPi.GPIO-Library."""
    
    mandatoryParameters = {'frequency': 10.0, 'rate': 50.0}
    
    def __init__(self):
        adapter.adapters.DMAAdapter.__init__(self)
        pass
        
            
    def setActive(self, state):
        adapter.adapters.DMAAdapter.setActive(self, state);

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
            #self.gpioManager.setPWMDutyCycle(self.gpios[0], float(0) )
            self.set_pwm (self.gpios[0], float(0))
    
    def high(self):
        if debug:
            logger.debug("executing %s %s", self.name, 'high')
        if self.active:
            if self.gpioManager == None:
                logger.debug("gpioManager == None !!")
            self.gpioManager.setPWMDutyCycle(self.gpios[0], float( self.parameters['rate']) )
            self.set_pwm (self.gpios[0], float( self.parameters['rate']))
# --------------------------------------------------------------------------------------

class DMA_PWMServo (adapter.adapters.DMAAdapter):
    """outputs a pwm signal to a pin with attached servo.
    Output is inverse, as a transistor line driver with pullup is used.
    Input          'rate'     : float, 0.0 to 100.0
    Configuration  'frequency': float, [Hz] 
    
    uses pwm-feature of RPi.GPIO-Library."""
    
    mandatoryParameters = {'frequency': 50.0, 'rate': 50.0}
    
    def __init__(self):
        adapter.adapters.DMAAdapter.__init__(self)
        pass
        
    def rate(self, value):
        """value is 0..100"""
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
        
        # the millisecond range (usual 1.0 to 2.0 ms can be limited or extended    
        vLow = self.getOptionalParameter('millisecond.min', 1.0 ) 
        vHigh = self.getOptionalParameter('millisecond.max', 2.0 )  
        
        try:
            vvLow = float(vLow)
        except:
            vvLow = 1

        try:
            vvHigh = float(vHigh)
        except:
            vvHigh = 2
        
        if vvLow < 0.5:
            vvLow = 0.5
        if vvHigh > 2.5:
            vvHigh = 2.5
        
        # pRate ist 0..100
        pRate = ( vvLow + (vvHigh - vvLow) * v / 100.0 ) / 20.0 * 100.0
        if debug:
            print(vLow, vHigh, vvLow, vvHigh, pRate)
                   
        if self.value_inverse:
            # Inverse Ausgabe, und   0 --> 95%
            #                      100 --> 90%
            v = 100 - pRate
        else:
            v = pRate
            
        if self.active:
            #self.gpioManager.setPWMDutyCycle(self.gpios[0], float(v) )
            self.set_pwm (self.gpios[0], float(v))
            
    def setActive(self, state):
        logger.info("Adapter, setActive " + self.name + ' ' + str(state) )
        self.value_inverse = self.isTrue( self.parameters['value.inverse'] )
       
        adapter.adapters.DMAAdapter.setActive(self, state)

        if state == True:
            # initially send data
            v = float(self.parameters['rate'])
            
            if self.value_inverse:
                # Inverse Ausgabe, und   0 --> 95%
                #                      100 --> 90%
                v = 95 -v/20.0
            else:
                v = 5 + v/20.0
                
            self.startPWM(self.gpios[0], 
                                    frequency = float( self.parameters['frequency']), 
                                    value=float( v ))
        else:
            self.resetPWM(self.gpios[0])
            # self.gpios[0].setActive(state)

        
# --------------------------------------------------------------------------------------
