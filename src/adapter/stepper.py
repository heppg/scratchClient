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

import logging
logger = logging.getLogger(__name__)

debug = False
    
class BipolarStepper(adapter.adapters.GPIOAdapter):
    
    mandatoryParameters = {}
    _speed = None
    _position = None
    _target = None
    
    def __init__(self):
        adapter.adapters.GPIOAdapter.__init__(self)
        self._speed = 0.005
        self._position = 0
        self._target = 0
        
    def run(self):
        index = 0
        pattern = [
                   [ 0, 1,   0, 1],
                   [ 1, 0,   0, 1],
                   [ 1, 0,   1, 0],
                   [ 0, 1,   1, 0],
                   ]    
        
        
        
        br0_0 = self.getChannelByAlias('br0.0')
        br0_1 = self.getChannelByAlias('br0.1')
        br1_0 = self.getChannelByAlias('br1.0')
        br1_1 = self.getChannelByAlias('br1.1')
        
        while not self.stopped():

            self.delay(self._speed)
                   
            di = 0
            if self._position < self._target:
                di = 1
            elif self._position > self._target:
                di = -1
                                   
            if pattern[index][0] == 1:
                self.gpioManager.high(br0_0)
            else:
                self.gpioManager.low(br0_0)
                
            if pattern[index][1] == 1:
                self.gpioManager.high(br0_1)
            else:
                self.gpioManager.low(br0_1)
                
            if pattern[index][2] == 1:
                self.gpioManager.high(br1_0)
            else:
                self.gpioManager.low(br1_0)
                
            if pattern[index][3] == 1:
                self.gpioManager.high(br1_1)
            else:
                self.gpioManager.low(br1_1)
                
            index += di
            index %= len(pattern)
            self._position += di
            
            if debug:
                print("position",self._position, "target", self._target)
            
    def startMotor(self):
        if debug:
            print("startMotor")
        
    def stopMotor(self):
        if debug:
            print("stopMotor")
        
    def speed(self, value):
        if debug:
            print("speed")
        self._speed = value

    def target(self, value):
        if debug:
            print("target", value)
        try:
            self._target = int(value)
        except Exception as e:
            print(e)
            pass
        
        
class UnipolarStepperStep(adapter.adapters.GPIOAdapter):
    
    mandatoryParameters = {}
    
    def __init__(self):
        adapter.adapters.GPIOAdapter.__init__(self)
        
    def binaryPattern(self, value):
        
        if not (value.startswith('b')):
            logger.error("{name:s}: binary pattern does not start with 'b'".format(name=self.name))
            return
        if not (len(value) == 5):
            logger.error("{name:s}: binary pattern is not 5 chars long".format(name=self.name))
            return
        br00 = self.getChannelByAlias('br0.0')
        br01 = self.getChannelByAlias('br0.1')
        br10 = self.getChannelByAlias('br1.0')
        br11 = self.getChannelByAlias('br1.1')
        
        try:
            if value[1]== '1':
                self.gpioManager.high(br00)
            if value[1]== '0':
                self.gpioManager.low(br00)
        except Exception as e:
            logger.error(e)
            pass      
        try:
            if value[2]== '1':
                self.gpioManager.high(br01)
            if value[2]== '0':
                self.gpioManager.low(br01)
        except Exception as e:
            logger.error(e)
            pass      
        try:
            if value[3]== '1':
                self.gpioManager.high(br10)
            if value[3]== '0':
                self.gpioManager.low(br10)
        except Exception as e:
            logger.error(e)
            pass      
        try:
            if value[4]== '1':
                self.gpioManager.high(br11)
            if value[4]== '0':
                self.gpioManager.low(br11)
        except Exception as e:
            logger.error(e)
            pass      
        
    def br0_0(self, value):
        if debug:
            print("br0_0", value)
        brx = self.getChannelByAlias('br0.0')
        
        try:
            if value== '1':
                self.gpioManager.high(brx)
            if value== '0':
                self.gpioManager.low(brx)
        except Exception as e:
            logger.error(e)
            pass        

    def br0_1(self, value):
        if debug:
            print("br0_1", value)
        brx = self.getChannelByAlias('br0.1')
        
        try:
            if value== '1':
                self.gpioManager.high(brx)
            if value== '0':
                self.gpioManager.low(brx)
        except Exception as e:
            logger.error(e)
            pass        

    def br1_0(self, value):
        if debug:
            print("br1_0", value)
        brx = self.getChannelByAlias('br1.0')
        
        try:
            if value== '1':
                self.gpioManager.high(brx)
            if value== '0':
                self.gpioManager.low(brx)
        except Exception as e:
            logger.error(e)
            pass        

    def br1_1(self, value):
        if debug:
            print("br1_1", value)
        brx = self.getChannelByAlias('br1.1')
        
        try:
            if value== '1':
                self.gpioManager.high(brx)
            if value== '0':
                self.gpioManager.low(brx)
        except Exception as e:
            logger.error(e)
            pass        

            
