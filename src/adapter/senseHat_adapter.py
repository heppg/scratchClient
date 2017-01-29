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
import threading
import time
import re 
from sense_hat import SenseHat

import logging
logger = logging.getLogger(__name__)

debug = False

# --------------------------------------------------------------------------------------

class SenseHat_Adapter (adapter.adapters.Adapter):
    """Interface to SenseHat-Adapter using SenseHat-Library """

    mandatoryParameters = { 'poll.interval': '0.1' }

    
    def __init__(self):
        adapter.adapters.Adapter.__init__(self)
        
        self.pixel_Color = self.getRGBFromString('default')  
        self.pixel_X = 0
        self.pixel_Y = 0

    def setActive (self, active):
        if active:
            self.sense = SenseHat() 
            adapter.adapters.Adapter.setActive(self, active)
        else:
            adapter.adapters.Adapter.setActive(self, active)
            self.sense = None
     
    def run(self):
        if debug:
            print("thread started")
        _del = float(self.parameters['poll.interval'])
        
        last_temperature = None
        last_pressure = None
        last_humidity = None
        
        last_orientation_pitch = None
        last_orientation_yaw = None
        last_orientation_roll = None
        #
        # the float values are all different in each send cycle
        # to allow some reduction in value sending, the values are 
        # converted to string and reduced to 2 digital points
        #
        formatString = "{:.1f}"
        
        while not self.stopped():
            #
            # delay 5 sec, but break time in small junks to allow stopping fast
            #
            self.delay(_del)   

            value = self.sense.get_temperature()
            sValue = formatString.format(value)
            if sValue != last_temperature:
                last_temperature = sValue
                self.temperature(sValue) 
            
            value = self.sense.get_pressure()
            sValue = formatString.format(value)
            if sValue != last_pressure:
                last_pressure = sValue
                self.pressure(sValue) 
            
            value = self.sense.get_humidity()
            sValue = formatString.format(value)
            if sValue != last_humidity:
                last_humidity = sValue
                self.humidity(sValue) 
                
            orientation = self.sense.get_orientation()
            
            value = orientation['pitch']
            sValue = formatString.format(value)
            if sValue != last_orientation_pitch:
                last_orientation_pitch = sValue
                self.orientation_pitch(sValue) 
            
            value = orientation['yaw']
            sValue = formatString.format(value)
            if sValue != last_orientation_yaw:
                last_orientation_yaw = sValue
                self.orientation_yaw(sValue) 
            
            value = orientation['roll']
            sValue = formatString.format(value)
            if sValue != last_orientation_roll:
                last_orientation_roll = sValue
                self.orientation_roll(sValue) 
                
    #
    # values from adapter to scratch
    #
    def temperature(self, value):
        """output from adapter to scratch"""
        self.sendValue(value)

    def pressure(self, value):
        """output from adapter to scratch"""
        self.sendValue(value)

    def humidity(self, value):
        """output from adapter to scratch"""
        self.sendValue(value)

    def orientation_pitch(self, value):
        """output from adapter to scratch"""
        self.sendValue(value)
        
    def orientation_roll(self, value):
        """output from adapter to scratch"""
        self.sendValue(value)
        
    def orientation_yaw(self, value):
        """output from adapter to scratch"""
        self.sendValue(value)
        
    #
    # values from scratch to adapter
    #
    pixel_Color = None
    pixel_X = 0
    pixel_Y = 0
    
    def pixelColor(self, color):
        self.pixel_Color = self.getRGBFromString(color)     
                          
    def pixelX(self, value):
        if debug:
            print("pixelX", value)
        try:
            self.pixel_X = int(value)
        except Exception:
            pass               
                
    def pixelY(self, value):
        if debug:
            print("pixelY", value)
        try:
            self.pixel_Y = int(value)  
            
        except Exception:
            pass                       
                              
    def color(self, color):
        if debug:
            print("color", color)
        self.pixel_Color = self.getRGBFromString(color) 
        if debug:
            print(self.pixel_Color)                      
                               
    def clear(self):
        if debug:
            print("clear")
        self.sense.clear()
                
    def setPixel_xy(self):
        if debug:
            print("setPixel_xy")
        try:    
            self.sense.set_pixel(
                             self.pixel_X, self.pixel_Y,
                             self.pixel_Color['red'],
                             self.pixel_Color['green'],
                             self.pixel_Color['blue']
                             )
        except Exception as e:
            # print(e)
            logger.error("{name:s}: {e:s}".format(name=self.name, e=str(e)))
            
    def clearPixel_xy(self):
        if debug:
            print("clearPixel_xy")
        try:    
            self.sense.set_pixel(
                             self.pixel_X, self.pixel_Y,
                             0,
                             0,
                             0
                             )
        except Exception as e:
            logger.error("{name:s}: {e:s}".format(name=self.name, e=str(e)))
        
