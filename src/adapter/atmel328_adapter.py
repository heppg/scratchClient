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

# -----------------------------------------------------------------------
#
# Adapter for an atmel atmega328-Controller attached to RPI and
# acting as a slave-SPI ADC-Converter and more.
# For communication, it is using SPI, one GPIO for RESET, one GPIO for 'signal'.
#
# This adapter needs two hardware managers, SPI and GPIO.
#
# -----------------------------------------------------------------------

import logging
logger = logging.getLogger(__name__)

import time
import adapter.adapters
from spi.manager import SPIManager


debug = False

# --------------------------------------------------------------------------------------
class Atmel328_Adapter(adapter.adapters.Adapter):
    # -----------------------------------------
    # 
    debug = False
    # -----------------------------------------
    
    # Fields, SPI-related 
    spiManager = None
    
    def setSPIManager(self, spiManager):
        if debug:
            print("setSPIManager()", self.name, spiManager)
        self.spiManager = spiManager

    # -----------------------------------------
    # Fields, GPIO related
    gpios = None
    gpioManager = None
    
    def setGpioManager(self, gpioManager):
        if debug:
            print("setGpioManager")
            
        logger.debug("setGpioManager() {name:s} {gm:s}".format( name=self.name, gm=str(gpioManager) ))
        self.gpioManager = gpioManager

    # -----------------------------------------
    # cached parameters, converted to int from the hashtable provided.
    int_spi_bus = 0 
    int_spi_device = 0 

    # -----------------------------------------
    # fields for adapter
    int_timer_enable = 0
    
    TIMERMODE_TIME_PERIOD = 'timePeriod'
    TIMERMODE_TIMEDCOUNTER_10MS = 'timedCounter_10ms'
    TIMERMODE_TIMEDCOUNTER_20MS = 'timedCounter_20ms'
    
    timer_mode = TIMERMODE_TIME_PERIOD
    
    int_adc_0_enable = 1
    int_adc_1_enable = 1
    str_adc_0_reference = 'avcc'
    str_adc_1_reference = 'avcc'
    
    # -----------------------------------------
    mandatoryParameters = { 
                           'spi.bus'      :  '0', 
                           'spi.device'   :  '0',
                           'poll.interval': '0.05',
                          }
    
    # -----------------------------------------
    # Constants for the connected Atmel328-Processor with special program
    # these code are not generic. They depend on what is coded in the micro-
    # controller   
    #
    SET_LED_0          = 0x84
    SET_LED_1          = 0x88
    
    GET_ADC_0          = 0x81
    GET_ADC_1          = 0x82
    
    GET_VERSION        = 0x80
    
    GET_ADC_CONFIG     = 0x91 
    SET_ADC_CONFIG     = 0x92
    
    GET_TIMER          = 0x87
    GET_TIMEDCOUNTER   = 0x89

    SET_COUNTER_CONFIG = 0x85
    GET_COUNTER_CONFIG = 0x86

    SET_IRC_CONFIG     = 0x8a
    GET_IRC_DATA_0     = 0x8b
    GET_IRC_DATA_1     = 0x8c
    GET_IRC_DATA_2     = 0x8d
    GET_IRC_DATA_3     = 0x8e
    
    
    def __init__(self):
        # SPI initialization code
        
        # GPIO initialization code
        self.gpios = []
        
        # General Adapter
        
        adapter.adapters.Adapter.__init__(self)

    def led_on(self):
        """command from scratch"""
        if debug:
            print("led_on" )
        
        self.spiManager.writeRawData( 
                                     self.int_spi_bus,      
                                     self.int_spi_device, 
                                     [self.SET_LED_1 ] )

    def led_off(self):
        """command from scratch"""
        if debug:
            print("led_off" )
        
        self.spiManager.writeRawData( self.int_spi_bus,      
                              self.int_spi_device,  [self.SET_LED_0 ] )

    def get_adc_0 (self):
        if self.debug:
            print("cpu_get_adc_0")
        
        r = self.spiManager.writeRawData( self.int_spi_bus,      
                              self.int_spi_device, list( [ self.GET_ADC_0, 0, 0]))
        return r[1] * 256 + r[2]
    
    def get_adc_1 (self):
        if self.debug:
            print("cpu_get_adc_1")
        
        r = self.spiManager.writeRawData( self.int_spi_bus,      
                              self.int_spi_device, list( [self.GET_ADC_1, 0, 0]))
        return r[1] * 256 + r[2]

    def get_irc_data(self):
        if self.debug:
            print("cpu_irc_data")
        
        e = []
        for _ in range(64):
            e.append(0);
        
        l = [ self.GET_IRC_DATA_0 ]
        l.extend(e);
        r0 = self.cpu_cmd_r( list( l ))
        
        l = [ self.GET_IRC_DATA_1 ]
        l.extend(e);
        r1 = self.cpu_cmd_r(  list( l ))

        l = [ self.GET_IRC_DATA_2 ]
        l.extend(e);
        r2 = self.cpu_cmd_r(  list( l ))
        
        l = [ self.GET_IRC_DATA_3 ]
        l.extend(e);
        r3 = self.cpu_cmd_r( list( l ))

        r = []
        
        r.extend(r0[1:])
        r.extend(r1[1:])
        r.extend(r2[1:])
        r.extend(r3[1:])
        
        return r
    
    def cpu_cmd_r(self, l):
        return self.spiManager.writeRawData( self.int_spi_bus,      
                              self.int_spi_device, list(l))
    
    def get_timer(self):
        if self.debug:
            print("cpu_get_timer")
        
        r = self.cpu_cmd_r(list([self.GET_TIMER, 0, 0, 0, 0]))
        return r

    def get_timedcounter(self):
        if self.debug:
            print("cpu_get_timedcounter")
        
        r = self.cpu_cmd_r(list([self.GET_TIMEDCOUNTER, 0, 0]))
        return r
 
    
    def set_irc_config(self, c0):
        if self.debug:
            print("cpu_set_irc_config", c0)
        
        r = self.cpu_cmd_r(list([self.SET_IRC_CONFIG, c0 ]))
        return r
    
    def set_counter_config(self, c0, c1):
        if self.debug:
            print("cpu_set_counterconfig", c0, c1)
        
        r = self.cpu_cmd_r(list([self.SET_COUNTER_CONFIG, c0, c1]))
        return r
    
    def get_counter_config(self):
        if self.debug:
            print("cpu_get_counterconfig" )
        
        r = self.cpu_cmd_r(list([self.GET_COUNTER_CONFIG, 0, 0]))
        return r
    
    def get_adc_config(self):
        r = self.spiManager.writeRawData (
                                        self.int_spi_bus,      
                                        self.int_spi_device, 
                                        [self.GET_ADC_CONFIG, 0, 0 ] )
        return r

    def set_adc_config(self, c0, c1):
        if self.debug:
            print("cpu_set_adc_config", c0, c1)
        
        r = self.cpu_cmd_r(list([self.SET_ADC_CONFIG, c0, c1]))
        return r

    def get_version(self):
        r = self.spiManager.writeRawData (
                            self.int_spi_bus,      
                            self.int_spi_device, 
                            [self.GET_VERSION, 0, 0 ] )
        return r

