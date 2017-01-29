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
#
# needs 'festival' to be installed on system
# apt-get install festival
#
import adapter
import subprocess
import logging
import helper.abstractQueue
import os
import time
import shlex
import threading

debug = False

logger = logging.getLogger(__name__)

# --------------------------------------------------------------------------------------

class Linux_Adapter (adapter.adapters.Adapter):
    """Interface to linux operation command line """
    
    mandatoryParameters = { 'queue.max' : 30, 'os.command': 'ls -l' }

    queue = None
    
    def __init__(self):
        self.queue = helper.abstractQueue.AbstractQueue()
       
        adapter.adapters.Adapter.__init__(self)

    def setActive (self, state):
        if debug:
            print(self.name, "setActive", state)
        #
        # use default implementation to start up SPI
        #
        adapter.adapters.Adapter.setActive(self, state);


    def run(self):
        cmd = self.parameters['os.command']

        while not(self.stopped()):
            try:
                #
                # the queue is only used to receive events
                #        
                self.queue.get(True, 0.1)
                
                    
                logger.info('{name:s}: call {r:s}'.format(name=self.name, r=str(cmd)))
                    
                return_code = subprocess.call( cmd, shell=True )
                
                logger.info('{name:s}: return code {r:s}'.format(name=self.name, r=str(return_code)))
            except helper.abstractQueue.AbstractQueue.Empty:
                pass
            except Exception as e:
                logger.warn(e)
               
    def trigger(self):
        if debug:
            print("trigger")

        if self.queue.qsize() > int(self.parameters['queue.max']):
            
            logger.warn('{name:s}: queue is full, trigger discarded '.format(name=self.name))
        else:
            self.queue.put('trigger')
        

class Linux_APLAY_Adapter (adapter.adapters.Adapter):
    """Interface to linux aplay-command line """
    
    mandatoryParameters = { 'queue.max' : 30, 
                            'aplay.device': 'sysdefault:CARD=Device',
                            
                            'sound.dir': '/opt/sonic-pi/etc/samples' 
                          }

    queue = None
    
    def __init__(self):
        self.queue = helper.abstractQueue.AbstractQueue()
       
        adapter.adapters.Adapter.__init__(self)

    def setActive (self, state):
        if debug:
            print(self.name, "setActive", state)
        #
        # use default implementation to start up SPI
        #
        adapter.adapters.Adapter.setActive(self, state);


    def run(self):
        cmd = 'aplay -D ' + self.parameters['aplay.device']

        while not(self.stopped()):
            try:
                #
                # the queue is only used to receive events
                #        
                filepath = self.queue.get(True, 0.1)
                
                arec_cmd = ["aplay", "-D",  self.parameters['aplay.device'],  filepath ]  
                logger.info('{name:s}: start {r:s}'.format(name=self.name, r=str(arec_cmd)))
                popen = subprocess.Popen(arec_cmd )   
                popen.wait()    
          
            except helper.abstractQueue.AbstractQueue.Empty:
                pass
            except Exception as e:
                logger.warn(e)
               
    def sound(self, wav_file_name):
        if debug:
            print("sound", wav_file_name)
        if wav_file_name.strip() == '':
            return
        
        fname = ''
        
        found = False
        if not found:
            fname = self.parameters['sound.dir'] + '/' + wav_file_name
            if  os.path.isfile(fname) :
                found = True
        if not found:
            fname = self.parameters['sound.dir'] + '/' + wav_file_name + '.wav'
            if  os.path.isfile(fname) :
                found = True
        
        if not found:
            logger.warn('{name:s}: file {f:s} not found in dir {d:s}'.format(name=self.name, f=wav_file_name, d= self.parameters['sound.dir']))
            return
        
        if self.queue.qsize() > int(self.parameters['queue.max']):
            
            logger.warn('{name:s}: queue is full, sound discarded '.format(name=self.name))
        else:
            self.queue.put(fname)
        

