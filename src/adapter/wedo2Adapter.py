# -*- coding: utf-8 -*-
    # --------------------------------------------------------------------------------------------
    # Copyright (C) 2016  Gerhard Hepp
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
    import bluepy
except ImportError:
    exit("""
    This library requires the bluepy module
      Install with: 
           sudo pip install bluepy
           sudo pip3 install bluepy
    """)
    
import array
import struct
import math
import sys
import time
import traceback
import threading
import helper.abstractQueue

import logging
logger = logging.getLogger(__name__)


debug_notify = False
debug_data = False
debug_trace_service_updates = False
debug_slow = False
bluepy.btle.Debugging = False

# --------------------------------------------------------------------------------------
    
    
class ServiceBroker:
    
    def __init__(self):
        self.registry = []
        self.listener = helper.abstractQueue.AbstractQueue()
    
    def register(self, names, implementation):
        logger.info("register names {n:s}".format(n=str(names)))
        for name in names:
            self.registry.append( (name, implementation) )
            
    def deregister(self, names, implementation):
        for name in names:
            for nameService in self.registry:
                if name == nameService[0] and implementation == nameService[1]:
                    del nameService
                    break
                 
    def registry_get(self, name):
        services = []
        for nameService in self.registry:
            if name == nameService[0]:
                services.append( nameService[1])
        return services
        
                
    def motor_run(self, direction, power):
        services = self.registry_get('motor')
        for service in services:
            service.run(direction, power)

    def motor1_run(self, direction, power):
        services = self.registry_get('motor1')
        for service in services:
            service.run(direction, power)
    
    def motor2_run(self, direction, power):
        services = self.registry_get('motor2')
        for service in services:
            service.run(direction, power)
            
    def motor_brake(self):
        services = self.registry_get('motor')
        for service in services:
            service.brake()
            
    def motor1_brake(self):
        services = self.registry_get('motor1')
        for service in services:
            service.brake()
            
    def motor2_brake(self):
        services = self.registry_get('motor2')
        for service in services:
            service.brake()

    def motor_drift(self):
        services = self.registry_get('motor')
        for service in services:
            service.drift()
    
    def motor1_drift(self):
        services = self.registry_get('motor1')
        for service in services:
            service.drift()
    
    def motor2_drift(self):
        services = self.registry_get('motor2')
        for service in services:
            service.drift()
    
    
    def rgblight_setColor(self, red, green, blue):
        services = self.registry_get('rgblight')
        for service in services:
            service.setColor(red, green, blue)
            
    def rgblight_setColorIndex(self, index):
        services = self.registry_get('rgblight')
        for service in services:
            service.setColorIndex(index)
    
    def piezotoneplayer_playFrequency(self, frequency, duration):
        services = self.registry_get('piezotoneplayer')
        for service in services:
            service.playFrequency(frequency, duration)
            
    def piezotoneplayer_stopPlaying(self):
        services = self.registry_get('piezotoneplayer')
        for service in services:
            service.stopPlaying()
            
    voltage = 0.0   
    def set_voltage(self, voltage):
        """there is only one voltage sensor"""
        self.voltage = voltage
        
    def get_voltage(self):
        """there is only one voltage sensor"""
        return self.voltage
    
    current = 0.0   
    def set_current(self, current):
        """there is only one voltage sensor"""
        self.current = current
        
    def get_current(self):
        """there is only one voltage sensor"""
        return self.current
        
    def set_motion_count(self, value, port):
        """TODO: there is a motion_1 and motion_2, should be handled"""
        self.motion_count = value
        self.listener.put({ 'name': 'motion{p:d}_count'.format(p=port), 'value': value})      
        
    def set_motion_distance(self, value, port):
        """TODO: there is a motion_1 and motion_2, should be handled"""
        self.motion_distance = value
        self.listener.put({ 'name': 'motion{p:d}_distance'.format(p=port), 'value': value})      
        
             
    def set_tilt_crash(self, values, port):
        self.tilt_crash = values
        self.listener.put({ 'name': 'tilt{p:d}_crash'.format(p=port), 'values': values})      

    def set_tilt_angle(self, values, port):
        self.tilt_angle = values
        self.listener.put({ 'name': 'tilt{p:d}_angle'.format(p=port), 'values': values})      
    
    def set_tilt_tilt(self, values, port):
        self.tilt_crash = values
        self.listener.put({ 'name': 'tilt{p:d}_tilt'.format(p=port), 'values': values})      
            
    def set_button_state(self, value):
        self.listener.put({ 'name': 'button_state', 'value': value})      
    
    def low_voltage_alert (self, value):
        self.listener.put({ 'name': 'low_voltage_alert', 'value': value})      
        
    def high_current_alert (self, value ):
        self.listener.put({ 'name': 'high_current_alert', 'value': value})
              
    def motion_reset(self):
        """there is a motion_1 and motion_2, should be handled"""
        services = self.registry_get('motion')
        if debug_trace_service_updates:
            print("motion_reset", services)
        for service in services:
            service.resetState()
            
    def motion1_reset(self):
        """there is a motion_1 and motion_2, should be handled"""
        services = self.registry_get('motion1')
        if debug_trace_service_updates:
            print("motion_reset", services)
        for service in services:
            service.resetState()
            
    def motion2_reset(self):
        """there is a motion_1 and motion_2, should be handled"""
        services = self.registry_get('motion2')
        if debug_trace_service_updates:
            print("motion_reset", services)
        for service in services:
            service.resetState()
            
    def tilt_reset(self):
        """there is a motion_1 and motion_2, should be handled"""
        services = self.registry_get('tilt')
        if debug_trace_service_updates:
            print("tilt_reset", services)
        for service in services:
            service.resetState()
            
    def tilt1_reset(self):
        """there is a motion_1 and motion_2, should be handled"""
        services = self.registry_get('tilt1')
        if debug_trace_service_updates:
            print("tilt1_reset", services)
        for service in services:
            service.resetState()
            
    def tilt2_reset(self):
        """there is a motion_1 and motion_2, should be handled"""
        services = self.registry_get('tilt2')
        if debug_trace_service_updates:
            print("tilt2_reset", services)
        for service in services:
            service.resetState()
            
            
    def set_motion_Mode(self, value):
        """there is a motion_1 and motion_2, should be handled"""
        services = self.registry_get('motion')
        if debug_trace_service_updates:
            print("set_motion_Mode", services)
        for service in services:
            service.setMode(value)
        
    def set_motion1_Mode(self, value):
        """there is a motion_1 and motion_2, should be handled"""
        services = self.registry_get('motion1')
        if debug_trace_service_updates:
            print("set_motion1_Mode", services)
        for service in services:
            service.setMode(value)
        
    def set_motion2_Mode(self, value):
        """there is a motion_1 and motion_2, should be handled"""
        services = self.registry_get('motion2')
        if debug_trace_service_updates:
            print("set_motion2_Mode", services)
        for service in services:
            service.setMode(value)
        
    def set_tilt_Mode(self, value):
        """there is a motion_1 and motion_2, should be handled"""
        services = self.registry_get('tilt')
        if debug_trace_service_updates:
            print("set_tilt_Mode", services)
        for service in services:
            service.setMode(value)
        
    def set_tilt1_Mode(self, value):
        """there is a motion_1 and motion_2, should be handled"""
        services = self.registry_get('tilt1')
        if debug_trace_service_updates:
            print("set_tilt1_Mode", services)
        for service in services:
            service.setMode(value)
        
    def set_tilt2_Mode(self, value):
        """TODO: there is a motion_1 and motion_2, should be handled"""
        services = self.registry_get('tilt2')
        if debug_trace_service_updates:
            print("set_tilt2_Mode", services)
        for service in services:
            service.setMode(value)
        
class IOType:
    IO_TYPE_MOTOR = 1
    IO_TYPE_VOLTAGE = 20
    IO_TYPE_CURRENT = 21
    IO_TYPE_PIEZO_TONE_PLAYER = 22
    IO_TYPE_RGB_LIGHT = 23
    IO_TYPE_TILT_SENSOR = 34
    IO_TYPE_MOTION_SENSOR = 35
    IO_TYPE_GENERIC = 0
    
    def __init__(self, i):
        self.fromInt(i)
    
    def fromInt(self, i):
        self.ioType = i

        if i ==  self.IO_TYPE_MOTOR:
            self.type='IO_TYPE_MOTOR'
            
        elif i ==  self.IO_TYPE_VOLTAGE:
            self.type='IO_TYPE_VOLTAGE'
            
        elif i ==  self.IO_TYPE_CURRENT:
            self.type='IO_TYPE_CURRENT'
            
        elif i ==  self.IO_TYPE_PIEZO_TONE_PLAYER:
            self.type='IO_TYPE_PIEZO_TONE_PLAYER'
            
        elif i ==  self.IO_TYPE_RGB_LIGHT:
            self.type='IO_TYPE_RGB_LIGHT'
            
        elif i ==  self.IO_TYPE_TILT_SENSOR:
            self.type='IO_TYPE_TILT_SENSOR'
            
        elif i == self.IO_TYPE_MOTION_SENSOR:
            self.type='IO_TYPE_MOTION_SENSOR'
            
        else:
            self.type='IO_TYPE_GENERIC'
    
    def __str__(self):
        return self.type
            
