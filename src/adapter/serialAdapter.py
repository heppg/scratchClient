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
import threading
import time
import serial
import re

import logging
logger = logging.getLogger(__name__)

debug = False

# --------------------------------------------------------------------------------------

class SIM800_State:
    """Base class for states"""
    parent = None
    
    def __init__(self, name, parent):
        self.parent = parent
        self.name = name
        pass
    def entry(self):
        pass
    def exit(self):
        pass
    def line(self, line):
        logger.info("{name:s} line '{line:s}'".format(name=self.name, line=line))
        pass
    def sms_receive(self, smsId): 
        pass
    def timeout(self):
        pass
    def stop(self):
        pass
    def ok(self):
        pass
    
class SIM800_TimeoutState(SIM800_State):
    """Base class for states"""
    timeoutThread = None
    _stopEvent = None
    t = None
    
    def __init__(self, name, parent):
        SIM800_State.__init__(self, name, parent)
        self._stopEvent = threading.Event()

    def entry(self):
        logger.info("{name:s} entry()".format(name=self.name))
        
        SIM800_State.entry(self)
        self.startTimeout(3)
        
    def stopTimeout(self):
        self._stopEvent.set()
        if self.timeoutThread != None:
            try:
                self.timeoutThread.join(0.2)
                if self.timeoutThread.isAlive():
                    logger.debug(self.name + " no timely join in adapter")
            except RuntimeError as e:
                logger.error(e)
                        
    def startTimeout(self, t):
        self.t0 = t
        self._stopEvent.clear()    
        self.timeoutThread = threading.Thread(target=self.run)
        self.timeoutThread.setName(self.name + "_timeout")
        self.timeoutThread.start()
    
    def run(self):
        logger.info("{name:s} timoutThread, run(), wait till {t0:d}".format(name=self.name, t0=self.t0))
        tx = 0
        while tx < self.t0 and not(self.parent.stopped()) and not(self.stopped()):
            time.sleep(0.1)
            tx += 0.1
        
        if not(self.stopped()):
            logger.info("tx = {tx:f}".format(tx=tx))
            logger.info("timeout parent " + str(self.parent.stopped()))
            logger.info("timeout")
            self.timeout()
            
    def exit(self):
        SIM800_State.exit(self)
        self.stopTimeout()
        
    def timeout(self):
        pass

    def stopped(self):
        """helper method for the thread's run method to find out whether a stop is pending"""
        return self._stopEvent.isSet()

class SIM800_State_START(SIM800_TimeoutState):
    """START"""
    
    def __init__(self, parent):
        
        SIM800_TimeoutState.__init__(self, "START", parent)
        pass
    
    def entry(self):
        SIM800_TimeoutState.entry(self)
        self.parent.modem_at()
    
    def ok(self):
        logger.info("{name:s} ok".format(name=self.name))
        self.parent.setStateByName('INIT_000')

    def stop(self):
        logger.info("{name:s} stop".format(name=self.name))
        self.parent.setStateByName('STOP')
        
    def timeout(self):
        logger.info("{name:s} modem not available".format(name=self.name))
        
        self.parent.setStateByName('FAIL')
        
class SIM800_State_INIT_PIN(SIM800_TimeoutState):
    """START"""
    def __init__(self, parent, name, nextState_ok, nextState_nopin):
        SIM800_TimeoutState.__init__(self, name, parent)
        self.nextState_ok = nextState_ok
        self.nextState_nopin = nextState_nopin
        pass
    
    def entry(self):
        SIM800_TimeoutState.entry(self)
        self.parent.modem_at_cmd('at+cpin?')
        
    def line(self, line):
        logger.info("{name:s} line {line:s}".format(name=self.name, line=line))
        if line == '+CPIN: SIM PIN':
            self.parent.setStateByName(self.nextState_ok)    
            return
        if line == '+CPIN: READY':
            self.parent.setStateByName(self.nextState_nopin)
            return
        if line == 'at+cpin?':
            return
        
        self.parent.setStateByName('STOP')
        
    def timeout(self):
        logger.info("{name:s} timeout: modem not available".format(name=self.name))
        self.parent.setStateByName('FAIL')

    def stop(self):
        logger.info("{name:s} stop".format(name=self.name))
        self.parent.setStateByName('STOP')