class Linux_ARECORD_Adapter (adapter.adapters.Adapter):
    """Interface to linux arecord-command line """
    
    mandatoryParameters = { 
                            'aplay.device': 'sysdefault:CARD=Device',
                            'aplay.rate': '16000',
                            'sound.dir': '/home/pi/temp' 
                          }

    queue = None
    
    def __init__(self):
        self.queue = helper.abstractQueue.AbstractQueue()
       
        adapter.adapters.Adapter.__init__(self)

    def setActive (self, state):
        if debug:
            print(self.name, "setActive", state)
        #
        # use default implementation to start up SPI
        #
        adapter.adapters.Adapter.setActive(self, state);


    def run(self):
        timeout=60
        try:
            timeout = int(self.parameters['timeout'])
        except:
            pass
        
        state = 0
        t0 = time.time()
        while not(self.stopped()):
            try:
                #
                # the queue is only used to receive events
                #  
                      
                action = self.queue.get(True, 0.1)
            except helper.abstractQueue.AbstractQueue.Empty:
                action = 'none'
            try:    
                if state == 0:
                        
                    if action == 'start':
                        
                        arec_cmd = ["arecord", "-D",  self.parameters['aplay.device'], "-r",  self.parameters['aplay.rate'], self.fname ]  
                        logger.info('{name:s}: start {r:s}'.format(name=self.name, r=str(arec_cmd)))
                        popen = subprocess.Popen(arec_cmd )   
                        
                        state = 1
                        t0 = time.time()
    
                elif state == 1:
                    if None != popen.poll():
                        logger.info('{name:s}: terminated {r:s}'.format(name=self.name, r=str(arec_cmd)))
                        state = 0

                    if time.time() > t0 + timeout:
                        logger.info('{name:s}: stop {r:s}'.format(name=self.name, r=str(arec_cmd)))
                        popen.terminate()
                        state = 0

                    if action == 'stop':
                        logger.info('{name:s}: stop {r:s}'.format(name=self.name, r=str(arec_cmd)))
                        popen.terminate()
                        state = 0
                            
                
            except Exception as e:
                logger.warn(e)
                break
            
        if state == 1:
            popen.terminate()    
            logger.info('{name:s}: stop {r:s}'.format(name=self.name, r=str(arec_cmd)))
                
    def sound(self, wav_file_name):
        if debug:
            print("sound", wav_file_name)
            
        if  wav_file_name.endswith('.wav'):
            fname = self.parameters['sound.dir'] + '/' + wav_file_name
        else:
            fname = self.parameters['sound.dir'] + '/' + wav_file_name + '.wav'
            
        self.fname = fname
        logger.info('{name:s}: file name is set {f:s} '.format(name=self.name, f=fname))
        

    def start_record(self):
        self.queue.put('start')
    
    def stop_record(self):
        self.queue.put('stop')

class TimedText:
    def __init__(self, text):
        self.text = text
        self.time = time.time()
    
    def isExpired(self, timeout):
        if time.time() > self.time + timeout:
            return True
        return False
    
    def getText(self):
        return self.text   