class Revision:
    def __init__(self, revisionData):
        """data are an array of 4
        self.majorVersion 
        self.minorVersion 
        self.bugFixVersion 
        self.buildNumber 
       """
        self.revisionData= revisionData
        
        if len(revisionData) >= 1:
            self.majorVersion = revisionData[0]
        if len(revisionData) >= 2:
            self.minorVersion = revisionData[1]
        if len(revisionData) >= 3:
            self.bugFixVersion = revisionData[2]
        if len(revisionData) >= 4:
            self.buildNumber = revisionData[3]
           
    def __str__(self):
        s = ''
        sep = ''
        for x in self.revisionData:
            s += sep + "{v:d}".format(v=x)
            sep = '.'     
        return s
         
class ConnectInfo:
    def __init__(self, bdata):
            
        self.port = bdata[0]
        self.used = bdata[1] == 1 
        if self.used:
            self.hubIndex = bdata[2]
            self.ioType = IOType(  bdata[3] )
            self.hwRevision = Revision( bdata[4:8]) 
            self.fwRevision =  Revision( bdata[8:12]) 
            
    def __str__(self):
        s0 = 'port {p:d} used = {u:s}'.format(p=self.port, u = str(self.used))
        if not self.used:
            return s0
        
        s1 = 'hubIndex = {h:d}, [{i:d}] {t:s}, {hw:s} {fw:s} '.format( h=self.hubIndex, i= self.ioType.ioType, t=str(self.ioType), hw=str(self.hwRevision), fw=str(self.fwRevision) )
        return s0 + ' ' + s1

    def isUsed(self):
        return self.used
    
class MyUUID (bluepy.btle.UUID):
    UUID_CUSTOM_BASE    = "1212-EFDE-1523-785FEABCD123";
    UUID_STANDARD_BASE  = "0000-1000-8000-00805f9b34fb";

    def __init__(self, val, base):
        bluepy.btle.UUID.__init__(self, '0000' + val + '-' + base)
        
class MyDelegate(bluepy.btle.DefaultDelegate):
    
    def __init__(self, params):
        bluepy.btle.DefaultDelegate.__init__(self)
        # ... initialise here
        self.registeredNotifications = {}
        
    def handleNotification(self, cHandle, data):
        name = ''
        
        if cHandle in self.registeredNotifications:
            name = self.registeredNotifications[cHandle]['name']
            # target is the 'self' value from the object called
            targetFunction = self.registeredNotifications[cHandle]['targetFunction'] 

            if debug_notify:
                print('notify: {n:12s}: handle {h:04x} data {d:s}'. format(n=name, h=cHandle, d =self._hexString(data) ) )
            #help(targetFunction)
            bdata = array.array('B')
            if sys.version_info.major == 2:
                for x in data:
                    bdata.append( ord(x) )
            if sys.version_info.major == 3:
                bdata = data
                
            targetFunction( cHandle, name, bdata)
        
        else:
            logger.error("unexpected notification on handle {h:d} data {d:s}".format(h=cHandle, d =self._hexData(data)))
                    
    def _hexString(self, d):
        s = ''
        for x in d:
            s += "{x:02x} ".format(x=ord(x) )
        return s
    def _hexData( self, d):
        s = ''
        for x in d:
            s += "{x:02x} ".format(x=x )
        return s
    
    def registerCallback(self, cHandle, name, target, targetFunction):
        logger.debug("MyDelegate registerCallback handle {}, name {} target {} targetFunction {}".format(cHandle, name, target, targetFunction) )
        self.registeredNotifications[cHandle] = { 'name':name, 'target' : target, 'targetFunction' : targetFunction}

class ServiceAdapterFactory:
    
    @staticmethod
    def createService( connectInfo, parent):
        service = None
        
        if connectInfo.ioType.ioType == IOType.IO_TYPE_CURRENT:
            service = CurrentSensor(connectInfo, parent)
            
        elif connectInfo.ioType.ioType == IOType.IO_TYPE_MOTOR:
            service = Motor(connectInfo, parent)
            
        elif connectInfo.ioType.ioType == IOType.IO_TYPE_PIEZO_TONE_PLAYER:
            service = PiezoTonePlayer(connectInfo, parent)
            
        elif connectInfo.ioType.ioType == IOType.IO_TYPE_RGB_LIGHT:
            service = RGBLight(connectInfo, parent)
            
        elif connectInfo.ioType.ioType == IOType.IO_TYPE_VOLTAGE:
            service = VoltageSensor(connectInfo, parent)
            
        elif connectInfo.ioType.ioType == IOType.IO_TYPE_MOTION_SENSOR:
            service = MotionSensor(connectInfo, parent)
            
        elif connectInfo.ioType.ioType == IOType.IO_TYPE_TILT_SENSOR:
            service = TiltSensor(connectInfo, parent)
            
        else:
            logger.error("no implementation for {}".format( connectInfo.ioType.ioType) )
        return service
    
class BHelper:
    def __init__(self, peripheral):
        self.peripheral = peripheral
        self.queue = helper.abstractQueue.AbstractQueue()
        self._invokeStart()
  
    def _invokeStart(self):
        self._stopEvent = threading.Event()

        self.thread = threading.Thread(target=self._invoke)
        self.thread.setName('BHelper')
        self._stopEvent.clear()
        self.thread.start()
 
    def stop(self):
        """stop adapter thread. It is the thread's responsibility to timely shut down"""
        self._stopEvent.set()
        
    def _invokeLater(self, **args):
        # print("_invokeLater {d:s}".format( d = str(args) ))
        d = args
        self.queue.put(d) 
    
    def _invoke(self):
        if debug_slow:
            logger.debug("BHelper._invoke 5")
            time.sleep(1)
            logger.debug("BHelper._invoke 4")
            time.sleep(1)
            logger.debug("BHelper._invoke 3")
            time.sleep(1)
            logger.debug("BHelper._invoke 2")
            time.sleep(1)
            logger.debug("BHelper._invoke 1")
            time.sleep(1)
        logger.info("BHelper._invoke running")
        
        while not self._stopEvent.isSet():
            
            if debug_slow:
                time.sleep(0.2)
            try:
                d = self.queue.get(block=False)
                logger.info("queue read, got entry")
                
                if d['op'] == 'write':
                    logger.info("queue read: {} {}".format( d['op'], d))
                    svc = self.peripheral.getServiceByUUID( d['service'] )
                    if debug_slow: time.sleep(1)
                    
                    ch = svc.getCharacteristics( d['characteristic'] )[0]
                    if debug_slow: time.sleep(1)
                    data = d['data']
                    logger.info("_invoke, write {} {}".format( ch.getHandle(), _hexData(data)))
                    ch.write(data)
                    
                elif d['op'] == 'writeHandle':
                    logger.info("queue read: {} {} ".format( d['op'], d))
                    self.peripheral.writeCharacteristic( d['handle'], d['data'] )
                    
                elif d['op'] == 'notification':
                    logger.info("queue read: {} {} ".format( d['op'], d) )
                    svc = self.peripheral.getServiceByUUID( d['service'] )
                    logger.info("svc {}".format( svc))
                    if debug_slow: time.sleep(1)
                    
                    ch = svc.getCharacteristics( d['characteristic'] )[0]
                    logger.info("ch {}".format( ch))
                    if debug_slow: time.sleep(1)
                    
                    logger.info("delegate.registerCallback")
                    delegate =  d['delegate']
                    name =  d['name']
                    target =  d['target']
                    targetFunction =  d['targetFunction']
                    delegate.registerCallback( ch.getHandle(), name, target, targetFunction)
                    logger.info("register completed ")
                    
                    data = d['data']
                    logger.info("_invoke, write {} {}".format( ch.getHandle()+1, _hexData(data) ))
                    self.peripheral.writeCharacteristic( ch.getHandle()+1, data )
                    logger.info("write complete ")
                    
                elif d['op'] == 'callback':
                    logger.info("queue read: {} {} ".format( d['op'], d) )
                  
                    targetFunction =  d['targetFunction']
                    targetFunction()
                
                elif d['op'] == 'read':
                    logger.info("queue read: {} {} ".format( d['op'], d) )
                    
                    svc = self.peripheral.getServiceByUUID( d['service'] )
                    chs = svc.getCharacteristics( d['characteristic'] )
                    ch = chs[0]
                    value = ch.read(  )
                    logger.info(value)
                    targetFunction =  d['targetFunction']
                    targetFunction ( value)
                    # print("read complete")
                    
                elif d['op'] == 'delay': 
                    self.peripheral.waitForNotifications(d['data'])   
                    
                else:
                    logger.error("_invoke, op unknown {} {}".format( d['op'], d ))
               
                    
            except helper.abstractQueue.AbstractQueue.Empty:
                self.peripheral.waitForNotifications(0.05)
                
            except Exception as e:
                logger.error("_invoke_ {}".format( e) )
                traceback.print_exc()
 

