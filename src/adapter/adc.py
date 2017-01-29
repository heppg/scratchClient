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

import adapter.adapters
from spi.manager import SPIManager

debug = False


# --------------------------------------------------------------------------------------
#
# TODO: these adapters need to migrate to adapter.spiAdapter
# 
# --------------------------------------------------------------------------------------

class IRDistanceInput (adapter.adapters.SPIAdapter):
    """Interface to Sharp GP2Y0A21YK IR Distance sensor on adc MCP3202, 10bit"""
    
    mandatoryParameters = { 'poll.interval': '0.2', 
                           'spi.bus' : '0', 
                           'spi.device' :'0',
                           'adc.channel' : '0',
                           'filter.use' : '1'
     }

    int_adc_channel = 0
    
    useFilter = True

    def __init__(self):
        adapter.adapters.SPIAdapter.__init__(self)

    def setActive (self, state):
        if debug:
            print(self.name, "setActive", state)
        #
        # use default implementation to start up GPIO
        #
        adapter.adapters.SPIAdapter.setActive(self, state);
        self.int_adc_channel=   int(self.parameters['adc.channel'])

        uf = self.parameters['filter.use']
        if self.isTrue ( uf ):
            self.useFilter = True
        else:
            self.useFilter = False
               
    def run(self):
        _del = float(self.parameters['poll.interval'])
            
        lastADC = self.getValue( self.spi, self.int_adc_channel )
        
        lastDISTANCE = self.linearize(lastADC)
        self.distance(lastDISTANCE)
        
        _filter=[]
        if self.useFilter:
            _filter = [lastDISTANCE,lastDISTANCE,lastDISTANCE,lastDISTANCE,lastDISTANCE]
        
        
        while not self.stopped():
            #
            # delay 5 sec, but break time in small junks to allow stopping fast
            #
            self.delay(_del)
                           
            currentADC = self.getValue( self.spi, self.int_adc_channel )

                
            currentDISTANCE = self.linearize(currentADC)

            if self.useFilter:
                _filter[0] = _filter[1]
                _filter[1] = _filter[2]
                _filter[2] = _filter[3]
                _filter[3] = _filter[4]
                _filter[4] = currentDISTANCE
            
                f = _filter[0] + _filter[1] + _filter[2] + _filter[3] + _filter[4]
                currentDISTANCE = f / 5;
                            
            if ( lastDISTANCE -10 ) < currentDISTANCE < ( lastDISTANCE +10 ):
                continue
            
            self.distance( str(currentDISTANCE) )
            lastDISTANCE = currentDISTANCE

    # values taken from sharp datasheet, normalized to 1024 points == 3.3V
    tabelle_mm_value = [ # (mm, ad-value)
                        ( 60, 1023), # helper value 
                        ( 60,  975),
                        ( 80,  852),
                        (100,  713),
                        (150,  510),
                        (200,  406),
                        (250,  336),
                        (300,  288),
                        (400,  229),
                        (500,  191),
                        (600,  159),
                        (700,  137),
                        (800,  128),
                        (800,    0) ]
        
    def linearize(self, current):
        # look for the interval in use
        for i in range(len(self.tabelle_mm_value)-1):
            if current <= self.tabelle_mm_value[i+0][1] and \
               current > self.tabelle_mm_value[i+1][1]:
                
                i0_mm = self.tabelle_mm_value[i+0][0]
                i1_mm = self.tabelle_mm_value[i+1][0]
                
                interval_width = self.tabelle_mm_value[i+1][1] - \
                                 self.tabelle_mm_value[i+0][1]
                interval = current - self.tabelle_mm_value[i+0][1]
                
                value = int (i0_mm + float(i1_mm - i0_mm) * float(interval) / float(interval_width))
                if debug:
                    print(current, "-->", value)
                return value
        return 800
      
    def distance(self, value):
        """output from adapter to scratch."""
        self.sendValue(value)
        
    def getValue(self, spi, channel):
        # EXPLANATION of 
        # r = spi.xfer2([1,(2+channel)<<6,0])
        # Send start bit, sgl/diff, odd/sign, MSBF 
        # channel = 0 sends 0000 0001 1000 0000 0000 0000
        # channel = 1 sends 0000 0001 1100 0000 0000 0000
        # sgl/diff = 1; odd/sign = channel; MSBF = 0 
        
        # EXPLANATION of 
        # ret = ((r[1]&31) << 6) + (r[2] >> 2)
        # spi.xfer2 returns same number of 8-bit bytes as sent. In this case, three 8-bit bytes are returned
        # We must then parse out the correct 10-bit byte from the 24 bits returned. The following line discards
        # all bits but the 10 data bits from the center of the last 2 bytes: XXXX XXXX - XXXX DDDD - DDDD DDXX 

        spi.max_speed_hz = 500000
        
        i = [ 1, (2+channel)<<6, 0  ]
        # print(" i {r0:08b} {r1:08b} {r2:08b} {r3:08b} ".format(r0=i[0], r1=i[1], r2=i[2], r3=i[3]))
        
        r = spi.xfer2( list( i ) )
        
        # print(" r {r0:08b} {r1:08b} {r2:08b} {r3:08b} ".format(r0=r[0], r1=r[1], r2=r[2], r3=r[3]))
        
        ret = ((r[1]&31) << 6) + (r[2] >> 2)
        return ret 


