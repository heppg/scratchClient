# -*- coding: utf-8 -*-
# --------------------------------------------------------------------------------------------
# Implementation of scratch Remote Sensor Protocol Client
#
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
#
import SocketServer
import logging
import logging.config
import re
from array import *

logger = logging.getLogger(__name__)

class RegistryData:
    def __init__(self, groupName, handler):
        self.groupName = groupName
        self.handler = handler
        
class Registry:
    handlers = None
    
    def __init__(self):
        self.handlers = []
        
    def register(self, groupName, handler):
        self.handlers.append( RegistryData(groupName, handler))
    
    def deregister(self, groupName):
        pass
    
    def forward (self, groupName, handler, data):
        
        for x in self.handlers:
            if handler != x.handler: 
                if groupName == x.groupName:
                    x.handler.forward(data)
    

registry = Registry()


class ChatRequestHandler(SocketServer.BaseRequestHandler):

    def forward(self, cmd):
        try:
            n = len(cmd)
            a = array('c')
            a.append(chr((n >> 24) & 0xFF))
            a.append(chr((n >> 16) & 0xFF))
            a.append(chr((n >>  8) & 0xFF))
            a.append(chr(n & 0xFF))
        
            print("[{addr:s}] remote: group send {cmd:s}".format(addr= self.addr, cmd = cmd ))
            self.request.send(a.tostring() + cmd)
        except Exception as e:
            print(e)
             
    def handle(self): 
        self.addr = self.client_address[0] 
        print ( "[%s] Connection established" % self.addr ) 
        while True: 
            data = self.request.recv(1024) 
            if data: 
                print ("[%s] %s" % (self.addr, data))
                while  len(data) >= 4:
                    recordLen = (ord(data[0]) << 24) +     \
                                (ord(data[1]) << 16) +     \
                                (ord(data[2]) <<  8) +     \
                                (ord(data[3]) <<  0 )                
                    
                    dataraw = data[4: 4+recordLen]
                    print( '[%s] data recvd from scratch-Length: %d, data: %s' % ( self.addr, len(dataraw), dataraw))
                    self.processRecord ( dataraw)
                
                    data = data[4+recordLen:]
                     
            else: 
                print ("[%s] Connection closed" % self.addr )
                registry.deregister(self.groupName)
                break

    def processRecord(self, dataraw):
        if 'group' in dataraw:
            print('group in data: %s' , dataraw)
            
            pattern = 'group "([^"]*)"'
            m = re.match(pattern, dataraw)
            self.groupName = m.group(1)
            registry.register(self.groupName, self)
            
        else:
            print('forward data: %s' , dataraw)
            registry.forward(self.groupName, self, dataraw)

                
server = SocketServer.ThreadingTCPServer(("", 42002),  ChatRequestHandler) 
server.serve_forever()