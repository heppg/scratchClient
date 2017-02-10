#!/usr/bin/python
# -*- coding: utf-8 -*-
# --------------------------------------------------------------------------------------------
# Implementation of scratch Remote Sensor Protocol Client
#
# Copyright (C) 2013, 2016  Gerhard Hepp
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
# This code is partially based on code from Simon Walters
# Here the original credentials:
#
#     This code is copyright Simon Walters under GPL v2
#     This code is derived from Pi-Face scratch_handler by Thomas Preston
#     This code now hosted on Github thanks to Ben Nuttall
#     Version 2.3 15Jun13
#
# Major rework done in July 2013 till 2017  by Gerhard Hepp
# Features are
# - protocol handling
# - Modularization of Software (server connection, data model, configuration, monitoring)
# - Configuration in XML for various IO-settings
# - GUI for monitoring and simulation on non-Raspberry environments 
# - added various devices
# --------------------------------------------------------------------------------------------
# target environment:  python release 2.7
#                      python release 3.3
# --------------------------------------------------------------------------------------------
# changes:
# 
changes = [
'2017-01-26 minor changes in log messages on scratch connection.',  
'2017-01-16 improved connection handling and last-value in arduinoUNO adapter.',  
'2017-01-10 all queue definitions wrapped by a helper class to fix python2/3 compatibility.',  
#
'2016-12-19 arduino nano used as neopixel driver',  
'2016-12-01 pico2wave tts adapter',  
'2016-10-30 bugfix adapter.arduino.UNO_Adapter (usage of analog pins on arduino for digital io)',  
'2016-09-26 optional parameters for servo adapter DMA_PWMServo',  
'2016-08-19 added hc-sr04 sensor based on pigpiod',           
'2016-08-14 added lego wedo2 adapter',           
'2016-07-31 added openweathermap-api access',
#
'2016-06-05 added config file config_AT42QT1070',
'2016-05-30 Adapter GpioButtonInput is deprecated, use GpioEventInput instead',
'2016-05-28 fixed bug in GpioEventInput (inverse did not work)',
'2016-05-22 DS1820-Adapter, added error messages',
'2016-05-21 UNO_Adapter, for posix systems: added an exclusive lock to serial connection',           
'2016-04-17 removed a flaw in accessing files.',
'2016-03-28 added twitter adapter.',
'2016-03-25 added arduino adapter for LEGO powerfunctions.',
'2016-03-19 added support for external speech recognition adapter.linux.Linux_ASR_Adapter.',
'2016-03-07 added aplay and arecord command adapter.',
'2016-02-29 bugfix RPIO2 library: pwm to zero did not reliably switch off when fullscale to zero.',
'2016-02-28 ident code for arduino sketch.',
'2016-02-21 performance optimizations in arduinoUNO adapter and arduino sketch.',
'2016-02-15 added servo capability for arduinoUNO-adapter, reworked reconnect policy for this adapter.',
'2016-01-02 dma based PWM added, gpioLib-switch removed.',
#
'2015-12-09 bug fixes in pwm-servo; value range checks added.',
'2015-11-16 pianoHat Adapter added.',
'2015-11-16 bugfix in GpioInput-Adapter.',
'2015-11-12 added blink(1)'
'2015-10-16 modified the "is the code already started"-code; made the code relative to current python code.',
'2015-10-16 corrected a bug in formatting an error message',
'2015-09-26 added senseHat-adapter LED, environmental, IMU',
'2015-09-25 added senseHat_Adapter (limited functionality, LED only)',
'2015-08-29 reworked CommunicationAdapter, which was broken after the publish-subscibe reengineering',
'2015-08-03 RFID-Reader adapter added',
'2015-08-01 pico board adapter added',
'2015-07-17 GpioValueInput-Adapter added. Allows to send predefined values on low/high',
'2015-07-14 MCP3008',
'2015-07-09 error recovery strategy for scratch 1.4 2015-jan-15, issue #136',
#
'2015-05-25 added arduinoUNO adapter.',
'2015-05-23 system time adapter added',
'2015-05-05 piFace support, piGlow support',
'2015-05-03 solved display issues in GUI for multiple scratch variables into one adapter method; added PCA9865; removed bugs in MCP23S17. Refactoring the i2c-system.',
'2015-04-19 removed bug in positioning popup editor in adapter display',
'2015-04-13 internal: implemented plugin methods for an adapter to modify web server.',
'2015-04-11 added DHT22 with atmega328-coprocessor; added smartphone positional sensors',
'2015-04-08 additional configuration check: input/output names unique in config.',
'2015-04-07 converted event publishing to pubsub pattern, web interface to websocket',
'2015-03-28 added atmel-328 adapter for hc-sr04',
'2015-03-16 added operation system command adapter',
'2015-03-16 added half-bridge motor adapter',
'2015-03-14 added MCP23S17-adapter',
'2015-03-01 added usb adapter for HID-barcode-scanner',
'2015-02-12 added servoblaster adapter',
'2015-01-04 removed quote-handling-problem in broadcast name strings',
#
'2014-12-22 added a lookup strategy for config files which allows for simpler command line syntax',
'2014-12-17 namevalueparser, corrected for quote in name',           
'2014-12-13 worked on python3 compatibility; changed package structure (adapter.adapter->adapter.adapters); fixed codepage conversion problems in web access.',           
'2014-11-14 Added DS1820 adapter.',           
'2014-10-17 Modified help output.',           
'2014-10-03 modified socket code, outgoing to better handle utf8 strings; modified test adapter with different data types.',           
'2014-09-20 changed dma channel to 4',           
'2014-09-01 Added BH1750 Luminosity Sensor, i2c bus',           
'2014-08-29 GpioInput, fixed "inverse"-Problem.',           
'2014-08-08 texttospeech, fixed an exception problem.',           
'2014-08-03 renamed ADCInput to ADC_MCP3202_10_Input.',           
'2014-08-01 bug fix for activation of adapters.',           
'2014-07-30 renamed adapter.stepper.Stepper to adapter.stepper.BipolarStepper\
            added adapter.stepper.UnipolarStepper',
'2014-07-26 added \'changes\' command line switch.',
'2014-07-26 added SIM800 GSM Modem support.',
'2014-07-12 added GpioStateOutput, for signalling client state. Needed some \
            adjustments in interrupt handling to allow for this special type of \
             adapter. ',
#
'2014-06-19 performance optimizations adapter, commandResolve-Logic (no eval).',
'2014-06-17 minor performance optimizations in namevalueparser.',
'2014-06-12 corrected some instability in receiving variables.',
'2014-05-01 changed send method to scratch, utf-8 aware and pytho3 compatible',
'2014-03-31 fixed config file config_ikg_7segment.xml  \
            added error checks in reading xml files.',
'2014-03-12 changed data receive logic/process dataraw to be more robust. \
            Instantiation the managers on need only.',
'2014-03-11 changed data receive logic to work even for very long records.',
'2014-02-22 added I2C-Handlers for ADC ADS1015 ',
'2014-02-03 enable one broadcast/value for multiple adapters',
'2014-01-24 fixed a conversion error from adapter to framework (now always strings)',
'2014-01-06 added WS2801-Adapter, some bug fixes in SPI handling',
#
'2013-12-26 added remote connection adapter',
'2013-12-01 configuration file for portMapping in xml',
'2013-11-16 added sighup in order to catch terminal closed.',
'2013-11-16 added code to enforce a singleton running instance',
]

