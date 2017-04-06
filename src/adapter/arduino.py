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

#
# the adapters in this module use USB serial connection to the arduino.
#

import xml.etree.ElementTree as ET
from types import MethodType
import helper.abstractQueue

import threading
import serial
import os
import traceback
import sys

if os.name == 'posix':
    import termios
    import fcntl

import time
import logging
import helper.logging
logger = logging.getLogger(__name__)

import errorManager

import adapter
#
# ----------------------------------------------------------
# debug flags, also exported to arduino
#
# debug_0_debug enable extra printouts on arduino
# debug_1_verbose enable comm debug on arduino
# debug_2_blink replaces blink on arduino au lieu de toggle on loop (runtime estimation)
#
# production settings: all are False
#
debug_0_debug   = False
debug_1_verbose = False 
debug_2_blink   = False
#
# ----------------------------------------------------------
#
# show lines received/send from serial line
#
show = False
#
# --------------------------------------------------------------------------------------


class UNO_Adapter (adapter.adapters.Adapter):
    """Interface to UNO, Arduino Serial adapter 
    """
    
    parameter_SERIAL_DEVICE = 'serial.device'
    parameter_SERIAL_BAUD = 'serial.baud'

    mandatoryParameters = { parameter_SERIAL_DEVICE: 'COM6',
                           parameter_SERIAL_BAUD : '115200'
                          }
    VOID = 'void'
    IN = 'in'
    OUT = 'out'
    
    PWM = 'pwm'
    SERVO = 'servo'
    COUNTER = 'counter'
    
    ANALOGIN = 'analog_in'
     
    ON = 'on'
    
    #
    # Function prototypes for input setters
    # these are added dynamically to the class if needed.
    #
    def _setInput_IO_A0(self, value):
        self._set_A_IO(value, 0)
    def _setInput_IO_A1(self, value):
        self._set_A_IO(value, 1)
    def _setInput_IO_A2(self, value):
        self._set_A_IO(value, 2)
    def _setInput_IO_A3(self, value):
        self._set_A_IO(value, 3)
    def _setInput_IO_A4(self, value):
        self._set_A_IO(value, 4)
    def _setInput_IO_A5(self, value):
        self._set_A_IO(value, 5)
    def _setInput_IO_A6(self, value):
        self._set_A_IO(value, 6)
    def _setInput_IO_A7(self, value):
        self._set_A_IO(value, 7)

    
    def _setInput_IO_0(self, value):
        self._set_IO(value, 0)
    def _setInput_IO_1(self, value):
        self._set_IO(value, 1)
    def _setInput_IO_2(self, value):
        self._set_IO(value, 2)
    def _setInput_IO_3(self, value):
        self._set_IO(value, 3)
    def _setInput_IO_4(self, value):
        self._set_IO(value, 4)
    def _setInput_IO_5(self, value):
        self._set_IO(value, 5)
    def _setInput_IO_6(self, value):
        self._set_IO(value, 6)
    def _setInput_IO_7(self, value):
        self._set_IO(value, 7)
        
    def _setInput_IO_8(self, value):
        self._set_IO(value, 8)
    def _setInput_IO_9(self, value):
        self._set_IO(value, 9)
    def _setInput_IO_10(self, value):
        self._set_IO(value, 10)
    def _setInput_IO_11(self, value):
        self._set_IO(value, 11)
    def _setInput_IO_12(self, value):
        self._set_IO(value, 12)
    def _setInput_IO_13(self, value):
        self._set_IO(value, 13)
        
    def _setInput_PWM_0(self, value):
        self._set_PWM(value, 0)
    def _setInput_PWM_1(self, value):
        self._set_PWM(value, 1)
    def _setInput_PWM_2(self, value):
        self._set_PWM(value, 2)
    def _setInput_PWM_3(self, value):
        self._set_PWM(value, 3)
    def _setInput_PWM_4(self, value):
        self._set_PWM(value, 4)
    def _setInput_PWM_5(self, value):
        self._set_PWM(value, 5)
    def _setInput_PWM_6(self, value):
        self._set_PWM(value, 6)
    def _setInput_PWM_7(self, value):
        self._set_PWM(value, 7)
    def _setInput_PWM_8(self, value):
        self._set_PWM(value, 8)
        
    def _setInput_PWM_9(self, value):
        self._set_PWM(value, 9)
    def _setInput_PWM_10(self, value):
        self._set_PWM(value, 10)
    def _setInput_PWM_11(self, value):
        self._set_PWM(value, 11)
    def _setInput_PWM_12(self, value):
        self._set_PWM(value, 12)
    def _setInput_PWM_13(self, value):
        self._set_PWM(value, 13)
   
    def _setInput_SERVO_0(self, value):
        self._set_SERVO(value, 0)
    def _setInput_SERVO_1(self, value):
        self._set_SERVO(value, 1)
    def _setInput_SERVO_2(self, value):
        self._set_SERVO(value, 2)
    def _setInput_SERVO_3(self, value):
        self._set_SERVO(value, 3)
    def _setInput_SERVO_4(self, value):
        self._set_SERVO(value, 4)
    def _setInput_SERVO_5(self, value):
        self._set_SERVO(value, 5)
    def _setInput_SERVO_6(self, value):
        self._set_SERVO(value, 6)
    def _setInput_SERVO_7(self, value):
        self._set_SERVO(value, 7)
    def _setInput_SERVO_8(self, value):
        self._set_SERVO(value, 8)
        
    def _setInput_SERVO_9(self, value):
        self._set_SERVO(value, 9)
    def _setInput_SERVO_10(self, value):
        self._set_SERVO(value, 10)
    def _setInput_SERVO_11(self, value):
        self._set_SERVO(value, 11)
    def _setInput_SERVO_12(self, value):
        self._set_SERVO(value, 12)
    def _setInput_SERVO_13(self, value):
        self._set_SERVO(value, 13)
      
    def _set_IO(self, value, bitvalue):
        """set Port to a specific value"""
        bValue = self.isTrue(value)
        if bValue:
            #self.queue.put("out:{bit:02d},1".format(bit=bitvalue))
            self._queue_put("o:{bit:d}".format(bit=bitvalue), "o{bit:d},1".format(bit=bitvalue))
        else:
            # self.queue.put("out:{bit:02d},0".format(bit=bitvalue))
            self._queue_put("o:{bit:d}".format(bit=bitvalue), "o{bit:d},0".format(bit=bitvalue))

    def _set_A_IO(self, value, bitvalue):
        """set PortA to a specific value"""
        bValue = self.isTrue(value)
        if bValue:
            #self._queue_put("out:{bit:02d},1".format(bit=bitvalue))
            self._queue_put("oa:{bit:d}".format(bit=bitvalue), "oa{bit:d},1".format(bit=bitvalue))
        else:
            # self._queue_put("out:{bit:02d},0".format(bit=bitvalue))
            self._queue_put("oa:{bit:d}".format(bit=bitvalue), "oa{bit:d},0".format(bit=bitvalue))

    def _set_PWM(self, value, bitvalue):
        """set PWM for UNO, value range 0..255"""
        v = int(float(value))
        if v < 0:
            v = 0
        if v > 255:
            v = 255
        # print("setPWM()") 
        self._queue_put("p:{bit:d}".format( bit=bitvalue), "p{bit:d},{value:d}".format( bit=bitvalue, value=v ))

    def _set_SERVO(self, value, bitvalue):
        """set SERVO for UNO, value range 0..180"""
        v = int(float(value))
        if v < 0:
            v = 0
        if v > 180:
            v = 180
        # print("setServo()") 
        self._queue_put("s:{bit:d}".format( bit=bitvalue ), "s{bit:d},{value:d}".format( bit=bitvalue, value=v ))

    def _sendValue(self, value):
        """Prototype for a send function"""
        self.sendValue('"' + value + '"')

    def __init__(self):
        self.state_arduinoConfigured = 'undef'
        adapter.adapters.Adapter.__init__(self)
        self.queue_data = helper.abstractQueue.PriorityQueue()
        self.queue_command = helper.abstractQueue.PriorityQueue()
        self.stateMachine = UNO_Adapter.StateMachine(self)
        self.lastInputValue = {}
        
    def setActive(self, state):
        adapter.adapters.Adapter.setActive(self, state)

        if state:
            # clear 'old' values on new connections
            self.lastInputValue = {}
            
            self.stateMachine._start()
            self.stateMachine.start()
        else:
            
            self.stateMachine.stop() 
            
            # wait till state == STOP, but max 0.5 sec
            t0 = time.time()
            while True:
                if t0 + 0.5 < time.time():
                    break
                if self.stateMachine.state.name() == "STOP":
                    break;
                time.sleep(0.01)
                  
            self.stateMachine._stop()       
        
    def _queue_put(self, key, s):
        """ put a data command to the queue, low level prio"""
        self.lastInputValue [ key] = s
        
        # only output when arduino is connected 
        if self.state_arduinoConfigured == "CONFIGURED":
            self.queue_data.put( 2, s )
        
    def _queue_put_prio(self, s):
        """ config commands need to be high level prio """
        self.queue_command.put( 1, s )
         
    # definitions for an atmel328 processor as available on arduino uno or arduino nano
        
    io_options = { 
          'D0' :  { 'pos': 0,  'ifunc' : _setInput_IO_0, 'pfunc' : _setInput_PWM_0,'sfunc' : _setInput_SERVO_0, 'options': ['void']},
          'D1' :  { 'pos': 1,  'ifunc' : _setInput_IO_1, 'pfunc' : _setInput_PWM_1,'sfunc' : _setInput_SERVO_1, 'options': ['void']},
          'D2' :  { 'pos': 2,  'ifunc' : _setInput_IO_2, 'pfunc' : _setInput_PWM_2,'sfunc' : _setInput_SERVO_2, 'options': ['void', 'in', 'out',        'servo', 'counter']},
          'D3' :  { 'pos': 3,  'ifunc' : _setInput_IO_3, 'pfunc' : _setInput_PWM_3,'sfunc' : _setInput_SERVO_3, 'options': ['void', 'in', 'out', 'pwm', 'servo', 'counter']},
          'D4' :  { 'pos': 4,  'ifunc' : _setInput_IO_4, 'pfunc' : _setInput_PWM_4,'sfunc' : _setInput_SERVO_4, 'options': ['void', 'in', 'out',        'servo', 'counter']},
          'D5' :  { 'pos': 5,  'ifunc' : _setInput_IO_5, 'pfunc' : _setInput_PWM_5,'sfunc' : _setInput_SERVO_5, 'options': ['void', 'in', 'out', 'pwm', 'servo', 'counter']},
          'D6' :  { 'pos': 6,  'ifunc' : _setInput_IO_6, 'pfunc' : _setInput_PWM_6,'sfunc' : _setInput_SERVO_6, 'options': ['void', 'in', 'out', 'pwm', 'servo', 'counter']},
          'D7' :  { 'pos': 7,  'ifunc' : _setInput_IO_7, 'pfunc' : _setInput_PWM_7,'sfunc' : _setInput_SERVO_7, 'options': ['void', 'in', 'out',        'servo', 'counter']},
          'D8' :  { 'pos': 8,  'ifunc' : _setInput_IO_8, 'pfunc' : _setInput_PWM_8,'sfunc' : _setInput_SERVO_8, 'options': ['void', 'in', 'out',        'servo', 'counter']},
          'D9' :  { 'pos': 9,  'ifunc' : _setInput_IO_9, 'pfunc' : _setInput_PWM_9, 'sfunc' : _setInput_SERVO_9,'options': ['void', 'in', 'out', 'pwm', 'servo', 'counter']},
          'D10' : { 'pos': 10, 'ifunc' : _setInput_IO_10,'pfunc' : _setInput_PWM_10,'sfunc' : _setInput_SERVO_10,'options': ['void', 'in', 'out', 'pwm', 'servo', 'counter']},
          'D11' : { 'pos': 11, 'ifunc' : _setInput_IO_11,'pfunc' : _setInput_PWM_11,'sfunc' : _setInput_SERVO_11,'options': ['void', 'in', 'out', 'pwm', 'servo', 'counter']},
          'D12' : { 'pos': 12, 'ifunc' : _setInput_IO_12,'pfunc' : _setInput_PWM_12,'sfunc' : _setInput_SERVO_12,'options': ['void', 'in', 'out',        'servo', 'counter']},
          # pin13, LED, is used for status display on uno, so do nut allow for IO
          'D13' : { 'pos': 13, 'ifunc' : _setInput_IO_13,'pfunc' : _setInput_PWM_13,'sfunc' : _setInput_SERVO_13,'options': ['void']},
          }
        
    analog_options = {
          'A0': { 'pos': 0,  'options': ['void', ANALOGIN, IN, OUT ],  'ifunc' : _setInput_IO_A0, },
          'A1': { 'pos': 1,  'options': ['void', ANALOGIN, IN, OUT ],  'ifunc' : _setInput_IO_A1, },
          'A2': { 'pos': 2,  'options': ['void', ANALOGIN, IN, OUT ],  'ifunc' : _setInput_IO_A2, },
          'A3': { 'pos': 3,  'options': ['void', ANALOGIN, IN, OUT ],  'ifunc' : _setInput_IO_A3, },
          'A4': { 'pos': 4,  'options': ['void', ANALOGIN, IN, OUT ],  'ifunc' : _setInput_IO_A4, },
          'A5': { 'pos': 5,  'options': ['void', ANALOGIN, IN, OUT ],  'ifunc' : _setInput_IO_A5, },
          # A6, A7 only have analog feature for 328-processor.
          'A6': { 'pos': 6,  'options': ['void', ANALOGIN          ],  'ifunc' : _setInput_IO_A6, },
          'A7': { 'pos': 7,  'options': ['void', ANALOGIN          ],  'ifunc' : _setInput_IO_A7, },
          }
                
    def setXMLConfig(self, child):
        #
        # read configuration from xml
        #
        loggingContext = "adapter '[{a:s}]'".format(a=self.name ) 
        
        # bitwise encoded pin usage
        self._analog_analog_inputs = 0
        self._analog_digital_inputs = 0
        self._analog_digital_input_pullups = 0
        self._analog_digital_outputs = 0
        
        self._digital_inputs = 0
        self._digital_input_pullups = 0
        
        self._digital_counter = 0
        self._digital_counter_pullups = 0
        
        
        self._digital_outputs = 0
        self._digital_pwms = 0
        self._digital_servos = 0
        #
        # TODO no checks for multiple usage of ID so far
        #  
        for tle in child:
            if 'extension' == tle.tag:
                child= tle
                break

        for tle in child:
            if 'io' == tle.tag:
                if not 'id' in  tle.attrib:
                    errorManager.append("{lc:s}: id missing arduino_uno:io".format( lc=loggingContext ))
                    continue
       
                _id = tle.attrib['id']
                
                _dir=self.VOID
                if 'dir' in  tle.attrib:
                    _dir = tle.attrib['dir']
                
                _pullup=None
                if 'pullup' in  tle.attrib:
                    _pullup = tle.attrib['pullup']

                options = self.io_options[_id]['options']
                if _dir in options:
                    if _dir == self.VOID:
                        pass
                    elif _dir == self.IN and _pullup ==  self.ON:
                        self._digital_input_pullups |= ( 1 << self.io_options[_id]['pos'])
                        if debug_0_debug:
                            print("register ", "output" + _id )
                        
                        # sending values towards scratch is done by name of the method. So no need to prepare additional 
                        # function prototypes as in the various output functions.
                        # The prototypes are only needed to allow for matching the functions with configuration.
                        #
                        setattr(self, "output" + _id, self._sendValue ) 
                        pass
                    elif _dir == self.IN:
                        self._digital_inputs |= ( 1 << self.io_options[_id]['pos'])
                        if debug_0_debug:
                            print("register ", "output" + _id )
                        setattr(self, "output" + _id, self._sendValue ) 
                        pass
                    # ---------------
                    elif _dir == self.COUNTER and _pullup ==  self.ON:
                        self._digital_counter_pullups |= ( 1 << self.io_options[_id]['pos'])
                        if debug_0_debug:
                            print("register ", "output" + _id )
                        
                        # sending values towards scratch is done by name of the method. So no need to prepare additional 
                        # function prototypes as in the various output functions.
                        # The prototypes are only needed to allow for matching the functions with configuration.
                        #
                        setattr(self, "counter" + _id, self._sendValue ) 
                        pass
                    elif _dir == self.COUNTER:
                        self._digital_counter |= ( 1 << self.io_options[_id]['pos'])
                        if debug_0_debug:
                            print("register ", "output" + _id )
                        setattr(self, "output" + _id, self._sendValue ) 
                        pass
                    # ------------------------
                    elif _dir == self.OUT:
                        self._digital_outputs |= ( 1 << self.io_options[_id]['pos'])
                        if sys.version_info.major == 2:
                            method = MethodType(self.io_options[_id]['ifunc'], self, type(self))
                        if sys.version_info.major == 3:
                            method = MethodType(self.io_options[_id]['ifunc'], self )
                            
                        if debug_0_debug:
                            print("register ", "input" + _id )
                        setattr(self, "input" + _id, method )   
                        pass
                    elif _dir == self.PWM:
                        self._digital_pwms |= ( 1 << self.io_options[_id]['pos'])
                        if sys.version_info.major == 2:
                            method = MethodType(self.io_options[_id]['pfunc'], self, type(self))
                        if sys.version_info.major == 3:
                            method = MethodType(self.io_options[_id]['pfunc'], self )
                        if debug_0_debug:
                            print("register ", "pwm" + _id )
                        setattr(self, "pwm" + _id, method )   
                        pass
                    elif _dir == self.SERVO:
                        self._digital_servos |= ( 1 << self.io_options[_id]['pos'])
                        if sys.version_info.major == 2:
                            method = MethodType(self.io_options[_id]['sfunc'], self, type(self))
                        if sys.version_info.major == 3:
                            method = MethodType(self.io_options[_id]['sfunc'], self )
                            
                        if debug_0_debug:
                            print("register ", "servo" + _id )
                        setattr(self, "servo" + _id, method )   
                        pass
                else:
                    errorManager.append("{lc:s}: invalid pin function 'dir' in arduino_uno:io".format( lc=loggingContext ))
                    continue
                
            if 'analog' == tle.tag:        
                if not 'id' in  tle.attrib:
                    errorManager.append("{lc:s}: id missing arduino_uno:analog".format( lc=loggingContext ))
                    continue    

                _id = tle.attrib['id']
                _dir=self.VOID
                if 'dir' in  tle.attrib:
                    _dir = tle.attrib['dir']
                    
                _pullup=None
                if 'pullup' in  tle.attrib:
                    _pullup = tle.attrib['pullup']

                options = self.analog_options[_id]['options']
                if _dir in options:
                    if _dir == self.VOID:
                        pass
                    
                    elif _dir == self.ANALOGIN:
                        self._analog_analog_inputs |= ( 1 << self.analog_options[_id]['pos'])
                        if debug_0_debug:
                            print("register ", "output" + _id )
                        setattr(self, "output" + _id, self._sendValue ) 
                        pass
                    
                    elif _dir == self.IN and _pullup ==  self.ON:
                        self._analog_digital_input_pullups |= ( 1 << self.analog_options[_id]['pos'])
                        if debug_0_debug:
                            print("register ", "output" + _id )
                        
                        # sending values towards scratch is done by name of the method. So no need to prepare additional 
                        # function prototypes as in the various output functions.
                        # The prototypes are only needed to allow for matching the functions with configuration.
                        #
                        setattr(self, "output" + _id, self._sendValue ) 
                        pass
                    
                    elif _dir == self.IN:
                        self._analog_digital_inputs |= ( 1 << self.analog_options[_id]['pos'])
                        if debug_0_debug:
                            print("register ", "output" + _id )
                        setattr(self, "output" + _id, self._sendValue ) 
                        pass
                    
                    elif _dir == self.OUT:
                        self._analog_digital_outputs |= ( 1 << self.analog_options[_id]['pos'])
                        if debug_0_debug:
                            print("register ", "output" + _id )
                        method = MethodType(self.analog_options[_id]['ifunc'], self, type(self))
                        if debug_0_debug:
                            print("register ", "input" + _id )
                        setattr(self, "input" + _id, method )  
                         
                        pass
                else:
                    errorManager.append("{lc:s}: invalid attribute dir='{dir:s}' in arduino_uno:analog, id='{id:s}'".format( lc=loggingContext, id=_id, dir=_dir ))
                    continue

    ##
    ## ------------------------------------------
    ##
    def actionConnect(self, log=True):
        """  
            log=True: log on connect failure
            log=False: log on connect success
            
            return
                success
                fail """
        try:
            self.ser = serial.Serial(self.parameters[self.parameter_SERIAL_DEVICE],
                         int(self.parameters[self.parameter_SERIAL_BAUD]),
                         timeout=0.1 )
            if log == False:
                with helper.logging.LoggingContext(logger, level=logging.DEBUG):
                    logger.info("{n:s}: connection established to arduino".format(n=self.name))
                
        except serial.SerialException as e:
            if log:
                logger.error("{n:s}: no connection to {s:s} {e:s}".format(n=self.name, s=self.parameters[self.parameter_SERIAL_DEVICE], e=e))
            return "fail"
        except Exception as e:
            traceback.print_exc()
            logger.error("{n:s}: no connection to {s:s} {e:s}".format(n=self.name, s=self.parameters[self.parameter_SERIAL_DEVICE], e=e))
            return "fail"
        return "success"
    
     
    def actionLock(self):
        """  success
            fail """
        try:
            fcntl.ioctl(self.ser.fileno(), termios.TIOCEXCL)
            
        except IOError as e:
            logger.error("{n:s}: no lock to {s:s} {e:s}".format(n=self.name, s=self.parameters[self.parameter_SERIAL_DEVICE], e=e))
            
            return 'fail'
        return "success" 
    
    
    def actionReportDisconnect(self):
        logger.warning("{n:s}: lost connection to arduino".format(n=self.name))
        
    def actionDisconnect(self):
        """  success
            fail """
        try:
            self.ser.close()
            
        except serial.SerialException:
            return 'fail'
        except Exception:
            traceback.print_exc()
            return 'fail'
        return "success"
     
    def _runReceive(self):
        logger.debug("_runReceive %s", "start")
        while not self._stopped:
            try:
                line = self.ser.readline()
            except serial.SerialException as e:
                self.state_arduinoConfigured = "undef"
                self.stateMachine.serialError()
                break
            except Exception as e:
                traceback.print_exc()
                
                continue
            
            if sys.version_info.major == 3:
                line = line.decode('ascii')
                
            if ( line == ''):
                continue 
            
            line = line.rstrip()
            if show or debug_0_debug or debug_1_verbose:
                with helper.logging.LoggingContext(logger, level=logging.DEBUG):
                    # print ( type(line), line)
                    logger.info("serial  in: {l:s}".format( l=line )) 
                
            if line.startswith( 'config?' ):
                if show: print("found config request")
                
                #if debug_0_debug:
                #    self._queue_put_prio("help")
                    
                if debug_0_debug or debug_1_verbose or debug_2_blink:    
                    xdebug = 0
                    if debug_0_debug:
                        xdebug |= 1
                    if debug_1_verbose:
                        xdebug |= 2 
                    if debug_2_blink:
                        xdebug |= 4
                    self._queue_put_prio("cdebug:{data:04x}".format(data= xdebug))
                    
                self._queue_put_prio("cversion?")
                
                self._queue_put_prio("cident?")
                #self.queue.put("cident:NANO_000")
                
                if self._analog_analog_inputs != 0:
                    self._queue_put_prio("caain:{data:04x}".format(data=self._analog_analog_inputs))  
                
                if self._analog_digital_inputs != 0:
                    self._queue_put_prio("cadin:{data:04x}".format(data=self._analog_digital_inputs))  
                
                if self._analog_digital_input_pullups != 0:
                    self._queue_put_prio("cadinp:{data:04x}".format(data=self._analog_digital_input_pullups))  
                
                if self._analog_digital_outputs != 0:
                    self._queue_put_prio("cadout:{data:04x}".format(data=self._analog_digital_outputs))
                
                if self._digital_inputs != 0:  
                    self._queue_put_prio("cdin:{data:04x}".format(data=self._digital_inputs))
                
                if self._digital_input_pullups != 0:  
                    self._queue_put_prio("cdinp:{data:04x}".format(data=self._digital_input_pullups))
                
                if self._digital_counter != 0:  
                    self._queue_put_prio("cdcnt:{data:04x}".format(data=self._digital_counter))
                
                if self._digital_counter_pullups != 0:  
                    self._queue_put_prio("cdcntp:{data:04x}".format(data=self._digital_counter_pullups))
                
                if self._digital_outputs != 0:
                    self._queue_put_prio("cdout:{data:04x}".format(data=self._digital_outputs))
                
                if self._digital_pwms != 0:
                    self._queue_put_prio("cdpwm:{data:04x}".format(data=self._digital_pwms))
                
                if self._digital_servos != 0:   
                    self._queue_put_prio("cdservo:{data:04x}".format(data=self._digital_servos))
                
                self._queue_put_prio("#CONFIGURED")
                
            elif line.startswith( 'arduino' ):
                # ignore 'arduino sending@115200 Bd'
                # ignore 'arduinoUno, version 2017-01-27'
                pass
            elif line.startswith( 'ident:' ):
                pass
            #
            elif line.startswith( 'v:' ):
                pass
            #
            elif line.startswith( 'ai' ):
                x = line[2:].split(',')
                port  = x[0]
                value = x[1]
                # print("port", port, "value", value)
                self.sendValueByName( 'outputA' + port,  value)
            #
            elif line.startswith( 'i' ):
                x = line[1:].split(',')
                port  = x[0]
                value = x[1]
                # print("port", port, "value", value)
                self.sendValueByName( 'outputD' + port,  value)
            #
            # counter values are hex
            #
            elif line.startswith( 'c' ):
                x = line[1:].split(',')
                port  = x[0]
                value = x[1]
                # counters are transmitted as HEX values
                value = int(value, 16)
                # print("port", port, "value", value)
                self.sendValueByName( 'outputD' + port,  value)
            #
            elif line.startswith( 'a' ):
                try:
                    x = line[1:].split(',')
                    port  = x[0]
                    value = x[1]
                    # print("port", port, "value", value)
                    self.sendValueByName( 'outputA' + port,  value)
                except IndexError:
                    print("IndexError")
            #
            elif line.startswith( 'e:' ):
                logger.info("error count " + line)
            else:
                logger.info("undefined: " + line)
        
        logger.debug("_runReceive %s", "end")

    def _runSend(self):
        logger.debug("_runSend %s", "start")
        self.state_arduinoConfigured = "undef"
        t_r = time.time()
        
        while not self._stopped:
            if self.state_arduinoConfigured == "undef":
                foundData = False         
                try:
                    s = self.queue_command.get(block=True, timeout= 0.1)
                    foundData = True
                except helper.abstractQueue.AbstractQueue.Empty:
                    pass 
                
                if foundData:
                    try:
                        if s == '#CONFIGURED':
                            self.state_arduinoConfigured = 'CONFIGURED'
                            # put the last value received from scratch onto the queue. Use prio 1 for this.
                            # there is the faint possibility that some values are lost, as only nextState is set here.
                            if sys.version_info.major == 2:
                                for val in self.lastInputValue.itervalues():
                                    self.queue_data.put(1, val)
                            if sys.version_info.major == 3:
                                for val in iter( self.lastInputValue.values() ):
                                    self.queue_data.put(1, val)
                            continue
                        else:        
                            if show:
                                logger.info("serial out: {l:s}".format( l=s )) 
                            self._sendSerial(s+"\n")
                        
                        # slow down in debug_0_debug mode
                        if debug_0_debug or debug_1_verbose:
                            self.delay(0.4)
                    except serial.SerialException as e:
                        self.state_arduinoConfigured = "undef"
                        logger.error("{n:s}: close serial line".format(n=self.name))
                        self.stateMachine.serialError()
                    except Exception as e:
                        self.state_arduinoConfigured = "undef"
                        traceback.print_exc()
                        self.stateMachine.serialError()
                
            elif self.state_arduinoConfigured == 'CONFIGURED':
                #
                # every now and when put a dummy command to arduino (reset buffer, newline is the 'clean'-magic)
                #
                if time.time() > t_r + 300:
                    t_r =  time.time()
                    self._queue_put_prio("cerr?")  
                
                foundData = False         
                try:
                    s = self.queue_command.get(block=False)
                    foundData = True
                except helper.abstractQueue.AbstractQueue.Empty:
                    pass
    
                if not( foundData):
                    try:
                        s = self.queue_data.get(block=True, timeout= 0.1)
                        foundData = True
                    except helper.abstractQueue.AbstractQueue.Empty:
                        pass
                     
                if foundData:
                    try:
                        if show:
                            with helper.logging.LoggingContext(logger, level=logging.DEBUG):
                                logger.info("{n:s}: serial out: {l:s}".format( n=self.name, l=s )) 
                            
                        self._sendSerial(s+"\n")
                        # slow down in debug_0_debug mode
                        if debug_0_debug or debug_1_verbose:
                            self.delay(0.4)
                    except serial.SerialException as e:
                        self.state_arduinoConfigured = "undef"
                        logger.error("{n:s}: close serial line".format(n=self.name))
                        self.stateMachine.serialError()
                    except Exception as e:
                        self.state_arduinoConfigured = "undef"
                        traceback.print_exc()
                        self.stateMachine.serialError()
            #
        try:
            if show:
                logger.info("serial out: {l:s}".format( l='disconnect' )) 
            self._sendSerial("disconnect"+"\n")

        except Exception as e:
            pass
                
        logger.debug("_runSend %s", "end")

    def _sendSerial(self, data):
        if sys.version_info.major == 2:
            self.ser.write(data+"\n");
            
        if sys.version_info.major == 3:
            self.ser.write( bytearray( data+"\n", encoding='ascii') );
            self.ser.flush()
            
    def actionStartThreads(self):
        """  success
            fail """
        self._stopped = False
        self.threadReceive = threading.Thread(target=self._runReceive)
        self.threadReceive.setName("_runReceive")
        self.threadReceive.start()
    
        self.threadSend = threading.Thread(target=self._runSend)
        self.threadSend.setName("_runSend")
        self.threadSend.start()
    
        return "success" 
    
    def actionStopThreads(self):
        self._stopped = True

        self.threadReceive.join(0.2)
        self.threadSend.join(0.2)
        
        if self.threadReceive.isAlive():
            logger.error("receive thread not stopped !")
        if self.threadSend.isAlive():
            logger.error("send thread not stopped !")
        return "success" 
    
    def checkPosix(self):
        """  posix 
             noposix  """
        if os.name == 'posix':
            return "posix"
        return 'noposix'

    ##
    ## ------ states for the connection handling -----------------------------
    ##
    class StateTimer:
        def start(self, t, statemachine):
            self.stateMachine = statemachine
            self.t = t
            self.stopped = False
            self.thread1 = threading.Thread(target=self.runTimer)
            self.thread1.setName("timer")
            self.thread1.start()
            
        def runTimer(self):
            tx = 0
            while tx < self.t:
                if self.stopped: break
                time.sleep(0.1)
                tx += 0.1
            if not self.stopped: self.stateMachine.addEvent("timeout")     
    
        def stop(self):
            self.stopped = True
                
    class STATE:
        def __init__(self):
            self.timer = UNO_Adapter.StateTimer()
            
        stateMachine = None
        parent = None
        
        def entry(self):
            pass
        def exit(self):
            pass
        
        def start(self):
            logger.error("{n:s}: start() not handled".format(n=self.name()))
            return None
        
        def stop(self):
            logger.error("{n:s}: stop() not handled".format(n=self.name()))
            return None
        def serialError(self):
            logger.error("{n:s}: serialError() not handled".format(n=self.name()))
            return None
        def success(self):
            logger.error("{n:s}: success() not handled".format(n=self.name()))
            return None
                
        def timeout(self):
            logger.error("{n:s}: timeout() not handled".format(n=self.name()))
            return None
                
        def startTimer(self, t):
            self.timer.start(t, self.stateMachine)
        def stopTimer(self):
            self.timer.stop()
            
        def name(self):
            return self.__class__.__name__
        
    class START ( STATE):
        def __init__(self):
            UNO_Adapter.STATE.__init__(self)
                    
        def start(self):
            return UNO_Adapter.CONNECTING()
    
    class CONNECTING ( STATE):
        def __init__(self):
            UNO_Adapter.STATE.__init__(self)
        
        def entry(self):
            res = self.parent.actionConnect(log=True)
            self.stateMachine.addEvent(res)
    
        def stop(self):
            return UNO_Adapter.STOP()
                
        def success(self):
            if "posix" == self.parent.checkPosix():
                return UNO_Adapter.LOCK()
            return UNO_Adapter.CONNECTED()
        
        def fail(self):
            return UNO_Adapter.WAIT_START()
        
    class CONNECTING2 ( CONNECTING):
        def __init__(self):
            UNO_Adapter.STATE.__init__(self)
        
        def entry(self):
            res = self.parent.actionConnect(log = False)
            self.stateMachine.addEvent(res)
    
        
    
    class LOCK ( STATE):
        def __init__(self):
            UNO_Adapter.STATE.__init__(self)

        def entry(self):
            res = self.parent.actionLock()
            self.stateMachine.addEvent(res)
            
        def success(self):
            return UNO_Adapter.CONNECTED()
        
        def stop(self):
            self.parent.actionDisconnect()
            return UNO_Adapter.STOP()
        
        def fail(self):
            return UNO_Adapter.WAIT_LOCK()
        
    class LOCK_RETRY ( STATE):
        def __init__(self):
            UNO_Adapter.STATE.__init__(self)

        def entry(self):
            res = self.parent.actionLock()
            self.stateMachine.addEvent(res)
            
        def success(self):
            return UNO_Adapter.CONNECTED()
        
        def fail(self):
            return UNO_Adapter.CONNECTING()
        
        def stop(self):
            self.parent.actionDisconnect()
            return UNO_Adapter.STOP()
    
        def exit(self):
            self.parent.Disconnect()
        
    class CONNECTED ( STATE):
        def __init__(self):
            UNO_Adapter.STATE.__init__(self)
        
        def entry(self):
            self.parent.actionStartThreads()
        
        def exit(self):
            self.parent.actionStopThreads()
            self.parent.actionDisconnect()
            
        def serialError(self):
            self.parent.actionReportDisconnect()
            return UNO_Adapter.WAIT_START()
        
        
        def stop(self):
            return UNO_Adapter.STOP()
        
    class STOP ( STATE):
        def __init__(self):
            UNO_Adapter.STATE.__init__(self)
        
        def start(self):
            return UNO_Adapter.CONNECTING()
        
    class WAIT_START ( STATE):
        def __init__(self):
            UNO_Adapter.STATE.__init__(self)
        
        def entry(self):
            self.startTimer(1.0)
        
        def exit(self):
            self.stopTimer()
    
        def timeout(self):
            return UNO_Adapter.CONNECTING2()
        
        def stop(self):
            return UNO_Adapter.STOP()
        
    class WAIT_LOCK ( STATE):
        def __init__(self):
            UNO_Adapter.STATE.__init__(self)
        
        def entry(self):
            self.startTimer(0.3)
        
        def exit(self):
            self.stopTimer()
    
        def timeout(self):
            return UNO_Adapter.LOCK_RETRY()
        
        def stop(self):
            self.parent.actionDisconnect()
            return UNO_Adapter.STOP()
    
    class StateMachine:
        state = None
        
        def __init__(self, parent):
            self.parent = parent
            self.state = UNO_Adapter.START()
            self.state.parent = parent
            self.queue = helper.abstractQueue.AbstractQueue()
        
        def run_stateQueueHandler(self):
            while not self.stopQueueHandler:
                s = ""
                try:
                    s = self.queue.get(block=True, timeout=0.1)
                except:
                    continue
                # print("event", s)
                if s == 'start': self._handle( s, self.state.start())
                elif s == 'stop': self._handle( s, self.state.stop())
                elif s == 'timeout': self._handle( s, self.state.timeout())
                elif s == 'success': self._handle( s, self.state.success())
                elif s == 'fail': self._handle( s, self.state.fail())
                elif s == 'serialError': self._handle( s, self.state.serialError())
                else: 
                    logger.error("no matching event found ", s)
                
        def _start(self):
            self.queue = helper.abstractQueue.AbstractQueue()
            
            self.stopQueueHandler = False
            self.thread1 = threading.Thread(target=self.run_stateQueueHandler)
            self.thread1.setName("run_stateQueueHandler")
            self.thread1.start()
            
        def _stop(self):
            self.stopQueueHandler = True
            self.thread1.join(0.2)
            if self.thread1.isAlive():
                logger.error("Error: thread {n:s}  not stopped".format(n=self.thread1.getName()))
                
        def addEvent(self, event):
            self.queue.put(event)
                
        def start(self):
            self.addEvent("start")
            
            
        def stop(self):
            self.addEvent("stop")
            
        def serialError(self):
            self.addEvent("serialError")
            
        def _handle(self, event,  newState):
            if newState == None:
                return
                
            stateName = self.state.name()
            newStateName = newState.name()
            
            if logger.isEnabledFor(logging.INFO):
                logger.info( '{f:s} --[{s:s}]--> {t:s}'.format(f=stateName,s=str(event),t=newStateName ) )
            if newState != self.state:
                self.state.exit()
                self.state = newState
                self.state.stateMachine = self
                self.state.parent = self.parent
                self.state.entry()
           
        def setActive (self, state):
            if debug_0_debug:
                print(self.name, "setActive", state)
            if state:
                pass
                
            adapter.adapters.Adapter.setActive(self, state)
            if state:
                self.thread1 = threading.Thread(target=self.run_queueHandler)
                self.thread1.setName(self.name+"queue")
                self.thread1.start()
                pass
            #print(self.name + ": setActibe finished")
    
    
    
                    
    def command(self, value):
        """low level command interface for arduino."""
        try:
            # print("value", value)
            allowed = ['help', 'cversion?', 'cerr?', 'cident?']
            if value in allowed:   
                self._queue_put_prio(value)
            else:
                logger.error("{n:s}: command not allowed {c:s} {a:s}".format(n=self.name, c=value, a=allowed))
        except Exception as e:
            print (e)
                                 