class SIM800_State_INIT_SETPIN(SIM800_TimeoutState):
    """START"""
    def __init__(self, parent, name, nextState):
        SIM800_TimeoutState.__init__(self, name, parent)
        self.nextState = nextState
        pass
    
    def entry(self):
        SIM800_TimeoutState.entry(self)
        self.parent.modem_pin()
        
    def ok(self):
        logger.info("{name:s} ok".format(name=self.name))
        self.parent.setStateByName(self.nextState)    
        pass
    
    def timeout(self):
        logger.info("{name:s} modem not available".format(name=self.name))
        self.parent.setStateByName('FAIL')

    def stop(self):
        logger.info("{name:s} stop".format(name=self.name))
        self.parent.setStateByName('STOP')
        
class SIM800_State_INIT_ATCMD(SIM800_TimeoutState):
    """START"""
    def __init__(self, parent, name, nextState, cmd):
        SIM800_TimeoutState.__init__(self, name, parent)
        self.cmd = cmd
        self.nextState = nextState
    
    def entry(self):
        SIM800_TimeoutState.entry(self)
        self.parent.modem_at_cmd(self.cmd)
        
    def ok(self):
        logger.info("{name:s} ok".format(name=self.name))
        self.parent.setStateByName(self.nextState)    
        pass
    
    def timeout(self):
        logger.info("{name:s} modem not available".format(name=self.name))
        self.parent.setStateByName('FAIL')

    def stop(self):
        logger.info("{name:s} stop".format(name=self.name))
        self.parent.setStateByName('STOP')

class SIM800_State_STOP(SIM800_State):
    """START"""
    def __init__(self, parent):
        SIM800_State.__init__(self, "STOP", parent)
        pass

class SIM800_State_FAIL(SIM800_TimeoutState):
    """START"""
    def __init__(self, parent):
        self.t = 30
        SIM800_TimeoutState.__init__(self, "FAIL", parent)
        pass
    
    def entry(self):
        SIM800_TimeoutState.entry(self)
        self.parent.modem_at()

    def timeout(self):
        logger.info("{name:s} modem not available".format(name=self.name))
        self.parent.setStateByName('START')

    def stop(self):
        logger.info("{name:s} stop".format(name=self.name))
        self.parent.setStateByName('STOP')

class SIM800_State_OPERATE(SIM800_TimeoutState):
    """START"""
    def __init__(self, parent):
        self.t = 30
        SIM800_TimeoutState.__init__(self, "OPERATE", parent)
        pass
    
    def entry(self):
        SIM800_TimeoutState.entry(self)
        self.parent.modem_at()

    def stop(self):
        logger.info("{name:s} stop".format(name=self.name))
        self.parent.setStateByName('STOP')
        
    def sendSMS(self, value):
        logger.info("{name:s} send sms {value:s}".format(name=self.name, value=value))
        self.parent.modem_sendSMS(value)
    
    def receiveSMS(self, startline, msg):
        self.parent.sms_in(msg)         
        
    def receiveSMSNotification(self, _index):
        print("index", _index, type(_index))
        self.parent.modem_at_cmd("at+cmgr={idx:d}".format(idx=_index))         
            