# --------------------------------------------------------------------------------------------
from array import *
import server.scratchClientServer

from adapter.adapters import GPIOAdapter
from adapter.adapters import SPIAdapter
from adapter.adapters import I2CAdapter

from spi.manager import SPIManager
from i2c.manager import I2CManager

import sys
    
import configuration
import errorManager
#import eventHandler
import publishSubscribe
import logging
import logging.config
import protocol
import os
import os.path
import helper.abstractQueue

if sys.platform.startswith('linux'):
    import grp

import re
import signal
import socket
import traceback
import threading
import time

import helper.logging

commandlineHelp = """
-host <ip>           Scratch Host ip or hostname, default 127.0.0.1
-port <number>       Port number, default 42001

-c <configfile>
-config <configfile> Name of config xml-file, default config/config.xml 
                     There is a lookup strategy used (add xml extension when needed, 
                     literal, then config/, ../config; then add 'config_' to 
                     filename and then literal, config/, ../config

-C <configfile>      Name of config xml-file, default config/config.xml 
                     There is NO lookup strategy used, only literal.
                     
-gpioLib             set the gpiolibrary, default 'RPi_GPIO_GPIOManager'
                     deprecated

web gui switches

-nogui               do not show GUI
-guiRemote           allows remote access to GUI web page, 
                     default is local access only

debug and test switches

-forceActive         force active mode, even if GPIO library is 
                     not available.
-validate            Validate config and terminate.

-h
-help                print command line usage and exit
-v                   verbose logging
-d                   debug logging
-license             print license and exit.
-changes             print changes list

"""

#
# Set some constants 
#
DEFAULT_PORT = '42001'
DEFAULT_HOST = '127.0.0.1'

DEFAULT_CONFIGFILENAME = 'config/config.xml'
DEFAULT_PORTMAPPINGFILENAME = 'config/portMapping.xml'

DEFAULT_GPIOLIB = 'RPi_GPIO_GPIOManager'
DEFAULT_GUIREMOTE = False

DEFAULT_PIDFILENAME = 'scratchClient.pid'


