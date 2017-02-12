# -*- coding: utf-8 -*-
# --------------------------------------------------------------------------------------------
# Implementation of scratch Remote Sensor Protocol Client
#
# Copyright (C) 2016  Gerhard Hepp
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

import sys

if sys.version_info.major == 2:
    import Queue
if sys.version_info.major == 3:
    import queue


class AbstractQueue:
    """python2/ python3 wrapper for Queue"""
     
    class Empty(Exception):
        def __init__(self, t):
            Exception.__init__(self, t)
    
    def __init__(self):
        
        if sys.version_info.major == 2:
            self.queue = Queue.Queue()
            self.EmptyException = Queue.Empty
            
        if sys.version_info.major == 3:
            self.queue = queue.Queue()
            self.EmptyException = queue.Empty
    
    def put(self, v):
        self.queue.put(v)

    def get(self, block=False, timeout=0.1):
        try:
            v = self.queue.get( block, timeout )
            return v
        except self.EmptyException:
            raise AbstractQueue.Empty("queue empty")
        
    def qsize(self):
        return self.queue.qsize()

class PriorityQueue:
    """python2/ python3 wrapper for Queue"""
     
    class Empty(Exception):
        def __init__(self, t):
            Exception.__init__(self, t)
    
    def __init__(self):
        
        if sys.version_info.major == 2:
            self.queue = Queue.PriorityQueue()
            self.EmptyException = Queue.Empty
            
        if sys.version_info.major == 3:
            self.queue = queue.PriorityQueue()
            self.EmptyException = queue.Empty
            
                
    def put(self, prio, v):
        self.queue.put( (prio,v) )

    def get(self, block=False, timeout=0.1):
        try:
            v = self.queue.get( block, timeout )
            return v[1]
        except self.EmptyException:
            raise AbstractQueue.Empty("queue empty")
        
    def qsize(self):
        return self.queue.qsize()