class SIM800_Adapter (adapter.adapters.SPIAdapter):
    """Interface to SIM800_Adapter Serial """
    
    parameter_SERIAL_DEVICE = 'serial.device'
    parameter_SERIAL_BAUD = 'serial.baud'
    parameter_SERIAL_PIN = 'sim.pin'
    parameter_REMOTE_NUMBER = 'remote.number'

    mandatoryParameters = { parameter_SERIAL_DEVICE: '/dev/ttyAMA0',
                           parameter_SERIAL_BAUD : '38400',
                           parameter_SERIAL_PIN :'0000',
                           parameter_REMOTE_NUMBER : '00009999999'
                          }

    statesByName = None
    
    def __init__(self):
        adapter.adapters.Adapter.__init__(self)
        self.statesByName = {
        'START' : SIM800_State_START(self),
        
        'INIT_000': SIM800_State_INIT_PIN(self, 'INIT_000', 'INIT_002', 'INIT_100'),
        
        'INIT_002': SIM800_State_INIT_SETPIN(self, 'INIT_002', 'INIT_100'),
        
        'INIT_100': SIM800_State_INIT_ATCMD(self, 'INIT_100', 'INIT_101', 'AT+CMGF=1'),
        'INIT_101': SIM800_State_INIT_ATCMD(self, 'INIT_101', 'INIT_102', 'AT+GSN'),
        'INIT_102': SIM800_State_INIT_ATCMD(self, 'INIT_002', 'OPERATE', 'AT'),

        'FAIL' : SIM800_State_FAIL(self),
        'STOP' : SIM800_State_STOP(self),
        'OPERATE': SIM800_State_OPERATE(self),
        
        }
    def setActive (self, state):
        if debug:
            print(self.name, "setActive", state)
        #
        # use default implementation to start up SPI
        #
        adapter.adapters.Adapter.setActive(self, state)
        if state == False:
            self.setStateByName('STOP')

    def sms_in(self, value):
        if debug:
            print("sms_in", value)
        self.sendValue('"' + unicode(value, encoding='iso-8859-1', errors='replace') + '"')
        
                
    def sms_out(self, value):
        logger.info("sms_out {value:s}".format(value=value))
        if '' == value.strip(' \t\n\r'):
            logger.error("{name:s} ignore empty sms value".format(self.name))
            return
        if len(value) > 160:
            logger.error("{name:s} text longer then 160 chars".format(self.name))
            return
        
        try:
            self.bState.sendSMS(value)                                
        except Exception as e:
            print(e)
            pass        

    ser = None
    
    threadSerialReceive = None

    def modem_at(self):
        logger.info("modem AT")
        self.ser.write("AT")
        self.ser.write("\n")
        self.ser.flush()
        
    def modem_at_cmd(self, cmd):
        logger.info("modem {cmd:s}".format(cmd=cmd))
        self.ser.write(cmd)
        self.ser.write("\n")
        self.ser.flush()
        
    def modem_pin(self):
        logger.info("modem set PIN")
        self.ser.write("at+cpin=")
        self.ser.write(self.parameters[self.parameter_SERIAL_PIN])
        self.ser.write("\n")
        self.ser.flush()
    
    def modem_sendSMS(self, value):
        logger.info("modem send SMS {value:s}".format(value=value))
        self.ser.write('AT+CMGS="{number:s}"'.format(number=self.parameters[ self.parameter_REMOTE_NUMBER ]))
        
        self.ser.write("\n")
        self.ser.write(value)
        #
        # write ctrl-z
        #
        self.ser.write("\x1a")
        self.ser.flush()
    
    def runSerialReceive(self):
        self.ser.timeout = 0.05
        #
        # the current line read in
        #
        line = ''
        #
        # stat == 0: normal line
        # stat = 1000: cmti, continued lines
        #
        stat = 0
        #
        # cmti results
        #
        cmti = []
                
        while not(self.stopped()):
            x = self.ser.read()
            #
            # empty result, due to timeout
            #
            if x == '':
                continue
            # print("received", x)
            if x == '\n':
                continue
            #
            # line complete
            #
            if x == '\r':
                logger.info("stat={stat:4d} line='{line:s}'".format(stat=stat, line=line))
                if stat == 1000:
                    cmti.append(line)
                    stat = 1001
                    line = ''
                    continue
                if stat == 1001:
                    cmti.append(line)
                    if line == '':
                        stat = 1002
                        line = ''
                        continue
                if stat == 1002:
                    if line == 'OK':
                        stat = 0
                        msg = ''
                        for x in cmti[1: ]:
                            msg += x.strip()

                        self.bState.receiveSMS(cmti[0], msg)
                        cmti = []    
                        line = ''
                        continue
                    else:
                        logger.error("unknown sequence, received {line:s}".format(line=line))
                        stat = 0
                        line = ''
                        continue
                if stat == 0:
                    if line == 'OK':
                        self.bState.ok()
                        
                    elif line.startswith('+CMTI:'):
                        index = None
                        expr = '\\+CMTI: "SM",([0-9]+)'
                        p = re.compile (expr)
                        m = p.match(line)
                        if m != None:
                            index = int(m.group(1))                     
                        else:
                            expr = '\\+CMTI: "ME",([0-9]+)'
                            p = re.compile (expr)
                            m = p.match(line)
                            if m != None:
                                index = int(m.group(1))                     
                        if index != None:
                            self.bState.receiveSMSNotification(int(index))   
                            line = ''
                            continue
                    elif line.startswith('+CMGR:'):
                        cmti.append(line)
                        stat = 1000
                        line = ''
                        continue
                    else:
                        self.bState.line(line)    
                    line = ''
                    continue
            line += x
            if debug:
                print ("line", line)    
        
    bState = None
        
    def setState(self, newState):
        _in = ''
        if self.bState != None:
            _in = self.bState.name
            self.bState.exit()
        
        self.bState = newState
        
        l = "STATE {_in:s} --> {out:s}".format(_in=_in, out=newState.name)
        # print(l)
        logger.info(l)
        self.bState.entry()
        
    def setStateByName(self, newState):
        _in = ''
        if self.bState != None:
            _in = self.bState.name
            self.bState.exit()
        
        self.bState = self.statesByName.get(newState)
        
        l = "STATE {_in:s} --> {out:s}".format(_in=_in, out=self.bState.name)
        # print(l)
        logger.info(l)
        self.bState.entry()
                        
    def run(self):
        #
        # initialize Modem 
        #
        self.ser = serial.Serial(self.parameters[self.parameter_SERIAL_DEVICE],
                                 int(self.parameters[self.parameter_SERIAL_BAUD]),
                                 timeout=1)
        
        self.threadSerialReceive = threading.Thread(target=self.runSerialReceive)
        self.threadSerialReceive.setName(self.name + "_SerialREceive")
        self.threadSerialReceive.start()

        self.setStateByName('START')
        
        while not (self.stopped()):
            time.sleep(0.05)

