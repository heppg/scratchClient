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

import usb.core
import usb.util
import usb.backend.libusb1

import time
import adapter.adapters



debug = False

# --------------------------------------------------------------------------------------
# if there are backend not found errors, or undefined symbol: libusb_strerror, then
# try installing the pyusb lib directly from github.
# the workaround to have libusb1 as a predefined backend results from a fix for this problem.
#
# --------------------------------------------------------------------------------------
class HIDScanner_Adapter(adapter.adapters.Adapter):
    # -----------------------------------------
    # fields for adapter
    # -----------------------------------------
    mandatoryParameters = { 
                           'usb.idVendor' :  '0x0c2e', 
                           'usb.idProduct':  '0x0200',
                          }
    
    # -----------------------------------------
    
    def __init__(self):
        
        # General Adapter
        adapter.adapters.Adapter.__init__(self)

   
    def run(self):
        idVendor = self.string2int(self.parameters['usb.idVendor'])
        idProduct = self.string2int(self.parameters['usb.idProduct'])
        
        _backend = usb.backend.libusb1.get_backend()
        
        dev = usb.core.find(idVendor= idVendor, idProduct=idProduct, backend=_backend)
        if dev is None:
            logger.error( 'configured device is not connected {idVendor:s}:{idProduct:s}'.format(idVendor=self.parameters['usb.idVendor'], idProduct=self.parameters['usb.idProduct']))
            devices = usb.core.find(find_all=True, backend=_backend)
            for p in devices:
                logger.error( "available device 0x{idVendor:04x}:0x{idProduct:04x}".format(idVendor=p.idVendor, idProduct=p.idProduct))
            logger.error("check configuration file")
            return


        interface = 0
        endpoint = dev[0][(0,0)][0]
        if debug:
            logger.info("wMaxPacketSize {ps:d}".format( ps= endpoint.wMaxPacketSize))

        if dev.is_kernel_driver_active(interface) is True:
            # tell the kernel to detach
            dev.detach_kernel_driver(interface)
            # claim the device
            usb.util.claim_interface(dev, interface)

        result = ''
        while not self.stopped() :
            try:
                data = dev.read( endpoint.bEndpointAddress, endpoint.wMaxPacketSize )
                empty = True
                for d in data:
                    if d != 0 :
                        empty = False
                        
                if empty:
                    continue
                if debug:
                    logger.info(data) 
                #
                # no code means no character
                #       
                if data[2] == 0:
                    continue
                
                c = self.decode(data)   
                if c == '\n':
                    self.scan(result)
                    result = ''
                else:
                    result += c
                
            except usb.core.USBError as e:
                
                if e.errno == 110: #'Operation timed out':
                    continue
                logger.error(e)
                continue
            
        # release the device
        usb.util.release_interface(dev, interface)
        # reattach the device to the OS kernel
        dev.attach_kernel_driver(interface)
        logger.info("scanner reading terminated")
        
    hid = { 4: 'a', 5: 'b', 6: 'c', 7: 'd', 8: 'e', 9: 'f', 10: 'g', 11: 'h', 12: 'i', 13: 'j', 14: 'k', 15: 'l', 16: 'm', 17: 'n', 18: 'o', 19: 'p', 20: 'q', 21: 'r', 22: 's', 23: 't', 24: 'u', 25: 'v', 26: 'w', 27: 'x', 28: 'y', 29: 'z', 30: '1', 31: '2', 32: '3', 33: '4', 34: '5', 35: '6', 36: '7', 37: '8', 38: '9', 39: '0', 40: '\n',  44: ' ', 45: '-', 46: '=', 47: '[', 48: ']', 49: '\\', 51: ';' , 52: '\'', 53: '~', 54: ',', 55: '.', 56: '/'  }
    hid2 = { 4: 'A', 5: 'B', 6: 'C', 7: 'D', 8: 'E', 9: 'F', 10: 'G', 11: 'H', 12: 'I', 13: 'J', 14: 'K', 15: 'L', 16: 'M', 17: 'N', 18: 'O', 19: 'P', 20: 'Q', 21: 'R', 22: 'S', 23: 'T', 24: 'U', 25: 'V', 26: 'W', 27: 'X', 28: 'Y', 29: 'Z', 30: '!', 31: '@', 32: '#', 33: '$', 34: '%', 35: '^', 36: '&', 37: '*', 38: '(', 39: ')', 40: '\n', 44: ' ', 45: '_', 46: '+', 47: '{', 48: '}', 49: '|', 51: ':' , 52: '"', 53: '~', 54: '<', 55: '>', 56: '?'  }
    
    def decode(self, data):
        """data[0] = modifier; data[2] = code"""
        try:
            if data[0] == 0:
                return self.hid  [data[2]]    
            if data[0] == 2:
                return self.hid2 [data[2]]    
        except KeyError:
            return '?'
        
    def scan(self, value):
        self.sendValue('"' + value + '"')