class UnipolarStepperModule(adapter.adapters.GPIOAdapter):
    
    mandatoryParameters = {}
    _speed = None
    _position = None
    _target = None
    pattern=None
    
    pattern4 = [
               [ 1, 1,   0, 0],
               [ 0, 1,   1, 0],
               [ 0, 0,   1, 1],
               [ 1, 0,   0, 1],
               ]    
    
    pattern8 = [
               [ 1, 0,   0, 0],
               [ 1, 1,   0, 0],
               [ 0, 1,   0, 0],
               [ 0, 1,   1, 0],
               [ 0, 0,   1, 0],
               [ 0, 0,   1, 1],
               [ 0, 0,   0, 1],
               [ 1, 0,   0, 1],
               ]    

    
    def __init__(self):
        adapter.adapters.GPIOAdapter.__init__(self)
        self._speed = 0.1
        self._position = 0
        self._target = 0
        self.pattern = self.pattern8

    def run(self):
        index = 0
        
        br0_0 = self.getChannelByAlias('br0.0')
        br0_1 = self.getChannelByAlias('br0.1')
        br1_0 = self.getChannelByAlias('br1.0')
        br1_1 = self.getChannelByAlias('br1.1')
        
        #
        # implement a state machine to handle wait times
        #
        STATE_START = 0
        STATE_WAIT = 10
        STATE_RUNNING = 20
        
        state = STATE_START
        
        while not self.stopped():
            
            di = 0
            if self._position < self._target:
                di = 1
            elif self._position > self._target:
                di = -1
            
            if state == STATE_START:
                self.gpioManager.low(br0_0)
                self.gpioManager.low(br0_1)
                self.gpioManager.low(br1_0)
                self.gpioManager.low(br1_1)
            
                if di != 0 :
                    state = STATE_RUNNING
                else:
                    state = STATE_WAIT
                                    
            elif state == STATE_WAIT:        
                if di != 0 :
                    state = STATE_RUNNING

            elif state == STATE_RUNNING:        
                if di == 0:
                    self.complete()
                    #
                    # if position is reached, then disable outputs 
                    ## to stop heating of motor
                    # add some extra time to ensure target is reached
                    #
                    if self._speed < 0.1:
                        self.delay(0.1 - self._speed)
                    state = STATE_START    
                
                if di != 0:                           
                    _pattern = self.pattern 
                    index %= len(_pattern)
                  
                    if _pattern[index][0] == 1:
                        self.gpioManager.high(br0_0)
                    else:
                        self.gpioManager.low(br0_0)
                        
                    if _pattern[index][1] == 1:
                        self.gpioManager.high(br0_1)
                    else:
                        self.gpioManager.low(br0_1)
                        
                    if _pattern[index][2] == 1:
                        self.gpioManager.high(br1_0)
                    else:
                        self.gpioManager.low(br1_0)
                        
                    if _pattern[index][3] == 1:
                        self.gpioManager.high(br1_1)
                    else:
                        self.gpioManager.low(br1_1)
                        
                    index += di
                    self._position += di
                
            if logger.isEnabledFor(logging.DEBUG):
                logger.debug("{name:s}: state={s:d} position={p:d} target={t:d}".format(name=self.name, s=state, p=self._position, t=self._target) )

            self.delay(self._speed)
            

    def reset_8(self):
        if logger.isEnabledFor(logging.DEBUG):
            logger.debug("reset_8")
        self._position = 0
        self._target = 0
        
        self.pattern = self.pattern8

    def reset_4(self):
        if logger.isEnabledFor(logging.DEBUG):
            logger.debug("reset_4")
        self._position = 0
        self._target = 0
        
        self.pattern = self.pattern4
               
                
    def speed(self, value):
        if logger.isEnabledFor(logging.DEBUG):
            logger.debug("{name:s}: speed={value:s}".format(name= self.name, value=value) )
        try:
            x = float(value)
            if x < 0.001:
                x = 0.001
            self._speed = x 
        except Exception as e:
            logger.error("exception setting 'speed': {e:s}".format(e=e) )
            pass        
        
    def target(self, value):
        if logger.isEnabledFor(logging.DEBUG):
            logger.debug("{name:s}: target={value:s}".format(name= self.name, value=value) )
        try:
            self._target = long(value)
        except Exception as e:
            try:
                self._target = long(float(value))
            except Exception as e:
                logger.error("exception setting 'target': {e:s}".format(e=e) )
                pass 
    
    def complete(self):
        """position is reached"""
        self.send()                   