# ----------------------------------------------------------------------------------------------

class Atmel328_ADC_Adapter (Atmel328_Adapter):
    """Interface to atmel atmega328, SPI and GPIO 
    The adapter uses spi- and gpio-related functionality.
    Not generic, a special implementation for a proof of concept.
    """
    
    def __init__(self):
        
        # General Adapter
        Atmel328_Adapter.__init__(self)

    def setActive (self, state):
        if debug:
            print(self.name, "setActive", state)
        # -----------------------------------------
        # SPI initialization
        #
        # cache the properties for faster access.
        self.int_spi_bus =      int(self.parameters['spi.bus'])
        self.int_spi_device =   int(self.parameters['spi.device']) 

        if self.parameters.has_key('adc.0.enable'):
            self.int_adc_0_enable = int(self.parameters['adc.0.enable'])
        if self.parameters.has_key('adc.1.enable'):
            self.int_adc_1_enable = int(self.parameters['adc.1.enable'])

        if self.parameters.has_key('adc.0.reference'):
            self.str_adc_0_reference = self.parameters['adc.0.reference']
        if self.parameters.has_key('adc.1.reference'):
            self.str_adc_1_reference = self.parameters['adc.1.reference']
        
        if debug:
            print(self.parameters)
        
        if self.parameters.has_key('timer.enable'):
            self.int_timer_enable = int(self.parameters['timer.enable'])
        if self.parameters.has_key('timer.noisecanceller'):
            self.int_timer_noisecanceller = int(self.parameters['timer.noisecanceller'])
        if self.parameters.has_key('timer.mode'):
            self.timer_mode = self.parameters['timer.mode']
            
        if state == True:
            if debug:
                print("spi open bus:{bus:d} device:{device:d}".format(bus=self.int_spi_bus, device=self.int_spi_device))   
                                  
            self.spiManager.open(self.int_spi_bus,      
                                 self.int_spi_device,
                                 self.spiManager.type_Atmel328     
                                )
        # ------------------------------------------------------
        # GPIO initialization
        #
        self.gpioManager.setActive(state)

        #if self.state == self.STATE_NORMAL:
        #    for gpio in self.gpios:
        #        self.gpioManager.setGPIOActive(gpio, state)
        #        pass 

        #
        # and generic things
        # starts thread, too
        #
        adapter.adapters.Adapter.setActive(self, state);
        
        if state == False:
            logger.debug("spi close bus:{bus:d} device:{device:d}".format(bus=self.int_spi_bus, device=self.int_spi_device))   
            self.spiManager.close()
              
               

    def run(self):
        _del = float(self.parameters['poll.interval'])
        # print (_del)

        # -----------------------
        # check version and wait till 
        # bus is active.
        #
        self.led_on()
        
        time.sleep(0.1)    
        for i in range (5):
            r = self.get_version()
            
            if r[1] != 0x93:
                logger.error( "Version mismatch [{i:3d}] {version:02x}".format(i=i, version=r[1]) )
                time.sleep(0.2)
            if r[1] == 0x93:
                break
        # -----------------------

        # special config (enable, reference level )
        c0 = c1  = 0
        
        if self.int_adc_0_enable == 1:            
            c0 |= 0x01
        if self.str_adc_0_reference == 'avcc':
            c0 |= 0x02
            
        if self.int_adc_1_enable == 1:            
            c1 |= 0x01
        if self.str_adc_1_reference == 'avcc':
            c1 |= 0x02
            
            
        r = self.get_adc_config()    

        logger.info("current config bytes  {adc_0:02x} {adc_1:02x} ".format(adc_0 = r[1], adc_1=r[2] ) )    
            
        logger.info("set     config bytes  {adc_0:02x} {adc_1:02x} ".format(adc_0 = c0, adc_1=c1 ) )
        
        r = self.set_adc_config( c0, c1  )
        r = self.get_adc_config()

        logger.info("current config bytes  {adc_0:02x} {adc_1:02x} ".format(adc_0 = r[1], adc_1=r[2] ) )    

        c0 = c1 = 0

        if self.int_timer_enable == 1:
            if self.timer_mode == self.TIMERMODE_TIME_PERIOD:
                c0 |= 0x08
            if self.timer_mode == self.TIMERMODE_TIMEDCOUNTER_10MS:
                c0 |= 0x20
            if self.timer_mode == self.TIMERMODE_TIMEDCOUNTER_20MS:
                c0 |= 0x60

            if self.int_timer_noisecanceller == 1:
                c0 |= 0x10
        
        self.set_counter_config(c0, c1) 
        r = self.get_counter_config() 

        logger.info("current counter config {adc_0:02x} {adc_1:02x} ".format(adc_0 = r[1], adc_1=r[2] ) )    
        
        self.led_on()

        l_adc_0 = 0
        l_adc_1 = 0    
                    
        if self.int_adc_0_enable == 1:            
            l_adc_0 = self.get_adc_0()
            self.analog_0( l_adc_0 )

        if self.int_adc_1_enable == 1:            
            l_adc_1 = self.get_adc_1()
            self.analog_1( l_adc_1 )

        if self.int_timer_enable == 1:            
            self.thread_readTimer()

        while not self.stopped():
        
            self.delay(_del)

            if self.int_adc_0_enable == 1:            
                c_adc_0 = self.get_adc_0()
                if c_adc_0 != l_adc_0:
                    self.analog_0 ( c_adc_0)
                    l_adc_0 = c_adc_0
        
            if self.int_adc_1_enable == 1:            
                c_adc_1 = self.get_adc_1()
                if c_adc_1 != l_adc_1:
                    self.analog_1 ( c_adc_1)
                    l_adc_1 = c_adc_1
                
            if self.int_timer_enable == 1:            
                self.thread_readTimer()
                    
    def thread_readTimer(self):
        if self.timer_mode == self.TIMERMODE_TIME_PERIOD:
            r = self.get_timer()
            if debug:
                logger.info( "Timer reported is: {v1:02x} {v2:02x} {v3:02x} {v4:02x} ".format(v1=r[1], v2=r[2], v3=r[3], v4=r[4]))

            timer_value = ( r[1] << 8 ) + r[2]
            timer_period = r[3]
            timer_err = r[4]
                
            if timer_err == 0:
                if timer_period > 0:
                    f = 8000000.0 *timer_period / timer_value 
                    self.timer( str(f) )
            else:
                logger.warn ("timer reports error {c1:02x} {c2:02x} {c3:02x} {c4:02x}".format(c1=r[1], c2=r[2], c3=r[3], c4=r[4] ))           
        if self.timer_mode == self.TIMERMODE_TIMEDCOUNTER_10MS:
            r = self.get_timedcounter()
            if debug:
                logger.info( "Timer reported is: {v1:02x} {v2:02x} ".format(v1=r[1], v2=r[2]))

            timer_value = ( r[1] << 8 ) + r[2]
            f = timer_value * 100
            self.timer( str(f) )
            
        if self.timer_mode == self.TIMERMODE_TIMEDCOUNTER_20MS:
            r = self.get_timedcounter()
            if debug:
                logger.info( "Timer reported is: {v1:02x} {v2:02x} ".format(v1=r[1], v2=r[2]))

            timer_value = ( r[1] << 8 ) + r[2]
            f = timer_value * 50
            self.timer( str(f) )
                
    def analog_0(self, value):
        """output from adapter to scratch."""
        self.sendValue(value)
    
    def analog_1(self, value):
        """output from adapter to scratch."""
        self.sendValue(value)
    
    def timer(self, value):
        """output from adapter to scratch."""
        self.sendValue(value)


