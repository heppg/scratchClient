# -*- coding: utf-8 -*-
    # --------------------------------------------------------------------------------------------
    # Copyright (C) 2017  Gerhard Hepp
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
import os
import re
import sys
import stat

if sys.platform.startswith('linux'):
    import grp
    
import logging
logger = logging.getLogger(__name__)

#
#
class SingletonPID():
    def __init__(self, modulePathHandler, pidFileName):
        self.modulePathHandler = modulePathHandler
        self.pidFileName = pidFileName
        
    def start(self):
        self.forceSingleton()
    
    #
    # Singleton things, use a PID-File
    #
    def _createPidFile(self, pidFileName, osPid ):
        pfn = self.modulePathHandler.getScratchClientBaseRelativePath(pidFileName)
        
        pidfile = open(pfn, "w")
        pidfile.write(str(osPid))
        pidfile.close()
        # make the file public, just in case it is generated by root, but next run of sw is user. 
        try:
            if sys.platform.startswith('linux'):
                groupinfo = grp.getgrnam('users')
                gid = groupinfo[2]
                os.chown(pfn, -1, gid )
        except Exception as e:
            logger.error("could not open PID File for chown {file:s} {e:s}".format(file=pfn, e=str(e)))
        try:
            os.chmod(pfn,  stat.S_IWUSR | stat.S_IRUSR | stat.S_IWGRP | stat.S_IRGRP  | stat.S_IWOTH | stat.S_IROTH)
        except Exception as e:
            logger.error("could not open PID File for chmod {file:s} {e:s}".format(file=pfn, e=str(e)))
            
        pass
        
    def _existPidFile(self, pidFileName ):
        return os.path.exists( self.modulePathHandler.getScratchClientBaseRelativePath(pidFileName) )
    
    def _readPidFile(self, pidFileName ):
        pidfile = open(self.modulePathHandler.getScratchClientBaseRelativePath(pidFileName),"r")
        pidString = pidfile.read()
        pidfile.close()
        return pidString
    
        
    def forceSingleton(self ):
        
        osPid = os.getpid()
        if not self._existPidFile(self.pidFileName) :
            self._createPidFile(self.pidFileName, osPid)
            return
    
        pidString = self._readPidFile(self.pidFileName)
        
        #
        # some strange format in file, ignore
        #
        if not re.match('[0-9]+', pidString):
            self._createPidFile((self.pidFileName), osPid)
            return
            
    
        if pidString == str(osPid):
            # something is real weird
            logger.error('quit program, forceSingleton found condition: os.pid == content current pid file')
            logger.error('try deleting pid file {name:s}'.format(name= self.pidFileName))
            sys.exit(19)
        else:
            if len(os.popen('ps %s' % pidString).read().split('\n')) > 2:
                logger.error(os.popen('ps %s' % pidString).read())
                
                logger.error('quit program, forceSingleton found running process, pid {pid:s}'.format(pid=pidString))
                sys.exit(20)
            else:
                logger.warning('forceSingleton: the previous server must have crashed' )
    
            self._createPidFile((self.pidFileName), osPid)
                    
    def cleanSingleton(self):
        if os.path.exists(self.modulePathHandler.getScratchClientBaseRelativePath(self.pidFileName)):
            os.remove( self.modulePathHandler.getScratchClientBaseRelativePath(self.pidFileName))
    
    def stop(self):
        self.cleanSingleton()
        
    def registerShutdown( self, scratchClient):
        self.shutdownListener =  scratchClient
        