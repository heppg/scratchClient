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

import datetime

logger = logging.getLogger(__name__)
debug = False

# --------------------------------------------------------------------------------------
#
# Sending start events or other broadcasts to scratch.
#
# ScratchStartclickedAdapter simulates the 'start-clicked' event
# --------------------------------------------------------------------------------------
class ScratchStartclickedAdapter (adapter.adapters.Adapter):
    """send broadcast-event 'scratch-startclicked' to scratch after startup of connection"""
    
    mandatoryParameters = { }
    
    def __init__(self):
        adapter.adapters.Adapter.__init__(self)
        pass
    
    def setActive (self, active):
        adapter.adapters.Adapter.setActive(self, active)
        pass
    
    def run(self):
        time.sleep(0.1)
        self.command()
        
    def command(self):
        self.send()
           
class TimeAdapter( adapter.adapters.Adapter):
    
    mandatoryParameters = { 'poll.interval': 1 }
    
    def __init__(self):
        if debug:
            print("TimeAdapter init")
        adapter.adapters.Adapter.__init__(self)
        
    def setActive (self, state):
        if debug:
            print(self.name, "setActive", state)
        #
        # use default implementation to start up GPIO
        #
        adapter.adapters.Adapter.setActive(self, state);
               
    def run(self):
        if debug:
            print("run in test Adapter")
        _del = float(self.parameters['poll.interval'])
        
        _second = None
        _minute = None
        _hour = None
        _day = None
        _month = None
        _year = None
        
        i = 0
        while not self.stopped():
            #
            # delay 5 sec, but break time in small junks to allow stopping fast
            #
            self.delay(_del)
            a = datetime.datetime.now()
            if debug:
                print('second', a.second)
                print('minute',a.minute)
                print('hour',a.hour)
                print('day',a.day)
                print('month',a.month)
                print('year',a.year)
                
            if _second != a.second:
                _second = a.second
                self.second(_second)
           
            if _minute != a.minute:
                _minute = a.minute
                self.minute(_minute)
            
            if _hour != a.hour:
                _hour = a.hour
                self.hour(_hour)
            
            if _day != a.day:
                _day = a.day
                self.day(_day)
            
            if _month != a.month:
                _month = a.month
                self.month(_month)
            
            if _year != a.year:
                _year = a.year
                self.year(_year)
            
            i += 1            

    def second(self, value):
        self.sendValue(str(value))
    def minute(self, value):
        self.sendValue(str(value))
    def hour(self, value):
        self.sendValue(str(value))
    def day(self, value):
        self.sendValue(str(value))
    def month(self, value):
        self.sendValue(str(value))
    def year(self, value):
        self.sendValue(str(value))