class Atmel328_IRC_Adapter ( Atmel328_Adapter):
    """Interface to atmel atmega328, SPI and GPIO 
    The adapter uses spi- and gpio-related functionality.
    Not generic, a special implementatin for a proof of concept.
    """
    
    
    def __init__(self):
        # SPI initialization code
        
        # GPIO initialization code
        self.gpios = []
        
        # General Adapter
        
        Atmel328_Adapter.__init__(self)

    def setActive (self, state):
        if debug:
            print(self.name, "setActive", state)
        # -----------------------------------------
        # SPI initialization
        #
        # cache the properties for faster access.
        self.int_spi_bus =      int(self.parameters['spi.bus'])
        self.int_spi_device =   int(self.parameters['spi.device']) 

        if self.parameters.has_key('adc.0.enable'):
            self.int_adc_0_enable = int(self.parameters['adc.0.enable'])
        if self.parameters.has_key('adc.1.enable'):
            self.int_adc_1_enable = int(self.parameters['adc.1.enable'])

        if self.parameters.has_key('adc.0.reference'):
            self.str_adc_0_reference = self.parameters['adc.0.reference']
        if self.parameters.has_key('adc.1.reference'):
            self.str_adc_1_reference = self.parameters['adc.1.reference']

        if self.parameters.has_key('irc.enable'):
            self.int_irc_enable = int(self.parameters['irc.enable'])
        
        if debug:
            print(self.parameters)
        
        if self.parameters.has_key('timer.enable'):
            self.int_timer_enable = int(self.parameters['timer.enable'])
        if self.parameters.has_key('timer.noisecanceller'):
            self.int_timer_noisecanceller = int(self.parameters['timer.noisecanceller'])
            
        if state == True:
            if debug:
                print("spi open bus:{bus:d} device:{device:d}".format(bus=self.int_spi_bus, device=self.int_spi_device))   
                                  
            self.spiManager.open(self.int_spi_bus,      
                                 self.int_spi_device,
                                 self.spiManager.type_Atmel328     
                                )
        # ------------------------------------------------------
        # GPIO initialization
        #
        self.gpioManager.setActive(state)

        #if self.state == self.STATE_NORMAL:
        #    for gpio in self.gpios:
        #        self.gpioManager.setGPIOActive(gpio, state)
        #        pass 

        #
        # and generic things
        # starts thread, too
        #
        adapter.adapters.Adapter.setActive(self, state);
        
        if state == False:
            logger.debug("spi close bus:{bus:d} device:{device:d}".format(bus=self.int_spi_bus, device=self.int_spi_device))   
            self.spiManager.close()
              
               

    def run(self):
        # print("test: wait some secs")
        time.sleep(5)
        _del = float(self.parameters['poll.interval'])

        # -----------------------
        # check version and wait till 
        # bus is active.
        #
        self.led_on()
        
        time.sleep(0.1)    
        for i in range (5):
            r = self.spiManager.writeRawData (
                                        self.int_spi_bus,      
                                        self.int_spi_device, 
                                        [self.GET_VERSION, 0, 0 ] )
            
            if r[1] != 0x93:
                logger.error( "Version mismatch [{i:3d}] {version:02x}".format(i=i, version=r[1]) )
                time.sleep(0.2)
            if r[1] == 0x93:
                break
        # -----------------------

        # special config (enable, reference level )
        c0 = c1  = 0
        
        if self.int_adc_0_enable == 1:            
            c0 |= 0x01
        if self.str_adc_0_reference == 'avcc':
            c0 |= 0x02
            
        if self.int_adc_1_enable == 1:            
            c1 |= 0x01
        if self.str_adc_1_reference == 'avcc':
            c1 |= 0x02
            
        r = self.get_adc_config()
        logger.info("current config bytes  {adc_0:02x} {adc_1:02x} ".format(adc_0 = r[1], adc_1=r[2] ) )    
            
        logger.info("set     config bytes  {adc_0:02x} {adc_1:02x} ".format(adc_0 = c0, adc_1=c1 ) )
        self.set_adc_config(c0, c1) 
        r = self.get_adc_config() 
        logger.info("current config bytes  {adc_0:02x} {adc_1:02x} ".format(adc_0 = r[1], adc_1=r[2] ) )    

        c0 = c1 = 0

        if self.int_timer_enable == 1:
            # enable timer and filter            
            c0 |= 0x08
            if self.int_timer_noisecanceller == 1:
                c0 |= 0x10
                    
        r = self.set_counter_config( c0, c1  )
        r = self.get_counter_config( )
        
        logger.info("current counter config {adc_0:02x} {adc_1:02x} ".format(adc_0 = r[1], adc_1=r[2] ) )    
        
        aquisitionStatus = 10
        lastAquisitionStatus = -1
        
        timeout = 0
        gpioSignal = self.getChannelByAlias('signal')
        
        while not self.stopped():
            
            # if lastAquisitionStatus != aquisitionStatus:
            #     logger.info(aquisitionStatus)
            #     lastAquisitionStatus = aquisitionStatus
                
            self.delay(_del)
            
            if aquisitionStatus == 10:
                aquisitionStatus = 15
                continue
            
            if aquisitionStatus == 15:
                self.led_on()
                self.set_irc_config(3)
                aquisitionStatus = 20
                continue
            #
            # enable and send start signal
            #

            if aquisitionStatus == 20:
                # print( 20, self.gpioManager.get(gpioSignal) )

                if self.gpioManager.get(gpioSignal):
                    # no data
                    continue
                else:
                    aquisitionStatus = 30
                continue
                    
            if aquisitionStatus == 30:
                self.led_off()
            
                irc = self.get_irc_data()
                # print (irc)
                res = ''
                for i in range(0, len(irc), 2):
                    if irc[i] == 255 and irc[i+1]== 255:
                        break
                     
                    val = irc[i] * 256 + irc[i+1]
                    res += str(val) + ';'
                
                # need to mark a string as a "string"
                
                res = '"' + res + '"'
                    
                self.timing(res)
                
                rc5 = self.decodeRC5(irc)
                if rc5 != None:
                    self.rc5('"' + rc5 + '"')
                    
                aquisitionStatus = 40    
                continue
            
            if aquisitionStatus == 40:
                #
                # reset start signal
                #
                self.set_irc_config(1)
                aquisitionStatus = 50
                timeout = 0
                continue
            
            if aquisitionStatus == 50:
                #
                # wait for status line to be high
                #
                x = self.gpioManager.get(gpioSignal)
                # print( '4.0', x )
                if  x == 1:
                    aquisitionStatus = 10
                else:
                    timeout = timeout + _del
                    if timeout > 10.1:
                        logger.error("timeout waiting to handshake low")
                        aquisitionStatus = 10
                continue
                
    def timing(self, value):
        """output from adapter to scratch."""
        self.sendValue(value)

    def rc5(self, value):
        """output from adapter to scratch."""
        self.sendValue(value)

    def decodeRC5(self, irc):
        """state based parser for RC5 and some plausi check for the time slots"""
        ll = 0
        for i in range(0, len(irc), 2):
            if irc[i] == 255 and irc[i+1]== 255:
                break
            ll += 2
        
        # check range
        error = ''
        for i in range (0, ll, 2):
            val = irc[i] * 256 + irc[i+1]
            if val < 100:
                error = 'value smaller than 100'
            if val > 240:
                error = 'value larger than 240'
            if 125 < val < 200:
                error = 'value larger than 125, less than 200'
        if error != '':
            logger.error (error)
            return None
                    
        res = '1'
        state = '1'
        
        for i in range (0, ll, 2):
            val = irc[i] * 256 + irc[i+1]
            if val < 175:
                x = 'S'
            else:
                x = 'L'
            newState = state
            
            if state == '1':
                if x == 'S':
                    newState = '1s'
                if x == 'L':
                    newState = '0'
                    res += '0'
            elif state == '1s':
                if x == 'S':
                    newState = '1'
                    res += '1'
                if x == 'L':
                    newState = 'error'
            elif state == '0':
                if x == 'S':
                    newState = '0s'
                if x == 'L':
                    newState = '1'
                    res += '1'
            elif state == '0s':
                if x == 'S':
                    newState = '0'
                    res += '0'
                if x == 'L':
                    newState = 'error'
            elif state == 'error':
                logger.error ("error" )
                break;
            
            if debug:
                logger.debug("{s:s} -- [ {x:s} ] --> {ns:s}".format(s=state, ns=newState, x=x ))
                logger.debug("    {b:s}".format(b=res))
            
            state = newState
            
        if len(res) != 14:
            logger.error("error, not exactly 14 Bits: {r:s}".format(r=res) )
            return None
        else:
            logger.info("RC5 {r:s}".format(r=res) ) 
            return res           