class BaseService:
    def __init__(self, name):
        self.name = name
        pass
    
    def queueDelay(self, data):
        logger.info("{}.queueDelay  {}".format( self.name, data) )
        self.bHelper._invokeLater(
                             op='delay', 
                             data= data)
        
    def writeInputFormat(self, data):
        logger.info("{}.writeInputFormat  {}".format( self.name, data) )
        self.bHelper._invokeLater(
                             op='write', 
                             service=self.INPUT_SERVICE_UUID, 
                             characteristic=self.CHARACTERISTIC_INPUT_COMMAND_UUID, 
                             data= data)
    
    def writeCommand(self, data):
        logger.info("{}.writeCommand {}".format( self.name, data) )
        self.bHelper._invokeLater(
                             op='write', 
                             service=self.INPUT_SERVICE_UUID, 
                             characteristic=self.CHARACTERISTIC_OUTPUT_COMMAND_UUID, 
                             data=data)
    
    def init_notification(self, name, service, charac, target, targetFunction):
        logger.info("{}.init_notification {}".format( self.name, name) )
        
        setup_data = array.array('B')
        setup_data.append(1)
        setup_data.append(0)
        
        # self.peripheral.writeCharacteristic( ch.getHandle()+1, setup_data )
        self.bHelper._invokeLater(op='notification', 
                                  service=service, 
                                  characteristic=charac, 
                                  data= setup_data,
                                  delegate =  self.delegate,
                                  name =  name,
                                  target =  target,
                                  targetFunction =  targetFunction
                                  )
        
    def init_callback(self, name,targetFunction):
        logger.info("{}.init_callback {}".format( self.name, name) )
        
        # self.peripheral.writeCharacteristic( ch.getHandle()+1, setup_data )
        self.bHelper._invokeLater(op='callback', 
                                  targetFunction =  targetFunction
                                  )
        
    def readCharacteristic(self, service, charac, targetFunction):
        logger.info("{}.readCharacteristic {} {}".format( self.name, service, charac) )
        self.bHelper._invokeLater(op='read', 
                                  service=service, 
                                  characteristic=charac, 
                                  targetFunction =  targetFunction
                                  )
    
class LegoService(BaseService):
    INPUT_SERVICE_UUID = MyUUID("4F0E", MyUUID.UUID_CUSTOM_BASE )

    CHARACTERISTIC_INPUT_VALUE_UUID    = MyUUID("1560", MyUUID.UUID_CUSTOM_BASE )
    CHARACTERISTIC_INPUT_FORMAT_UUID   = MyUUID("1561", MyUUID.UUID_CUSTOM_BASE )
    CHARACTERISTIC_INPUT_COMMAND_UUID  = MyUUID("1563", MyUUID.UUID_CUSTOM_BASE )
    CHARACTERISTIC_OUTPUT_COMMAND_UUID = MyUUID("1565", MyUUID.UUID_CUSTOM_BASE )

    def __init__(self, peripheral, delegate, bHelper):
        BaseService.__init__(self, 'LegoService')
        
        self.peripheral = peripheral
        self.delegate = delegate
        self.bHelper = bHelper
        self.registry= {}
        
        # self._invokeStart()
        
        self.init_notification('notify_data',  self.INPUT_SERVICE_UUID, self.CHARACTERISTIC_INPUT_VALUE_UUID, self, self.notify_data )    
        # self.init_notification('notify_format',  self.INPUT_SERVICE_UUID, self.CHARACTERISTIC_INPUT_FORMAT_UUID, self, self.notify_data )    

    def notify_data (self, cHandle, name, data):
        # catch Exceptions, just in case something goes wrong then BT-stack is
        # not working
        try:
            if logger.isEnabledFor(logging.DEBUG):
                print('notify_data', cHandle, 'name', name, 'data', data)
                
            service = self.getService(data[1])
            if service == None:
                logger.error('notify_data, no service for port {p:d}'.format(p= data[1]))
            else:
                service.setData(data)
        except Exception as e:
            logger.error("_invoke_ {}".format( e) )
            traceback.print_exc()

    def notify_attached_io(self, connectInfo):
        
        if logger.isEnabledFor(logging.DEBUG):
            port=connectInfo.port
            try:
                ioType=connectInfo.ioType.ioType 
            except Exception:
                ioType= '<undef>'
            logger.debug('notify_attached_io, iotype={iotype:s} port={port:s}'.format( port=str(port) , iotype=str(ioType) ) )
        
        if connectInfo.isUsed():
            service = ServiceAdapterFactory.createService( connectInfo,self)
            if service == None:
                return
            self.registerService(connectInfo.port, service)
            # print self.registry
            service.setDefaultMode()
            # found it sometimes unreliable to set the formats, so repeat
            service.setDefaultMode()
            
            serviceBroker.register(service.names, service)
            
            # put a callback event for the service to the queue
            # the queue thing is needed as the bluetooth events are in the queue and 
            # animation can be performed only after the init is executed
            self.init_callback ( 'animation', service.animationCallback )
            
        else:
            service = self.getService(connectInfo.port)
            if service == None:
                logger.warn("notify_attached_io, no service to remove on port {}".format(connectInfo.port))
            else:
                serviceBroker.deregister(service.names, service)
                service.destroy()
                self.removeService(connectInfo.port)

    def registerService(self, port, service):
        # print("registerService", port, service)
        self.registry[port] = service
        # print self.registry
    
    def getService(self, port):
        try:
            return self.registry[port] 
        except:
            # print (self.registry)
            return None
        
    def removeService(self, port):
        del self.registry[port]
     
             
