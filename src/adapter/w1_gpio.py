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

# need to enable w2-driver
#
# sudo modprobe w1-gpio pullup=1
#
import adapter
import glob
import logging
import re

logger = logging.getLogger(__name__)
debug = False

# --------------------------------------------------------------------------------------
#
# Two wire protocol (not I2C).
#
class W1_DS1820 (adapter.adapters.Adapter):
    """DS1820 and related devices, using w1-gpio driver"""
    mandatoryParameters = {
                           'poll.interval': 0.5,
                           'w1.device' : '10-000000000000'
                        }
    
    def __init__(self):
        adapter.adapters.Adapter.__init__(self)
        # self.start()
        pass

    baseDir = '/sys/bus/w1/devices'
    
    #
    # report file errors only once
    # this variable is to track state of already reported errors
    #
    fileError = False
    
    def _read_Temperature(self):
        
        path = self.baseDir + '/' + self.parameters['w1.device'] + '/' + 'w1_slave'

        #0f 00 4b 46 ff ff 06 10 0c : crc=0c YES
        #0f 00 4b 46 ff ff 06 10 0c t=7375         
        try:
            f = open( path, 'r')
            text_0 = f.readline()
            text_1 = f.readline()
            f.close()
        except IOError:
            if self.fileError == False:
                logger.error('{name:s}: cannot open file {file:s}'.format(name=self.name, file=path))
            self.fileError = True
            return None
        self.fileError = False
            
        text_0 = text_0.rstrip('\n')
        
        if not( text_0.endswith('YES') ):   
            logger.error('{name:s}: file {file:s} no YES'.format(name=self.name, file=path))
            logger.error(text_0)
            return None     

        pattern = 't=(-?[0-9]+)([0-9]{3})'
        m = re.search( pattern, text_1)
        if m == None:   
            logger.error('{name:s}: file {file:s} no temp-value'.format(name=self.name, file=path))
            logger.error(text_0)
            return None     
        t = m.group(1) + '.' + m.group(2)
        return t

    def _checkBootConfig(self):
        """in raspbian, there is a driver entry in /boot/config.txt"""
        found = False
        filename='/boot/config.txt'
        try:
            f = open( filename, 'r')
            lines = f.readlines()
            
            f.close()
        except IOError:
            return
        for line in  lines:
            pattern = 'dtoverlay[ \t]*=[ \t]*w1-gpio'
            m = re.search( pattern, line)
            if m != None:
                found = True
                break
        if not found:
            logger.warn('{name:s}: file {file:s} no driver configured "dtoverlay=w1-gpio"'.format(name=self.name, file=filename))
            
    def _checkDeviceConfig(self):
        """check the names in wire driver data structure"""
        w1_device= self.parameters['w1.device']
        configuredDevice = self.baseDir + '/' + w1_device
        
        devicePattern = self.baseDir + '/' + '[0-9][0-9]-[0-9a-f]*'
        names = glob.glob( devicePattern )
        
        if debug:
            print("configuredDevice", configuredDevice)
            print("devicePattern", devicePattern)
            print("names", names)
        
        if configuredDevice in names:
            # the configred path is found. Perfect.
            return
        if len(names) == 0:
            logger.warn('{name:s}: no DS18-device found. '.format(name=self.name ))
        
        devices = []
        for configuredDevice in names:
            devices.append( configuredDevice[ len(self.baseDir + '/') : ] )
        logger.warn('{name:s}: device {cn:s} not found. But found {de:s}. Edit config file! '.format(name=self.name, cn=w1_device, de=str(devices) ))
        
        
    def run(self):
        self._checkBootConfig()
        self._checkDeviceConfig()
        
        _del = float(self.parameters['poll.interval'])
                
        lastT = None
                                
        while not self.stopped():
            T = self._read_Temperature()
            
            if T != None:
                if T != lastT:
                    self.temperature(T)
                    lastT = T                   
            self.delay(_del)
                
    def temperature(self, value):
        """output from adapter to scratch."""
        
        logging.debug("{name:s} temperature {t:s}".format(name=self.name, t=value))
        self.sendValue(str(value))
                                   
# --------------------------------------------------------------------------------------
