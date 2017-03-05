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
import errorManager
import configuration 

from types import MethodType
import socket
import time
import inspect
from  array import *
import logging
import re

logger = logging.getLogger(__name__)
debug = False

BUFFER_SIZE = 1024

# --------------------------------------------------------------------------------------


class CommunicationAdapter (adapter.adapters.Adapter):
    """needs xml configuration for the input-Events, outputEvents"""
    
    mandatoryParameters = {'server': '192.168.2.102', 'group': 'groupA' }
    
    socket = None
    
    STATE_START = 10
    STATE_START_N = 15
    STATE_CONNECTED = 20
    STATE_OPERATIONAL = 30
    STATE_DISCONNECT = 40
    STATE_END = 90
    
    def __init__(self):
        
        adapter.adapters.Adapter.__init__(self)
        
        pass
    
    def setActive (self, active):
        adapter.adapters.Adapter.setActive(self, active)
        pass
    
    def _input(self, key):
        if debug:
            print("input_" + key)
        event = self.reg[key]
        self.sock_send( 'broadcast "' + event + '"')
        
    def input_000(self):
        self._input('000')
    def input_001(self):
        self._input('001')
    def input_002(self):
        self._input('002')
    def input_003(self):
        self._input('003')
    def input_004(self):
        self._input('004')
    def input_005(self):
        self._input('005')
    def input_006(self):
        self._input('006')
    def input_007(self):
        self._input('007')
    def input_008(self):
        self._input('008')
    def input_009(self):
        self._input('009')
    def input_00A(self):
        self._input('00A')
    def input_00B(self):
        self._input('00B')
    def input_00C(self):
        self._input('00C')
    def input_00D(self):
        self._input('00D')
    def input_00E(self):
        self._input('00E')
    def input_00F(self):
        self._input('00F')

    def input_010(self):
        self._input('010')
    def input_011(self):
        self._input('011')
    def input_012(self):
        self._input('012')
    def input_013(self):
        self._input('013')
    def input_014(self):
        self._input('014')
    def input_015(self):
        self._input('015')
    def input_016(self):
        self._input('016')
    def input_017(self):
        self._input('017')
    def input_018(self):
        self._input('018')
    def input_019(self):
        self._input('019')
    def input_01A(self):
        self._input('01A')
    def input_01B(self):
        self._input('01B')
    def input_01C(self):
        self._input('01C')
    def input_01D(self):
        self._input('01D')
    def input_01E(self):
        self._input('01E')
    def input_01F(self):
        self._input('01F')
   
    def _output(self):
        callerName = inspect.stack()  
        """template for a adapter-->scratch send function"""
        if debug:
            print("_output")
            print(callerName)
        self.send()

    def setXMLConfig(self, child):
        # print "setXMLConfig"
        #
        # read configuration from xml
        #
        input_broadcasts = []
        output_broadcasts = []
        
        
        loggingContext = "adapter '[{a:s}]'".format(a=self.name ) 

        # look for extension tag (new from 2017)
        for tle in child:
            if 'extension' == tle.tag:
                child= tle
                break
        
        for tle in child:
            if 'remote' == tle.tag:
                if 'type' in  tle.attrib:
                    _type = tle.attrib['type']
                    if _type == 'forward':
                        
                        for tle2 in tle:
                            if 'broadcast' == tle2.tag:
                                _broadcast_name = tle2.attrib['name']
                                input_broadcasts.append( _broadcast_name)
                            else:
                                errorManager.append("{lc:s}: no name in remout:input.broadcast".format( lc=loggingContext ))
                    elif _type == 'receive':
                        for tle2 in tle:
                            if 'broadcast' == tle2.tag:
                                _broadcast_name = tle2.attrib['name']
                                output_broadcasts.append( _broadcast_name)
                            else:
                                errorManager.append("{lc:s}: no name in remout:output.broadcast".format( lc=loggingContext ))
                    else:
                        errorManager.append("{lc:s}: wrong type in remote (forward,receive): {type:s}".format( lc=loggingContext,type=_type ))            
                else:
                    errorManager.append("{lc:s}: no type in remote".format( lc=loggingContext ))
            
            
        #
        # bind function to class.
        # see http://www.ianlewis.org/en/dynamically-adding-method-classes-or-class-instanc 
        #
        # Dynamic bindings do not work on input side (unfortunately)
        if False:
            for _name in input_broadcasts:
                # set the input mechanism for the adapter
                if debug:
                    print ( "define input function", "input_" + _name , " func ", self._input )
                method = MethodType(self._input, self, type(self))
                setattr(self, "input_" + _name, method )
                     
        for _name in output_broadcasts:
            # set the output mechanism for the adapter
         
            if debug:
                print ( "define output function", "output_" + _name , " func ", self._output )
            method = MethodType(self._output, self, type(self))
            setattr(self, "output_" + _name, method )         

        if debug:
            for c in inspect.getmembers(self, inspect.ismethod ):
                print("debug methods", c)
        #
        # register the functionName and scratchName on the framework
        #
        inputs = []
        outputs = []
        # registry from input keys to event names
        self.reg = {}
        #
        ni = 0
        for _name in input_broadcasts:
            # set the input mechanism for the adapter
            _is = configuration.InputSetting ("input_{n:03d}".format(n=ni))
            _is.scratchNames.append(_name)
            inputs.append(_is)
            
            self.reg["{n:03d}".format(n=ni)] = _name
            ni += 1
           
        for _name in output_broadcasts:
            # set the output mechanism for the adapter
            _os = configuration.OutputSetting ("output_"+ _name)
            _os.scratchNames.append(_name)
            outputs.append(_os)
            
        self.addInputs(inputs)
        self.addOutputs(outputs)
        
        
    def run(self):
        self.state = self.STATE_START
        
        while True:
            if self.stopped():
                try:
                    self.sock.close()
                except Exception as e:
                    logger.warning(e)
                self.state = self.STATE_END
                break
            
            if self.state == self.STATE_START:
                # clean any available data
                data = ""
                #
                self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                ip = self.parameters[ 'server' ] 
                try:
                    self.sock.connect((ip, 42002))
                    logger.info("{name:s}: connected to server".format(name=self.name))
                    self.state = self.STATE_CONNECTED
                except Exception as e:
                    logger.warning(e)
                    self.state = self.STATE_START_N
                    time.sleep(1)
                    pass
                
            if self.state == self.STATE_START_N:
                self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                ip = self.parameters[ 'server' ] 
                try:
                    self.sock.connect((ip, 42002))
                    logger.info("connected to server")
                    self.state = self.STATE_CONNECTED
                except Exception as e:
                    time.sleep(1)
                    pass
                
            if self.state == self.STATE_CONNECTED:
                try:
                    cmd = 'group "{group:s}"'.format( group=self.parameters[ 'group' ] )
                    self.sock_send(cmd)
                    logger.info("{name:s}: register group {group:s}".format(name=self.name, group=self.parameters[ 'group' ]))
                    self.state = self.STATE_OPERATIONAL
                    
                except Exception as e:
                    logger.warning(e)
                    time.sleep(1)
                    
                    self.STATE_DISCONNECT
                    pass
                
            if self.state == self.STATE_DISCONNECT:
                try:
                    self.sock.close()
                    self.STATE_START
                    
                except Exception as e:
                    logger.warning(e)
                    self.STATE_START
                    pass

            if self.state == self.STATE_OPERATIONAL:                
                try:
                    chunk = self.sock.recv(BUFFER_SIZE) # get the data from the socket
                    if len(chunk) == 0:
                        self.state = self.STATE_DISCONNECT
                        # lost connection, break
                        break
    
                    # there are multiple records possible in one 
                    # received buffer
                    data += chunk
                    while  len(data) >= 4:
                        recordLen = (ord(data[0]) << 24) +     \
                                    (ord(data[1]) << 16) +     \
                                    (ord(data[2]) <<  8) +     \
                                    (ord(data[3]) <<  0 )                
                        
                        if len(data) < 4+recordLen:
                            break   
                    
                        dataraw = data[4: 4+recordLen]
                        logger.debug( 'data recvd from scratch-Length: %d, Data: %s' , len(dataraw), dataraw)
                        self.processRecord (dataraw)
                    
                        data = data[4+recordLen:]
                except socket.timeout:
                    if logger.isEnabledFor(logging.DEBUG):
                        logger.debug( "No data received: socket timeout")
                    continue
                except Exception as e:
                    self.state = self.STATE_DISCONNECT
                    logger.warn(e)
                    time.sleep(1)
                    continue

    def sock_send(self, cmd):
        if self.state in [self.STATE_OPERATIONAL, self.STATE_CONNECTED]: 
            n = len(cmd)
            a = array('c')
            a.append(chr((n >> 24) & 0xFF))
            a.append(chr((n >> 16) & 0xFF))
            a.append(chr((n >>  8) & 0xFF))
            a.append(chr(n & 0xFF))
    
            logger.debug("remote: send {group:s}".format(group=cmd ))
            self.sock.send(a.tostring() + cmd)
        else:
            logger.debug("discarded: send {group:s}".format(group=cmd ))
            
    def processRecord(self, dataraw):
        if 'broadcast' in dataraw:
            logger.debug('broadcast in data: %s' , dataraw)
            
            pattern = 'broadcast "([^"]*)"'
            m = re.match(pattern, dataraw)
            cmd = m.group(1)
            
            self.sendCommandAlias( cmd )
        
        elif 'sensor-update' in dataraw:
            logger.debug( "sensor-update rcvd %s" , dataraw) 
            #
            # data are name value pairs. name always in quotes, values either quotes or not (for numeric values)
            # String quotes are handled by doubling the quotes.
            # 
            # nameValueString = dataraw[len('sensor-update'):]
            # nameValueArray = namevalueparser.NameValueParser(nameValueString ).parse()
    
            # for nv in nameValueArray:
            #     self.commandResolver.resolveValue(nv[0], nv[1])            
        else:
            logger.warn("unknown command in received data " + dataraw )

        