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

import logging
logger = logging.getLogger(__name__)

# 
# GPIO-Manager for test purposes. Does not need any external library 
#

debug = True

        
class GPIOManager:
    """GPIOManager"""
    
    pwms = None
    name = 'TESTManager'
        
    def __init__(self):
        pass

    #def getSimulation(self):
    #    return False
        
    def setActive(self, state):
        if debug:
            print(self.name, "setActive", state)
        if state:
            if debug:
                print(self.name, "activate system")
        else:
            pass
        
    def low(self, gpio):
        if debug:
            print( self.name, "low()", gpio )

    def high(self, gpio):
        if debug:
            print( self.name, "high()", gpio )

    def direction_in(self, gpio):
        print( self.name, "direction_in()", gpio )
    
    def direction_out(self, gpio):
        print( self.name, "direction_out()", gpio )

    def get(self, gpio):
        print(self.name, "get", gpio.getPort())
        return False    

    def startPWM(self, gpio, frequency=20.0, rate=50.0):
        pass
       
    def setPWMDutyCycle(self, gpio, value):
        pass
    
    def resetPWM(self, gpio):
        pass
            
    
    def setGPIOActive(self, gpioConfiguration, state):
        pass
        
    def setGpioState(self, gpioConfiguration, setting):        
        pass