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
# a simple name-Value-parser for scratch sensor-data
#

import logging
logger = logging.getLogger(__name__)


class NVToken:
    CHAR = 0
    EOL = 1
    
    def __init__(self, _type, char):
        self.type = _type
        self.char = char
    
    def __str__(self):
        if self.type == self.CHAR:
            return "NVToken[CHAR, " + self.char + "]"
        if self.type == self.EOL:
            return "NVToken[EOL]"

class SParser:
    def __init__(self, _input):
        self._input = _input
        self.pos = 0
        
    # instead of having new tokens each time, prepare one and reuse
    tchar = NVToken(NVToken.CHAR, 'x')
    teol = NVToken(NVToken.EOL, ' ')
    
    def getToken(self):
        if self.pos < len(self._input):
        
            self.tchar.char = self._input[self.pos]
            # t = NVToken(NVToken.CHAR, self._input[self.pos])
            self.pos += 1
            #return t
            return self.tchar
        return self.teol
    
           
class NameValueParser (SParser) :
    """parsing name value pairs for scratch remote sensor protocol
       input is without the trailing 'sensor-update'
       example: "a" 1 
       example: "a" 1 "c" "c-value" 
       example: "a" 1 "c" "c-value" "d""d" "d-val""ue" 
       
    """
    def __init__(self, _input):
        SParser.__init__(self, _input)
        
    def parse(self):
        result = []
        state = 0
        name = ''
        value = ''
        
        while True:

            c = self.getToken()
            # logger.debug("state = " + str(state) + " " + str(c) + " @" + str(self.pos))
            if state == 0:
                #
                # expect namevalue, at least one
                #
                if c.type == NVToken.CHAR:
                    if c.char == ' ':
                        pass
                    elif c.char == '"':
                        state = 1
                    else:
                        logger.error("%d: failure in parsing input, unexpected char %s at %d (%s)", state, c.char, self.pos, self._input )
                        break;
                elif c.type == NVToken.EOL:
                    logger.error("%d: failure in parsing input, unexpected EOL (%s)", state, self._input)
                    break;  
            elif state == 1000:
                #
                # expect namevalue or empty input
                #
                if c.type == NVToken.CHAR:
                    if c.char == ' ':
                        pass
                    elif c.char == '"':
                        state = 1
                    else:
                        logger.error("%d: failure in parsing input, unexpected char %s at %d (%s)", state, c.char, self.pos, self._input )
                        break;
                elif c.type == NVToken.EOL:
                    break;  
            elif state == 1:
                #
                # a quoted name has started, wait for closing '"'
                #
                if c.type == NVToken.CHAR:
                    if c.char == '"':
                        state = 20
                    else:
                        name += c.char  
                elif c.type == NVToken.EOL:
                    logger.error("%d: failure in parsing input, unexpected EOL (%s)", state, self._input)
                    break;  
            elif state == 20:
                #
                # closing quote, or a quote in a name ?
                #
                if c.type == NVToken.CHAR:
                    if c.char == '"':
                        name += c.char 
                        state = 1
                    elif c.char == ' ' :
                        state = 2  
                elif c.type == NVToken.EOL:
                    logger.error("%d: failure in parsing input, unexpected EOL (%s)", state, self._input)
                    break;  
            elif state == 2:
                #
                # name is complete, wait for value
                #
                if c.type == NVToken.CHAR:
                    if c.char == ' ':
                        pass
                    elif c.char == '"':
                        state = 100
                    else:
                        value = c.char
                        state = 200
                         
                elif c.type == NVToken.EOL:
                    logger.error("%d: failure in parsing input, unexpected EOL (%s)", state, self._input)
                    break;  
            elif state == 100:
                #
                # quoted value
                #
                if c.type == NVToken.CHAR:
                    if c.char == ' ':
                        value += c.char
                    elif c.char == '"':
                        state = 110
                    else:
                        value += c.char
                         
                elif c.type == NVToken.EOL:
                    logger.error("%d: failure in parsing input, unexpected EOL (%s)", state, self._input)
                    break;  
            elif state == 110:
                #
                # quoted value, end quote received. When now again a quote is received
                # it is a quote as char
                #
                if c.type == NVToken.CHAR:
                    if c.char == ' ':
                        
                        if logger.isEnabledFor(logging.DEBUG):
                            logger.debug("%d: name value =  %s, %s", state, name, value)
                        
                        result.append(list([name, value]))
                        name = ''
                        value = ''
                        state = 1000
                    elif c.char == '"':
                        value += '"'
                        state = 100
                    else:
                        logger.error("%d: failure in parsing input, unexpected char %c at %d (%s)", state, c.char, self.pos, self._input)
                        break 
                elif c.type == NVToken.EOL:
                    
                    if logger.isEnabledFor(logging.DEBUG):
                        logger.debug("%d: name value =  %s, %s", state, name, value)
                    
                    result.append(list([name, value]))
                    break;  
            elif state == 200:
                #
                # unquoted value
                #
                if c.type == NVToken.CHAR:
                    if c.char == ' ':
                        
                        if logger.isEnabledFor(logging.DEBUG):
                            logger.debug("%d: name value = %s, %s", state, name, value)
                        
                        result.append([name, value])
                        name = ''
                        value = ''
                        state = 1000
                    else:
                        value += c.char
                         
                elif c.type == NVToken.EOL:
                    result.append([name, value])
                    logger.warning("%d: unexpected EOL, but sequence is complete", state)
                    break;  
        return result

class BroadcastParser (SParser):
    """parsing broadcast strings for  scratch remote sensor protocol
       input is without the trailing 'broadcast'
       example: "a"
       example: "b""c"  
       example: \"\"\"a\"\"\" 
       
    """
    def __init__(self, _input):
        SParser.__init__(self, _input)
        
    def parse(self):
        state = 0
        name = ''
        
        while True:

            c = self.getToken()
            # logger.debug("state = " + str(state) + " " + str(c) + " @" + str(self.pos))
            if state == 0:
                #
                # expect quote or blank
                #
                if c.type == NVToken.CHAR:
                    if c.char == ' ':
                        pass
                    elif c.char == '"':
                        state = 1
                    else:
                        logger.error("%d: failure in parsing input, unexpected char %s at %d (%s)", state, c.char, self.pos, self._input )
                        break;
                elif c.type == NVToken.EOL:
                    logger.error("%d: failure in parsing input, unexpected EOL (%s)", state, self._input)
                    break;  
            elif state == 1:
                #
                # a quoted name has started, wait for closing '"'
                #
                if c.type == NVToken.CHAR:
                    if c.char == '"':
                        state = 20
                    else:
                        name += c.char  
                elif c.type == NVToken.EOL:
                    logger.error("%d: failure in parsing input, unexpected EOL (%s)", state, self._input)
                    break;  
            elif state == 20:
                #
                # closing quote, or a quote in a name ?
                #
                if c.type == NVToken.CHAR:
                    if c.char == '"':
                        name += c.char 
                        state = 1
                    elif c.char == ' ' :
                        break  
                elif c.type == NVToken.EOL:
                    logger.error("%d: failure in parsing input, unexpected EOL (%s)", state, self._input)
                    break  
                
        return name