class DeviceService(BaseService):
    
    HUB_SERVICE = MyUUID( "1523", MyUUID.UUID_CUSTOM_BASE )
        
    HUB_CHARACTERISTIC_NAME = MyUUID( "1524", MyUUID.UUID_CUSTOM_BASE )
    HUB_CHARACTERISTIC_COLOR = MyUUID( "1525", MyUUID.UUID_CUSTOM_BASE )
    HUB_CHARACTERISTIC_BUTTON_STATE = MyUUID( "1526", MyUUID.UUID_CUSTOM_BASE )
    HUB_CHARACTERISTIC_ATTACHED_IO = MyUUID( "1527", MyUUID.UUID_CUSTOM_BASE )
    HUB_CHARACTERISTIC_LOW_VOLTAGE_ALERT = MyUUID( "1528", MyUUID.UUID_CUSTOM_BASE )
    HUB_CHARACTERISTIC_HIGH_CURRENT_ALERT = MyUUID( "1529", MyUUID.UUID_CUSTOM_BASE )
    
    DEVICE_INFORMATION = MyUUID( "180A", MyUUID.UUID_STANDARD_BASE )
    DEVICE_INFORMATION_FIRMWARE = MyUUID( "2A26", MyUUID.UUID_STANDARD_BASE )
    DEVICE_INFORMATION_SOFTWARE = MyUUID( "2A28", MyUUID.UUID_STANDARD_BASE )
    DEVICE_INFORMATION_MANUFACTURER = MyUUID( "2A29", MyUUID.UUID_STANDARD_BASE )
    
    deviceName = None
    buttonState = None
    
    def __init__(self, peripheral, delegate, legoService, bHelper):
        BaseService.__init__(self, 'DeviceService')

        self.peripheral = peripheral
        self.delegate = delegate
        self.legoService = legoService
        self.bHelper = bHelper
        
        if True: 
            self.readCharacteristic(self.HUB_SERVICE, self.HUB_CHARACTERISTIC_NAME, self.setDeviceName)
            #
            # for debug purpose only
            self.readCharacteristic(self.DEVICE_INFORMATION, self.DEVICE_INFORMATION_FIRMWARE, self.setFirmwareRevision)
            self.readCharacteristic(self.DEVICE_INFORMATION, self.DEVICE_INFORMATION_SOFTWARE, self.setSoftwareRevision)
            self.readCharacteristic(self.DEVICE_INFORMATION, self.DEVICE_INFORMATION_MANUFACTURER, self.setManufacturerName)
        
        self.init_notification('attached_io',        self.HUB_SERVICE, self.HUB_CHARACTERISTIC_ATTACHED_IO,        self, self.notify_attached_io        )    
        # found it sometimes unreliable to set up the formats (voltage sometimes not send)
        # looks as if a small delay here cures the problem.
        self.queueDelay(2.0)    
        self.init_notification('button_state',       self.HUB_SERVICE, self.HUB_CHARACTERISTIC_BUTTON_STATE,       self, self.notify_button_state       )    
        self.init_notification('low_voltage_alert',  self.HUB_SERVICE, self.HUB_CHARACTERISTIC_LOW_VOLTAGE_ALERT , self, self.notify_low_voltage_alert  )    
        self.init_notification('high_current_alert', self.HUB_SERVICE, self.HUB_CHARACTERISTIC_HIGH_CURRENT_ALERT, self, self.notify_high_current_alert )    

    def setDeviceName(self, name):
        logger.info("DeviceService.setDeviceName {}".format(name))
        self.deviceName = name
        
    def setFirmwareRevision(self, name):
        logger.info("DeviceService.setFirmwareRevision {}".format(name))
        self.firmwareRevision = name
        
    def setSoftwareRevision(self, name):
        logger.info("DeviceService.setSoftwareRevision {}".format(name))
        self.softwareRevision = name
        
    def setManufacturerName(self, name):
        logger.info("DeviceService.setManufacturerName {}".format(name))
        self.manufacturerName = name
        
    def notify_attached_io (self, cHandle, name, data):
        # print(name)
        connectInfo = ConnectInfo(data)
        # print( "    connectInfo {p:s}".format(p= str(connectInfo)))
        self.legoService.notify_attached_io(connectInfo)
    
        
    def notify_button_state (self, cHandle, name, data):
        # print(name, data[0])
        self.buttonState = data[0]
        serviceBroker.set_button_state(data[0])
        
    def notify_low_voltage_alert (self, cHandle, name, data):
        logger.error("notify_low_voltage_alert {}".format( name))
        serviceBroker.low_voltage_alert (data[0])
        
    def notify_high_current_alert (self, cHandle, name, data):
        logger.error("notify_high_current_alert {}".format( name) )
        serviceBroker.high_current_alert(data[0])
               
def floatFromArray( data):
    if debug_notify:
        print("data", data, type(data) )
    
    f = struct.unpack_from( "<f", data )
    # print("f", f, type(f))
    # print("f[0]", f[0], type(f[0]))
    return f[0]
                
def uint32FromArray( data):
    if debug_notify:
        print("data", data, type(data) )
    
    f = struct.unpack_from( "<I", data )
    # print("f", f, type(f))
    # print("f[0]", f[0], type(f[0]))
    return f[0]

def uint16FromArray( data):
    if debug_notify:
        print("data", data, type(data) )
    
    f = struct.unpack_from( "<h", data )
    # print("f", f, type(f))
    # print("f[0]", f[0], type(f[0]))
    return f[0]

def uint8FromArray( data):
    if debug_notify:
        print("data", data, type(data) )
    
    f = struct.unpack_from( "<B", data )
    # print("f", f, type(f))
    # print("f[0]", f[0], type(f[0]))
    return f[0]
                

def uint32_le (n):
    s = array.array('B')
    s.append( ( n>>0) & 0xff)
    s.append( ( n>>8) & 0xff)
    s.append( ( n>>16) & 0xff)
    s.append( ( n>>24) & 0xff)
    return s
 
class LegoAdapterService:
    WRITE_DIRECT_ID = 0x05
    
    def __init__(self, connectInfo):
        self.connectInfo = connectInfo
        
    def destroy(self):
        pass
    
    def setDefaultMode(self):
        pass
    
    def setData(self, data):
        pass
        
    def animationCallback(self):
        pass
    
    def resetState(self):
        logger.info("resetState")
        s = array.array('B')
        # port 
        s.append(self.connectInfo.port)
        s.append(self.WRITE_DIRECT_ID)
           
        s.append(3)
        
        s.append( 0x44)
        s.append( 0x11)
        s.append( 0xAA);
        
        self.parent.writeCommand(s) 

    
class RGBLight( LegoAdapterService):
    """ this adapter is run in RGB_LIGHT_MODE_DISCRETE-mode only. 
        color index can be used, but is managed by mapping the index colors into rgb values
        """
    RGB_LIGHT_MODE_DISCRETE = 0
    RGB_LIGHT_MODE_ABSOLUTE = 1
    RGB_LIGHT_MODE_UNDEF = 2
    
    WRITE_RGB_COMMAND_ID = 0x04
    
    RGB_RGB_OFF = (0,0,0)
    RGB_RGB_PINK = (255,192,203)
    RGB_RGB_PURPLE = (128,0,128)
    RGB_RGB_BLUE = (0,0,255)
    RGB_RGB_BLUE_SKY = (135,206,235) # himmelblau
    RGB_RGB_TEAL = (0,255,255) # aquamarin, petrol, seegruen
    RGB_RGB_GREEN = (0,255,0)
    RGB_RGB_YELLOW = (255,255,0)
    RGB_RGB_ORANGE = (255,215,0)
    RGB_RGB_RED = (255,0,0)
    RGB_RGB_WHITE = (255,255,255)

    color_codes = { 0: RGB_RGB_OFF,
                    1: RGB_RGB_PINK,
                    2: RGB_RGB_PURPLE,
                    3: RGB_RGB_BLUE,
                    4: RGB_RGB_BLUE_SKY,
                    5: RGB_RGB_TEAL,
                    6: RGB_RGB_GREEN,
                    7: RGB_RGB_YELLOW,
                    8: RGB_RGB_ORANGE,
                    9: RGB_RGB_RED,
                    10:RGB_RGB_WHITE,
                    }
    names = ['rgblight']
    def __init__(self, connectInfo, parent):
        LegoAdapterService.__init__(self, connectInfo)
        self.mode = self.RGB_LIGHT_MODE_UNDEF
        self.parent = parent
        
    def animationCallback(self):
        self.thread = threading.Thread(target=self._animation)
        self.thread.setName('RGBAnimation')
        self.thread.start()
     
    def _animation(self):
        for _ in range(3):
            # the colr values are choosen to produce quite equal luminosity
            # so green is very effective, red next and blue very bad.
            #
            self.setColor( 0x70, 0, 0 )
            time.sleep(0.4)
               
            self.setColor( 0, 0x20, 0 )
            time.sleep(0.4)
    
            self.setColor( 0, 0, 0xff )
            time.sleep(0.4)

        self.setColor( 0, 0x20, 0 )

    def setColorIndex(self, index):
        logger.debug("setColorIndex {}".format(index))
        if self.mode == self.RGB_LIGHT_MODE_ABSOLUTE:
            if 0 <= index <= 10:
                color = self.color_codes[index]
                self.setColor( color[0], color[1], color[2] )
            return
        
        s = array.array('B')
        # port 
        s.append(self.connectInfo.port)
        # hubIndex
        s.append(self.WRITE_RGB_COMMAND_ID)
        # number of bytes to follow
        s.append( 1 )
        s.append( index )
        self.parent.writeCommand( s)
    
    def setColor(self, red=0, green = 0, blue=0 ):
        if self.mode == self.RGB_LIGHT_MODE_ABSOLUTE:
            s = array.array('B')
            # port 
            s.append(self.connectInfo.port)
            # hubIndex
            s.append(self.WRITE_RGB_COMMAND_ID)
            # number of bytes to follow
            s.append( 3 )
            s.append( red )
            s.append( green )
            s.append( blue )
            
            self.parent.writeCommand(s) 
    
    def setDefaultMode(self):
        self.setMode(self.RGB_LIGHT_MODE_ABSOLUTE)
        
    def setMode(self, mode):
        if self.mode == mode:
            return None
        self.mode = mode
        if mode == self.RGB_LIGHT_MODE_ABSOLUTE:
            s = array.array('B')
            # command = set definition 
            s.append( 1 )
            s.append( 2 )
            # port
            s.append(self.connectInfo.port) 
            # type == RGB_LED
            s.append(self.connectInfo.ioType.ioType)
            
            s.append(self.mode)
            # offset for notifications TODO: what is this
            s.extend( uint32_le ( 1)) 
            # unit used, SI
            s.append( InputFormat.INPUT_FORMAT_UNIT_SI )
            # notification enabled
            s.append( 1 )
            self.parent.writeInputFormat(s) 
        
