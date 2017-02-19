
import threading
import socket
import SocketServer
import helper.abstractQueue
import time
import singleton.SingletonIPCFSM
import logging
logger = logging.getLogger(__name__)


debug = False

class SingletonSocketClient(SocketServer.TCPServer):
    
    def __init__(self, server_address, parent):
        self.singleton = parent
        self.server_address = server_address
        self.shutdown()
        
    def shutdown(self):
        self._send("shutdown")
        if debug:
            print("Client, shutdown is on its way")
        
    def _send(self, data):
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(0.2)
        try:
            # Connect to server and send data
            sock.connect(self.server_address)
            if debug:
                print("_send connect")
            sock.sendall(data + "\n")
            if debug:
                print("_send sendall")
        
            # Receive data from the server and shut down
            received = sock.recv(1024)
        finally:
            sock.close()
            if debug:
                print("_send close")
        
        if debug:
            print("_send complete")

class SingletonTCPHandler(SocketServer.BaseRequestHandler):
    """
    The request handler class for our server.

    It is instantiated once per connection to the server, and must
    override the handle() method to implement communication to the
    client.
    """

    def handle(self):
        if debug:
            print("handle")
        
        # self.request is the TCP socket connected to the client
        self.data = self.request.recv(1024).strip()
        if debug:
            print "{} wrote:".format(self.client_address[0])
        
        if self.data == 'time':
            self.request.sendall( "{t:20.6f}\n".format( t=self.server.singleton.startTime) )    
        else:
            queue = self.server.getQueue()
            if debug:
                print self.data
            queue.put(self.data)
            # just send back the same data, but upper-case 
            self.request.sendall(self.data.upper())

class SingletonSocketServer(SocketServer.TCPServer):
    
    def __init__(self, server_address, RequestHandlerClass, parent, bind_and_activate=True):
        self.singleton = parent
        SocketServer.TCPServer.__init__(self, server_address, RequestHandlerClass, False)
        self.allow_reuse_address = True
        try:
            self.server_bind()
            self.server_activate()
        except:
            self.server_close()
            raise

    def setQueue(self, queue):
        self.queue = queue
        
    def getQueue(self):
        return self.queue

class SingletonIPC:
    """Singleton class by using a web server as 'unique' instance.
    Terminates other running instances"""
    
    def __init__(self, port=8888):
        self.startTime = time.time()
        
        self.host='127.0.0.1'
        self.port = port
        self.server = None
        
        self._stop = threading.Event()
        self.name="Singleton"
        self.queue = helper.abstractQueue.AbstractQueue()
        self._fsm = singleton.SingletonIPCFSM.SingletonIPC_sm(self)
        self._fsm.setDebugFlag( debug )
    
        
    def registerShutdown( self, scratchClient):
        self.shutdownListener =  scratchClient
            
    def start(self):
        if debug:
            print("SingletonIPC.start()")
        self._stop.clear()
        self.thread = threading.Thread(target=self._run)
        self.thread.setName('SingletonIPC_run')
        self.thread.start()
        pass    
    
    def stop(self):
        if debug:
            print("SingletonIPC.stop()")
        if self.server != None:
            self.action_StopServer()
            
        self._stop.set()
        if self.thread != None:
            self.thread.join(1)
            if self.thread.isAlive():
                logger.debug(self.name +  " no timely join in adapter")
    
    def _timer(self):
        if debug:
            print("Timeout !!")
        self.queue.put('timeout')
    
    # --- actions called from the state machine
    #    
    def action_Timer(self, timeout):
        if debug:
            print("action_Timer")
        self.timer = threading.Timer(timeout, self._timer)
        self.timer.start()
        
    def action_TimerStop(self):
        self.timer.cancel()
     
    def action_StartServer(self):
        if debug:
            print("action_StartServer")
        self.server_thread = threading.Thread( target=self._run_Server )
        self.server_thread.setName('singleton_server_thread')
        self.server_thread.start()
    
    def action_StopServer(self):
        if debug:
            print("action_StopServer")
        if self.server != None:
            logger.info("wait for server close")
            self.server.shutdown()
            logger.info("server closed")
            try:
                self.server.server_close()
            except:
                pass

    def action_Shutdown(self):
        if debug:
            print("action_Shutdown")
        logger.warn("initiate shutdown from singletonIPC")
        self.shutdownListener.shutdown()
        
    def action_Client(self):
        if debug:
            print("action_Client")
        try:
            client = SingletonSocketClient((self.host, self.port), self)
            self.queue.put('success')
        except Exception as e:
            logger.error ( "actionClient %", e)
            self.queue.put('socketfail')
            pass     
        
    def _run_Server(self):
        try:
            self.server = SingletonSocketServer((self.host, self.port), SingletonTCPHandler, self) 
            self.server.setQueue(self.queue)
            if debug:
                print ("server created")
            # Activate the server; this will keep running until you
            # interrupt the program with Ctrl-C
            self.server.serve_forever(poll_interval = 0.1)
            self.server.server_close()
            if debug:
                print("server past 'serve_forever'")
            self.server = None
        except Exception as e:
            
            if debug:
                print(e)
            try:
                self.server.server_close()
            except:
                pass
            self.queue.put('socketfail')
            pass
        if debug:
            print("_run_Server stopped")
        
    def _debug(self, args):
        if debug:
            print (args)
        logger.debug(args)
        
    def _run(self):
        """Thread method."""
        self._fsm.Start(0)
        
        logger.debug(self.name + " " + "Thread start")
            
        while not self._stop.isSet():
            # logger.debug(self.name + " " + "Thread loop" )
            try:
                s = self.queue.get(block=True, timeout= 0.0666)
                
                if s == 'shutdown':
                    logger.debug("received shutdown")
                    self._fsm.Shutdown()
                   
                    
                if s == 'timeout':
                    logger.debug("received timeout")
                    self._fsm.Timeout()
                    
                if s == 'success':
                    logger.debug("received success")
                    self._fsm.Success()
                    
                if s == 'socketfail':
                    logger.debug("received socketfail")
                    self._fsm.SocketFail()
                    
            except helper.abstractQueue.AbstractQueue.Empty:
                continue 

        logger.info(self.name + " " + "thread end")
  