BUFFER_SIZE = 240 #used to be 100
SOCKET_TIMEOUT = 2

verbose = False
debug = False
nogui = False
guiRemote = DEFAULT_GUIREMOTE
validate = False

#forceSimulation = False
forceActive = False

configFileName = DEFAULT_CONFIGFILENAME
portmappingFileName = DEFAULT_PORTMAPPINGFILENAME

host = DEFAULT_HOST
port = DEFAULT_PORT
gpioLib = DEFAULT_GPIOLIB

pidFileName = DEFAULT_PIDFILENAME

gpl2 = """
 Copyright (C) 2013, 2016  Gerhard Hepp

 This program is free software; you can redistribute it and/or modify it under the terms of 
 the GNU General Public License as published by the Free Software Foundation; either version 2 
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; 
 without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 See the GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License along with this program; if 
 not, write to the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, 
 MA 02110, USA 
"""

runIt = True

import environment        

class ScratchSender(threading.Thread):
    
    def __init__(self):
        threading.Thread.__init__(self)
        self._stop = threading.Event()
        self._lock = threading.Lock()
        
        #publishSubscribe.Pub.subscribe('scratch.output.value', self.sendValue)
        #publishSubscribe.Pub.subscribe('scratch.output.command', self.send)

    def stop(self):
        self._stop.set()

    def stopped(self):
        return self._stop.isSet()

    def setSocket(self, socket):
        self.scratch_socket = socket
        
        
    def run(self):
        pass

    def sendValue(self, message):
        """send a 'sensor-update'"""
        
        bcast_str = 'sensor-update "' + message['name'] + '" ' +  message['value']
        if logger.isEnabledFor(logging.INFO):
            logger.info('ScratchSender, send value: %s' , bcast_str)
        
        self.send_scratch(bcast_str)

    def sendCommand(self, message):
        """send a 'broadcast'"""
        bcast_str = 'broadcast "{name:s}"'.format( name= message['name'] )
        if logger.isEnabledFor(logging.INFO):
            logger.info('ScratchSender, send broadcast: %s', bcast_str)
        
        self.send_scratch(bcast_str)

    def send_scratch(self, cmd):
        """this method will be used by multiple threads, so synchronizing
        looks reasonable. Missing synchronization could explain sporadic
        scratch breakdowns."""
        self._lock.acquire()
        try:
            # cmd is a str
            # in python3, use following code
            if True:
                
                if sys.version_info.major == 2:
                    try:
                        c = bytearray(cmd)
                    except TypeError as e:
                        logger.error(e)
                        c = bytearray(cmd, 'utf-8' )
                        
                    n = len( c )
                if sys.version_info.major >= 3:
                    c = bytes( cmd, 'utf-8' )
                    n = len( c )
    
                a = array('B')
                # convert len to the first four bytes
                a.append(((n >> 24) & 0xFF))
                a.append(((n >> 16) & 0xFF))
                a.append(((n >>  8) & 0xFF))
                a.append((n & 0xFF))
                
                if sys.version_info.major == 2:
                    a.extend( c )
                    # print('send a', a)
                if sys.version_info.major >= 3:
                    a.frombytes( c )
                try:
                    totalsent = 0
                    while totalsent < len(a):
                        sent = self.scratch_socket.send(a[totalsent:])
                        if sent == 0:
                            scratchClient.event_disconnect()
                            # TODO: ordentlich alles abbrechen
                            return
                        totalsent += sent
                        
                except Exception as e:
                    if logger.isEnabledFor(logging.INFO):
                        logging.info(e)
                    pass
            else:
                n = len(cmd)
                a = array('c')
                a.append(chr((n >> 24) & 0xFF))
                a.append(chr((n >> 16) & 0xFF))
                a.append(chr((n >>  8) & 0xFF))
                a.append(chr(n & 0xFF))
                
                try:
                    self.scratch_socket.send(a.tostring() + cmd)
                except Exception as e:
                    if logger.isEnabledFor(logging.INFO):
                        logging.info(e)
                    pass
        finally:
            self._lock.release()