# ----------------------------------------------------------------------------------------------
class Blink_Adapter(adapter.adapters.Adapter):
    """Blink adapter, see https://blink1.thingm.com/"""
    
    # -----------------------------------------
    # fields for adapter
    # -----------------------------------------
    mandatoryParameters = { 
                           
                          }
    
    # -----------------------------------------
    
    def __init__(self):
        
        # General Adapter
        adapter.adapters.Adapter.__init__(self)

    vendor = '0x27b8'
    product = '0x01ed'
   
    dev = None
    
    def run(self):
        idVendor = self.string2int ( self.vendor )
        idProduct = self.string2int( self.product )
        
        _backend = usb.backend.libusb1.get_backend()
        
        self.dev = usb.core.find(idVendor= idVendor, idProduct=idProduct, backend=_backend)
        if self.dev is None:
            logger.error( 'configured device is not connected {idVendor:s}:{idProduct:s}'.format(idVendor=self.vendor, idProduct=self.product))
            devices = usb.core.find(find_all=True, backend=_backend)
            for p in devices:
                logger.error( "available device 0x{idVendor:04x}:0x{idProduct:04x}".format(idVendor=p.idVendor, idProduct=p.idProduct))
            logger.error("check configuration file")
            return


        interface = 0
        endpoint = self.dev[0][(0,0)][0]
        if debug:
            logger.info("wMaxPacketSize {ps:d}".format( ps= endpoint.wMaxPacketSize))

        if self.dev.is_kernel_driver_active(interface) is True:
            # tell the kernel to detach
            self.dev.detach_kernel_driver(interface)
            # claim the device
            usb.util.claim_interface(self.dev, interface)

        while not self.stopped() :
            try:
                data = self.dev.read( endpoint.bEndpointAddress, endpoint.wMaxPacketSize )
                empty = True
                for d in data:
                    if d != 0 :
                        empty = False
                        
                if empty:
                    continue
                if debug:
                    logger.info(data) 
                #
                # no code means no character
                #       
                
            except usb.core.USBError as e:
                
                if e.errno == 110: #'Operation timed out':
                    continue
                logger.error(e)
                continue
            
        # release the device
        usb.util.release_interface(self.dev, interface)
        # reattach the device to the OS kernel
        self.dev.attach_kernel_driver(interface)
        logger.info("blink reading terminated")
    
    def _write(self,buf):
        """
        Write command to blink(1)
        Send USB Feature Report 0x01 to blink(1) with 8-byte payload
        Note: arg 'buf' must be 8 bytes or bad things happen
        """
        # if debug_rw : print "blink1write:"+",".join('0x%02x' % v for v in buf)
        report_id = 0x01
        if( self.dev == None ): return self.notfound()
        bmRequestTypeOut = usb.util.build_request_type(usb.util.CTRL_OUT, usb.util.CTRL_TYPE_CLASS, usb.util.CTRL_RECIPIENT_INTERFACE)
        self.dev.ctrl_transfer( bmRequestTypeOut, 
                                0x09,    # == HID set_report
                                (3 << 8) | report_id,  # (3==HID feat.report)
                                0, 
                                buf) 
        
    def _fade_to_rgbn(self, fadeMillis, red,green,blue, ledn):
        """
        Command blink(1) to fade to RGB color
        
        """
        report_id = 0x01
        action = ord('c')
        fadeMillis = fadeMillis/10
        th = (fadeMillis & 0xff00) >> 8
        tl = fadeMillis & 0x00ff
        buf = [report_id, action, red,green,blue, th,tl, ledn]
        return self._write(buf)
    

    def led_2(self, data):
        color = self.getRGBFromString(data)

        r = color [  'red' ] 
        g = color [  'green' ] 
        b = color [  'blue' ] 
        self._fade_to_rgbn(0, r, g, b, 2)

    def led_0(self, data):
        """led_0 is controlling ALL LED"""
        color = self.getRGBFromString(data)

        r = color [  'red' ] 
        g = color [  'green' ] 
        b = color [  'blue' ] 
        self._fade_to_rgbn(0, r, g, b, 0)

    def led_1(self, data):
        color = self.getRGBFromString(data)

        r = color [  'red' ] 
        g = color [  'green' ] 
        b = color [  'blue' ] 
        self._fade_to_rgbn(0, r, g, b, 1)
            
        
# ----------------------------------------------------------------------------------------------