class PicoBoard_Adapter (adapter.adapters.Adapter):
    """Interface to sparkfun PicoBoard Serial """
    
    parameter_SERIAL_DEVICE = 'serial.device'
    parameter_SERIAL_BAUD = 'serial.baud'

    parameter_PICOBOARD_RAW = 'picoBoard.raw'

    mandatoryParameters = { parameter_SERIAL_DEVICE: '/dev/ttyUSB0',
                           parameter_SERIAL_BAUD : '38400',
                           parameter_PICOBOARD_RAW : 'false'
                          }

    
    def __init__(self):
        adapter.adapters.Adapter.__init__(self)
    
    def run(self):
        if debug:
            print("thread started")
            
        self.raw = self.isTrue(self.parameters[self.parameter_PICOBOARD_RAW])   
        STATE_START = 0
        STATE_START_N = 1
        STATE_RECONNECT = 10
        STATE_CONNECTED_INIT = 20
        STATE_CONNECTED = 22
        
        STATE_RECORD_INIT = 42
        STATE_RECORD = 52
        STATE_COMPLETE = 100
        
        state = STATE_START
        newState = None
        
        data_0 = 0
        data_1 = 0                
        cnt = 0
        
        data = {}
        data_old = {}
        packets = [15, 0, 1, 2, 3, 4, 5, 6, 7 ]
        
        while not(self.stopped()):
            
            if state == STATE_START:
                try:
                    self.ser = serial.Serial(self.parameters[self.parameter_SERIAL_DEVICE],
                                 int(self.parameters[self.parameter_SERIAL_BAUD]),
                                 timeout=0.5)
                    newState = STATE_CONNECTED_INIT
                except Exception as e:
                    if debug:
                        print(e)
                    logger.error(self.name + ": could not connect to " + self.parameters[self.parameter_SERIAL_DEVICE])
                    newState = STATE_RECONNECT

            elif state == STATE_START_N:
                try:
                    self.ser = serial.Serial(self.parameters[self.parameter_SERIAL_DEVICE],
                                 int(self.parameters[self.parameter_SERIAL_BAUD]),
                                 timeout=0.5)
                    logger.info(self.name + ": connected to " + self.parameters[self.parameter_SERIAL_DEVICE])
                    newState = STATE_CONNECTED_INIT
                except Exception as e:
                    if debug:
                        print(e)
                    newState = STATE_RECONNECT
                
            elif state == STATE_RECONNECT:
                self.delay(0.5)
                newState = STATE_START_N
                  
            elif state == STATE_CONNECTED_INIT:
                cnt = 0
                newState = STATE_CONNECTED
                
            elif state == STATE_CONNECTED:
                # can be there are data in some receive buffer
                # or a system is sending data not according to the protocol
                b = self.ser.read(size=1)
                if len(b) > 0:
                    cnt += 1
                    if cnt > 100:
                        newState = STATE_RECONNECT
                else:
                    # 0.5 sec long no data, looks good
                    newState = STATE_RECORD_INIT
                
            elif state == STATE_RECORD_INIT:  
                self.delay(0.0825)   
                try:
                    self.ser.write('\01')
                    newState = STATE_RECORD
                except Exception as e:
                    logger.error("{name:s}: write error ({err:s})".format(name=self.name, err=str(e)))
                    newState = STATE_START
                
            
            elif state == STATE_RECORD:     
                b = self.ser.read(size=18)
                if len(b) < 18:
                    logger.info("did not get 18 bytes")
                    newState = STATE_RECONNECT
                else:
                    # 18 bytes received, check data
                    error = False
                    for i in range (0, 9):
                        data_0 = ord(b[i * 2 + 0])
                        data_1 = ord(b[i * 2 + 1])
                        
                        if data_0 & 0x80 == 0:
                            error = True
                            logger.error("{name:s}: protocol error (bit 0:7==0 of {n:d})".format(name=self.name, n=i))
                            newState = STATE_CONNECTED_INIT
                            break
                        if data_1 & 0x80 == 0x80:
                            error = True
                            logger.error("protocol error (bit 1:7==1 of {n:d})".format(name=self.name, n=i))
                            newState = STATE_CONNECTED_INIT
                            break
                        channel = (data_0 & 0b01111000) >> 3
                        value = (data_0 & 0b00000111) << 7 | data_1
                        expChannel = packets[i]
                        if channel != expChannel:
                            logger.error("{name:s}: protocol error (packet channel cur={cur:d} exp={exp:d} ) ".format(name=self.name, exp=expChannel, cur=channel))
                            error = True
                            newState = STATE_CONNECTED_INIT
                            break 
                        if debug:
                            print("channel", channel, "value", value)
                        data[channel] = value   
                    if  error:
                        pass
                    else:
                        newState = STATE_COMPLETE
                
                
            elif state == STATE_COMPLETE:
                for c in data:
                    if debug:
                        print("process channel", c)
                    val = data[c]
                    val_old = None
                    if c in data_old:
                        val_old = data_old[c]
                        
                    if val != val_old:
                        data_old[c] = val
                        if c == 0:
                            self.sensorD(val)
                        elif c == 1:
                            self.sensorC(val)
                        elif c == 2:
                            self.sensorB(val)
                        elif c == 3:
                            self.button(val)
                        elif c == 4:
                            self.sensorA(val)
                        elif c == 5:
                            self.light(val)
                        elif c == 6:
                            self.sound(val)
                        elif c == 7:
                            self.slider(val)
                            
                newState = STATE_RECORD_INIT

            else:
                logger.error(self.name + "undefined state " + state)   
            if debug:
                print("{s:d} --> {ns:d}".format(s=state, ns=newState))
            state = newState     
        try:
            self.ser.close()
        except Exception:
            pass
        
    def reverseScale(self, value):
        """ 100/1023 """
        return 0.097752 * (1023.0 - value) 
    def scale(self, value):
        """ 100/1023 """
        return 0.097752 * value 
    
    def slider(self, value):
        if debug:
            print("slider", value)
        if self.raw:
            self.sendValue(value)
        else:
            self.sendValue(self.scale(value))
            
    def light(self, value):
        if debug:
            print("light", value)
        if self.raw:
            self.sendValue(value)
        else:
            self.sendValue(self.reverseScale(value))
            
    def sound(self, value):
        if debug:
            print("sound", value)
        if self.raw:
            self.sendValue(value)
        else:
            self.sendValue(self.scale(value))
        
    def button(self, value):
        if debug:
            print("button", value)
        if self.raw:
            self.sendValue(value)
        else:
            if value == 0:
                self.sendValue('"true"')
            else:
                self.sendValue('"false"')
                    
    def sensorA(self, value):
        if debug:
            print("sensorA", value)
        if self.raw:
            self.sendValue(value)
        else:
            self.sendValue(self.scale(value))
            
    def sensorB(self, value):
        if debug:
            print("sensorB", value)
        if self.raw:
            self.sendValue(value)
        else:
            self.sendValue(self.scale(value))
    def sensorC(self, value):
        if debug:
            print("sensorC", value)
        if self.raw:
            self.sendValue(value)
        else:
            self.sendValue(self.scale(value))
        
    def sensorD(self, value):
        if debug:
            print("sensorD", value)
        if self.raw:
            self.sendValue(value)
        else:
            self.sendValue(self.scale(value))

