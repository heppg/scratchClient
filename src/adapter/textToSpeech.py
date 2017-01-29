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

debug = False

logger = logging.getLogger(__name__)

# --------------------------------------------------------------------------------------

class Festival_Adapter (adapter.adapters.Adapter):
    """Interface to Festival """
    mandatoryParameters = { 'queue.max' : 30 }

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
        while not(self.stopped()):
            try:        
                value = self.queue.get(True, 0.1)
                
                # replace some characters to prevent security problems
                
                value = value.replace("'" ,  ' ')
                value = value.replace("|" ,  ' ')
                value = value.replace("$" ,  ' ')
                value = value.replace("\\",  ' ')
                value = value.replace("/" ,  ' ')
                value = value.replace(">" ,  ' ')
                value = value.replace("<" ,  ' ')
                value = value.replace("&" ,  ' ')
                value = value.replace("~" ,  ' ')
                value = value.replace("*" ,  ' ')
                
                cmd = "echo '{val:s}' | festival --tts".format(val=value)
                if debug:
                    print ( cmd )
                return_code = subprocess.call( cmd, shell=True )
            except helper.abstractQueue.AbstractQueue.Empty:
                pass
            except Exception as e:
                logger.warn(e)
               
    def speech(self, value):
        if debug:
            print("speech", value)

        if self.queue.qsize() > int(self.parameters['queue.max']):
            
            logger.warn('{name:s}: queue is full, value discarded {val}'.format(name=self.name, val=value))
        else:
            self.queue.put(value)
        

# --------------------------------------------------------------------------------------

# sudo apt-get install libttspico-utils
# pico2wave -w lookdave.wav "Look Dave, I can see you're really upset about this." && aplay lookdave.wav


class Pico2Wave_Adapter (adapter.adapters.Adapter):
    """Interface to Pico2Wave """
    # language en-US
    # language en-GB
    # language de-DE
    # language es-ES
    # language fr-FR
    # language it-IT
 
    mandatoryParameters = { 'queue.max' : 30, 'tts.lang' : 'de-DE' }

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
        lang = self.parameters['tts.lang']
        if not lang in [ 'en-US', 'en-GB', 'de-DE', 'es-ES', 'fr-FR', 'it-IT' ]:
            lang = 'de-DE'
        
        while not(self.stopped()):
            try:        
                value = self.queue.get(True, 0.1)
                
                # replace some characters to prevent security problems
                
                value = value.replace("'" ,  ' ')
                value = value.replace("|" ,  ' ')
                value = value.replace("$" ,  ' ')
                value = value.replace("\\",  ' ')
                value = value.replace("/" ,  ' ')
                value = value.replace(">" ,  ' ')
                value = value.replace("<" ,  ' ')
                value = value.replace("&" ,  ' ')
                value = value.replace("~" ,  ' ')
                value = value.replace("*" ,  ' ')
                
                cmd = "tmp=`mktemp -t XXXXXX.wav` && chmod 666 $tmp && pico2wave -l '{lang:s}' -w $tmp '{val:s}' && aplay $tmp && rm $tmp".format(lang=lang, val=value)
                if debug:
                    print ( cmd )
                return_code = subprocess.call( cmd, shell=True )
            except helper.abstractQueue.AbstractQueue.Empty:
                pass
            except Exception as e:
                logger.warn(e)
               
    def speech(self, value):
        if debug:
            print("speech", value)

        if self.queue.qsize() > int(self.parameters['queue.max']):
            
            logger.warn('{name:s}: queue is full, value discarded {val}'.format(name=self.name, val=value))
        else:
            self.queue.put(value)
        