#
# ##########################################################################################
#
class UNO_POWERFUNCTIONS_Adapter (adapter.adapters.Adapter):
    """Interface to UNO, Arduino Serial adapter 
       Controls LEGO Powerfunctions sending by IR transmitter
       Needs sketch 'power_functions' on arduino.
       The command choosen for powerfunctions have timeout enabled; refresh is handled in adapter.
    """
    
    parameter_SERIAL_DEVICE = 'serial.device'
    parameter_SERIAL_BAUD = 'serial.baud'

    mandatoryParameters = { parameter_SERIAL_DEVICE: 'COM6',
                           parameter_SERIAL_BAUD : '115200'
                          }
    
    def __init__(self):
        
        adapter.adapters.Adapter.__init__(self)
        self.state = 0
        self.queue = helper.abstractQueue.AbstractQueue()
        self.channelData = { 
                        '1': { 'update': 0, 'set': False, 'A': '0', 'B': '0' },
                        '2': { 'update': 0, 'set': False, 'A': '0', 'B': '0' },
                        '3': { 'update': 0, 'set': False, 'A': '0', 'B': '0' },
                        '4': { 'update': 0, 'set': False, 'A': '0', 'B': '0' },
                       }
    #
    # Functions  input setters
    # these are added dynamically to the class if needed.
    #
    def CHANNEL_1_A(self, value):
        self._set_A_IO( '1', 'A', value)
    def CHANNEL_1_B(self, value):
        self._set_A_IO( '1', 'B', value)
        
    def CHANNEL_2_A(self, value):
        self._set_A_IO( '2', 'A', value)
    def CHANNEL_2_B(self, value):
        self._set_A_IO( '2', 'B', value)
        
    def CHANNEL_3_A(self, value):
        self._set_A_IO( '3', 'A', value)
    def CHANNEL_3_B(self, value):
        self._set_A_IO( '3', 'B', value)
        
    def CHANNEL_4_A(self, value):
        self._set_A_IO( '4', 'A', value)
    def CHANNEL_4_B(self, value):
        self._set_A_IO( '4', 'B', value)
  
   
    def _refreshChannelData(self):
        for channel in ('1', '2', '3', '4'):
            portData = self.channelData[channel]
            if portData['set']:
                if portData['update'] + 0.40 < time.time():
                    portData['update'] = time.time()
                    self.queue.put(self._getChannelCommand(channel))
            
    def _getChannelCommand (self, channel):
        portData = self.channelData[channel]
        aaaa = portData['A']
        bbbb = portData['B']

        s = "P0{channel:s}{bbbb:s}{aaaa:s}". format(channel=channel, bbbb=bbbb, aaaa=aaaa)
        return s
        
    def _setChannelData (self, channel, port, value):
        portData = self.channelData[channel]
        portData[port] = value
        portData['update'] = time.time()
        
        if portData['A'] == '0' and portData['B'] == '0':
            portData['set'] = False
        else:        
            portData['set'] = True
            
        self.queue.put(self._getChannelCommand(channel))

    def _set_A_IO(self, channel, port, _value):
        """ BRAKE or -7 to 7
            As values could be floats, try to convert to integer
        """
        
        if _value == 'BRAKE':
            pass
        else:
            try:
                x = int( _value)
                _value = str(x)
            except:
                # convert float falues to next int
                try:
                    x = float( _value)
                    _value = round(x, 0)
                    _value = int(_value)
                    _value = str(_value)
                except:
                    # print("error in int()")
                    pass
            
        if _value == '7':
            _x='7'
        elif _value == '6':
            _x='6'
        elif _value == '5':
            _x='5'
        elif _value == '4':
            _x='4'
        elif _value == '3':
            _x='3'
        elif _value == '2':
            _x='2'
        elif _value == '1':
            _x='1'
        elif _value == '0':
            _x='0'
        elif _value == '-7':
            _x='9'
        elif _value == '-6':
            _x='A'
        elif _value == '-5':
            _x='B'
        elif _value == '-4':
            _x='C'
        elif _value == '-3':
            _x='D'
        elif _value == '-2':
            _x='E'
        elif _value == '-1':
            _x='F'
        elif _value == 'BRAKE':
            _x='8'
        else:
            return
                        
        self._setChannelData(channel, port, _x)
        
        
      
    def setActive (self, state):
        if debug_0_debug:
            print(self.name, "setActive", state)
        if state:
            pass
            
        adapter.adapters.Adapter.setActive(self, state)
        if state:
            self.thread1 = threading.Thread(target=self.run_queueHandler)
            self.thread1.setName(self.name+"queue")
            self.thread1.start()
            pass
        #print(self.name + ": setActibe finished")
    
    START = 1000
    WAIT_CONNECT = 1001
    CONNECTED_0 = 1002
    CONNECTED = 1003
    STOPPED = 9999   
    
    def run_queueHandler(self):
        if debug_0_debug:
            print("run_queueHandler() start")
        self.state = self.START 
           
        
        while not self.stopped():
                
            if self.state == self.START:
                try:
                    self.ser = serial.Serial(self.parameters[self.parameter_SERIAL_DEVICE],
                                 int(self.parameters[self.parameter_SERIAL_BAUD]),
                                 timeout=0.1 )
                    # print("ser open")
                     
                    self.state = self.CONNECTED_0
                except Exception as e:
                    logger.error("{n:s}: no connection to {s:s}; {e:s}".format(n=self.name, s=self.parameters[self.parameter_SERIAL_DEVICE], e=e))
                    self.state = self.WAIT_CONNECT
                    
            elif self.state == self.WAIT_CONNECT:
                self.delay(10)
                self.state = self.START
                
            elif self.state == self.CONNECTED_0:
                self.state = self.CONNECTED
                if debug_0_debug or debug_1_verbose or debug_2_blink:
                    self.queue.put("D1")
                    
            elif self.state == self.CONNECTED:
                #
                # every now and when put a dummy command to arduino (reset buffer, newline is the 'clean'-magic)
                #
                
                try:
                    s = self.queue.get(block=True, timeout= 0.1)
                except helper.abstractQueue.AbstractQueue.Empty:
                    continue 
                try:
                    if show:
                        logger.debug("{n:s}: serial out: {l:s}".format(n=self.name,l= s )) 
                        
                    self.ser.write(s+"\n");
                    self.ser.flush()
                   
                except:
                    self.state = self.START
            #         
        if self.state == self.CONNECTED:
            self.state = self.STOPPED
            self.ser.close()
            
        logger.debug("{n:s}: run_queueHandler() stopped".format(n=self.name))
            
    def run(self):
        if debug_0_debug:
            print("run()")
        
        while not self.stopped():
            if self.state == self.CONNECTED:
                self._refreshChannelData()
                
                try:
                    line = self.ser.readline()
                except:
                    continue
                if ( line == ''):
                    continue 
                
                line = line.rstrip()
                logger.info("{n:s}: line {l:s}".format( n=self.name,l= line )) 
                #



