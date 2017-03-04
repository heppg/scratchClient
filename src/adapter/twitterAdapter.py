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
# Module to receive twitter messages
# needs installation of python_twitter
#
import logging
logger = logging.getLogger(__name__)

import time
import sys
import os
import threading
import json
import twitter
import helper       

from scratchClient import ModulePathHandler
import adapter.adapters


debug = False


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
# -------------------------------------------------------
class Twitter_Properties:
    json_data = None 
    def __init__(self, fname):
        self.fname = fname
        self.json_data = {}
        
        
        if os.path.isfile(fname):
            self.readFile()
            
    def readFile(self):
        try:
            json_file = open(self.fname, 'r') 
            self.json_data = json.load(json_file)
            json_file.close()
        except Exception as e:
            logger.warn("can't read properties file " + str(e) )
            return
        
    def writeFile(self):
        self.json_data['writeTime'] = time.time()
        try:
            json_file = open(self.fname, 'w') 
            json.dump(self.json_data, json_file)
            json_file.close()
            try:
                os.chmod(self.fname, 0666 )
            except Exception as e:
                pass
        except Exception as e:
            logger.warn("can't write properties file " + str(e) )
            return  
            
    def getSinceIdTerm(self):
        if  'sinceIdTerm' in self.json_data:
            return self.json_data['sinceIdTerm']
        else:
            return None
    
    def setSinceIdTerm(self, sinceId):
        self.json_data['sinceIdTerm'] = sinceId
    
    def getSinceIdDirect(self):
        if  'sinceIdDirect' in self.json_data:
            return self.json_data['sinceIdDirect']
        else:
            return None
    
    def setSinceIdDirect(self, sinceId):
        self.json_data['sinceIdDirect'] = sinceId
    
    def setScreenName(self, screenName):
        self.json_data['screenName'] = screenName

# -------------------------------------------------------
            
class Twitter_Adapter(adapter.adapters.Adapter):
    
    # -----------------------------------------
    # fields for adapter
    queueThread = None
    
    # -----------------------------------------
   
    mandatoryParameters = { 
                'twitter.consumer_key' : '',
                'twitter.consumer_secret':'',
                'twitter.access_token_key':'',
                'twitter.access_token_secret':'',
                  
                'twitter.term':'#raspberrytweet',
                  
                'twitter.datafile' : 'data/twitter_data.json' ,
                'scratch.timeout' : '60' ,
    
                'twitter.read.direct': 'true',
                'twitter.read.term' : 'true'
    }
    # -----------------------------------------
    
    def __init__(self):
        self.queue = helper.abstractQueue.AbstractQueue()
        # General Adapter
        adapter.adapters.Adapter.__init__(self)

    
                        
    def setActive (self, active):
        adapter.adapters.Adapter.setActive(self, active)
        if active:
            self.queueThread = threading.Thread(target=self.run2)
            self.queueThread.setName(self.name + "Queue")
            self.queueThread.start()
        
        
        else:
            pass
               
    def run(self):
        self.twitter_pollrate=60
        try:
            self.twitter_pollrate = int(self.parameters['twitter.pollrate'])
        except:
            pass
        
        self.twitter_read_direct = self.isTrue( self.parameters['twitter.read.direct'])
        self.twitter_read_term = self.isTrue( self.parameters['twitter.read.term'])
        
        if debug:
            print("twitter_read_direct", self.twitter_read_direct )
            print("twitter_read_term", self.twitter_read_term )
        
        self.term = self.parameters['twitter.term']
        self.api = twitter.Api(
                          consumer_key=self.parameters['twitter.consumer_key'],
                          consumer_secret=self.parameters['twitter.consumer_secret'],
                          access_token_key=self.parameters['twitter.access_token_key'],
                          access_token_secret=self.parameters['twitter.access_token_secret'])
        
        user = self.api.VerifyCredentials()
        screen_name = user.GetScreenName()
        
        modulePathHandler = ModulePathHandler()
        properties_file = modulePathHandler.getScratchClientBaseRelativePath(self.parameters['twitter.datafile'])
        
        self.properties = Twitter_Properties(properties_file)
        self.properties.setScreenName(screen_name)    

        state = 0
        while not(self.stopped()):
            
            if self.twitter_read_direct and self.twitter_read_term:
                if state == 0:
                    self._readDirect()
                    state = 1
                elif state == 1:
                    self._readTerm()
                    state = 0
                    
            elif self.twitter_read_direct:
                self._readDirect()
                
            elif self.twitter_read_term:
                self._readTerm()
                
            self.delay(self.twitter_pollrate)

                    
    def _readTerm(self): 
        if debug:
            print("_readTerm")
        minIdTerm  = self.properties.getSinceIdTerm()           
        try:
            if minIdTerm == None:
                msgs = self.api.GetSearch(term=self.term)
            else:
                msgs = self.api.GetSearch(term=self.term, since_id=minIdTerm)
            
            if len(msgs) == 0:
                if debug:
                    print("no term messages")
            else:
                for msg in msgs:
                    if debug:
                        print(msg.id, msg.text)
                    timedText = TimedText(msg.text)
                    self.queue.put( timedText )
                    
                    minIdTerm = max(minIdTerm, msg.id)
                    self.properties.setSinceIdTerm(minIdTerm)
                    self.properties.writeFile()
                    
        except twitter.TwitterError as e:
            logger.error(str(e))
            self.status(str(e))


    def _readDirect(self):            
        if debug:
            print("_readDirect")
        minIdDirect  = self.properties.getSinceIdDirect() 
        try:
            if minIdDirect == None:
                msgs = self.api.GetDirectMessages()
            else:
                msgs = self.api.GetDirectMessages(since_id=minIdDirect)
            
            if len(msgs) == 0:
                if debug:
                    print("no direct messages")
            else:
                for msg in msgs:
                    if debug:
                        print(msg.id, msg.text)
                    timedText = TimedText(msg.text)
                    self.queue.put( timedText )
                    
                    minIdDirect = max(minIdDirect, msg.id)
                    self.properties.setSinceIdDirect(minIdDirect)
                    self.properties.writeFile()
                    
        except twitter.TwitterError as e:
            logger.error(str(e))
            self.status(str(e))
         
    def run2(self):
        """handle protocol with scratch"""
        self.status("ready")
        self.text("")
        timeout=60
        try:
            timeout = int(self.parameters['scratch.timeout'])
        except:
            pass
        
        self.sendState = 0
        newState = None
        
        while not(self.stopped()):
            self.delay(0.1)
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
                
         
    def text(self, value):
        """output from adapter to scratch"""
        self.sendValue('"' + value + '"')
        
    def status(self, value):
        """output from adapter to scratch"""
        self.sendValue('"' + value + '"')
        
    def textAvailable(self):
        """to scratch: announces an available text"""
        if debug: 
            print("textAvailable")
        self.send()

    def textAcknowledge(self):
        """from scratch: text is read"""
        if debug: 
            print("textAcknowledge")
        self.textAck = True

