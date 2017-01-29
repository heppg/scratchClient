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

# 
# For this adapter, there are additional libraries needed.
#
# 
# sudo apt-get python-smbus
# sudo modprobe i2c-bcm2708
# sudo modprobe i2c-dev
#
#
import RPIO2.PWM 
import logging
logger = logging.getLogger(__name__)

debug=True

         
class _DMARegistry:
    channels = None
    
    def __init__(self):
        self.channels = {}
    
    def isChannelInitialized( self, channel):
        if self.channels.has_key( channel):
            return True
        return False
         
    def channelInitialize(self, channel,time_n_micro_seconds):
        self.channels[channel] = time_n_micro_seconds
 
    def getChannelPeriod(self, channel):
        return self.channels[channel]
        
class DMAManager:
    """DMAManager for dma-pulse width"""
    CHANNEL = 14
    dmaRegistry = None

    def __init__(self):
        self.dmaRegistry = _DMARegistry()

    def setActive(self, state):
        """called from main module, but has no function for this manager"""
        if state == True:
            if debug:
                RPIO2.PWM.set_loglevel( RPIO2.PWM.LOG_LEVEL_DEBUG )
            else:
                RPIO2.PWM.set_loglevel( RPIO2.PWM.LOG_LEVEL_ERRORS )
            RPIO2.PWM.setup()
        else:
            RPIO2.PWM.clear_channel(self.CHANNEL) 
     
    def startPWM(self, gpio, frequency, value ):
        """value is a float in range 0..1"""
        if debug:
            print("startPWM", gpio)
        # pulse width
        if self.dmaRegistry.isChannelInitialized( self.CHANNEL ):
            pass
        else:
            self.dmaRegistry.channelInitialize( self.CHANNEL, int(1000000 / frequency))
            RPIO2.PWM.init_channel( self.CHANNEL, int(1000000 / frequency) )
            
        nd = 1000000/frequency / 10
        ndx = int( nd * value )    
        RPIO2.PWM.set_pwm ( self.CHANNEL, gpio.getPort(), ndx )

    def set_pwm(self, gpio, value):
        """value is a float in range 0..1"""
        nd = self.dmaRegistry.getChannelPeriod(self.CHANNEL)
        nd = nd / 10
        ndx = int( nd * value )
        #print("set_pwm", value, nd, ndx)
        if ndx < 0:
            ndx = 0
        if ndx > nd:
            ndx = nd
        RPIO2.PWM.set_pwm ( self.CHANNEL, gpio.getPort(), ndx )
        
    def clear_channel_gpio( self, gpio):
        RPIO2.PWM.clear_channel_gpio ( self.CHANNEL, gpio.getPort() )
            
    def close(self):
        """closes all connections, selective shutdowns are not implemented"""
        # pass
        RPIO2.PWM.clear_channel(self.CHANNEL)
        