class ScratchListener(threading.Thread):
    
    def __init__(self, socket):
        threading.Thread.__init__(self)
        self.scratch_socket = socket
        self._stop = threading.Event()
        
        
    def stop(self):
        self._stop.set()

    def stopped(self):
        return self._stop.isSet()

    def run(self):
        """main listening routine to remote sensor protocol"""
        #print("ScratchListener thread started")
        logger.debug("ScratchListener thread started")
        global scratchClient
        
        # 
        # data is current, aggregated bytes received. From these, records are extracted.
        #
        data = ''
        # 
        # chunk are bytes arriving from the socket
        #
        chunk = ''
        #
        # record is a full, interpretable array of bytes. 
        #
        record = ''
        #
        while not self.stopped():
            try:
                #
                # get the bytes from the socket
                # This is not necessarily a full record, just some bytes.
                # 
                chunk =  self.scratch_socket.recv(BUFFER_SIZE) 
                
                if logger.isEnabledFor( logging.DEBUG):
                    x =  map(ord, chunk)
                    s = ''
                    for xx in x:
                        s += "{xx:02x} ".format(xx=xx)
                    logger.debug("received " + s )

                #
                # no data arriving means: connection closed
                #
                if len(chunk) == 0:
                    scratchClient.event_disconnect()
                    break

                data += chunk
                #
                # there are multiple records possible in one 
                # received chunk
                # ... as well as the data could not be long enough for a full record.
                #
                # need at least 4 bytes to identify length of record.
                #
                while  len(data) >= 4:
                    #
                    # there are problems with scratch 1.4 2015-jan-15 on raspbian, not sending data according to bytes, but 
                    # length according to chars. When there are utf-8-chars in data stream, this is
                    # not the same.
                    # For this situation, an emergency recovery strategy is implemented: look for first two bytes of buffer 
                    # to be zero- it is reasonable that messages are less then 65536 bytes long.
                    if ord(data[0]) != 0 or ord(data[1]) != 0:
                        logger.error("fatal: first two bytes of message are not zero, discard data till zeros found")   
                        discard = 0
                        while len(data) > 2:
                            if ord(data[0]) == 0 and ord(data[1]) == 0:
                                break
                            else:
                                data = data[1:]
                                discard += 1
                        logger.error("discarded {disc:d} bytes".format(disc=discard))
                        if not (len(data) >= 4 ):
                            break
                    # end of error stratgey
                    
                    recordLen = (ord(data[0]) << 24) +     \
                                (ord(data[1]) << 16) +     \
                                (ord(data[2]) <<  8) +     \
                                (ord(data[3]) <<  0 )                
                    #            
                    if recordLen > 512:
                        logger.debug("unusual large record length received: {len:d}".format(len=recordLen))   
                    #
                    # are there enough bytes in data for a full record ?
                    # if not, leave the loop here and wait for more chunks to arrive.
                    #
                    if len(data) < 4+recordLen:
                        if logger.isEnabledFor(logging.DEBUG):
                            logger.debug("not enough data in buffer, have {have:d}, need {len:d}".format(have=len(data),len=recordLen))   
                        break   
                    
                    record = data[4: 4+recordLen]
                    if logger.isEnabledFor(logging.DEBUG):
                        logger.debug( 'data received from scratch-Length: %d, Data: %s' , len(record), record)
                    
                    self.processRecord ( record )
                    #
                    # cut off the record from the received data
                    #               
                    data = data[4+recordLen:]
                    #
            except socket.timeout:
                # if logger.isEnabledFor(logging.DEBUG):
                #    logger.debug( "No data received: socket timeout")
                continue
            except Exception as e:
                logger.warn(e)
                if logger.isEnabledFor(logging.DEBUG):
                    traceback.print_exc(file=sys.stdout)
                scratchClient.event_disconnect()
                self.stop()
                continue
        logger.debug("ScratchListener thread stopped")
        
    broadcast_pattern = re.compile('broadcast "([^"]*)"')
    
    def processRecord(self, dataraw):
        if  dataraw.startswith('broadcast'):

            if logger.isEnabledFor(logging.DEBUG):
                logger.debug('broadcast in data: %s' , dataraw)
            broadcastString = dataraw[len('broadcast'):]
            broadcastName = protocol.BroadcastParser(broadcastString ).parse()
            
            publishSubscribe.Pub.publish("scratch.input.command.{name:s}".format(name=broadcastName), { 'name':broadcastName } )
            # self.commandResolver.resolveBroadcast( broadcastName )
            
        elif  dataraw.startswith('sensor-update'):
            #print(dataraw)
            if logger.isEnabledFor(logging.DEBUG):
                logger.debug( "sensor-update rcvd %s" , dataraw) 
            
            #
            # data are name value pairs. name always in quotes, values either quotes or not (for numeric values)
            # String quotes are handled by doubling the quotes.
            # 
            nameValueString = dataraw[len('sensor-update'):]
            # print("parse ", nameValueString)
            nameValueArray = protocol.NameValueParser(nameValueString ).parse()
            # print(nameValueArray)
            # for nv in nameValueArray:
            #    print("single nv = ", nv)
            for nv in nameValueArray:
                # print("process single nv = ", nv)
                # import pdb; pdb.set_trace()
                if logger.isEnabledFor(logging.INFO):
                    logger.info('sensor-update: {name:s}, {value:s}'.format(name=nv[0], value=nv[1]) )
                    
                publishSubscribe.Pub.publish("scratch.input.value.{name:s}".format(name=nv[0]), { 'name':nv[0], 'value':nv[1] } )
            # self.commandResolver.resolveValue(nv[0], nv[1])            
        else:
            logger.warn("unknown command in received data " + dataraw )
    