class NEOPIXEL_Adapter (adapter.adapters.Adapter):
    """Interface to UNO, Arduino Serial adapter 
    
    """
    
    parameter_SERIAL_DEVICE = 'serial.device'
    parameter_SERIAL_BAUD = 'serial.baud'
    parameter_SHADOW = 'led.shadow'
    parameter_LENGTH = 'led.length'
    
    mandatoryParameters = { parameter_SERIAL_DEVICE: 'COM6',
                           parameter_SERIAL_BAUD : '115200',
                           parameter_SHADOW : 'true',
                           parameter_LENGTH: '144'
                          }

    def __init__(self):
        
        adapter.adapters.Adapter.__init__(self)
        self.commandQueue = helper.abstractQueue.AbstractQueue()

        self.received = ''
        self.waitFor = ''
        
        self.state = self.START
        #
       
    def setActive (self, state):
        if debug_0_debug:
            print(self.name, "setActive", state)
        if state:
            pass
            
        adapter.adapters.Adapter.setActive(self, state)
        if state:
            self.shadow = self.isTrue(self.parameters[self.parameter_SHADOW] )
            self.length = int( self.parameters[self.parameter_LENGTH] )
            self.shadow = []
            for _ in range( self.length):
                self.shadow.append( '')
                
            self.thread1 = threading.Thread(target=self.run_queueHandler)
            self.thread1.setName(self.name+"queue")
            self.thread1.start()
            pass
        #print(self.name + ": setActibe finished")
    
    START = 1000
    WAIT_START = 1003
    
    
    WAIT_LOCK = 1002
    LOCK = 1004
    CONNECTED = 1005
    WAIT_RESPONSE = 1010
    STOPPED = 9999   
    
    def run_queueHandler(self):
        if debug_0_debug:
            print("run_queueHandler() start")
        self.state = self.START 
        nextState = None
           
        lock_loop = 0
        
        while not self.stopped():
                
            if self.state == self.START:
                try:
                    self.ser = serial.Serial(self.parameters[self.parameter_SERIAL_DEVICE],
                                 int(self.parameters[self.parameter_SERIAL_BAUD]),
                                 timeout=0.1 )
                    # print("ser open")
                    lock_loop = 0 
                    nextState = self.WAIT_LOCK
                except Exception as e:
                    logger.error("{n:s}: no connection to {s:s} {e:s}".format(n=self.name, s=self.parameters[self.parameter_SERIAL_DEVICE], e=e))
                    nextState = self.WAIT_START
                    
            elif self.state == self.WAIT_START:
                self.delay(5)
                
                nextState = self.START
            
            elif self.state == self.WAIT_LOCK:
                if os.name == 'posix':
                    
                    self.delay(0.02)
                    lock_loop += 1
                    if lock_loop == 100:
                        logger.error("{n:s}: no lock available to {s:s} {e:s}".format(n=self.name, s=self.parameters[self.parameter_SERIAL_DEVICE], e=e))
                        self.ser.close()
                        nextState = self.WAIT_START
                    else:
                        nextState = self.LOCK
                else:
                    nextState = self.CONNECTED
                
                
            elif self.state == self.LOCK:
                # lock access if system is a linux 'something
                #
                try:
                    fcntl.ioctl(self.ser.fileno(), termios.TIOCEXCL)
                    nextState = self.CONNECTED
                except IOError as e:
                    logger.error("{n:s}: no lock to {s:s} {e:s}".format(n=self.name, s=self.parameters[self.parameter_SERIAL_DEVICE], e=e))
                    self.ser.close()
                    nextState = self.WAIT_LOCK
                pass
            
            elif self.state == self.CONNECTED:
                #
                # every now and when put a dummy command to arduino (reset buffer, newline is the 'clean'-magic)
                         
                try:
                    s = self.commandQueue.get(block=True, timeout= 0.01)
                except helper.abstractQueue.AbstractQueue.Empty:
                    continue 
                try:
                    if show:
                        logger.debug("serial out: {l:s}".format( l=s )) 
                
                    self.waitFor = s        
                    self.writeTime = time.time()
                    self.ser.write(s+"\n");
                    self.ser.flush()
                    nextState = self.WAIT_RESPONSE
                except:
                    nextState = self.START
            #
            
            elif self.state == self.WAIT_RESPONSE:
                if False:
                    time.sleep(0.2)
                    nextState = self.CONNECTED
                else:
                    # logger.debug ( "compare '"+   self.waitFor + "'  '" + self.received + "'")
                    if self.waitFor == self.received:
                        
                        # print("found input string", self.waitFor)
                        nextState = self.CONNECTED
                        
                    else:
                        time.sleep(0.04)
                        # print(self.writeTime + 5 , time.time())
                        if self.writeTime + 2 < time.time():
                            logger.error("{n:s}: did not receive correct string '{s:s}'".format(n=self.name, s=self.waitFor) )
                            nextState = self.CONNECTED
                        
            if show:
                if self.state != nextState:
                    print("state ({s:d}) --> ({ns:d})".format(s=self.state, ns=nextState))

            self.state = nextState
            # end while
                     
        if self.state == self.CONNECTED:
            self.state = self.STOPPED
            self.ser.close()
            
        if debug_0_debug:
            print("run_queueHandler() stopped")
            
    def run(self):
        """receive data from serial"""
        if debug_0_debug:
            print("run() start")
        
        while not self.stopped():
            try:
                line = self.ser.readline()
            except:
                continue
            if ( line == ''):
                continue 
            
            line = line.rstrip()
            if ( line == ''):
                continue 
            if show or debug_0_debug or debug_1_verbose:
                logger.debug("line: {l:s}".format( l=line )) 
            #
            self.received = line
        if debug_0_debug:
            print("run() stopped")
            
    def led(self, value):
        if debug_0_debug:
            print("led", value)
        if value == '':
            return
        try:
            # sequence of r, g, b-Values 
            _bytes = []
            values = value.split(' ')
            
            if len(values) > int(self.parameters['led.length']):
                value = value[0:int(self.parameters['led.length'])]
                
            for i in range( len( values) ):    
                k = values[i]
                
                if self.shadow:
                    if self.shadow[i] == k:
                        continue
                    self.shadow[i] = k
                
                color = self.getRGBFromString(k)
                if i == 0:
                    si = ''
                else:
                    si="{n:d}".format(n=i)
                    
                if color['red'] == 0:
                    sr = ''
                else:
                    sr = "{r:d}".format(r=color['red'])
                    
                if color['green'] == 0:
                    sg = ''
                else:
                    sg = "{r:d}".format(r=color['green'])
                    
                if color['blue'] == 0:
                    sb = ''
                else:
                    sb = "{r:d}".format(r=color['blue'])
               
                s = "s," + si + "," + sr + ',' + sg + ',' + sb 
                
                self.commandQueue.put(s)
            self.commandQueue.put('w')
            
        except Exception as e:
            logger.error(e)
            pass        

    def red(self):
        if debug_0_debug:
            print("red")
        self.commandQueue.put('red')
            
    def green(self):
        if debug_0_debug:
            print("green")
        self.commandQueue.put('green')
            
    def blue(self):
        if debug_0_debug:
            print("blue")
        self.commandQueue.put('blue')
            
    def clear(self ):
        if debug_0_debug:
            print("clear")

        if self.shadow:
            for i in range( self.length ):
                self.shadow[i] = ''

        self.commandQueue.put('clear')
            
