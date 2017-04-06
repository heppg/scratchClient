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

import xml.etree.ElementTree as ET
from types import MethodType
import traceback
import threading
import logging
logger = logging.getLogger(__name__)
import configuration
import errorManager

import adapter

import paho.mqtt.client as mqtt

debug = False

# --------------------------------------------------------------------------------------


class MQTT_Adapter (adapter.adapters.Adapter):
    """Interface to MQTT """
    
    mandatoryParameters = {  
                           'mqtt.server' : 'localhost', 
                           'mqtt.port'   : '1883'
                          }
    optionalParameters = {  
                           'mqtt.username' : 'test', 
                           'mqtt.password' : 'test'
                          }
    publishConfig = None
    subscribeConfig = None
    
    #
    # Function prototypes for input setters
    # these are added dynamically to the class if needed.
    #
    def get_function_name (self):
        return traceback.extract_stack(None, 2)[0][2]

    #
    # the construction with an inner method allows to implement methods for a class with
    # a specific 'name'-variable
    #
    def _add_receiveValueMethod(self, cls, name):
        def _receiveValue(self, value ):
            if debug:
                print("_receiveValue:", name, value )
            """Prototype for a receive function (scratch --> adapter)"""
            
            methodname = name
            if debug:
                print("_receiveValue", methodname)
            for p in self.publishConfig:
                _topic = p[0]
                _variable = p[1]
                if debug:
                    print("topic", _topic, _variable, "methodname=", methodname)
                if methodname == "input_" + _variable:
                    self.client.publish( _topic, value, qos=2, retain=True)
                    break
          
        _receiveValue.__name__ = name
        setattr( cls, _receiveValue.__name__, _receiveValue)


            
    def _sendValue(self, value):
        """Prototype for a send function (adapter --> scratch)"""
        methodname = self.get_function_name()
        if debug:
            print("_sendValue", methodname)
        self.sendValue('"' + value + '"')


    def __init__(self):
        adapter.adapters.Adapter.__init__(self)
        self.publishConfig = []
        self.subscribeConfig = []

    def setXMLConfig(self, child):
        # print "setXMLConfig"
        #
        # read configuration from xml
        #
        adapter_input_values = []
        adapter_output_values = []

        loggingContext = "adapter '[{a:s}]'".format(a=self.name ) 
        
        foundConfig = False
        _topic = None
        _variable = None
        for _extension in child:
            if 'extension' == _extension.tag:
                for _mqtt in _extension:
                    if 'mqtt' == _mqtt.tag:
                        if foundConfig:
                            errorManager.append("{lc:s}: more than one 'mqtt'-config for adapter.".format( lc=loggingContext ))   
                        foundConfig = True
                        for ps in _mqtt:
                            if 'publish' == ps.tag:
                                if 'topic' in  ps.attrib:
                                    _topic = ps.attrib['topic']
                                else:
                                    errorManager.append("{lc:s}: no 'topic'-attribute in <publish>".format( lc=loggingContext ))    
                                
                                if 'variable' in  ps.attrib:
                                    _variable = ps.attrib['variable']
                                else:
                                    _variable = _topic
                                    errorManager.appendWarning("{lc:s}: no 'variable'-attribute in <publish>".format( lc=loggingContext )) 
                                
                                self.publishConfig.append( ( _topic, _variable))   
                                
                            if 'subscribe' == ps.tag:
                                if 'topic' in  ps.attrib:
                                    _topic = ps.attrib['topic']
                                else:
                                    errorManager.append("{lc:s}: no 'topic'-attribute in <publish>".format( lc=loggingContext ))    
                                
                                if 'variable' in  ps.attrib:
                                    _variable = ps.attrib['variable']
                                else:
                                    _variable = _topic
                                    errorManager.appendWarning("{lc:s}: no 'variable'-attribute in <publish>".format( lc=loggingContext )) 
                                
                                self.subscribeConfig.append( ( _topic, _variable) )   
            
        if not foundConfig:
            errorManager.append("{lc:s}: no 'mqtt'-config for adapter.".format( lc=loggingContext ))             
        #
        if debug:
            print ( self.publishConfig   )
            print ( self.subscribeConfig )
        #
        # process data
        # dynamically add methods to self and register variables here.
        #
        for p in self.subscribeConfig:
            _topic = p[0]
            _variable = p[1]
            methodName = "output_" + _variable 
            setattr(self, methodName, self._sendValue )
        
            value = configuration.OutputSetting(methodName)
            value.scratchNames.append( _variable)
            adapter_output_values.append(value)
              
        for p in self.publishConfig:
            _topic = p[0]
            _variable = p[1]
            methodName = "input_" + _variable 
            self._add_receiveValueMethod( self.__class__,  methodName )
            
            value = configuration.InputSetting(methodName)
            value.scratchNames.append( _variable)
            adapter_input_values.append(value)
       
        self.addInputValues(adapter_input_values)  
        self.addOutputValues(adapter_output_values)
        
    def setActive (self, state):
        if debug:
            print(self.name, "setActive", state)
        #
        # use default implementation to start up Threads
        #
        adapter.adapters.Adapter.setActive(self, state);
        #
        if state == False:
            self.client.disconnect() 

    def on_message(self, client, userdata, msg):
        if debug:
            print( "on_message: " + msg.topic + " "+str( msg.payload )) 
        for p in self.subscribeConfig:
            _topic = p[0]
            _variable = p[1]
            if debug:
                print("sendValueByName:", _variable, msg.payload)
            self.sendValueByName( "output_" + _variable,  msg.payload)
            
    def run(self):
        _port = int(self.parameters['mqtt.port'])
        _host = self.parameters['mqtt.server']
        
        _username = None
        if 'mqtt.username' in self.parameters:
            _username = self.parameters['mqtt.username']
        _password = None
        if 'mqtt.password' in self.parameters:
            _password = self.parameters['mqtt.password']
            
        self.client = mqtt.Client()
        
        if _username != None:
            self.client.username_pw_set(_username, _password)
            
        self.client.on_message = self.on_message

        START = 0
        CONNECTED = 1
        state = START
        
        while not self.stopped():
            if state == START:  
                try:
                    self.client.connect( _host, _port, 30)
                    state = CONNECTED
                except Exception as e:
                    logger.error("{name:s}: could not connect to server {server:s}:{port:d}".format(name=self.name, server=_host, port=_port))
                    self.delay(10)
                    
            elif state == CONNECTED:            
                subscribers = []
                for p in self.subscribeConfig:
                    _topic = p[0]
                    subscribers.append( ( _topic, 2 ) )
                if debug:
                    print("subscribe to ", subscribers)    
                self.client.subscribe( subscribers )
                
                self.client.loop_forever()
                
                
