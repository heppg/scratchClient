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


import logging
logger = logging.getLogger(__name__)

import time
import sys

import adapter.adapters
import environment

       


debug = False
import json

from ws4py.websocket import WebSocket

websocketQueue = helper.abstractQueue.AbstractQueue()
    
class PendelWebSocket(WebSocket):
    def __init__(self, sock, protocols=None, extensions=None, environ=None, heartbeat_freq=None):
        WebSocket.__init__(self, sock, protocols, extensions, environ, heartbeat_freq)
    
    cnt = 1    
        
    def received_message(self, message):
        """
        Automatically sends back the provided ``message`` to
        its originating endpoint.
        """
        
        self.cnt += 1
        if self.cnt == 100:
            self.cnt = 0
            if debug:
                logger.info("100:", message.data, websocketQueue.qsize())
            
        websocketQueue.put(message.data)
       
        # self.send(message.data, message.is_binary)




class WebsocketXY_Adapter(adapter.adapters.Adapter):
    
    # -----------------------------------------
    # fields for adapter
    # -----------------------------------------
    mandatoryParameters = {  }
    # -----------------------------------------
    
    def __init__(self):
        
        # General Adapter
        adapter.adapters.Adapter.__init__(self)

    def setXMLConfig(self, child):
        # print "setXMLConfig"
        #
        # read configuration from xml
        #
        loggingContext = "adapter '[{a:s}]'".format(a=self.name ) 
        if not environment.has_key('gui') :
            logger.error("No gui enabled . Do not configure gui")
            return
        
        for tle in child:
            if 'webserver' == tle.tag:
                for tle2 in tle:
                    if 'route' == tle2.tag:
                        name = tle2.attrib['name']
                        route =  tle2.attrib['route']
                        environment.get('gui').websocketPlugin( name, route, PendelWebSocket)
                    if 'html' ==  tle2.tag:
                        name = tle2.attrib['name']
                        path =  tle2.attrib['path']
                        comment =  tle2.attrib['comment']
                        environment.get('gui').htmlPlugin( name, path, comment )   
                        
    def setActive (self, active):
        adapter.adapters.Adapter.setActive(self, active)
        if active:
            pass
        else:
            pass
               
    def run(self):
        lastX = None
        lastY = None
        cnt = 0
        while not(self.stopped()):
            self.delay(0.05)
            value = None
            
            try:  
                if websocketQueue.empty():
                    v = websocketQueue.get(False)
                    if v != None:
                        value = v    
                else:      
                    while not websocketQueue.empty(): 
                        v = websocketQueue.get(False)
                        if v != None:
                            value = v    
                
            except Queue.Empty:
                pass
                
            if value == None:
                continue
            
            if debug:
                cnt += 1
                if cnt == 100:
                    cnt = 0
                    print("queue get 100", value)
                         
            j = json.loads(value)
            # print(j)
            if 'cnt' in j:
                self.cntValue ( str(j.get( 'cnt' ))) 
                
            if 'x' in value:
                x = str(j.get( 'x'))
                if x != lastX:
                    lastX = x
                    self.xValue ( x ) 
                
            if 'y' in value:
                y = str(j.get( 'y'))
                if y != lastY:
                    lastY = y
                    self.yValue ( y )
                     
            if 'click' in value:
               self.click ( ) 
         
    def cntValue(self, value):
        self.sendValue('"' + value + '"')
   
    def xValue(self, value):
        self.sendValue('"' + value + '"')
   
    def yValue(self, value):
        self.sendValue('"' + value + '"')

    def click(self):
        self.send()
        
        
