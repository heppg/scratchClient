# -*- coding: utf-8 -*-
    # --------------------------------------------------------------------------------------------
    # Copyright (C) 2016  Gerhard Hepp
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

import pigpio
import time

import logging
logger = logging.getLogger(__name__)

debug = True

class HC_SR04_Error(Exception):
    NO_CONNECTION = 'no connection'
    ECHO_PIN_HIGH = 'echo pin is high'
    NO_RESPONSE = 'no response'
    def __init__(self, value):
        self.value = value
    def __str__(self):
        return repr(self.value)

class HC_SR04():
    def __init__(self, trigger, echo):
        self.pi = pigpio.pi()
        if self.pi == None:
            raise HC_SR04_Error(HC_SR04_Error.NO_CONNECTION)
        
        self.trigger = trigger
        self.echo = echo
        
        self.pi.set_mode(echo, pigpio.INPUT)
        self.pi.set_pull_up_down(echo, pigpio.PUD_DOWN)
        self.pi.set_mode(trigger, pigpio.OUTPUT)
        
        self.pi.write ( trigger, 0)
        
        self._cb = self.pi.callback(echo, pigpio.EITHER_EDGE, self._cbf)
 
    def stop(self):
        self.pi.set_mode(self.echo, pigpio.INPUT)
        self.pi.set_pull_up_down(self.echo, pigpio.PUD_DOWN)

        self.pi.set_mode(self.trigger, pigpio.INPUT)
        self.pi.set_pull_up_down(self.trigger, pigpio.PUD_DOWN)

    def measure(self):
        
        self.state = 0
        lastState = None
        
        echo = self.pi.read(  self.echo)
        # print("echo", echo)
        if echo == 1:
            raise HC_SR04_Error(HC_SR04_Error.ECHO_PIN_HIGH)
          
        self.pi.write ( self.trigger, 1)
        
        cnt = 0
        while self.state != 2:
            if self.state != lastState:
                # print("state", self.state)
                lastState = self.state
            cnt += 1
            time.sleep(0.01)
            if cnt == 5:
                self.pi.write ( self.trigger, 0)
                raise HC_SR04_Error(HC_SR04_Error.NO_RESPONSE) 
            
        self.pi.write ( self.trigger, 0)
        
        return self.t
             
    def _cbf(self, gpio, level, tick):
        if level == 1:
            if debug:
                print("callback level 1", tick)
            self.state = 1;
            self._high_tick = tick
    
        elif level == 0:
            if debug:
                print("callback level 0", tick)
    
            if self._high_tick is not None:
                t = pigpio.tickDiff(self._high_tick, tick)
    
                if debug:
                    print ("calcualted time", t)
                self.t = t   
            self.state = 2
    
    
class HC_SR04_Adapter(adapter.adapters.GPIOAdapter):
    """Build a connection to pigpiod"""
    
    mandatoryParameters = {'poll.interval'}
    
    
    def __init__(self):
        adapter.adapters.GPIOAdapter.__init__(self)
    
    def setActive (self, state):        
        adapter.adapters.GPIOAdapter.setActive(self, state)
            
    def run(self):
        _del = float(self.parameters['poll.interval'])
        if _del < 0.02:
            _del = 0.02
        
        gpio_trigger = self.getChannelByAlias('trigger')    
        gpio_echo = self.getChannelByAlias('echo')    
        
        try:
            hc_sr04 = HC_SR04(gpio_trigger.portNumber, gpio_echo.portNumber)    
        except HC_SR04_Error as e:
            logger.error("{name:s}: Error in connecting to pigpiod {msg:s}".format(name=self.name, msg=e.value))
            self.error(e.value)
            return
        
        last_time = None
        last_error = None
        while not self.stopped():
            #
            self.delay(_del)
            # 
            try:               
                time = hc_sr04.measure()
                error = ''
            except HC_SR04_Error as e:
                logger.error("{name:s}: Error in response from HC-SR04 {msg:s}".format(name=self.name, msg=e.value))
                error = e.value
                time = 0
            
            if time > 20000:
                error ="time too long > 20ms"
                time = 0
                
            if last_error != error:
                self.error(error)
                last_error = error
            
            if last_time != time:
                self.time( time )
                last_time = time 

        hc_sr04.stop()
              
    def time(self, value):
        """receives measured time in microseconds, sends seconds towards scratch"""
        if debug:
            print("time", value)
        self.sendValue( float(value)/1000000.0 )    
 
    def error(self, value):
        """error"""
        if debug:
            print("error", value)
        self.sendValue( '"'+ value + '"' )    
 
        