class RFID_Reader_Adapter (adapter.adapters.Adapter):
    """Interface to INNOVATIONS ID-12LA, ID-Reader on Serial Line """
    
    parameter_SERIAL_DEVICE = 'serial.device'
    parameter_SERIAL_BAUD = 'serial.baud'


    mandatoryParameters = { parameter_SERIAL_DEVICE: '/dev/ttyAMA0',
                           parameter_SERIAL_BAUD : '9600'
                          }

    
    def __init__(self):
        adapter.adapters.Adapter.__init__(self)
    
    def run(self):
        if debug:
            print("thread started")
            
        try:    
            self.ser = serial.Serial(self.parameters[self.parameter_SERIAL_DEVICE],
                 int(self.parameters[self.parameter_SERIAL_BAUD]),
                 timeout=0.5)
        except:
            logger.error ("{name:s}: fatal exception, cannot connect to {line:s}", format (name=self.name, line=self.parameters[self.parameter_SERIAL_DEVICE]))
            return
        
        # protocol is 0x02 STX
        #             Data 10 ASCII
        #             CHKSUM 2 ASCII
        #             CR
        #             LF
        #             0x03 ETX
        state = 0
        data = '' 
        chksum = ''
        badcount = 0
        while not(self.stopped()):
            b = self.ser.read()
            if len(b) == 0:
                # no data received
                if state == 0:
                    continue
                else:
                    logger.error("{name:s}: protocol error, no data although midst in a transmission".format(name=self.name))
                    state = 0
                    continue
                
            if state == 0:
                if ord(b[0]) == 0x02:
                    data = ''
                    chksum = ''
                    state = 1
                if b[0] not in ( "\02\03\0d\0a0123456789ABCDEF"):
                    # print this error not too often
                    # allow for some bad chars at the beginning.
                    badcount += 1
                    if 10 < badcount < 20:
                        logger.error("{name:s}: char error, char not [0-9A-Z] or STX,ETX,CR,LF: 0x{x:02x}".format(name=self.name, x=ord(b[0])))
                    if badcount ==21:
                        logger.error("{name:s}: check baud rate or output of ID-12LA is D0".format(name=self.name, x=ord(b[0])))
                    
            elif state in (1,2,3,4,5,6,7,8,9,10):
                data += b[0]
                state += 1
            elif state in (11,12):
                chksum += b[0]
                state += 1
            elif state == 13:
                if ord(b[0]) == 0x0d:
                    state = 14
                else:
                    logger.error("{name:s}: protocol error, expected CR, but received {x:02x}".format(name=self.name, x=b[0]))
                    state = 0
            elif state == 14:
                if ord(b[0]) == 0x0a:
                    state = 15
                else:
                    logger.error("{name:s}: protocol error, expected LF, but received {x:02x}".format(name=self.name, x=b[0]))
                    state = 0
            elif state == 15:
                if ord(b[0]) == 0x03:
                    state = 0
                    if self.checksum(data, chksum):
                        self.data(data)
                        self.data_event()
                    else:
                        logger.error("{name:s}: checksum error,  {data:s}".format(name=self.name, data=data))
                else:
                    logger.error("{name:s}: protocol error, expected ETX, but received {x:02x}".format(name=self.name, x=b[0]))
                    state = 0
                    
    def checksum(self, data,chksum):
        return True
                
    def data(self, value):
        if debug:
            print("data", value)
        self.sendValue( '"' + value + '"')
        
    def data_event(self):
        if debug:
            print("data_event")
        self.send()
