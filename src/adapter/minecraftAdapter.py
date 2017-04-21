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
import adapter
try:
    import mcpi
    import mcpi.minecraft
    import mcpi.connection
    
except ImportError:
    exit("This adapter requires mcpi library to be installed.")

import helper.abstractQueue
import select
import sys
import traceback
import logging
import helper.logging
logger = logging.getLogger(__name__)


debug = False

class ProblemAwareConnection( mcpi.connection.Connection):
    """overwrite drain method, as in case of error this does not throw an exception"""
    
    def __init__(self, address , port):
        if debug:
            print("address", address, 'port', port)
        mcpi.connection.Connection.__init__(self, address, port)
        
    def drain(self):
        """Drains the socket of incoming data"""
        while True:
            readable, _, _ = select.select([self.socket], [], [], 0.0)
            if not readable:
                break
            data = self.socket.recv(1500)
            #
            # fix
            #
            if len(data) == 0:
                raise Exception("connection lost")

            e =  "Drained Data: <%s>\n"%data.strip()
            e += "Last Message: <%s>\n"%self.lastSent.strip()
            sys.stderr.write(e)
            
#    def send(self, f, *data):
#        super(type(mcpi.connection.Connection), self).send(  f, data)
#        print(self.lastSend)
 
class MinecraftAdapter(adapter.adapters.Adapter):
    
    # -----------------------------------------
    # fields for adapter
    queueThread = None
    
    # -----------------------------------------
   
    mandatoryParameters = { 
                'minecraft.server'  : 'localhost',
                'minecraft.port': '4711',
    }
    # -----------------------------------------
    
    def __init__(self):
        # General Adapter
        adapter.adapters.Adapter.__init__(self)
        self.commandQueue = helper.abstractQueue.AbstractQueue()
        
        self.connected = False 
        
                            
    def setActive (self, active):
        adapter.adapters.Adapter.setActive(self, active)
        if active:
            self.variables = dict()
            self.variables ['playerX'] = 0.0
            self.variables ['playerY'] = 0.0
            self.variables ['playerZ'] = 0.0
            
            self.variables ['cameraX'] = 0.0
            self.variables ['cameraY'] = 0.0
            self.variables ['cameraZ'] = 0.0
            
            self.variables ['mcpiX'] = 0
            self.variables ['mcpiY'] = 0
            self.variables ['mcpiZ'] = 0
            self.variables ['mcpiX0'] = 0
            self.variables ['mcpiY0'] = 0
            self.variables ['mcpiZ0'] = 0
            #self.variables ['mcpiX1'] = 0
            #self.variables ['mcpiY1'] = 0
            #self.variables ['mcpiZ1'] = 0
            self.variables ['blockTypeId'] =  0
            self.variables ['blockData'] = 0
            # Minecraft Graphics Turtle
            #self. variables ['speed'] = 0
            #self.variables ['steps'] = 0
            #self.variables ['degrees'] = 0

            # Minecraft Stuff
            #self.variables ['radius'] = 0
            #self.variables ['fill'] = 0

            #self.mcDrawing = mcstuff.minecraftstuff.MinecraftDrawing(self.mc)
            
            pass
        else:
            pass   
        
    def run(self):
           
        server =  self.parameters['minecraft.server']  
        port = int( self.parameters['minecraft.port'] )
         
        state = 0
        
        while not self.stopped():
        
            if state == 0:
                self.connected = False
                try:

                    if server.strip() == '':
                        connection= ProblemAwareConnection( "localhost",  4711)
                    else:
                        connection= ProblemAwareConnection( server, port)
                    
                    if debug: print(connection)
                    
                    self.mc = mcpi.minecraft.Minecraft ( connection)

                    with helper.logging.LoggingContext(logger, level=logging.DEBUG):
                        logger.info("{name:s}: connected to minecraft server".format(name=self.name))
                    state = 4
                    
                except Exception as e:
                    if debug: print(e)
                    logger.error("{name:s}: cannot connect to minecraft server".format(name=self.name))
                    state = 2

            elif state == 2:
                if debug: print("delay")
                self.connected = False
                self.delay(10)
                state = 0
                
            elif state == 4:    
                self.connected = True
                try:
                    d = self.commandQueue.get(block=True, timeout= 0.1)
                except helper.abstractQueue.AbstractQueue.Empty:
                    continue 
        
                try:
                    if d['command'] == 'postToChat':
                        self.mc.postToChat( d['args'] )
                        
                    elif d['command'] == 'setPos':
                        self.mc.player.setPos( d['args'] )
                        
                        
                    elif d['command'] == 'cameraSetFixed':
                        self.mc.camera.setFixed( )
                        
                    elif d['command'] == 'cameraSetPos':
                        self.mc.camera.setPos( d['args'] )
                        
                    elif d['command'] == 'cameraSetNormal':
                        self.mc.camera.setNormal( self.mc.getPlayerEntityIds() [0] )
                        
                    elif d['command'] == 'cameraSetFollow':
                        self.mc.camera.setFollow( self.mc.getPlayerEntityIds() [0] )
                        
                    elif d['command'] == 'setBlock':
                        self.mc.setBlock( d['args'] )
                        
                    elif d['command'] == 'setBlocks':
                        self.mc.setBlocks( d['args'] )
                        
                    elif d['command'] == 'getHeight':
                        _posY = self.mc.getHeight( d['args'] )
                        
                        self.posY ( _posY)
                        self.mc.postToChat("posY: %d" % _posY)
                        
                    elif d['command'] == 'getBlockWithData':
                        blockFound = self.mc.getBlockWithData( d['args'])
                        if debug:
                            print( blockFound )
                        self.blockFound_id( blockFound.id )
                        self.blockFound_data( blockFound.data)
         
                    elif d['command'] == 'reset':
                        self.mc.postToChat('reset the world')
                        self.mc.setBlocks(-100, 0, -100, 100, 63, 100, 0, 0)
                        self.mc.setBlocks(-100, -63, -100, 100, -2, 100, 1, 0)
                        self.mc.setBlocks(-100, -1, -100, 100, -1, 100, 2, 0)
                        self.mc.player.setPos(0, 0, 0)
                    
                    #elif d['command'] == 'stuffDrawLine':
                    #    self.mcDrawing.drawLine( d['args'])
                    #elif d['command'] == 'stuffDrawSphere':
                    #    self.mcDrawing.drawLine( d['args'])
                    #elif d['command'] == 'stuffDrawCircle':
                    #    self.mcDrawing.drawCircle( d['args'])
                except Exception as e:
                    if logger.isEnabledFor(logging.DEBUG):
                        traceback.print_exc(file=sys.stdout)
                    logger.error("{name:s}: exception {e:s}".format(name=self.name, e=str(e)))
                    state = 0
                    
        self.connected = True
                        
    def _handle_int(self, name, value):
        """input from scratch to adapter"""
        #  print("mcpiX, value   ", name, type(value), value)
        try:
            self.variables[name] = int(value)
        except:
            pass
        
    def _handle_float(self, name, value):
        """input from scratch to adapter"""
        #  print("mcpiX, value   ", name, type(value), value)
        try:
            self.variables[name] = float(value)
        except:
            pass
     
    def playerX(self, value): self._handle_float('playerX', value)
    def playerY(self, value): self._handle_float('playerY', value)
    def playerZ(self, value): self._handle_float('playerZ', value)
    
    def cameraX(self, value): self._handle_float('cameraX', value)
    def cameraY(self, value): self._handle_float('cameraY', value)
    def cameraZ(self, value): self._handle_float('cameraZ', value)
    
    def mcpiX(self, value): self._handle_int('mcpiX', value)
    def mcpiY(self, value): self._handle_int('mcpiY', value)
    def mcpiZ(self, value): self._handle_int('mcpiZ', value)
    
    def mcpiX0(self, value): self._handle_int('mcpiX0', value)
    def mcpiY0(self, value): self._handle_int('mcpiY0', value)
    def mcpiZ0(self, value): self._handle_int('mcpiZ0', value)
    
        