class ADC_MCP3202_10_Input (adapter.adapters.SPIAdapter):
    """ADC Interface; rename this to ADC_MCP3202_10_Input"""
    
    int_adc_channel = 0

    mandatoryParameters = { 'poll.interval': '0.2', 
                           'spi.bus' : '0', 
                           'spi.device' :'0',
                           'adc.channel' : '0' }

    def __init__(self):
        adapter.adapters.SPIAdapter.__init__(self)


    def setActive (self, state):
        if debug:
            print(self.name, "setActive", state)
        #
        # use default implementation to start up SPI
        #
        adapter.adapters.SPIAdapter.setActive(self, state);
        self.int_adc_channel=   int(self.parameters['adc.channel'])

    def run(self):
        if debug:
            print(self.name, "ADCInput, setActive")
        _del = float(self.parameters['poll.interval'])
            
        last = self.getValue( self.spi, self.int_adc_channel )
        self.adc(last)   
             
        while not self.stopped():
            #
            # delay 5 sec, but break time in small junks to allow stopping fast
            #
            self.delay(_del)
                           
            current = self.getValue( self.spi, self.int_adc_channel )
            
            # plus/minus ein Punkt ist noch 'identisch'
            
            if not( last -2 < current < last + 2) :
                self.adc( current )
                last = current

    def adc(self, value):
        """output from adapter to scratch."""
        self.sendValue(str(value))

    def getValue(self, spi, channel):
        # EXPLANATION of 
        # r = spi.xfer2([1,(2+channel)<<6,0])
        # Send start bit, sgl/diff, odd/sign, MSBF 
        # channel = 0 sends 0000 0001 1000 0000 0000 0000
        # channel = 1 sends 0000 0001 1100 0000 0000 0000
        # sgl/diff = 1; odd/sign = channel; MSBF = 0 
        
        # EXPLANATION of 
        # ret = ((r[1]&31) << 6) + (r[2] >> 2)
        # spi.xfer2 returns same number of 8-bit bytes as sent. In this case, three 8-bit bytes are returned
        # We must then parse out the correct 10-bit byte from the 24 bits returned. The following line discards
        # all bits but the 10 data bits from the center of the last 2 bytes: XXXX XXXX - XXXX DDDD - DDDD DDXX 

        spi.max_speed_hz = 500000
        
        i = [ 1, (2+channel)<<6, 0  ]
        # print(" i {r0:08b} {r1:08b} {r2:08b}  ".format(r0=i[0], r1=i[1], r2=i[2] ))
        
        r = spi.xfer2( list( i)  )  
        
        # print(" r {r0:08b} {r1:08b} {r2:08b} {r3:08b} ".format(r0=r[0], r1=r[1], r2=r[2], r3=r[3]))
        
        ret = ((r[1]&31) << 6) + (r[2] >> 2)
        return ret 
        