class ThreadManager:
    """the threads are collected here in order to have one point to terminate each of them"""
    threads = None
    socketThreads = None
    
    def __init__(self):
        self.threads = []
        self.socketThreads = []
        
    def append(self, t):
        self.threads.append(t)
            
    def append_socket(self, t):
        self.socketThreads.append(t)    

    def cleanup_socket(self):
        
        logger.debug("ThreadManager, cleanup_socket")

        for thread in self.socketThreads:
            logger.debug("stop thread %s", str(thread))
            thread.stop()
    
        for thread in self.socketThreads:
            logger.debug("wait join %s", str(thread))
            thread.join(60)
            logger.debug("wait join ok")

        
    def cleanup_threads(self):
        
        logger.debug("ThreadManager, cleanup_threads")
        
        for thread in self.threads:
            logger.debug("stop thread %s", str(thread))
            thread.stop()
    
        for thread in self.threads:
            logger.debug("wait join %s", thread)
            try:
                thread.join(60)
            except Exception as e:
                logger.warn("wait join %s: %s", thread.name, str(e))
            logger.debug("wait join ok")

threadManager = ThreadManager()

class ScratchClient(threading.Thread):
    
    config = None
    gui = None
    gpioManager = None
    spiManager = None
    i2cManager = None
        
    STATE_START = 0
    STATE_CONNECTED = 1
    STATE_DISCONNECTED = 2
    
    state = STATE_DISCONNECTED
    
    myQueue = None
    managers = None
    
    def __init__(self):   
        # import pdb; pdb.set_trace() 
        #global forceSimulation 
        threading.Thread.__init__(self, name="ScratchClient")
        self._stop = threading.Event()

        self.myQueue = helper.abstractQueue.AbstractQueue()

        self.listener = None
        self.sender = ScratchSender()
        #self.commandResolver = CommandResolver()    
        self.gpioManager = None

        if nogui == False:
            self.gui = server.scratchClientServer.ServerThread( parent = self, remote = guiRemote )
            
            environment.append('gui',  self.gui )
       
        # read config files
        configuration.allEverGpios = configuration.GPIORegistry(modulePathHandler.getScratchClientBaseRelativePath(portmappingFileName) )   
        
        self.config = configuration.ConfigManager(configFileName)
        self.config.configure()

        if errorManager.hasErrors() :
            logger.error("Errors: %s", str(errorManager.errors ))
            logger.error("There are errors in configuration file '{f:s}'".format(f=configFileName))
            sys.exit(2)

        self.config.check()
        
        if errorManager.hasErrors() :
            logger.error("Errors: %s", str(errorManager.errors ))
            logger.error("There are errors in configuration file '{f:s}'".format(f=configFileName))
            sys.exit(2)

        if validate:
            logger.warn("Validating, exit with no errors.")
            sys.exit(0)
        # 
        if nogui == False:
            self.gui.start()
            threadManager.append(self.gui)
        # -----------------
        #
        # Instantiate the managers for the various hardware resources
        #
        needGPIO = False
        needSPI = False
        needI2C = False
        needDMA = False
        
        for module in self.config.getAdapters():
            adapterMethods = configuration.AdapterMethods (module)

            if adapterMethods.hasMethod('setGpioManager'):
                needGPIO = True
            
            if adapterMethods.hasMethod('setSPIManager'):
                needSPI = True

            if adapterMethods.hasMethod('setI2CManager'):
                needI2C = True
            
            if adapterMethods.hasMethod('setDMAManager'):
                needDMA = True
            
        self.managers = []    
        
        if needGPIO:
            self.gpioManager = configuration.GPIOManager( lib=gpioLib )
            self.managers.append(self.gpioManager)
            # forceSimulation = self.gpioManager.getSimulation()
    
        if needSPI:
            self.spiManager = SPIManager()
            self.managers.append(self.spiManager)
        
        if needI2C:
            self.i2cManager = I2CManager()
            self.managers.append(self.i2cManager)

        if needDMA:
            import dma.manager
            self.dmaManager = dma.manager.DMAManager()
            self.managers.append(self.dmaManager)
        # -----------------
        for m in self.managers:
            m.setActive(True)
        # -----------------        
        if forceActive:
            logger.info("forceActive, switch to active Mode, disabled")
            pass
        else:
            #if forceSimulation:
            #    for module in self.config.getAdapters():
            #        module.setSimulation()
            pass
        # ------------------------------------------
        # Assign the managers for hardware resources to the adapters.
        # There are adapters needing more than one manager.
        #
        # the code here ensures that the various adapter types 
        # use only one instance ofs the managers
        # could be solved by static methods and singleton instances,
        # but I could not find a solution for this.
        #
        
        for module in self.config.getAdapters():
            adapterMethods = configuration.AdapterMethods (module)

            if adapterMethods.hasMethod('setGpioManager'):
                module.setGpioManager(self.gpioManager)
            
            if adapterMethods.hasMethod('setSPIManager'):
                module.setSPIManager(self.spiManager)
            
            if adapterMethods.hasMethod('setI2CManager'):
                module.setI2CManager(self.i2cManager)
            
            if adapterMethods.hasMethod('setDMAManager'):
                module.setDMAManager(self.dmaManager)
            
        # -----------------------------------------------
        
        threadManager.append(self)
              
        self.config.configureCommandResolver(self.sender)

        if debug:
            publishSubscribe.Pub.report()
            

        # removed SIGKILL     
        signals = ("SIGINT", "SIGTERM", "SIGHUP")    
        for sig in signals:
            try:
                # on windows, not all signals available
                s = eval("signal." + sig)
                signal.signal(s, self.sigHandler)
            except AttributeError as e:
                logger.warning('AttributeError: setting signals {signal:s}: {exception:s} '.format(signal=sig, exception=str(e) ) )
                pass
            except RuntimeError as e:
                logger.debug('RuntimeError: setting signals {signal:s}: {exception:s} '.format(signal=sig, exception=str(e) ) )
                pass
        self.start()
        self.event_connect()

    def stop(self):
        self._stop.set()

    def stopped(self):
        return self._stop.isSet()
        
    def run(self):
        logger.debug("ScratchClient thread started")

        while not(self.stopped()):
            try:
                s = self.myQueue.get(True, 0.1)
            except helper.abstractQueue.AbstractQueue.Empty:
                if self.stopped():
                    break
                continue
            if s == 'disconnect':
                #print("disconnect received")
                self._disconnect()
            if s == 'connect':
                #print("connect received")
                self._connect()
        logger.debug("ScratchClient thread terminated")
        
    def event_disconnect(self):
        self.myQueue.put('disconnect')
    
    def event_connect(self):
        #print("event connect")
        self.myQueue.put('connect')
        
    def _disconnect(self):    
        logger.info("event_disconnect")
        if self.state == self.STATE_CONNECTED:
            if verbose:
                print ("Scratch disconnected")

            logger.info("set adapters inactive")
            for module in self.config.getAdapters():
                module.setActive(False)
            # self.gpioManager.setActive(False)
            threadManager.cleanup_socket()
            self.state = self.STATE_DISCONNECTED
            self.event_connect()
            
    def _connect(self):
        logger.info("event_connect")
        if self.state == self.STATE_DISCONNECTED:
            # open the socket
            if debug:
                logger.debug( 'Starting to connect...')
            the_socket = self.create_socket(host, port)
            
            if the_socket == None:
                # print("no socket")
                logger.error("no socket")
                return
            
            with helper.logging.LoggingContext(logger, level=logging.DEBUG):
                logger.info('Connected to Scratch !')
            
            the_socket.settimeout(SOCKET_TIMEOUT)

            listener = ScratchListener(the_socket)
            self.sender.setSocket(the_socket)
            
            threadManager.append_socket(listener)
            #threadManager.append_socket(sender)
            #
            for module in self.config.getAdapters():
                module.setActive(True)
            #print("listener starting")
            
            if logger.isEnabledFor(logging.INFO):
                logger.info ("Running....")
            listener.start()
            # self.sender.start()
         
            self.state = self.STATE_CONNECTED
                   
    
    def create_socket(self, host, port):
        scratch_sock = None
        # count is used to limit the number of log messages on console
        count = 0
        while not( self.stopped() ):
        
            try:
                if count == 0:
                    logger.info( 'Trying to connect to scratch.' )
                scratch_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                scratch_sock.connect((host, int(port)))
                break
            except socket.error:
                scratch_sock = None
                if count == 0:
                    logger.warn( "There was an error connecting to Scratch!" )
                    # in german for the kids in school:
                    logger.warn( "  Unterstützung für Netzwerksensoren einschalten!" )
                    logger.warn( "  Activate remote sensor connections!" )
                    logger.info( "  No Mesh session at host: %s, port: %s" , host, port) 
                for _ in range(0,50):
                    if self.stopped():
                        break
                    time.sleep(0.1)
    
                count += 1
                count %= 40
        return scratch_sock

    def sigHandler(self, signum, frame):
        if logger.isEnabledFor(logging.WARN):
            logger.warn ("received signal %s", str(signum))
        # self.runIt = False
        self.stop()

        for adapter in self.config.getAdapters():
            #
            # kindly ask the adapters to terminate
            #
            if adapter.isActive():
                adapter.setActive(False)
            #    
            # stop those adapters not bound to active/inactive    
            # TODO: let the adapters register automatically on thread manager.
            adapter.stop()

        for m in self.managers:
            m.setActive(False)
            
        threadManager.cleanup_socket()
        threadManager.cleanup_threads()

        global runIt
        runIt = False

