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
