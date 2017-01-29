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
#
# Module to receive openweathermap messages
# needs installation of pyowm
#
import logging
logger = logging.getLogger(__name__)

import time
import sys
import os
import threading

import pyowm

from scratchClient import ModulePathHandler
import adapter.adapters


debug = False

           
class Openweathermap_Adapter(adapter.adapters.Adapter):
    
    # -----------------------------------------
    # fields for adapter
    queueThread = None
    
    # -----------------------------------------
   
    mandatoryParameters = { 
                'openweather.api_key' : '',
                'pollrate' : '600', 
                'location' : 'Leinfelden-Echterdingen' 
    }
    # -----------------------------------------
    
    def __init__(self):
        # General Adapter
        adapter.adapters.Adapter.__init__(self)

    def setActive (self, active):
        adapter.adapters.Adapter.setActive(self, active)
                 
    def run(self):
        pollrate=600
        try:
            pollrate = int(self.parameters['openweather.pollrate'])
        except:
            pass
        
        key = self.parameters['openweather.api_key']
        location = self.parameters['location']
        owm = pyowm.OWM(key)

        api_version = owm.get_API_version()
        logger.info("api_version {v:s}".format(v= api_version) )
  
        self.location(location)
        
        while not(self.stopped()):
            
            try:
                observation = owm.weather_at_place(location)
            except Exception as e:
                logger.error(e)
                self.delay(pollrate)
                continue
            
            w = observation.get_weather()
            temperature = w.get_temperature('celsius')['temp']
            humidity = w.get_humidity() 
            
            pressure = w.get_pressure() ['press']
            
            rain = w.get_rain()
            if '1h' in rain:
                rainfall = rain['1h']
            else:
                rainfall = 0
                
            snow = w.get_snow()
            if '1h' in snow:
                snowfall = snow['1h']
            else:
                snowfall = 0
                
            wind = w.get_wind()
            if 'speed' in wind:
                wind_speed = wind['speed']
            else:
                wind_speed = 0
                
            if 'deg' in wind:
                wind_direction = wind['deg']
            else:
                wind_direction = 0
                
            clouds = w.get_clouds()
                           
            self.clouds(clouds)
            self.rainfall(rainfall)
            self.snowfall(snowfall)
            self.temperature(temperature)
            self.humidity(humidity)
            self.pressure(pressure)
            self.wind_speed(wind_speed)
            self.wind_direction(wind_direction)
             
            self.delay(pollrate)

        
    def location(self, value):
        """output from adapter to scratch"""
        self.sendValue('"' + value + '"')
        
    def clouds(self, value):
        """output from adapter to scratch"""
        self.sendValue('"' + str(value )+ '"')
        
    def rainfall(self, value):
        """output from adapter to scratch"""
        self.sendValue('"' + str(value )+ '"')
        
    def snowfall(self, value):
        """output from adapter to scratch"""
        self.sendValue('"' + str(value )+ '"')
        
    def temperature(self, value):
        """output from adapter to scratch"""
        self.sendValue('"' + str(value )+ '"')
        
    def humidity(self, value):
        """output from adapter to scratch"""
        self.sendValue('"' + str(value )+ '"')
        
    def pressure(self, value):
        """output from adapter to scratch"""
        self.sendValue('"' + str(value )+ '"')
        
    def wind_speed(self, value):
        """output from adapter to scratch"""
        self.sendValue('"' + str(value )+ '"')
        
    def wind_direction(self, value):
        """output from adapter to scratch"""
        self.sendValue('"' + str(value )+ '"')
