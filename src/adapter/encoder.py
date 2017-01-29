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

debug = False

class GPIOEncoder(adapter.adapters.GPIOAdapter):
    
    pos = 0
    sensorName = None
    mandatoryParameters = { 'poll.interval': 0.005 }
    mandatoryAlias = [ 'p0', 'p1' ]
    
    def __init__(self):
        adapter.adapters.GPIOAdapter.__init__(self)
        
    def setActive (self, state):
        if debug:
            print(self.name, "setActive", state)
        #
        # use default implementation to start up GPIO
        #
        adapter.adapters.GPIOAdapter.setActive(self, state);

        if state == True:
            # initially send data
            self.position( '0')
            
               
    def run(self):
        if debug:
            print( self.name, "run encoder adapter")
            
        gpio_0 = self.getChannelByAlias('p0')    
        gpio_1 = self.getChannelByAlias('p1')    
        
        if debug:
            print ( gpio_0)
        
        _del = float(self.parameters['poll.interval'])
        
        i0_last = None
        i1_last = None
        
        i = 0
        while not self.stopped():
            #
            #
            self.delay(_del)
                       
            i0 =  self.gpioManager.get( gpio_0)
            i1 =  self.gpioManager.get( gpio_1)

            if i0 != i0_last:
                if i0 == 0 and i1 == 0:
                    if debug:
                        print(self.name, "neg 0", i0, i1)
                    self.pos -= 1
                if i0 == 1 and i1 == 1:
                    if debug:
                        print(self.name, "pos 1", i0, i1)
                    self.pos -= 1

                if i0 == 0 and i1 == 1:
                    if debug:
                        print(self.name, "neg 1", i0, i1)
                    self.pos += 1
                if i0 == 1 and i1 == 0:
                    if debug:
                        print(self.name, "pos 0", i0, i1)
                    self.pos += 1

                self.position(str(self.pos))
                i0_last = i0

            if i1 != i1_last:
                if i0 == 0 and i1 == 0:
                    if debug:
                        print(self.name, "0 neg", i0, i1)
                    self.pos += 1
                if i0 == 1 and i1 == 1:
                    if debug:
                        print(self.name, "1 pos", i0, i1)
                    self.pos += 1

                if i0 == 0 and i1 == 1:
                    if debug:
                        print(self.name, "0 pos", i0, i1)
                    self.pos -= 1
                if i0 == 1 and i1 == 0:
                    if debug:
                        print(self.name, "1 neg", i0, i1)
                    self.pos -= 1

                self.position(str(self.pos))
                i1_last = i1
                
#            if debug:
#                print('encoder input i0 = {i0:s} i1 = {i1:s}'.format(i0=str(i0), i1 =str(i1)))

            i += 1            

    def position(self, value):
        """output from adapter to scratch."""
        self.sendValue(value)

class GPIODialPlateEncoder(adapter.adapters.GPIOAdapter):
    
    sensorName = None
    # mandatoryParameters = { 'poll.interval': 1.01 }
    mandatoryAlias = [ 'nsi', 'nsa' ]
    mandatoryParameters = {  }
    
    def __init__(self):
        adapter.adapters.GPIOAdapter.__init__(self)
        
    def setActive (self, state):
        if debug:
            print(self.name, "setActive", state)
        #
        # use default implementation to start up GPIO
        #
        adapter.adapters.GPIOAdapter.setActive(self, state);

        if state == True:
            # initially send data
            self.start()
        else:
            self.stop()
               
    def run(self):
        recordCnt = 0
        recordData = []
        
        if debug:
            print("run encoder adapter")
            
        gpio_nsa = self.getChannelByAlias('nsa')    
        gpio_nsi = self.getChannelByAlias('nsi')    
        
        if debug:
            print ( "nsa", gpio_nsa, gpio_nsa.getPort())
        if debug:
            print ( "nsi", gpio_nsi, gpio_nsi.getPort())
        
        _del_wait_for_movement = 0.2
        _del_wait_for_trigger  = 0.005
        
        state = 'wait_for_movement'
        cnt = 0
        counter = 0
        while not self.stopped():
            if state == 'wait_for_movement':
                self.delay(_del_wait_for_movement)
                i0 =  self.gpioManager.get( gpio_nsa )
                if i0 == False:
                    state = 'record_trigger'
                    recordCnt = 0
                    recordData = []
                    counter = 0
                                         
            if state == 'record_trigger':
  
                self.delay(_del_wait_for_trigger)
                i0 =  self.gpioManager.get( gpio_nsi )                  
                
                if i0 == False:
                    cnt -= 1
                    if cnt < -4:
                        cnt = -4
                if i0 == True:
                    cnt += 1
                    if cnt == 3:
                        counter += 1
                    if cnt > 4:
                        cnt = 4
                if debug:
                    if i0:
                        recordData.append('1;' + str(cnt))
                    else:
                        recordData.append('0;' + str(cnt))

                # 
                # nsa high = movement has stopped
                #                
                i0 = self.gpioManager.get( gpio_nsa )
                if i0 == True:
                    state = 'wait_for_movement'
                    if counter == 10:
                        self.number(0)
                    else:
                        self.number(counter)
                 
                    if debug:
                        f = open('workfile', 'w')
                        for r in recordData:
                            f.write(r + '\n')
                        f.close()
                    
    def number(self, value):
        """output from adapter to scratch."""
        self.sendValue(value)
