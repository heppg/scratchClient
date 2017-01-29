# -*- coding: utf-8 -*-


import adapter.test
import adapter.adapters
import threading
import sys
import logging
logger = logging.getLogger(__name__)

debug = True

class TestAdapter( adapter.adapters.Adapter):
    
    sensorName = None
    mandatoryParameters = { 'poll.interval': 1 }
    
    def __init__(self):
        if debug:
            print("TestAdapter init")
        adapter.adapters.Adapter.__init__(self)
        
    def setActive (self, state):
        if debug:
            print(self.name, "setActive", state)
        #
        # use default implementation to start up GPIO
        #
        adapter.adapters.Adapter.setActive(self, state);

        if state == True:
            self.thread2 = threading.Thread(target=self.run2)
            self.thread2.start()
            self.test ( '1')
               
    def run(self):
        if debug:
            print("run in test Adapter")
        _del = float(self.parameters['poll.interval'])
        
        strings = ['apfel', 'apfeläöü', 'äöü' ]
        floats = [18.8, 18.9, 19.000, 19.123, 19.2]
        
        i = 0
        while not self.stopped():
            #
            # delay 5 sec, but break time in small junks to allow stopping fast
            #
            self.delay(_del)
            
            self.toggleValue(str(i%2))
            
            self.iValue(str(i))
            self.sValue(strings[ i % len(strings)])
            self.fValue(str(floats[ i % len(floats)]))
            self.event()
            
            i += 1      
                  
    def run2(self):
        if debug:
            print("run2 in test Adapter")
        _del = float(self.parameters['poll.interval2'])
        
        i = 0
        while not self.stopped():
            #
            self.delay(_del)
            
            self.adcValue(str(i))

            i += 1
            i = i % 1024 
    def low(self):
        if debug:
            print("Adapter, low")

    def high(self):
        if debug:
            print("Adapter, high")

    def inValue(self, value):
        if debug:
            print("Adapter, inValue", value )
        self.iValue(value)
        
    def adcValue(self, value):
        if debug:
            print("Adapter, adcValue", value )
        self.sendValue(value)
        
    def iValue(self, value):
        if debug:
            print("Adapter, iValue", value )
        self.sendValue(value)
        
    def toggleValue(self, value):
        if debug:
            print("Adapter, iValue", value )
        self.sendValue(value)
    
    def sValue(self, value):
        if debug:
            print("Adapter, sValue", value )
        self.sendValue('"' + value + '"')
    
    def fValue(self, value):
        if debug:
            print("Adapter, fValue", value )
        self.sendValue( value )
    
    def test(self, value):
        """output from adapter to scratch."""
        logger.info("adapter.test, test %s", str(value))
        self.sendValue(value)

    def event(self):
        """output from adapter to scratch."""
        logger.info("{name:s}: event".format(name=self.name))
        self.send()

class TestPingPongAdapter(adapter.adapters.Adapter):
    
    mandatoryParameters = {  }
    
    def __init__(self):
        adapter.adapters.Adapter.__init__(self)
        
    def setActive (self, state):
        if debug:
            print(self.name, "setActive", state)
        #
        # use default implementation to start up GPIO
        #
        adapter.adapters.Adapter.setActive(self, state);

        if state == True:
            self.start()
        else:
            self.stop()
               

    def ping(self):
        self.pong()


    def pong(self):
        self.send()
    
    def a(self, value):
        self.b( str( int(value) +1 ))
    
    def b(self, value):
        self.sendValue(value)
        