#
# Singleton things, use a PID-File
#
def _createPidFile(pidFileName, osPid ):
    pfn = modulePathHandler.getScratchClientBaseRelativePath(pidFileName)
    
    pidfile = open(pfn, "w")
    pidfile.write(str(osPid))
    pidfile.close()
    # make the file public, just in case it is generated by root, but next run of sw is user. 
    try:
        if sys.platform.startswith('linux'):
            groupinfo = grp.getgrnam('users')
            gid = groupinfo[2]
            os.chown(pfn, -1, gid )
    except Exception as e:
        logger.error("could not open PID File for chown {file:s} {e:s}".format(file=pfn, e=str(e)))
    try:
        os.chmod(pfn, 0666 )
    except Exception as e:
        logger.error("could not open PID File for chmod {file:s} {e:s}".format(file=pfn, e=str(e)))
        
    pass
    
def _existPidFile(pidFileName ):
    return os.path.exists( modulePathHandler.getScratchClientBaseRelativePath(pidFileName) )

def _readPidFile(pidFileName ):
    pidfile = open(modulePathHandler.getScratchClientBaseRelativePath(pidFileName),"r")
    pidString = pidfile.read()
    pidfile.close()
    return pidString

def forceSingleton():
    
    osPid = os.getpid()
    if not _existPidFile(pidFileName) :
        _createPidFile(pidFileName, osPid)
        return

    pidString = _readPidFile(pidFileName)
    
    #
    # some strange format in file, ignore
    #
    if not re.match('[0-9]+', pidString):
        _createPidFile((pidFileName), osPid)
        return
        

    if pidString == str(osPid):
        # something is real weird
        logger.error('quit program, forceSingleton found condition: os.pid == content current pid file')
        logger.error('try deleting pid file {name:s}'.format(name= pidFileName))
        sys.exit(19)
    else:
        if len(os.popen('ps %s' % pidString).read().split('\n')) > 2:
            logger.error(os.popen('ps %s' % pidString).read())
            
            logger.error('quit program, forceSingleton found running process, pid {pid:s}'.format(pid=pidString))
            sys.exit(20)
        else:
            logger.warning('forceSingleton: the previous server must have crashed' )

        _createPidFile((pidFileName), osPid)
            