class ADC_MCP3202_12_Input (adapter.adapters.SPIAdapter):
    """ADC_MCP3202_12_Input, having a FILTER
    The filter is an array with filter.depth length."""
    
    int_adc_channel = 0

    mandatoryParameters = {'filter.depth': '8', 
                           'poll.interval': '0.2', 
                           'spi.bus' : '0', 
                           'spi.device' :'0',
                           'adc.channel' : '0' }

    def __init__(self):
        adapter.adapters.SPIAdapter.__init__(self )


    def setActive (self, state):
        if debug:
            print(self.name, "setActive", state)
        #
        # use default implementation to start up SPI
        #
        adapter.adapters.SPIAdapter.setActive(self, state);
        self.int_adc_channel=   int(self.parameters['adc.channel'])

    def run(self):
        if debug:
            print(self.name, "ADCInput, setActive")
        _del = float(self.parameters['poll.interval'])
            
        last = self.getValue( self.spi,  
                                self.int_adc_channel
                                )
        self.adc(last)   
        
        FILTER = int( self.parameters['filter.depth'] )
        if FILTER < 1 :
            FILTER = 1
            
        filterData = []
        for _ in range(FILTER):
            filterData.append(last)
        
        nFilter = 0
        
             
        while not self.stopped():
            #
            # delay 5 sec, but break time in small junks to allow stopping fast
            #
            self.delay(_del)
                           
            current = self.getValue( self.spi, self.int_adc_channel )
            
            # plus/minus ein Punkt ist noch 'identisch'
            if True:
                filterData[nFilter] = current
                nFilter += 1
                nFilter = nFilter % FILTER
                
                current = 0
                for x in filterData:
                    current += x
                current /= FILTER 
            else:
                pass
            
            if not( last -2 < current < last + 2) :
                self.adc( current )
                last = current

    def adc(self, value):
        """output from adapter to scratch."""
        self.sendValue(str(value))
        
    def getValue(self, spi, channel):
        # EXPLANATION of 
        # r = spi.xfer2([1,(2+channel)<<6,0])
        # Send start bit, sgl/diff, odd/sign, MSBF 
        # channel = 0 sends 0000 0001 1000 0000 0000 0000
        # channel = 1 sends 0000 0001 1100 0000 0000 0000
        # sgl/diff = 1; odd/sign = channel; MSBF = 0 
        
        # EXPLANATION of 
        # ret = ((r[1]&31) << 6) + (r[2] >> 2)
        # spi.xfer2 returns same number of 8-bit bytes as sent. In this case, three 8-bit bytes are returned
        # We must then parse out the correct 12-bit byte from the 24 bits returned. The following line discards
        # all bits but the 12 data bits from the center of the last 2 bytes: XXXX XXXX - XXXX DDDD - DDDD DDDD 

        spi.max_speed_hz = 800000
        r = spi.xfer2(list([1, (2+channel)<<6, 0])) 
        ret = ((r[1]&31) << 8) + (r[2] )
        return ret 


class ADC_MCP3008_10_Input (adapter.adapters.SPIAdapter):
    """ADC Interface; rename this to ADC_MCP3008_10_Input"""
    
    int_adc_channel = 0

    mandatoryParameters = { 
                           'poll.interval': '0.2', 
                           'poll.band': '2', 
                           'spi.bus' : '0', 
                           'spi.device' :'0',
                           'adc.channel' : '0' }

    def __init__(self):
        adapter.adapters.SPIAdapter.__init__(self)


    def setActive (self, state):
        if debug:
            print(self.name, "setActive", state)
        #
        # use default implementation to start up SPI
        #
        adapter.adapters.SPIAdapter.setActive(self, state);
        self.int_adc_channel=   int(self.parameters['adc.channel'])

    def run(self):
        if debug:
            print(self.name, "ADCInput, setActive")
        _del = float(self.parameters['poll.interval'])
        pollband = float(self.parameters['poll.band'])
            
        last = self.getValue( self.spi, self.int_adc_channel )
        self.adc(last)     
        while not self.stopped():
            #
            # delay 5 sec, but break time in small junks to allow stopping fast
            #
            self.delay(_del)
                           
            current = self.getValue( self.spi, self.int_adc_channel )
            
            # plus/minus ein Punkt ist noch 'identisch'
            
            if not( last -pollband < current < last + pollband) :
                self.adc( current )
                last = current

    def adc(self, value):
        """output from adapter to scratch."""
        self.sendValue(str(value))

    def getValue(self, spi, channel):
        # data sheet: 1.35MHz @2.7V
        spi.max_speed_hz = 1350000
        
        r = spi.xfer2([1, (8+channel) << 4, 0])

        #Filter data bits from returned bits
        ret = ((r[1]&3) << 8) + r[2]
        return ret 