class Motor( LegoAdapterService):
    MOTOR_MIN_SPEED = 1
    MOTOR_MAX_SPEED = 100
    MOTOR_POWER_DRIFT = 0
    MOTOR_POWER_BRAKE = 127
 
    MOTOR_POWER_OFFSET = 35
 
    MOTOR_DIRECTION_DRIFTING = 0
    MOTOR_DIRECTION_LEFT = 1
    MOTOR_DIRECTION_RIGHT = 2
    MOTOR_DIRECTION_BRAKING = 3
 
    WRITE_MOTOR_POWER_COMMAND_ID = 1

    def __init__(self, connectInfo, parent):
        LegoAdapterService.__init__(self, connectInfo)
        
        self.names = ['motor']
        self.names.append( 'motor{p:d}'.format( p= connectInfo.port ) )
        
        self.mode = 0
        self.parent = parent
        
    def run(self, direction, power):
        
        if direction == self.MOTOR_DIRECTION_DRIFTING:
            self.drift()
            return
        
        elif direction == self.MOTOR_DIRECTION_BRAKING:
            self.brake()
            return

        if power == self.MOTOR_POWER_BRAKE:
            self.brake()
            return
        if power == self.MOTOR_POWER_DRIFT:
            self.drift()
            return
         
        if power > self.MOTOR_MAX_SPEED:
            power = self.MOTOR_MAX_SPEED;
        if direction == self.MOTOR_DIRECTION_LEFT:
            power = -1 * power
 
        offset = 0
        if self.connectInfo.fwRevision.majorVersion >= 1:
            offset = self.MOTOR_POWER_OFFSET
        self.writeMotorPowerOffset(power, offset)
            
    def writeMotorPowerOffset(self, power, offset):
        """ aus BluetoothIO kopiert """
        logger.info("Motor.writeMotorPowerOffset() {} {}".format( power, offset ))

        isPositive = power >= 0
        power =  abs(power)

        actualPower = (100.0 - offset) / 100.0 * power + offset
        actualPower = round(actualPower);
        actualResultInt = int(actualPower);
        
        if not isPositive :
            actualResultInt = -actualResultInt;
        # print ( actualResultInt)
        
        s = array.array('B')
        s.append(self.connectInfo.port)
        # hubIndex
        
        s.append( self. WRITE_MOTOR_POWER_COMMAND_ID )
        # len
        s.append( 1) 
        s.append( actualResultInt & 0xff )
        # print ( actualResultInt,":",  _hexData(s))
        
        self.parent.writeCommand(s) 
        
    def drift(self):
        logger.info("Motor.drift()")
        self.writeMotorPowerOffset(self.MOTOR_POWER_DRIFT, 0)
    
    def brake(self):
        logger.info("Motor.brake()")
        self.writeMotorPowerOffset(self.MOTOR_POWER_BRAKE, 0)
    

class MotionSensor( LegoAdapterService):
    
    #  Detect mode - produces value that reflect the relative distance from the sensor to objects in front of it
    MOTION_SENSOR_MODE_DETECT = 0
    
    # Count mode - produces values that reflect how many times the sensor has been activated 
    MOTION_SENSOR_MODE_COUNT = 1
    
    # Unknown (unsupported) mode
    MOTION_SENSOR_MODE_UNKNOWN = 2
    
    def __init__(self, connectInfo, parent):
        LegoAdapterService.__init__(self, connectInfo)
        
        self.names= [ 'motion', 'motion{p:d}'.format( p= connectInfo.port ) ]

        self.mode = self.MOTION_SENSOR_MODE_UNKNOWN
        self.parent = parent
    
    def setDefaultMode(self):
        self.setMode(self.MOTION_SENSOR_MODE_DETECT)
            
    def setMode(self, mode):
        logger.info("set mode {} --> {}".format(self.mode, mode))
        if self.mode == mode:
            return
        self.mode = mode
        if mode in [ self.MOTION_SENSOR_MODE_DETECT]:
            logger.info("set mode ok " )
            s = array.array('B')
            # command = set definition 
            s.append( 1 )
            s.append( 2 )
            # port
            s.append(self.connectInfo.port) 
            # type == RGB_LED
            s.append(self.connectInfo.ioType.ioType)
            
            s.append(self.mode)
            # offset for notifications 
            s.extend( uint32_le ( 1)) 
            # unit used, SI
            s.append( InputFormat.INPUT_FORMAT_UNIT_SI )
            # notification enabled
            s.append( 1 )
            self.parent.writeInputFormat(s) 
            
        if mode in [  self.MOTION_SENSOR_MODE_COUNT ]:
            logger.info("set mode ok " )
            s = array.array('B')
            # command = set definition 
            s.append( 1 )
            s.append( 2 )
            # port
            s.append(self.connectInfo.port) 
            # type == RGB_LED
            s.append(self.connectInfo.ioType.ioType)
            
            s.append(self.mode)
            # offset for notifications 
            s.extend( uint32_le ( 1)) 
            # unit used, SI
            s.append( InputFormat.INPUT_FORMAT_UNIT_RAW )
            # notification enabled
            s.append( 1 )
            self.parent.writeInputFormat(s) 
    
    def setData(self, data):
        if debug_data:
            logger.error("motion, data {}".format( _hexData(data)))  
        found = False
        if self.mode == self.MOTION_SENSOR_MODE_DETECT:
            if len(data) == 6 :
                v=floatFromArray(data[2:6])
                #print( "    motion  {p:d} {v:f}".format(p= data[1], v=v ))   
                serviceBroker.set_motion_distance(v, self.connectInfo.port)
                found = True
                
        if self.mode == self.MOTION_SENSOR_MODE_COUNT:
            if len(data) == 6 :
                v=uint32FromArray(data[2:6])
                #print( "    motion  {p:d} {v:f}".format(p= data[1], v=v ))   
                serviceBroker.set_motion_count(v, self.connectInfo.port)
                found = True
            if len(data) == 4 :
                v=uint16FromArray(data[2:4])
                #print( "    motion  {p:d} {v:f}".format(p= data[1], v=v ))   
                serviceBroker.set_motion_count(v, self.connectInfo.port)
                found = True
        if not found:
            logger.error("motion, data not parsed {}".format( _hexData(data)))        
            
class InputFormat:
    INPUT_FORMAT_UNIT_RAW = 0            
    INPUT_FORMAT_UNIT_PERCENTAGE = 1
    INPUT_FORMAT_UNIT_SI = 2
    INPUT_FORMAT_UNIT_UNKNOWN = 3
    