def cleanSingleton():
    if os.path.exists(modulePathHandler.getScratchClientBaseRelativePath(pidFileName)):
        os.remove( modulePathHandler.getScratchClientBaseRelativePath(pidFileName))

class ModulePathHandler:
    modulePath = None
    moduleDir = None
    
    def __init__(self):
        _cwd = os.getcwd()
        _file = __file__
        
        if sys.platform == "linux" or sys.platform == "linux2":
            # linux
            self.modulePath = _cwd + '/' + _file
            self.moduleDir =  os.path.split(self.modulePath)[0]
            
        elif sys.platform == "darwin":
            # MAC OS X
            self.modulePath = _cwd + '/' + _file
            self.moduleDir =  os.path.split(self.modulePath)[0]
            
        elif sys.platform == "win32":
            # Windows
            self.modulePath = _file
            self.moduleDir =  os.path.split(self.modulePath)[0]
            
        
    def getModulePath(self):
        return self.modulePath
    
    def getModuleDir(self):
        return self.moduleDir
    #
    # specific methods for scratchClient
    #
    def getScratchClientBaseDir(self):
        return os.path.join( sys.path[0], '..')
    
    def getScratchClientBaseRelativePath(self, relPath):
        return os.path.normpath( 
                                os.path.join( self.getScratchClientBaseDir(), relPath )
                                )
        
modulePathHandler = ModulePathHandler()
scratchClient = None
logger = None
    