#    
#    def mcpiX1(self, value): self._handle_int('mcpiX1', value)
#    def mcpiY1(self, value): self._handle_int('mcpiY1', value)
#    def mcpiZ1(self, value): self._handle_int('mcpiZ1', value)

    def blockTypeId(self, value): self._handle_int('blockTypeId', value)
    def blockData(self, value): self._handle_int('blockData', value)
    
#    def speed(self, value): self._handle_int('speed', value)
#    def steps(self, value): self._handle_int('steps', value)
#    def degrees(self, value): self._handle_int('degrees', value)
    
#    def radius(self, value): self._handle_int('radius', value)
#    def fill(self, value): self._handle_int('fill', value)
    
    queueMax = 0
    
    def _pushCommand(self, name, args):
        if self.connected:
            self.queueMax = max( self.queueMax, self.commandQueue.qsize())
            d = dict()
            d['command'] = name
            d['args'] = args
            self.commandQueue.put(d)
            if debug:
                print ("queue length ", self.commandQueue.qsize(), self.queueMax )
                
    def hello_minecraft(self):
        self._pushCommand( 'postToChat', ("hello minecraft") )   
         
    def postToChat(self, value): 
        self._pushCommand( 'postToChat', (value) )   

    def reset(self):
        self._pushCommand( 'reset', () )    
    
    def setPos(self):
        self._pushCommand( 'setPos', (self.variables['playerX'], 
                                     self.variables['playerY'], 
                                     self.variables['playerZ'] ))    
    def cameraSetPos(self):
        self._pushCommand( 'cameraSetPos', (self.variables['cameraX'], 
                                     self.variables['cameraY'], 
                                     self.variables['cameraZ'] ))    
      

    def cameraSetFixed(self):
        self._pushCommand( 'cameraSetFixed', ())
        
    def cameraSetNormal(self):
        self._pushCommand( 'cameraSetNormal', ())
        
    def cameraSetFollow(self):
        self._pushCommand( 'cameraSetFollow', ())
        
    def setBlock(self):
        self._pushCommand( 'setBlock', (self.variables['mcpiX'], 
                                       self.variables['mcpiY'], 
                                       self.variables['mcpiZ'],
                                       self.variables['blockTypeId'],
                                       self.variables['blockData']   ))   
    def setBlocks(self):
        self._pushCommand( 'setBlocks', (self.variables['mcpiX'], 
                                       self.variables['mcpiY'], 
                                       self.variables['mcpiZ'],
                                       
                                       self.variables['mcpiX0'], 
                                       self.variables['mcpiY0'], 
                                       self.variables['mcpiZ0'],
                                       
                                       self.variables['blockTypeId'],
                                       self.variables['blockData']   ))   
    def getHeight(self):
        self._pushCommand( 'getHeight', (self.variables['mcpiX'], 
                                        self.variables['mcpiY'] ) ) 
        
    def getBlockWithData(self):
        self._pushCommand( 'getBlockWithData',  
                            ( self.variables['mcpiX'], 
                              self.variables['mcpiY'],
                              self.variables['mcpiZ'] ) )
        
         