class TiltSensor( LegoAdapterService):
    
    TILT_SENSOR_MODE_ANGLE = 0
    TILT_SENSOR_MODE_TILT = 1
    TILT_SENSOR_MODE_CRASH = 2

    TILT_SENSOR_MODE_UNKNOWN = 4
    
    TILT_SENSOR_DIRECTION_NEUTRAL = 0
    TILT_SENSOR_DIRECTION_BACKWARD = 3
    TILT_SENSOR_DIRECTION_RIGHT = 5
    TILT_SENSOR_DIRECTION_LEFT = 7
    TILT_SENSOR_DIRECTION_FORWARD = 9
    TILT_SENSOR_DIRECTION_UNKNOWN = 10 
       
    def __init__(self, connectInfo, parent):
        LegoAdapterService.__init__(self, connectInfo)
        self.mode = self.TILT_SENSOR_MODE_UNKNOWN
        
        self.names= [ 'tilt', 'tilt{p:d}'.format( p= connectInfo.port ) ]
        self.parent = parent
       
    def setDefaultMode(self):
        self.setMode(self.TILT_SENSOR_MODE_TILT)   
              
    def setMode(self, mode):
        # print("TiltSensor, setMode", mode )
        if mode == self.mode:
            return
        self.mode = mode
        if mode in [ self.TILT_SENSOR_MODE_ANGLE] :
            s = array.array('B')
            # command = set definition 
            s.append( 1 )
            s.append( 2 )
            # port
            s.append(self.connectInfo.port) 
            # type == RGB_LED
            s.append(self.connectInfo.ioType.ioType)
            
            s.append(self.mode)
            # offset for notifications TODO: what is this
            s.extend( uint32_le ( 1)) 
            # unit used, SI
            s.append( InputFormat.INPUT_FORMAT_UNIT_SI )
            # notification enabled
            s.append( 1 )
            self.parent.writeInputFormat(s)
            
        if mode in [ self.TILT_SENSOR_MODE_TILT, self.TILT_SENSOR_MODE_CRASH] :
            s = array.array('B')
            # command = set definition 
            s.append( 1 )
            s.append( 2 )
            # port
            s.append(self.connectInfo.port) 
            # type == RGB_LED
            s.append(self.connectInfo.ioType.ioType)
            
            s.append(self.mode)
            # offset for notifications TODO: what is this
            s.extend( uint32_le ( 1)) 
            # unit used, SI
            s.append( InputFormat.INPUT_FORMAT_UNIT_RAW )
            # notification enabled
            s.append( 1 )
            self.parent.writeInputFormat(s)
    
    def setData(self, data):
        if debug_data:
            logger.error("tilt, data {}".format( _hexData(data)))  
        found = False
        if self.mode == self.TILT_SENSOR_MODE_ANGLE:
            if len(data) == 10 :
                v0=floatFromArray(data[2:6] )
                v1=floatFromArray(data[6:10])
                                  
                #print( "    angle  {p:d} {v0:f} {v1:f}".format(p= data[1], v0=v0, v1=v1 ))
                serviceBroker.set_tilt_angle( [v0, v1], self.connectInfo.port)
                found = True
                
        if self.mode == self.TILT_SENSOR_MODE_TILT:
            if len(data) == 3:
                v0=data[2]
                #print( "    tilt  {p:d} {v0:d}".format(p= data[1], v0=v0 ))
                serviceBroker.set_tilt_tilt( [v0], self.connectInfo.port)
                found = True
                
        if self.mode == self.TILT_SENSOR_MODE_CRASH:
            if len(data) == 5:
                v0=uint8FromArray(data[2:3] )
                v1=uint8FromArray(data[3:4])
                v2=uint8FromArray(data[4:5])

                #print( "    crash  {p:d} {v0:f} {v1:f} {v2:f}".format(p= data[1], v0=v0, v1=v1, v2=v2 ))
                serviceBroker.set_tilt_crash( [v0,v1,v2], self.connectInfo.port)
                found = True
                
        if not found:
            logger.error("tilt, data not parsed {}".format( _hexData(data)))        
         
class VoltageSensor( LegoAdapterService):
    VOLTAGE_SENSOR_MODE_DEFAULT = 99

    names=['voltagesensor']
    def __init__(self, connectInfo, parent):
        LegoAdapterService.__init__(self, connectInfo)
        self.parent = parent
                
    def setDefaultMode(self):
        self.setMode(self.VOLTAGE_SENSOR_MODE_DEFAULT)   

    def setMode(self, mode):
            s = array.array('B')
            # command = set definition 
            s.append( 1 )
            s.append( 2 )
            # port
            s.append(self.connectInfo.port) 
            # type == RGB_LED
            s.append(self.connectInfo.ioType.ioType)
            
            # SDK says Mode 0, delta 30
            s.append(0)
            # offset for notifications TODO: what is this
            s.extend( uint32_le ( 30 )) 
            # unit used, SI
            s.append( 2 )
            # notification enabled
            s.append( 1 )
            self.parent.writeInputFormat(s) 
    
    def setData(self, data):
        # print("voltage, setData")
        if len(data) == 6 :
            v=floatFromArray(data[2:6])
            logger.info( "voltage {p:d} {v:f}".format(p= data[1], v=v ))   
            serviceBroker.set_voltage(v)

            
class CurrentSensor( LegoAdapterService):
    CURRENT_SENSOR_MODE_DEFAULT = 99
    names=['currentsensor']
    
    def __init__(self, connectInfo, parent):
        LegoAdapterService.__init__(self, connectInfo)
        self.parent = parent
                
    def setDefaultMode(self):
        self.setMode(self.CURRENT_SENSOR_MODE_DEFAULT)   
    
    def setMode(self, mode):
            s = array.array('B')
            # command = set definition 
            s.append( 1 )
            s.append( 2 )
            # port fixed
            s.append(self.connectInfo.port) 
            # type == RGB_LED
            s.append(self.connectInfo.ioType.ioType)

            # SDK says Mode 0, delta 30
            s.append(0)
            # offset for notifications TODO: what is this
            s.extend( uint32_le ( 30 )) 
            # unit used, SI
            s.append( 2 )
            # notification enabled
            s.append( 1 )
            self.parent.writeInputFormat(s) 
            
    def setData(self, data):
        if len(data) == 6 :
            v=floatFromArray(data[2:6])
            logger.info("current  {p:d} {v:f}".format(p= data[1], v=v ))   
            serviceBroker.set_current(v)

class PiezoTonePlayer( LegoAdapterService):
    PIEZOTONEPLAYER_MODE = 99
    
    PLAY_PIEZO_TONE_COMMAND_ID = 0x02
    STOP_PIEZO_TONE_COMMAND_ID = 0x03
    
    PIEZO_TONE_MAX_FREQUENCY = 1500
    PIEZO_TONE_MIN_FREQUENCY = 30 # not in SDK
    
    PIEZO_TONE_MAX_DURATION = 65536
    
    PIEZO_NOTE_C   =  1
    PIEZO_NOTE_CIS =  2
    PIEZO_NOTE_D   =  3
    PIEZO_NOTE_DIS =  4
    PIEZO_NOTE_E   =  5
    PIEZO_NOTE_F   =  6
    PIEZO_NOTE_FIS =  7
    PIEZO_NOTE_G   =  8
    PIEZO_NOTE_GIS =  9
    PIEZO_NOTE_A   = 10
    PIEZO_NOTE_AIS = 11
    PIEZO_NOTE_B   = 12
    
    names=['piezotoneplayer']

    def __init__(self, connectInfo, parent):
        LegoAdapterService.__init__(self, connectInfo)
        self.parent = parent
                
    
    def setMode(self, mode):
        pass
    
    def playFrequency(self,  frequency, duration):
        if frequency > self.PIEZO_TONE_MAX_FREQUENCY:
            print( "Cannot play frequenzy %d, max supported frequency is %d", frequency, self.PIEZO_TONE_MAX_FREQUENCY)
            frequency = self. PIEZO_TONE_MAX_FREQUENCY
        if frequency < 30:
            print( "Cannot play frequenzy %d, max supported frequency is %d", frequency, self.PIEZO_TONE_MIN_FREQUENCY)
            frequency = self.PIEZO_TONE_MIN_FREQUENCY
       
        if duration > self.PIEZO_TONE_MAX_DURATION:
            print("Cannot play piezo tone with duration %d ms, max supported frequency is %d ms", duration, self.PIEZO_TONE_MAX_DURATION)
            duration = self.PIEZO_TONE_MAX_DURATION
        
        if duration < 1:
            duration = 1
            
        
        s = array.array('B')
        # command = set definition 
        s.append( self.connectInfo.port )
        s.append( self.PLAY_PIEZO_TONE_COMMAND_ID )
        s.append( 4)
        s.append( (frequency >> 0 ) & 0xff)
        s.append( (frequency >> 8 ) & 0xff)
        s.append( (duration >> 0 ) & 0xff)
        s.append( (duration >> 8 ) & 0xff)

        self.parent.writeCommand(s) 


    def playNote( self,  note,  octave, duration):
        if (octave < 1) :
            logger.error("Invalid octave: {o:d}".format(o=octave));
            octave = 1
            
        if (octave > 6) :
            logger.error("Highest supported note is F# in 6th octave - invalid octave: {o:d}".format(o=octave));
        
        if (octave == 6 and note > self.PIEZO_NOTE_FIS):
            logger.error("Cannot play note. Highest supported note is F# in 6th octave");
        
        if not( self.PIEZO_NOTE_C <= note <= self.PIEZO_NOTE_B):
            logger.error("Cannot play note {n:d}. Note needs to be in [{min:d},{max:d}]".format(min=self.PIEZO_NOTE_C, max=self.PIEZO_NOTE_B, n=note));
            note = self.PIEZO_NOTE_C
        # the following sectioon is copied from SDK
        #   /**
        #    * The basic formula for the frequencies of the notes of the equal tempered scale is given by
        #    * fn = f0 * (a)n
        #    * where
        #    * f0 = the frequency of one fixed note which must be defined. A common choice is setting the A above middle C (A4) at f0 = 440 Hz.
        #    * n = the number of half steps away from the fixed note you are. If you are at a higher note, n is positive. If you are on a lower note, n is negative.
        #    * fn = the frequency of the note n half steps away.
        #    * a = (2)1/12 = the twelfth root of 2 = the number which when multiplied by itself 12 times equals 2 = 1.059463094359...
        #    */

        base = 440.0
        octavesAboveMiddle = octave - 4;
        halfStepsAwayFromBase = note - self.PIEZO_NOTE_A.getValue() + (octavesAboveMiddle * 12)
        noteRelation = math.pow(2.0, 1.0 / 12)
        frequency = base * math.pow(noteRelation, halfStepsAwayFromBase) 

        self.playFrequency( int ( round(frequency)) , duration)
    
    def stopPlaying(self):
        s = array.array('B')
        # command = set definition 
        s.append( self.connectInfo.port )
        s.append( self.STOP_PIEZO_TONE_COMMAND_ID )
        s.append( 0)

        self.parent.writeCommand(s) 

    