if __name__ == '__main__':
    
    configLookupStrategy = True
    
    try:
        i = 1
        while i <  len (sys.argv):
            if '-host' == sys.argv[i]:
                host = sys.argv[i+1]
                i += 1
                
            elif '-port' == sys.argv[i]:
                port = sys.argv[i+1]
                i += 1
            
            elif ( '-config' == sys.argv[i] ) or ( '-c' == sys.argv[i] ):
                configLookupStrategy = True
                configFileName = sys.argv[i+1]
                i += 1
            
            elif ( '-C' == sys.argv[i] ):
                configLookupStrategy = False
                configFileName = sys.argv[i+1]
                i += 1
            
            elif '-gpioLib' == sys.argv[i]:
                print('gpioLib is deprecated, ignored')
                # gpioLib = sys.argv[i+1]
                i += 1
            
            elif '-v' == sys.argv[i]:
                verbose = True
            
            elif '-d' == sys.argv[i]:
                debug = True
            
            elif '-nogui' == sys.argv[i]:
                nogui = True
            
            elif '-guiRemote' == sys.argv[i]:
                guiRemote = True
            
            elif '-help' == sys.argv[i]:
                print(commandlineHelp)
                sys.exit(1)
            elif '-h' == sys.argv[i]:
                print(commandlineHelp)
                sys.exit(1)
            
            elif '-license' == sys.argv[i]:
                print(gpl2)
                sys.exit(1)
            
            elif '-changes' == sys.argv[i]:
                for x in changes:
                    print(x)
                sys.exit(1)
            elif '-forceActive' == sys.argv[i]:
                forceActive = True
            elif '-validate' == sys.argv[i]:
                validate = True
            else:
                print("Command line error, unknown switch", sys.argv[i])    
            i += 1
                                   
    except Exception:
        print(commandlineHelp)
        sys.exit(1)
    
    lFile = ''    
    if debug == True:
        lFile = 'logging/logging_debug.conf'
    elif verbose == True:
        lFile = 'logging/logging_verbose.conf'
    else:        
        lFile = 'logging/logging.conf'
    
    
    print(modulePathHandler.getScratchClientBaseRelativePath(lFile))
            
    logging.config.fileConfig( modulePathHandler.getScratchClientBaseRelativePath(lFile) )
    
    logger = logging.getLogger(__name__)
    
    logging.debug("create ScratchClient")
    # ----------------------------------------------------------
    # look for config files
    #
    cFileFound = False
    cFile = configFileName
    
    if configLookupStrategy:
        #
        # add '.xml' if not available
        #
        if not (cFile.endswith( '.xml' )):
            cFile += '.xml'
            logger.debug("config file: add xml extension")
        #
        # take it literally
        #
        if not( cFileFound):
            pathConfigFile =   modulePathHandler.getScratchClientBaseRelativePath( cFile )
            cFileFound = os.path.isfile( pathConfigFile )
        #
        # look in config/
        #
        if not( cFileFound):
            pathConfigFile =  modulePathHandler.getScratchClientBaseRelativePath( 'config/' + cFile )
            cFileFound = os.path.isfile( pathConfigFile )
            if cFileFound:
                logger.info("config file found: "+ pathConfigFile)
        
        #
        # look in ../config/
        #
        if not( cFileFound):
            pathConfigFile =  modulePathHandler.getScratchClientBaseRelativePath( '../config/' + cFile )
            cFileFound = os.path.isfile( pathConfigFile )
            if cFileFound:
                logger.info("config file found: "+ pathConfigFile)
        #
        # check if a 'config_'-prefix is missing. Do this only when no path is given.
        #
        if cFile.find('/') < 0:
            if not( configFileName.startswith('config_')):
     
                if not( cFileFound):
                    pathConfigFile = modulePathHandler.getScratchClientBaseRelativePath('config_' + cFile)
                    cFileFound = os.path.isfile( pathConfigFile ) 
                    if cFileFound:
                        logger.debug("config file: add prefix 'config_'")   
                        logger.info("config file found: "+ pathConfigFile)
                    
                if not( cFileFound):
                    pathConfigFile =  modulePathHandler.getScratchClientBaseRelativePath('config/' + 'config_' + cFile )
                    cFileFound = os.path.isfile( pathConfigFile )
                    if cFileFound:
                        logger.debug("config file: add prefix 'config_'") 
                        logger.info("config file found: "+ pathConfigFile)
                    
                if not( cFileFound):
                    pathConfigFile =  modulePathHandler.getScratchClientBaseRelativePath('../config/' + 'config_' + cFile)
                    cFileFound = os.path.isfile( pathConfigFile )
                    if cFileFound:
                        logger.debug("config file: add prefix 'config_'") 
                        logger.info("config file found: "+ pathConfigFile)
    
        #
        # if not found, go back to what was defined on command line.
        #
        if cFileFound:
            configFileName = pathConfigFile
        else:
            configFileName = cFile
    # ----------------------------------------------------------
   
    
    forceSingleton()
    
    logger.debug("sys.path    = {p:s}".format(p=sys.path))
    # print('start scratch client')
    
    scratchClient = ScratchClient()
    
    nWTR = 0
    while runIt:
        if nWTR % 20000 == 0:
            logger.debug("still running")
        time.sleep(0.1)
        nWTR += 1
    time.sleep(0.1)
    
    # for debugging purpose, list out not yet terminated threads.
    # MainThread counts as 1, so list only if activeCount > 1
    # 
    cnt = 0    
    while cnt < 2 and threading.activeCount() > 1:
        cnt += 1
        for t in threading.enumerate():
            print("active threads ", t)
        print("")
        time.sleep(3)
        
    cleanSingleton ()
    print("scratchClient terminated")
    logger.debug("main ended")
    
    quit()
    