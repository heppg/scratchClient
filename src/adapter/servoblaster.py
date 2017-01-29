# -*- coding: utf-8 -*-
    # --------------------------------------------------------------------------------------------
    # Copyright (C) 2015  Gerhard Hepp
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

import os
import stat

import logging
logger = logging.getLogger(__name__)

debug = False
    
class ServoBlaster(adapter.adapters.Adapter):
    """Build a connection to pipe /dev/servoblaster; implements reconnect logic"""
    
    mandatoryParameters = {}
    pipe = '/dev/servoblaster'
    
    
    def __init__(self):
        adapter.adapters.Adapter.__init__(self)
    
    # values in uSecond
    currentPos = [0,0,0,0,0,0,0,0]    
    previousPos = [0,0,0,0,0,0,0,0]
    
    # values in milliSeconds
    milliseconds_min = [0,0,0,0,0,0,0,0]
    milliseconds_max = [0,0,0,0,0,0,0,0]
        
    def run(self):
        
        for i in range(0,8):
            vLow = self.getOptionalParameter( "millisecond.{i:d}.min".format(i=i), 1.0 )
            try:
                vvLow = float(vLow)
            except:
                vvLow = 1
            if vvLow < 0.5:
                vvLow = 0.5
            if vvLow > 1.9:
                vvLow = 1.9
                
            vHigh = self.getOptionalParameter( "millisecond.{i:d}.max".format(i=i), 2.0 )  
        
            try:
                vvHigh = float(vHigh)
            except:
                vvHigh = 2
        
            if vvHigh > 2.5:
                vvHigh = 2.5
            if vvHigh < 1.1:
                vvHigh = 1.1
 
            self.milliseconds_min[i] = vvLow
            self.milliseconds_max[i] = vvHigh
            
            
        START = 10
        RESTART = 20
        CONNECT_ERROR = 30
        CONNECTED = 40
        DISCONNECT = 50
        STOP = 60
        #
        # for debugging the names are available
        #
        names = {START: "START",
                 RESTART: "RESTART",
                 CONNECT_ERROR: "CONNECT_ERROR",
                 CONNECTED: "CONNECTED",
                 DISCONNECT: "DISCONNECT",
                 STOP: "STOP"}
        
        _del_update = 0.020
        _del_connect = 0.5
        
        state = START
        newState = None
        
        while True:
 
            if state == START:
                if self.stopped():
                    newState = STOP
                else: 
                    f = self.connectPipe(self.pipe)
                    if f == None:
                        logger.warn("Can not open " + self.pipe )
                        newState = CONNECT_ERROR
                    else:
                        newState = CONNECTED
                    
                        
            elif state == RESTART:
                if self.stopped():
                    newState = STOP
                else: 
                    f = self.connectPipe(self.pipe)
                    if f == None:
                        newState = CONNECT_ERROR
                    else:
                        logger.warn("Connection available {n:s}".format(n=self.pipe))
                        newState = CONNECTED
                        # we possibly have already values received.
                        # force them to be output again.
                        self.resetValues()
                        #
                        
            elif state == CONNECT_ERROR:
                self.delay( _del_connect )
                if debug:
                    print("CONNECT_ERROR")
                if self.stopped():
                    newState = STOP
                else:
                    newState = RESTART
            
            elif state == CONNECTED:
                # do delay first.if a stop is available, this 'falls through'
                self.delay( _del_update)
                
                if self.stopped():
                    newState = DISCONNECT
                else:       
                    
                    for index in range(0,8):
                        if self.stopped():
                            newState = DISCONNECT
                            break
                    
                        if self.previousPos[index]  != self.currentPos[index] :
        
                            s = "{i:d}={val:d}us".format(i=index, val=self.currentPos[index])
                            if debug:
                                print(s )
                            try:    
                                f.write(s)
                                f.write(os.linesep)
                                f.flush()                    
                                
                                #
                                # avoid setting values too often
                                #
                                self.previousPos[index] = self.currentPos[index]
                            except IOError as e:
                                logger.warn(e)
                                
                                try:
                                    f.close()
                                except:
                                    pass
                                
                                newState = RESTART
                                break
                            
            elif state == DISCONNECT:
                f.close()
                newState = STOP
                
            elif state == STOP:
                return        
            
            if state != newState:
                if logger.isEnabledFor(logging.DEBUG):
                    if state == None:
                        fs = 'None'
                    else:
                        fs = names[state]
                    
                    if newState == None:
                        fn = 'None'
                    else:
                        fn = names[newState]
                        
                    logger.debug ("state change from {s:s} -> {n:s}".format(s=fs, n=fn))
                    
                state = newState
      
    def resetValues(self):
        for i in range(0,8):
            self.previousPos[i] = 0
                      
    def connectPipe(self, name):
        try:
            if stat.S_ISFIFO(os.stat(name).st_mode):
                f = open(name, 'w')
                return f
            else:
                logger.debug("file {n:s} is no pipe".format(n=name))
                return None
        except OSError as e:
            logger.debug(e)
            return None
        except IOError as e:
            logger.debug(e)
            return None
        
            
    def servo_0(self, value):
        """expected values are 0..100"""
        if debug:
            print("servo_0", value )
        self.setCurrentPos(0, value)
    
    def setCurrentPos(self, index, value):
        try:
            p = float( value)
        except:
            # silently ignore wrong values
            return
        # calculate microsecond values
        p = self.milliseconds_min[index] + value * ( self.milliseconds_max[index]- self.milliseconds_min[index]) / 100.0
        t = int (p * 1000 )
        self.currentPos[index] = t
           
    def servo_1(self, value):
        """expected values are 0..100"""
        if debug:
            print("servo_1", value )
        self.setCurrentPos(1, value)     
               
    def servo_2(self, value):
        """expected values are 0..100"""
        if debug:
            print("servo_2", value )
        self.setCurrentPos(2, value)        
    
    def servo_3(self, value):
        """expected values are 0..100"""
        if debug:
            print("servo_0", value )
        self.setCurrentPos(3, value)        
    
    def servo_4(self, value):
        """expected values are 0..100"""
        if debug:
            print("servo_4", value )
        self.setCurrentPos(4, value)        
    
    def servo_5(self, value):
        """expected values are 0..100"""
        if debug:
            print("servo_5", value )
        self.setCurrentPos(5, value)        
    
    def servo_6(self, value):
        """expected values are 0..100"""
        if debug:
            print("servo_6", value )
        self.setCurrentPos(6, value)        

    def servo_7(self, value):
        """expected values are 0..100"""
        if debug:
            print("servo_7", value )
        self.setCurrentPos(7, value)        
        