def _hexString( d):
    s = ''
    for x in d:
        s += "{x:02x} ".format(x=ord(x) )
    return s

def _hexData( d):
    s = ''
    for x in d:
        s += "{x:02x} ".format(x=x )
    return s
    
        
def print_channels(chs):
    i = 0
    for ch in chs:
        print("[{i:d}] characteristic".format(i=i), ch, "{h:02x}".format(h=ch.getHandle()), ch.propertiesToString())
        i += 1
        
def print_channel(ch):
    print("characteristic {ch:s} {h:02x}, {p:s}".format( ch=str(ch), h=ch.getHandle(), p=ch.propertiesToString()))
 


# --------------------------------------------------------------------------------------
serviceBroker = ServiceBroker()
# --------------------------------------------------------------------------------------


class Wedo2Adapter(adapter.adapters.Adapter):
    # -----------------------------------------
    # fields for adapter
    # -----------------------------------------
    mandatoryParameters = { 
                           'btle.address' :  'a0:e6:f8:6d:0e:67', 
                           'mode.strict'  :  'true',
                          }
    
    # -----------------------------------------
    
    def __init__(self):
        
        # General Adapter
        adapter.adapters.Adapter.__init__(self)
        self.motor_init()
   
    def run(self):
        
        self.strict  = self.isTrue( self.parameters['mode.strict'] )
        
        btle_name = self.parameters['btle.name']
        btle_address = self.parameters['btle.address']
        btle_policy = self.parameters['btle.policy']
        
        address = btle_address
        
        with helper.logging.LoggingContext(logger, level=logging.DEBUG):
            logger.info("{name:s}: Press 'connect'-Button on Hub".format(name=self.name)) 
        
        peripheral = None
        
        if 'name' == btle_policy:
            scanner = bluepy.btle.Scanner()
            
            found = False
            while not ( self.stopped() or found ):
                try:
                    devices = scanner.scan(timeout=5)
                    for device in devices:
                        # 9 : 'Complete Local Name'
                        with helper.logging.LoggingContext(logger, level=logging.DEBUG):
                            logger.info("{name:s}: found device {addr:s}, {dname:s}".format(name=self.name, addr=device.addr, dname=device.getValueText( 9 )))
                        
                        if  btle_name == device.getValueText( 9 ):
                            address = device.addr
                            peripheral = bluepy.btle.Peripheral(device)
                            peripheral.discoverServices()
                            found = True
                            break  
                except bluepy.btle.BTLEException as e:
                    if "Failed to execute mgmt cmd 'le on'" == e.message:
                        logger.warn("{name:s}: Check if bluetooth is enabled !".format(name=self.name ) )
                    logger.error("{name:s}: {msg:s}".format(name=self.name, msg=e.message) )
                self.delay(3)
                    
        if 'address' == btle_policy:
            while not self.stopped() :
                try:
                    peripheral = bluepy.btle.Peripheral( address )
                    peripheral.discoverServices()
                except bluepy.btle.BTLEException as e:
                    logger.error("{name:s}: Could not connect to device {address:s}".format(name=self.name, address=address))
                self.delay(3)
        
        if peripheral == None:
            logger.error("{name:s}: ABORT: Could not connect to device. ".format(name=self.name ) )
            return  
        
        if self.stopped():
            return
        
        listener = serviceBroker.listener         
        params = None
        delegate = MyDelegate(params)
        peripheral.setDelegate( delegate )
     
        bHelper = BHelper(peripheral)
        
        legoService = LegoService(peripheral, delegate, bHelper)
        deviceService = DeviceService(peripheral, delegate, legoService, bHelper)

        lastVoltage = None
        lastCurrent = None
        last_t = time.time()
         
        while not self.stopped() :
            self.delay(0.05)
            # voltage and current are updated quite often.
            # therfor these are rate-limited
            
            t = time.time()
            if t > last_t + 0.1:
                last_t = t
                voltage = serviceBroker.get_voltage()
                if lastVoltage != voltage:
                    lastVoltage = voltage
                    self.voltage(voltage)
                    
            current = serviceBroker.get_current()
            if lastCurrent != current:
                lastCurrent = current
                self.current(current)
                
            try:
                #
                # there can be many entries in the queue.
                # group them by topic in a dictionary
                # this results in 'last entry is the winner'
                cmds = {}
                cnt = 0    
                while True:
                    # emergency: really too many when there are 1000 in 
                    # queue, then break to receive timely processing
                    cnt += 1
                    if cnt > 1000:
                        break
                    try:
                        cmd = listener.get()
                        cmds[ cmd[ 'name'] ] = cmd
                    except helper.abstractQueue.AbstractQueue.Empty as e:
                        break
            
                for name in cmds:
                    cmd = cmds[name]
                    
                    if cmd[ 'name'] == 'button_state':
                        value = cmd['value']
                        if value == 0:
                            self.button_released()
                        if value == 1:
                            self.button_pressed()
                            
                    elif cmd[ 'name'] == 'low_voltage':
                        value = cmd['value']
                        if value == 0:
                            self.button_released()
                        if value == 1:
                            self.button_pressed()
                            
                    elif cmd[ 'name'] == 'low_voltage_alert':
                        self.low_voltage_alert()
                        
                    elif cmd[ 'name'] == 'high_current_alert':
                        self.high_current_alert()
                        
                    elif cmd[ 'name'] == 'motion1_count':
                        value = cmd['value']
                        if self.strict:
                            self.motion1_count(value)
                        else:
                            self.motion_count(value)
                            
                    elif cmd[ 'name'] == 'motion2_count':
                        value = cmd['value']
                        if self.strict:
                            self.motion2_count(value)
                        else:
                            self.motion_count(value)
                        
                    elif cmd[ 'name'] == 'motion1_distance':
                        value = cmd['value']
                        if self.strict:
                            self.motion1_distance(value)
                        else:
                            self.motion_distance(value)
                    elif cmd[ 'name'] == 'motion2_distance':
                        value = cmd['value']
                        if self.strict:
                            self.motion2_distance(value)
                        else:
                            self.motion_distance(value)
                    
                    elif cmd[ 'name'] == 'tilt1_tilt':
                        values = cmd['values']
                        if self.strict:
                            self.tilt1_tilt1(values[0])
                        else:
                            self.tilt_tilt(values[0])
                            
                    elif cmd[ 'name'] == 'tilt2_tilt':
                        values = cmd['values']
                        if self.strict:
                            self.tilt2_tilt(values[0])
                        else:
                            self.tilt_tilt(values[0])
                    
                    elif cmd[ 'name'] == 'tilt1_angle':
                        values = cmd['values']
                        if self.strict:
                            self.tilt1_angle_1(values[0])
                            self.tilt1_angle_2(values[1])
                        else:
                            self.tilt_angle_1(values[0])
                            self.tilt_angle_2(values[1])
                    elif cmd[ 'name'] == 'tilt2_angle':
                        values = cmd['values']
                        if self.strict:
                            self.tilt2_angle_1(values[0])
                            self.tilt2_angle_2(values[1])
                        else:
                            self.tilt_angle_1(values[0])
                            self.tilt_angle_2(values[1])
                    
                    elif cmd[ 'name'] == 'tilt1_crash':
                        values = cmd['values']
                        if self.strict:
                            self.tilt1_crash_1(values[0])
                            self.tilt1_crash_2(values[1])
                            self.tilt1_crash_3(values[2])
                        else:
                            self.tilt_crash_1(values[0])
                            self.tilt_crash_2(values[1])
                            self.tilt_crash_3(values[2])
                    elif cmd[ 'name'] == 'tilt2_crash':
                        values = cmd['values']
                        if self.strict:
                            self.tilt2_crash_1(values[0])
                            self.tilt2_crash_2(values[1])
                            self.tilt2_crash_3(values[2])
                        else:
                            self.tilt_crash_1(values[0])
                            self.tilt_crash_2(values[1])
                            self.tilt_crash_3(values[2])
                    
                    else:
                        logger.error("unknown command in listener queue {}".format(cmd[ 'name']))  
            except Exception as e:
                logger.error(e)
            
        bHelper.stop()
        peripheral.disconnect()
        
    def low_voltage_alert(self):
        """output command from adapter to scratch."""
        self.send()
        
    def high_current_alert(self):
        """output command from adapter to scratch."""
        self.send()
        
        
    def button_pressed(self):
        """output command from adapter to scratch."""
        self.send()
        
    def button_released(self):
        """output command from adapter to scratch."""
        self.send()
            
    def color(self, value):
        """ value can be plain integer [0..10]
            or any of the adapter standard color definitions """
            
        logger.info("color {}".format(value))
        try:
            index = int(value)
            # print ( "index = ", index)
            if 0 <= index <= 10:
                serviceBroker.rgblight_setColorIndex(index)
                return
        except:
            pass
        
        color = self.getRGBFromString( value)
        serviceBroker.rgblight_setColor( color['red'], color['green'], color['blue'] )
       
    def motor_init(self):
        self._motor = 0
        self._motor1 = 0
        self._motor2 = 0
           
    def motor(self, value):
        """ values are [-100, +100] """
        logger.info("motor {}".format(value))
        if self.strict:
            return
        try:
            value = int(value)
            if -100 <=value <= 100:
                self._motor = value
                if value >= 0:
                    direction = Motor.MOTOR_DIRECTION_RIGHT 
                if value < 0:
                    direction = Motor.MOTOR_DIRECTION_LEFT 
                serviceBroker.motor_run(direction, abs(value)) 
        except:
            pass
        
    def motor1(self, value):
        logger.info("motor1 {}".format(value))
        if not self.strict:
            return

        try:
            value = int(value)
            if -100 <=value <= 100:
                self._motor1 = value
                if value >= 0:
                    direction = Motor.MOTOR_DIRECTION_RIGHT 
                if value < 0:
                    direction = Motor.MOTOR_DIRECTION_LEFT 
                serviceBroker.motor1_run(direction, abs(value)) 
        except:
            pass
        
    def motor2(self, value):
        logger.info("motor2 {}".format(value))
        if not self.strict:
            return
        try:
            value = int(value)
            if -100 <=value <= 100:
                self._motor2 = value
                if value >= 0:
                    direction = Motor.MOTOR_DIRECTION_RIGHT 
                if value < 0:
                    direction = Motor.MOTOR_DIRECTION_LEFT 
                serviceBroker.motor2_run(direction, abs(value)) 
        except:
            pass
 
    def motor_run(self):
        logger.info("motor_run")
        serviceBroker.motor(self._motor)
        
    def motor1_run(self):
        logger.info("motor1_run")
        serviceBroker.motor1(self._motor1)
         
    def motor2_run(self):
        logger.info("motor2_run")
        serviceBroker.motor2(self._motor2)
        
    def motor_brake(self):
        logger.info("motor_brake")
        serviceBroker.motor_brake()
        
    def motor1_brake(self):
        logger.info("motor1_brake")
        serviceBroker.motor1_brake()
        
    def motor2_brake(self):
        logger.info("motor2_brake")
        serviceBroker.motor2_brake()
 
    def motor_drift(self):
        logger.info("motor_drift")
        serviceBroker.motor_drift()
        
    def motor1_drift(self):
        logger.info("motor1_drift")
        serviceBroker.motor1_drift()
        
    def motor2_drift(self):
        logger.info("motor2_drift")
        serviceBroker.motor2_drift()
    
    def voltage(self, value):
        self.sendValue(value)
        
    def current(self, value):
        self.sendValue(value)

    def motion_count(self, value):
        self.sendValue(value)
    def motion_distance(self, value):
        self.sendValue(value)
        
    def motion1_count(self, value):
        self.sendValue(value)
    def motion1_distance(self, value):
        self.sendValue(value)
        
    def motion2_count(self, value):
        self.sendValue(value)
    def motion2_distance(self, value):
        self.sendValue(value)

    def motion_mode(self, value):
        logger.info("motion_mode {}".format(value))
        try:
            value = int(value)
        except:
            logger.error("could not parse int value")
            return
        if not self.strict:
            #print("call service")
            serviceBroker.set_motion_Mode(value)
            
    def motion1_mode(self, value):
        logger.info("motion1_mode {}".format(value))
        try:
            value = int(value)
        except:
            logger.error("could not parse int value")
            return
        if self.strict:
            #print("call service")
            serviceBroker.set_motion1_Mode(value)
            
    def motion2_mode(self, value):
        logger.info("motion2_mode {}".format(value))
        try:
            value = int(value)
        except:
            logger.error("could not parse int value")
            return
        if self.strict:
            #print("call service")
            serviceBroker.set_motion2_Mode(value)
    
    def motion_reset(self):
        logger.info("motion_reset")
        
        if not self.strict:
            #print("call service")
            serviceBroker.motion_reset()
            
    def motion1_reset(self):
        logger.info("motion1_reset")
        
        if self.strict:
            #print("call service")
            serviceBroker.motion1_reset()
            
    def motion2_reset(self):
        logger.info("motion2_reset")
        
        if self.strict:
            #print("call service")
            serviceBroker.motion2_reset()

    def tilt_tilt(self, value):
        self.sendValue(value)
        
    def tilt_angle_1(self, value):
        self.sendValue(value)
    def tilt_angle_2(self, value):
        self.sendValue(value)
        
    def tilt_crash_1(self, value):
        self.sendValue(value)
    def tilt_crash_2(self, value):
        self.sendValue(value)
    def tilt_crash_3(self, value):
        self.sendValue(value)


    def tilt1_tilt(self, value):
        self.sendValue(value)
        
    def tilt1_angle_1(self, value):
        self.sendValue(value)
    def tilt1_angle_2(self, value):
        self.sendValue(value)
        
    def tilt1_crash_1(self, value):
        self.sendValue(value)
    def tilt1_crash_2(self, value):
        self.sendValue(value)
    def tilt1_crash_3(self, value):
        self.sendValue(value)


    def tilt2_tilt(self, value):
        self.sendValue(value)
        
    def tilt2_angle_1(self, value):
        self.sendValue(value)
    def tilt2_angle_2(self, value):
        self.sendValue(value)
        
    def tilt2_crash_1(self, value):
        self.sendValue(value)
    def tilt2_crash_2(self, value):
        self.sendValue(value)
    def tilt2_crash_3(self, value):
        self.sendValue(value)


    def tilt_mode(self, value):
        logger.info("tilt_mode")
        try:
            value = int(value)
        except:
            return
        if self.strict:
            return
        serviceBroker.set_tilt_Mode(value)
        
    def tilt1_mode(self, value):
        logger.info("tilt1_mode")
        try:
            value = int(value)
        except:
            return
        if self.strict:
            serviceBroker.set_tilt1_Mode(value)
            
    def tilt2_mode(self, value):
        logger.info("tilt2_mode")
        try:
            value = int(value)
        except:
            return
        if self.strict:
            serviceBroker.set_tilt2_Mode(value)
   
    def tilt_reset(self):
        logger.info("tilt_reset")
        
        if not self.strict:
            #print("call service")
            serviceBroker.tilt_reset()
            
    def tilt1_reset(self):
        logger.info("tilt1_reset")
        
        if self.strict:
            #print("call service")
            serviceBroker.tilt1_reset()
            
    def tilt2_reset(self):
        logger.info("tilt2_reset")
        
        if self.strict:
            #print("call service")
            serviceBroker.tilt2_reset()

    def piezo_frequency(self, value):
        logger.info("piezo_frequency {}".format(value))
        try:
            v = value.split(';')
            frequency = int( v[0] )
            duration = int( v[1] )
            serviceBroker.piezotoneplayer_playFrequency(frequency, duration)
        except:
            return
        if self.strict:
            serviceBroker.set_tilt2_Mode(value)
   
