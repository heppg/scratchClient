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

import logging
logger = logging.getLogger(__name__)

try:
    import  pythonosc
    import  pythonosc.osc_message_builder
except Exception:
    logger.error("sonicpiAdapter needs python-osc, install with 'sudo pip3 install python-osc'")
    quit()
    
import sys
if sys.version_info.major != 3:
    logger.error("sonicpiAdapter only runs with python3")
    quit()

import queue
import adapter
import socket

debug = False

# --------------------------------------------------------------------------------------


class SonicPi_Adapter (adapter.adapters.Adapter):
    """Interface to SonicPi """
    
    mandatoryParameters = {  
                           'sonicpi.server' : 'localhost', 
                           'sonicpi.port'   : '4557',
                           'sonicpi.agent'  : 'SONIC_PI_CLI'
                          }
    
    def __init__(self):
        adapter.adapters.Adapter.__init__(self)
        self.queue = queue.Queue()
        
    def setActive (self, state):
        if debug:
            print(self.name, "setActive", state)
        #
        # use default implementation to start up Threads
        #
        adapter.adapters.Adapter.setActive(self, state);
        #
         

    START = 'START'
    WAIT = 'WAIT'
    CONNECTED_INIT = 'CONNECT_INIT'
    CONNECTED = 'CONNECTED'
    
    tune_disconnect = 'use_synth :chiplead ; play 80,release: 0.08 ; sleep 0.1 ; play 83, release: 0.08 ; sleep 0.1 ;play 80, release: 0.08 ;sleep 0.15 ;play 75, release: 0.5 ;sleep 0.5 ;play 75, release: 0.1'
    tune_connect = 'use_synth :chiplead ; play 80,release: 0.08 ; sleep 0.1 ; play 83, release: 0.08 ; sleep 0.1 ;play 80, release: 0.08 ;sleep 0.15 ;play 85, release: 0.5 ;sleep 0.5 ;play 85, release: 0.1'

    def run(self):
        _port = int(self.parameters['sonicpi.port'])
        _host = self.parameters['sonicpi.server']
        _agent = self.parameters['sonicpi.agent']
        
        self.state = self.START            
        new_state = None    
        while not self.stopped():
            if self.state == self.START:
                try:  
                    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                    s.connect((_host, _port))
                    new_state = self.CONNECTED_INIT
                    
                except Exception as e:
                    logger.error("{name:s}: could not connect to server {server:s}:{port:d}".format(name=self.name, server=_host, port=_port))
                    new_state = self.WAIT

            elif self.state == self.WAIT:
                self.delay(5)
                new_state = self.START
                        
            elif self.state == self.CONNECTED_INIT:
                cmd = self._osc_message("/run-code", [ _agent, self.tune_connect ] )
                s.sendall(cmd )  
                self.delay(0.7)
                # empty queue
                while True:
                    try:
                        s = self.queue.get(block=False)
                    except Exception:
                        break
                
                new_state = self.CONNECTED
                            
            elif self.state == self.CONNECTED:
                msg = None            
                try:
                    msg = self.queue.get(block=True, timeout=0.1)
                    
                except Exception:
                    pass
                if msg != None:
                    cmd = self._osc_message("/run-code", [ _agent, msg ] )
                    if debug: print(cmd)
                    try:
                        s.sendall(cmd )
                    except:
                        s.close()
                        new_state = self.WAIT
                
                    
            if new_state != self.state:
                if debug:
                    print("{s:s}-->{ns:s}".format(s=self.state, ns=new_state))
                self.state = new_state
        if self.state == self.CONNECTED:
            cmd = self._osc_message("/run-code", [ _agent, self.tune_disconnect ] )
            s.sendall(cmd )        
            s.close()
        if (debug): print("thread stopped")
            
    def _osc_message( self, address, values):
        """Compose an OSC message and send it."""
        builder = pythonosc.osc_message_builder.OscMessageBuilder(address=address)
    
        for val in values:
            builder.add_arg(val)
        msg = builder.build()
        return msg.dgram

    def sonicpi(self, value):
        if debug: print ( value)
        
        if self.state == self.CONNECTED:
            self.queue.put(value)            
                