#    def stuffDrawLine(self):
#        self._pushCommand( 'stuffDrawLine', 
#                            ( self.variables['mcpiX1'], 
#                              self.variables['mcpiY1'],
#                              self.variables['mcpiZ1'],
#                              self.variables['mcpiX'], 
#                              self.variables['mcpiY'],
#                              self.variables['mcpiZ'],
#                              self.variables['blockTypeId'],
#                              self.variables['blockData']   ) )
#    
#    def stuffDrawSphere(self):
#        self._pushCommand( 'stuffDrawLine', 
#                            ( self.variables['mcpiX'], 
#                              self.variables['mcpiY'],
#                              self.variables['mcpiZ'],
#                              self.variables['radius'],
#                              self.variables['blockTypeId'],
#                              self.variables['blockData']   ) )
#        
#    def stuffDrawCircle(self):
#        self._pushCommand( 'stuffDrawCircle', 
#                            ( self.variables['mcpiX'], 
#                              self.variables['mcpiY'],
#                              self.variables['mcpiZ'],
#                              self.variables['radius'],
#                              self.variables['blockTypeId'],
#                              self.variables['blockData']   ) )
    def posY(self, value):
        self.sendValue(value)   
         
    def blockFound_id(self, value):
        self.sendValue(value) 
           
    def blockFound_data(self, value):
        self.sendValue(value)    