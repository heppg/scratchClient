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

import threading
import helper.abstractQueue
import pyowm

import adapter.adapters

debug = False
           
class Openweathermap_Adapter(adapter.adapters.Adapter):
    
    # -----------------------------------------
    # fields for adapter
        
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
        self.queue = helper.abstractQueue.AbstractQueue()
        self._location = None
        
    def setActive (self, active):
        
        # take over default parameter only when not yet set
        if self._location == None:
            self._location = self.parameters['location']
            
        adapter.adapters.Adapter.setActive(self, active)
        if active:
            self.thread2 = threading.Thread(target=self.run_queue)
            self.thread2.start()
        
    def run_queue(self):
        while not self.stopped():
            try:
                location = self.queue.get(block=True, timeout=0.1)
            except:
                continue
            self._call_api(location)
        

    def _call_api(self, location):
        key = self.parameters['openweather.api_key']
        owm = pyowm.OWM(key)

        api_version = owm.get_API_version()
        logger.info("api_version {v:s}".format(v= api_version) )
  
        self.owm_location(location)
        
        try:
            observation = owm.weather_at_place(location)
        except Exception as e:
            logger.error(e)
            results = (
                ( "owm_time"                  , self.owm_time                  , '' ),
                
                ( "owm_coord_lon"             , self.owm_coord_lon             , '' ),
                ( "owm_coord_lat"             , self.owm_coord_lat             , '' ),
                ( "owm_coord_country"         , self.owm_coord_country         , '' ),
                
                ( "owm_weather_clouds"        , self.owm_weather_clouds        , '' ),
                ( "owm_weather_rainfall"      , self.owm_weather_rainfall      , '' ),
                ( "owm_weather_snowfall"      , self.owm_weather_snowfall      , '' ),
                ( "owm_weather_temperature"   , self.owm_weather_temperature   , '' ),
                ( "owm_weather_humidity"      , self.owm_weather_humidity      , '' ),
                ( "owm_weather_wind_speed"    , self.owm_weather_wind_speed    , '' ),
                ( "owm_weather_wind_direction", self.owm_weather_wind_direction, '' ),
                )  
            for sett in results:
                sett[1] (  sett[2] )
            return
        
        time = observation.get_reception_time('iso')
        
        coord = observation.get_location()
        if debug: print( coord )
        lon=coord.get_lon()
        lat=coord.get_lat()
        country = coord.get_country()
        
        weather = observation.get_weather()
        temperature = weather.get_temperature('celsius')['temp']
        humidity = weather.get_humidity() 
        
        pressure = weather.get_pressure() ['press']
        
        rain = weather.get_rain()
        if '1h' in rain:
            rainfall = rain['1h']
        else:
            rainfall = 0
            
        snow = weather.get_snow()
        if '1h' in snow:
            snowfall = snow['1h']
        else:
            snowfall = 0
            
        wind = weather.get_wind()
        if 'speed' in wind:
            wind_speed = wind['speed']
        else:
            wind_speed = 0
            
        if 'deg' in wind:
            wind_direction = wind['deg']
        else:
            wind_direction = 0
        
        
        clouds = weather.get_clouds()
        
        results = (
            ( "owm_time"                  , self.owm_time                  , time           ),
            
            ( "owm_coord_lon"             , self.owm_coord_lon             , lon            ),
            ( "owm_coord_lat"             , self.owm_coord_lat             , lat            ),
            ( "owm_coord_country"         , self.owm_coord_country         , country        ),
            
            ( "owm_weather_clouds"        , self.owm_weather_clouds        , clouds         ),
            ( "owm_weather_rainfall"      , self.owm_weather_rainfall      , rainfall       ),
            ( "owm_weather_snowfall"      , self.owm_weather_snowfall      , snowfall       ),
            ( "owm_weather_temperature"   , self.owm_weather_temperature   , temperature    ),
            ( "owm_weather_humidity"      , self.owm_weather_humidity      , humidity       ),
            ( "owm_weather_wind_speed"    , self.owm_weather_wind_speed    , wind_speed     ),
            ( "owm_weather_wind_direction", self.owm_weather_wind_direction, wind_direction ),
            )  
        for sett in results:
            sett[1] (  sett[2] )
#                         
    def run(self):
        pollrate=600
        try:
            pollrate = int(self.parameters['openweather.pollrate'])
        except:
            pass
        
        while not(self.stopped()):
            self.queue.put( self._location )     
            self.delay(pollrate)

        
    def owm_location(self, value):
        """output from adapter to scratch"""
        self.sendValue('"' + value + '"')
        
    def owm_time(self, value):
        """output from adapter to scratch"""
        self.sendValue('"' + value + '"')
        
    def location(self, value):
        """input from scratch to adapter"""
        if value == '':
            return
        self._location = value
        self.queue.put(value)
        
    def owm_weather_clouds(self, value):
        """output from adapter to scratch"""
        self.sendValue('"' + str(value )+ '"')
        
    def owm_weather_rainfall(self, value):
        """output from adapter to scratch"""
        self.sendValue('"' + str(value )+ '"')
        
    def owm_weather_snowfall(self, value):
        """output from adapter to scratch"""
        self.sendValue('"' + str(value )+ '"')
        
    def owm_weather_temperature(self, value):
        """output from adapter to scratch"""
        self.sendValue('"' + str(value )+ '"')
        
    def owm_weather_humidity(self, value):
        """output from adapter to scratch"""
        self.sendValue('"' + str(value )+ '"')
        
    def owm_weather_pressure(self, value):
        """output from adapter to scratch"""
        self.sendValue('"' + str(value )+ '"')
        
    def owm_weather_wind_speed(self, value):
        """output from adapter to scratch"""
        self.sendValue('"' + str(value )+ '"')
        
    def owm_weather_wind_direction(self, value):
        """output from adapter to scratch"""
        self.sendValue('"' + str(value )+ '"')
        
    def owm_coord_lat(self, value):
        """output from adapter to scratch"""
        self.sendValue('"' + str(value )+ '"')
        
    def owm_coord_lon(self, value):
        """output from adapter to scratch"""
        self.sendValue('"' + str(value )+ '"')
        
    def owm_coord_country(self, value):
        """output from adapter to scratch"""
        self.sendValue('"' + str(value )+ '"')