# ----------------------------------------------------------------------------------------------

class Atmel328_DHT22_Adapter (Atmel328_Adapter):
    """Interface to atmel atmega328, SPI and GPIO 
    The adapter uses spi- and gpio-related functionality.
    Not generic, a special implementation for a proof of concept.
    """
    # -----------------------------------------
    mandatoryParameters = { 
                           'spi.bus'      :  '0', 
                           'spi.device'   :  '0',
                           'poll.interval':  '5.0',
                           
                           'sensor.device':  'DHT22'
                          }
    
    def __init__(self):
        
        # General Adapter
        Atmel328_Adapter.__init__(self)

    def setActive (self, state):
        if debug:
            print(self.name, "setActive", state)
        # -----------------------------------------
        # SPI initialization
        #
        # cache the properties for faster access.
        self.int_spi_bus =      int(self.parameters['spi.bus'])
        self.int_spi_device =   int(self.parameters['spi.device']) 


        if debug:
            print(self.parameters)
        
        if state == True:
            if debug:
                print("spi open bus:{bus:d} device:{device:d}".format(bus=self.int_spi_bus, device=self.int_spi_device))   
                                  
            self.spiManager.open(self.int_spi_bus,      
                                 self.int_spi_device,
                                 self.spiManager.type_Atmel328     
                                )
        # ------------------------------------------------------
        # GPIO initialization
        #
        self.gpioManager.setActive(state)

        #if self.state == self.STATE_NORMAL:
        #    for gpio in self.gpios:
        #        self.gpioManager.setGPIOActive(gpio, state)
        #        pass 

        #
        # and generic things
        # starts thread, too
        #
        adapter.adapters.Adapter.setActive(self, state);
        
        if state == False:
            logger.debug("spi close bus:{bus:d} device:{device:d}".format(bus=self.int_spi_bus, device=self.int_spi_device))   
            self.spiManager.close()
              
               

    def run(self):
        _del = float(self.parameters['poll.interval'])
        # print (_del)

        # -----------------------
        # check version and wait till 
        # bus is active.
        #
        self.led_on()
        
        time.sleep(0.1)    
        for i in range (5):
            r = self.get_version()
            
            if r[1] != 0x41:
                logger.error( "Version mismatch [{i:3d}] {version:02x}".format(i=i, version=r[1]) )
                time.sleep(0.2)
            if r[1] == 0x41:
                break
        # -----------------------

        self.led_on()

        c_temperature = None 
        l_temperature  = None
        
        c_humidity = None
        l_humidity = None
                
        while not self.stopped():
            self.delay(_del)
       
            r = self.get_result()
            # check error byte
            if r[5] == 0:
                if "DHT22" == self.parameters['sensor.device']:
                    c_temperature = self._convert_dht22(r[3], r[4])
                    c_humidity = self._convert_dht22(r[1], r[2])
                
                if "DHT11" == self.parameters['sensor.device']:
                    c_temperature = self._convert_dht11(r[3], r[4])
                    c_humidity = self._convert_dht11(r[1], r[2])
                
                if c_temperature != l_temperature:
                    l_temperature = c_temperature
                    self.temperature( c_temperature )
                    
                
                if c_humidity != l_humidity:
                    l_humidity = c_humidity
                    self.humidity( c_humidity )
            else:
                logger.error( "Error byte set 0x{error:02x}".format(error=r[5]) )
                
        self.led_off()
                   
    def _convert_dht22(self, data_2, data_3 ):
        f = data_2 & 0x7F;
        f *= 256;
        f += data_3;
        f = float(f) / 10.0;
        if (data_2 & 0x80):
            f *= -1.0;
        return f
    
    def _convert_dht11(self, data_2, data_3 ):
        f = data_2 & 0x7F;
        f = float(f) 
        return f
                
    def temperature(self, value):
        """output from adapter to scratch."""
        self.sendValue(value)
    
    def humidity(self, value):
        """output from adapter to scratch."""
        self.sendValue(value)
    
    GET_VERSION = 0x4e
    GET_RESULT  = 0x40
    
    SET_LED_0_0 = 0x44
    SET_LED_0_1 = 0x48
     
    def get_result(self):
        if self.debug:
            print("get_result")
        
        r = self.spiManager.writeRawData( self.int_spi_bus,      
                              self.int_spi_device, list( [self.GET_RESULT, 0, 0, 0, 0, 0]))
        return r
    

    def led_on(self):
        """command from scratch"""
        if debug:
            print("led_on" )
        
        self.spiManager.writeRawData( 
                                     self.int_spi_bus,      
                                     self.int_spi_device, 
                                     [self.SET_LED_0_1 ] )

    def led_off(self):
        """command from scratch"""
        if debug:
            print("led_off" )
        
        self.spiManager.writeRawData( self.int_spi_bus,      
                              self.int_spi_device,  [self.SET_LED_0_0 ] )

