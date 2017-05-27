# -*- coding: utf-8 -*-
    # --------------------------------------------------------------------------------------------
    # Copyright (C) 2017  Gerhard Hepp
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
import re
try:
    import evdev
except ImportError:
    exit("This adapter requires evdev library to be installed.")
import select
import logging
logger = logging.getLogger(__name__)
import configuration
import errorManager

import adapter

debug = False

# --------------------------------------------------------------------------------------

class MyInputDevice ( evdev.InputDevice):
    """allows to stop the loop """
    
    def __init__(self, parentAdapter, fn):
        evdev.InputDevice.__init__( self, fn)
        self.parentAdapter = parentAdapter
        
    def read_loop(self):
        '''
        Enter an (((almost))) endless 'select.select()' loop that yields input events.
        '''
        while not ( self.parentAdapter.stopped() ):
            r, w, x = select.select([self.fd], [], [], 0.1)
            try:
                for event in self.read():
                    yield event
            except IOError:
                pass
    
class LIRC_Adapter (adapter.adapters.Adapter):
    """Interface to LIRC """
    
    mandatoryParameters = {  
                           'lircd.conf' : '/etc/lirc/lircd.conf'
                          }
    optionalParameters = {  
                          }
    

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
            
    def _sendValue(self):
        """Prototype for a send function (adapter --> scratch)"""
        methodname = self.get_function_name()
        if debug:
            print("_send", methodname)
        self.send()


    def __init__(self):
        adapter.adapters.Adapter.__init__(self)

    def setXMLConfig(self, child):
        #
        # read configuration from xml
        # the xml config is only needed to allow configuration to access this method here.
        #
        lircd_conf_file = self.parameters['lircd.conf']
        
        codes = []
        state = 0
        try:
            fi = open( lircd_conf_file, 'r')

            for line in fi:
                line = line.rstrip()
                if debug: print(line)
                if state == 0:
                    if re.search( r"begin codes", line):
                        state = 1
                elif state == 1:
                    if re.search( r"end codes", line):
                        state = 2
                    else:
                        if "" == line.strip():
                            continue
                        if debug: print("state 1", line)
                        m = re.match( r"[ \t]*([A-Za-z][A-Za-z0-9_]*)(|[ \t]+.*)", line)
                        if m:  
                            code = m.group(1)
                            codes.append(code) 
            
            fi.close()
        
        except Exception as e:
            err = "{n:s}: error reading config file {e:s}".format(n=self.name, e=str(e) )
            logger.error( err )
            errorManager.append( err )
            return
        
        logger.debug("{n:s}: codes are {c:s}".format(n=self.name, c=str(codes) ))
        #
        # process data
        # dynamically add methods to self and register send methods here.
        #
        adapter_output = []
        
        for code in codes:
            methodName = "output_" + code + "_down" 
            setattr(self, methodName, self._sendValue )
            
            value = configuration.OutputSetting(methodName)
            value.scratchNames.append( methodName)
            adapter_output.append(value)

            
        for code in codes:
            methodName = "output_" + code + "_up" 
            setattr(self, methodName, self._sendValue )
            value = configuration.OutputSetting(methodName)
            value.scratchNames.append( methodName)
            adapter_output.append(value)
        
        self.addOutputs (adapter_output)
        
    def setActive (self, state):
        if debug:
            print(self.name, "setActive", state)
        #
        # use default implementation to start up Threads
        #
        adapter.adapters.Adapter.setActive(self, state);
        #

    def run(self):
        #from evdev import InputDevice, categorize, ecodes

        devices = [evdev.InputDevice(fn) for fn in evdev.list_devices()]
        fn_lircd = None
        for device in devices:
            logger.debug("{n:s}: device '{fn:s}' '{na:s}' '{ph:s}'".format(n=self.name, fn=device.fn, na=device.name, ph=device.phys ) )
            if "lircd" == device.name:
                fn_lircd = device.fn
        if fn_lircd == None:
            logger.error("{n:s}: no lirc-device found in input devices; reinstall lirc and restart scratchClient".format(n=self.name) )
            return
                 
        dev = MyInputDevice( self, fn_lircd )
        if debug: print(dev)
        
        for event in dev.read_loop():
            if event.type == evdev.ecodes.EV_KEY:
                kevent = evdev.events.KeyEvent( event)
                if kevent.keystate == evdev.events.KeyEvent.key_down:
                    self.sendCommandAlias( 'output_'+ kevent.keycode + '_down' )
                if kevent.keystate == evdev.events.KeyEvent.key_up:
                    self.sendCommandAlias( 'output_'+ kevent.keycode + '_up' )
        if debug: print ("loop terminated")        