class Linux_ASR_Adapter (adapter.adapters.Adapter):
    """Interface to linux speech recogition-command line 
     A process is started, and each line output to stdout is transferred to scratch in a synchronous protocol.
     The usage is not limited to ASR, but can be used for any process.
     The adapter is designed to use pocketspynx in betach mode."""
    
    mandatoryParameters = { 
                            'command.line': 'pocketsphinx_continuous -hmm /usr/local/share/pocketsphinx/model/en-us/en-us -lm 0609.lm -dict 0609.dic -samprate 16000/8000/48000 -logfn /dev/null -infile {indir:s}/{infile:s}',
                            'sound.file': 'sample.wav',
                            'sound.dir': '/home/pi/temp',
                            'timeout': '60' 
                          }

    queue = None
    popen = None
    textAck = None
    
    def __init__(self):
        self.queue = helper.abstractQueue.AbstractQueue()
        self.textAck = False
        adapter.adapters.Adapter.__init__(self)

    def setActive (self, state):
        if debug:
            print(self.name, "setActive", state)
        #
        # use default implementation to start up SPI
        #
        adapter.adapters.Adapter.setActive(self, state);


    def scanStdin(self):
        
        if debug: print("scanStdin")
        
        self._scan_stop.isSet()
        while not(self._scan_stop.isSet()):
            try:
                sx = self.s_stdout.readline().rstrip()
                #
                # there are occasions when process is terminating, when stdout still 'open'
                #
                if sx == '':
                    time.sleep(0.01)
                else:
                    if debug: print("text in queue[{len:d}]={t:s}".format( t= sx, len=self.queue.qsize()))
                    timedText = TimedText(sx)
                    self.queue.put( timedText )
                    
            except Exception as e:
                print("scanStdin", e)
                time.sleep(0.01)
                pass
            
    def execute(self):
        if debug: print ("execute")
        
        if self.popen != None:
            self.status("conversion already running")
            return
        
        self.threadExecute = threading.Thread(target=self._execute)
        self.threadExecute.setName('_execute')
        self.threadExecute.start()
           
    def _execute(self):
        
        self.status("conversion running")
        
        c = self.parameters['command.line']
        c = c.replace('${sound.dir}', self.parameters['sound.dir'])
        c = c.replace('${sound.file}', self.parameters['sound.file'])
        
        # c = "ls -l".format(infile=filename)
    
        args = shlex.split(c)
        if debug: print (args)
        self.popen = subprocess.Popen( args, stdout=subprocess.PIPE )
        if debug: print ("popen", self.popen)
        self.s_stdout = self.popen.stdout
        
         
        self._scan_stop = threading.Event()
        self.scanStdinThread = threading.Thread(target=self.scanStdin)
        self.scanStdinThread.setName('scanStdin')
        self._scan_stop.clear()
        self.scanStdinThread.start()
   
        self.popen.wait()
        self._scan_stop.set()
        self.complete()
        self.status("conversion complete")
        self.popen = None
        

    
    def run(self):
        self.status("ready")
        timeout=60
        try:
            timeout = int(self.parameters['timeout'])
        except:
            pass
        
        self.sendState = 0
        newState = None
        
        while not(self.stopped()):
            self.delay(0.05)
            if self.sendState == 0:
                self.textAck = False
                try:
                    #
                    # the queue is only used to receive events
                    #  
                    timedText = self.queue.get(True, 0.1)
                except helper.abstractQueue.AbstractQueue.Empty:
                    timedText = None
                    continue
                
                if timedText.isExpired(timeout):
                    logger.debug("{name:s}: text expired, too old {t:s}".format(name=self.name, t = timedText.getText() ))
                    continue
                self.text( timedText.getText() )
                newState = 1
                
            elif self.sendState == 1:
                self.textAck = False
                self.textAvailable()
                newState = 2
                
            elif self.sendState == 2:
                if self.textAck:
                    newState = 0
                else:
                    if debug: 
                        print("wait for ack")
                        self.delay(1)
                        
            if self.sendState != newState and newState != None :
                if debug: print("state", self.sendState, "newState", newState)
                self.sendState = newState
                
        if self.popen != None:            
            self.popen.terminate()    
        
        

    def textAvailable(self):
        """to scratch: announces an available text"""
        if debug: 
            print("textAvailable")
        self.send()
    
    def complete(self):
        """to scratch: announces completion of linux command"""
        if debug: 
            print("complete")
        self.send()
    
    def textAcknowledge(self):
        """from scratch: text is read"""
        if debug: 
            print("textAcknowledge")
        self.textAck = True

    def text(self, value):
        if debug: print("text", value)
        self.sendValue('"' + value + '"')    
    
    def status(self, value):
        if debug: print("status", value)
        self.sendValue('"' + value + '"')