# ----------------------------------------------------------------------------------------------

class Atmel328_HCSR04_Adapter (Atmel328_Adapter):
    """Interface to atmel atmega328, SPI and GPIO 
    The adapter uses spi- and gpio-related functionality.
    Not generic, a special implementation for a proof of concept.
    """
    
    def __init__(self):
        
        # General Adapter
        Atmel328_Adapter.__init__(self)

    def setActive (self, state):
        if debug:
            print(self.name, "setActive", state)
        # -----------------------------------------
        # SPI initialization
        #
        # cache the properties for faster access.
        self.int_spi_bus =      int(self.parameters['spi.bus'])
        self.int_spi_device =   int(self.parameters['spi.device']) 

        if self.parameters.has_key('device.0.enable'):
            self.int_device_0_enable = int(self.parameters['device.0.enable'])
        if self.parameters.has_key('device.1.enable'):
            self.int_device_1_enable = int(self.parameters['device.1.enable'])

        if self.parameters.has_key('device.2.enable'):
            self.int_device_2_enable = int(self.parameters['device.2.enable'])
        if self.parameters.has_key('device.3.enable'):
            self.int_device_3_enable = int(self.parameters['device.3.enable'])

        if debug:
            print(self.parameters)
        
        if state == True:
            if debug:
                print("spi open bus:{bus:d} device:{device:d}".format(bus=self.int_spi_bus, device=self.int_spi_device))   
                                  
            self.spiManager.open(self.int_spi_bus,      
                                 self.int_spi_device,
                                 self.spiManager.type_Atmel328     
                                )
        # ------------------------------------------------------
        # GPIO initialization
        #
        self.gpioManager.setActive(state)

        #if self.state == self.STATE_NORMAL:
        #    for gpio in self.gpios:
        #        self.gpioManager.setGPIOActive(gpio, state)
        #        pass 

        #
        # and generic things
        # starts thread, too
        #
        adapter.adapters.Adapter.setActive(self, state);
        
        if state == False:
            logger.debug("spi close bus:{bus:d} device:{device:d}".format(bus=self.int_spi_bus, device=self.int_spi_device))   
            self.spiManager.close()
              
               

    def run(self):
        _del = float(self.parameters['poll.interval'])
        # print (_del)

        # -----------------------
        # check version and wait till 
        # bus is active.
        #
        self.led_on()
        
        time.sleep(0.1)    
        for i in range (5):
            r = self.get_version()
            
            if r[1] != 0x61:
                logger.error( "Version mismatch [{i:3d}] {version:02x}".format(i=i, version=r[1]) )
                time.sleep(0.2)
            if r[1] == 0x61:
                break
        # -----------------------

        # special config (enable, reference level )
        c0 = c1  = 0
        
        if self.int_device_0_enable == 1:            
            c0 |= 0x01
        if self.int_device_1_enable == 1:            
            c0 |= 0x02
        if self.int_device_2_enable == 1:            
            c0 |= 0x04
        if self.int_device_3_enable == 1:            
            c0 |= 0x08
            
            
        r = self.get_counter_config()    

        logger.info("current config bytes  {adc_0:02x} {adc_1:02x} ".format(adc_0 = r[1], adc_1=r[2] ) )    
            
        logger.info("set     config bytes  {adc_0:02x} {adc_1:02x} ".format(adc_0 = c0, adc_1=c1 ) )
        
        r = self.set_counter_config( c0, c1  )
        r = self.get_counter_config()

        logger.info("current config bytes  {adc_0:02x} {adc_1:02x} ".format(adc_0 = r[1], adc_1=r[2] ) )    
        
        self.led_on()

        l_device_0 = None
        l_device_1 = None
        l_device_2 = None
        l_device_3 = None
                    
        while not self.stopped():
            self.delay(_del)
       
            if self.int_device_0_enable == 1:            
                c_device_0 = self.get_counter_0()
                if c_device_0 != l_device_0:
                        l_device_0 = c_device_0
                        self.distance_0( c_device_0 )
    
            if self.int_device_1_enable == 1:            
                c_device_1 = self.get_counter_1()
                if c_device_1 != l_device_1:
                        l_device_1 = c_device_1
                        self.distance_1( c_device_1 )
    
            if self.int_device_2_enable == 1:            
                c_device_2 = self.get_counter_2()
                if c_device_2 != l_device_2:
                        l_device_2 = c_device_2
                        self.distance_2( c_device_2 )
    
            if self.int_device_3_enable == 1:            
                c_device_3 = self.get_counter_3()
                if c_device_3 != l_device_3:
                        l_device_3 = c_device_3
                        self.distance_3( c_device_3 )
                        
        self.led_off()
                   
                
    def distance_0(self, value):
        """output from adapter to scratch."""
        self.sendValue(value)
    
    def distance_1(self, value):
        """output from adapter to scratch."""
        self.sendValue(value)
    
    def distance_2(self, value):
        """output from adapter to scratch."""
        self.sendValue(value)
    
    def distance_3(self, value):
        """output from adapter to scratch."""
        self.sendValue(value)

    GET_VERSION = 0x6e
    GET_TESTDATA = 0x6f
    
    SET_COUNTER_CONFIG = 0x65
    GET_COUNTER_CONFIG = 0x66
    GET_COUNTER_0      = 0x60
    GET_COUNTER_1      = 0x61
    GET_COUNTER_2      = 0x62
    GET_COUNTER_3      = 0x63
    
    SET_LED_0 = 0x64
    SET_LED_1 = 0x68
     
    def get_counter_0(self):
        if self.debug:
            print("get_counter_0")
        
        r = self.spiManager.writeRawData( self.int_spi_bus,      
                              self.int_spi_device, list( [self.GET_COUNTER_0, 0, 0, 0, 0, 0, 0, 0]))
        err = r[7]
        if err != 0x00:
            return None
        
        t0 = r[1] + r[2] *256
        t1 = r[3] + r[4] *256
        s = (t1 - t0 ) * 0.000008 *340/2
        return "{d:.2f}".format(d=s)
    
    def get_counter_1(self):
        if self.debug:
            print("get_counter_0")
        
        r = self.spiManager.writeRawData( self.int_spi_bus,      
                              self.int_spi_device, list( [self.GET_COUNTER_1, 0, 0, 0, 0, 0, 0, 0]))
        err = r[7]
        if err != 0x00:
            return None
        
        t0 = r[1] + r[2] *256
        t1 = r[3] + r[4] *256
        s = (t1 - t0 ) * 0.000008 *340/2
        return "{d:.2f}".format(d=s)
    
    def get_counter_2(self):
        if self.debug:
            print("get_counter_0")
        
        r = self.spiManager.writeRawData( self.int_spi_bus,      
                              self.int_spi_device, list( [self.GET_COUNTER_2, 0, 0, 0, 0, 0, 0, 0]))
        err = r[7]
        if err != 0x00:
            return None
        
        t0 = r[1] + r[2] *256
        t1 = r[3] + r[4] *256
        s = (t1 - t0 ) * 0.000008 *340/2
        return "{d:.2f}".format(d=s)

    def get_counter_3(self):
        if self.debug:
            print("get_counter_0")
        
        r = self.spiManager.writeRawData( self.int_spi_bus,      
                              self.int_spi_device, list( [self.GET_COUNTER_3, 0, 0, 0, 0, 0, 0, 0]))
        err = r[7]
        if err != 0x00:
            return None
        
        t0 = r[1] + r[2] *256
        t1 = r[3] + r[4] *256
        s = (t1 - t0 ) * 0.000008 *340/2
        return "{d:.2f}".format(d=s)

    def led_on(self):
        """command from scratch"""
        if debug:
            print("led_on" )
        
        self.spiManager.writeRawData( 
                                     self.int_spi_bus,      
                                     self.int_spi_device, 
                                     [self.SET_LED_1 ] )

    def led_off(self):
        """command from scratch"""
        if debug:
            print("led_off" )
        
        self.spiManager.writeRawData( self.int_spi_bus,      
                              self.int_spi_device,  [self.SET_LED_0 ] )

    def set_counter_config(self, c0, c1):
        if self.debug:
            print("cpu_set_counterconfig", c0, c1)
        
        r = self.cpu_cmd_r(list([self.SET_COUNTER_CONFIG, c0, c1]))
        return r
    
    def get_counter_config(self):
        if self.debug:
            print("cpu_get_counterconfig" )
        
        r = self.cpu_cmd_r(list([self.GET_COUNTER_CONFIG, 0, 0]))
        return r
    
            