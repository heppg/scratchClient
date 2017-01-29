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
# a simple publish-subscribe-mechanism
#

import logging

debug = False

class Pub:
    
    topics = []
    logger = logging.getLogger("Pub")
     
    @staticmethod
    def report():
        for t in Pub.topics:
            print("pubsub report ", t)
                
    @staticmethod
    def subscribe(topic, receiver):
        found = False
        for t in Pub.topics:
            if t[0] == topic and t[1] == receiver:
                found = True
        if found:
            Pub.logger.info("subscribe: already known " + topic )
            if debug:
                print("subscribe ", topic, receiver )
        else:
            Pub.topics.append ( [topic, receiver] )
       
            Pub.logger.info("subscribe: " + topic )
            if debug:
                print("subscribe " + topic )
        
    @staticmethod
    def unsubscribe(topic, receiver):
        try:
            Pub.topics.remove( [topic, receiver] )
        except ValueError as err:
            if debug:
                print("unsubscribe error", topic, receiver, err )
            return
        
        Pub.logger.info("unsubscribe " + topic )
        if debug:
            print("unsubscribe " + topic )
        
    @staticmethod   
    def publish(topic, message):
        if debug:
            print("publish", topic, message)
        found = False
        for t in Pub.topics:
            if t[0] == topic:
            
                receiver = t[1]
                receiver(message)
                found = True
        if not found:
           
            Pub.logger.error("Topic not found " + topic)
            if debug:
                print( "Topic not found " + topic )
