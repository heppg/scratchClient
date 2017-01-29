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
# install
#  sudo apt-get install python-pip
#  sudo  pip install cherrypy routes routes Mako


debug = False
debug_grid = False

import time
import os
import json

import threading

import sys 

       
import cherrypy
import mimetypes
import posixpath

from mako.template import Template
from mako.lookup import TemplateLookup


from ws4py.server.cherrypyserver import WebSocketPlugin, WebSocketTool
from ws4py.websocket import WebSocket


    
import xml.etree.ElementTree as ET
import publishSubscribe

import adapter.adapters 

import logging
logger = logging.getLogger(__name__)

import scratchClient

configuration = {'webapp2_static': { 'static_file_path': scratchClient.modulePathHandler.getScratchClientBaseRelativePath('htdocs/static') }}
commandResolver = None
#
# strange circular dependency to base module

lookup = TemplateLookup(directories=[scratchClient.modulePathHandler.getScratchClientBaseRelativePath('htdocs/static')
                                    ], input_encoding='utf-8', output_encoding='utf-8',encoding_errors='replace')

# ------------------------------------------------------------------------------------------------
# handle item-ID values throughout code

class IDManager():
    """internal use to produce unique ID for display elements"""
    
    def __init__(self):
        self.reset()
        
    def reset(self):
        self.scratch_input_command_2_display = []
        self.scratch_input_value_2_display = []
        self.scratch_output_command_2_display = []
        self.scratch_output_value_2_display = []
        self.displayid = 1000
            
    def getDisplayId_scratchInputCommand (self, scratch):
        _id = str(self.displayid)
        self.scratch_input_command_2_display.append( {'name': scratch, 'id': _id} )
        self.displayid += 1
        return _id  
     
    def getDisplayId_scratchInputValue (self, scratch):
        _id = str(self.displayid)
        self.scratch_input_value_2_display.append( {'name': scratch, 'id': _id} )
        self.displayid += 1
        return _id  
     
    def getDisplayId_scratchOutputCommand (self, scratch):
        _id = str(self.displayid)
        self.scratch_output_command_2_display.append( {'name': scratch, 'id': _id} )
        self.displayid += 1
        return _id  
     
    def getDisplayId_scratchOutputValue (self, scratch):
        _id = str(self.displayid)
        self.scratch_output_value_2_display.append( {'name': scratch, 'id': _id} )
        self.displayid += 1
        return _id  
     
    def getId (self):
        _id = str(self.displayid)
        self.displayid += 1
        return _id   
    
    def getDisplayId_scratchInputCommandSummary (self):
        """return a dict with scratch names as keys; values are list of to be animated elements"""
        res = {}
        for sic2d in  self.scratch_input_command_2_display:
            sn = sic2d['name']
            ln = sic2d['id']
            if res.has_key( sn):
                res[sn].append( ln )
            else:
                res[sn] = [ ln ]
                
        return res   
    def getDisplayId_scratchInputValueSummary (self):
        """return a dict with scratch names as keys; values are list of to be animated elements"""
        res = {}
        for sic2d in  self.scratch_input_value_2_display:
            sn = sic2d['name']
            ln = sic2d['id']
            if res.has_key( sn):
                res[sn].append( ln )
            else:
                res[sn] = [ ln ]
                
        return res   
    def getDisplayId_scratchOutputCommandSummary (self):
        """return a dict with scratch names as keys; values are list of to be animated elements"""
        res = {}
        for sic2d in  self.scratch_output_command_2_display:
            sn = sic2d['name']
            ln = sic2d['id']
            if res.has_key( sn):
                res[sn].append( ln )
            else:
                res[sn] = [ ln ]
                
        return res   
    def getDisplayId_scratchOutputValueSummary (self):
        """return a dict with scratch names as keys; values are list of to be animated elements"""
        res = {}
        for sic2d in  self.scratch_output_value_2_display:
            sn = sic2d['name']
            ln = sic2d['id']
            if res.has_key( sn):
                res[sn].append( ln )
            else:
                res[sn] = [ ln ]
                
        return res   
    
idManager = IDManager()
# ------------------------------------------------------------------------------------------------
# cherrypy-Handler
    
class BaseHandler():
    def __init__(self):
        pass
    
    def render_response(self, _template, context):
#        lookup = TemplateLookup(directories=['htdocs/static',
#                                           '../htdocs/static' 
#                                           ], input_encoding='utf-8', output_encoding='utf-8',encoding_errors='replace')
        
        # Renders a template and writes the result to the response.
 
        tmpl = lookup.get_template(_template)
        
        if logger.isEnabledFor(logging.DEBUG):
            logger.debug ( "template: %s", tmpl)
        
        ret =  tmpl.render( ** context )
        
        logger.debug ("rendered: %s", ret)
        return ret
    
#    def relPath(self, path):
#        p =  configuration['webapp2_static']['static_file_path'] + path
#        logger.debug("relPath %s, %s, %s", p, "pwd", os.getcwd())
#        return p#

#    def fullPath(self, path):
#        p = os.path.abspath(os.path.join( configuration['webapp2_static']['static_file_path'], path))
#        return p
    
# ------------------------------------------------------------------------------------------------
# cherrypy-Handler
class ScratchClientMain(BaseHandler):
    def __init__(self):
        BaseHandler.__init__(self)
        self.additionalpaths = []
        
    def addPath(self, name, path, comment):
        
        self.additionalpaths.append( {'name': name, 'path': path, 'comment': comment } )
        
        
    def get(self):
        context = { 'additionalpaths' :self.additionalpaths }
        
        return self.render_response('html/main.html', context)


# ------------------------------------------------------------------------------------------------
# cherrypy-Handler
#
class ConfigHandler(BaseHandler ):
    """return the xml-config"""
    config = None
    
    def __init__(self, config):
        self.config = config
        
    def get(self):
        xmlConfigName = parentApplication.config.filename
        
        
        # xmlConfig = ET.tostring(parentApplication.config.tree.getroot(),  encoding='us-ascii', method='xml')
        xmlConfigInput = ET.tostring(parentApplication.config.tree.getroot(),  encoding='UTF-8', method='html')
        
        #
        # in python 2.7, there is a string array
        # in python 3.2, there is a bytes array
        
        if xmlConfigInput.__class__.__name__ == 'bytes':
            xmlConfig = xmlConfigInput.decode('UTF-8')
        else:
            
            xmlConfig = xmlConfigInput.decode('utf-8')
            
       
        
        xmlConfig= xmlConfig.replace('<', '&lt;')
        xmlConfig= xmlConfig.replace('>', '&gt;')
        xmlConfig= xmlConfig.replace('\n', '<br/>')

        context = {'configName': xmlConfigName, 'config':xmlConfig }
        
        return self.render_response('html/config.html', context)

# ------------------------------------------------------------------------------------------------
# cherrypy-Handler
#
class CommandHandler(BaseHandler ):
    """Command input from web interface"""
    
    def __init__(self):
        #publishSubscribe.Pub.subscribe('input.scratch.command')
        pass
    
    def postInput(self, adapter='', command='somecommand'):
        logger.debug("input, command=%s", command)
        publishSubscribe.Pub.publish('input.scratch.{name:s}'.format(name=command), {'name':command})
        ## eventHandler.resolveCommand(self, adapter, command, qualifier='input')
        return "no_return"

    def postOutput(self, adapter='', command='somecommand'):
        logger.debug("output, command=%s", command)
        publishSubscribe.Pub.publish('output.scratch.command', {'name':command})
        # eventHandler.resolveCommand(self, adapter, command, qualifier='output')
        return "no_return"

# ------------------------------------------------------------------------------------------------
# cherrypy-Handler
#
           
class AdaptersHandler(BaseHandler):
    """return a graphical representation of the adapter config"""

    def rectHelper(self, gg, _id=None,
                           x = 0,
                           y = 0, 
                           width=0,
                           height = 0,
                           style = ''):
            
        rect_Adapter = ET.SubElement(gg, "rect")
        if _id != None:
            rect_Adapter.set('id', str(_id) )
        rect_Adapter.set('x', str(x))
        rect_Adapter.set('y', str(y))
        rect_Adapter.set('width', str(width))
        rect_Adapter.set('height', str(height))
        rect_Adapter.set('style', style)

        return rect_Adapter

    def textHelper(self, gg,  _id = None, 
                       text='notext', 
                            x= 0,
                            y= 0,
                            style = ''):
            
            rect_AdapterName = ET.SubElement(gg, "text")
            if _id != None:
                rect_AdapterName.set('id', str(_id))
            rect_AdapterName.set('x', str(x))
            rect_AdapterName.set('y', str(y))
            rect_AdapterName.text = text
            rect_AdapterName.set('style', style)

        
    def calculateHeight(self, _adapter):
        """height in logical grid height units; depends on number of input/output, parameter etc"""
        
        l = 2 # minimum 2 text columns in adapter for names
        if isinstance(_adapter,  adapter.adapters.GPIOAdapter):
            l += len( _adapter.gpios )
        
        i = 0
        o = 0
        # inputs can have multiple scratch names
        for inp in _adapter.inputs:
            ix = len( inp.scratchNames )
            i += ix
        for inp in _adapter.input_values:
            ix = len( inp.scratchNames )
            i += ix
        i += len(_adapter.parameters)
        
        # outputs are 'unique'
        o += len(_adapter.outputs)
        o += len(_adapter.output_values)
        h = max(i, o, l)
        
        return h 

    def createOnclickInputCommand(self, _id, scratch_name ):
        jScript = ''
        jScript += '{ \n' 
        jScript += '    obj = document.getElementById("{on:s}");  \n'.format(on=_id)
        jScript += '    if ( obj != null) \n' 
        jScript += '        obj.setAttribute("onclick", "click_Command(evt,\'{id:s}\' ,\'{co:s}\' , \'{sc:s}\' );");  \n'.format( id=_id, co='input.command', sc=scratch_name )
        jScript += '} \n'
        return jScript
    
    def createOnclickOutputCommand(self, _id, scratch_name):
        jScript = ''
        jScript += '{ \n' 
        jScript += '    obj = document.getElementById("{id:s}"); \n'.format(id=_id)
        jScript += '    if ( obj != null) \n' 
        jScript += '        obj.setAttribute("onclick", "click_Command(evt,\'{id:s}\' ,\'{co:s}\' , \'{sc:s}\' );");  \n'.format( id=_id, co='output.command', sc=scratch_name )
        jScript += '} \n'
        return jScript

    def createOnclickInputValue(self, objId, rectId, adapter_name, command_name):
        jScript = ''
        jScript += '{ \n' 
        #jScript += '    document.getElementById("{id:s}").setAttribute("onclick", "click_Value_input(evt, \'{rectId:s}\', \'{ad:s}\', \'{co:s}\');" ); \n'.format(id=objId, rectId=rectId, ad=adapter_name, co=command_name)
        jScript += '    document.getElementById("{id:s}").setAttribute("onclick", "click_Value_input_float(evt, \'{rectId:s}\', \'{ad:s}\', \'{co:s}\');" ); \n'.format(id=objId, rectId=rectId, ad='input.value', co=command_name)
        jScript += '} \n'
        return jScript
    
    def createOnclickOutputValue(self, objId, rectId, adapter_name, command_name):
        jScript = ''
        jScript += '{   \n' 
        #jScript += '    document.getElementById("{id:s}").setAttribute("onclick", "click_Value_output(evt, \'{rectId:s}\', \'{ad:s}\', \'{co:s}\');" ); \n'.format(id=objId, rectId=rectId, ad=adapter_name, co=command_name)
        jScript += '    document.getElementById("{id:s}").setAttribute("onclick", "click_Value_input_float(evt, \'{rectId:s}\', \'{ad:s}\', \'{co:s}\');" ); \n'.format(id=objId, rectId=rectId, ad='output.value', co=command_name)
        jScript += '}   \n'
        return jScript
    
    def get(self):
        idManager.reset()
        xmlConfigName = parentApplication.config.filename
        description = parentApplication.config.getDescription().replace('\n', '<br/>')
        
        jScript = ''
        list_adapter_input_command = []
        list_adapter_output_command = []
        #
        # build svn tree from adapters
        #
        # styles
        #
        textStyleDefault = 'font-size:12px;font-style:normal;font-weight:normal;line-current_height:125%;letter-spacing:0px;word-spacing:0px;fill:#000000;fill-opacity:1;stroke:none;font-family:sans-serif'
        textStyle = textStyleDefault
        
        boldTextStyleDefault = 'font-size:12px;font-style:normal;font-weight:bold;line-current_height:125%;letter-spacing:0px;word-spacing:0px;fill:#0000ff;fill-opacity:1;stroke:none;font-family:sans-serif'
        boldTextStyle = boldTextStyleDefault
        
        gpioTextStyleDefault = 'font-size:8px;font-style:normal;font-weight:normal;line-current_height:125%;letter-spacing:0px;word-spacing:0px;fill:#808080;fill-opacity:1;stroke:none;font-family:sans-serif'
        gpioTextStyle = gpioTextStyleDefault
        
        rectStyleDefault = 'fill:#e0e0e0;fill-opacity:0.52208835;stroke:#0000ff;stroke-width:1;stroke-miterlimit:4;stroke-opacity:1;stroke-dasharray:none'
        rectStyle = rectStyleDefault
        # make command input areas visible by slightly darker background
        commandBackgroundStyleDefault = 'fill:#e0e0e0'
        commandBackgroundStyleTest = 'fill:#808080'
        
        commandBackgroundStyle = commandBackgroundStyleDefault
        if debug:
            commandBackgroundStyle = commandBackgroundStyleTest
            
        commandBackgroundStyle2Default = 'fill:#ff0000'
        commandBackgroundStyle2 = commandBackgroundStyle2Default
        
        valueStyleDefault = 'stroke:#0000ff;stroke-width:1'
        valueStyle = valueStyleDefault
        paraLineStyleDefault = 'fill:none;stroke:#00FF00;stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;stroke-opacity:1'
        paraLineStyle = paraLineStyleDefault
        
        inputLineStyleDefault = 'fill:none;stroke:#000000;stroke-width:1px;stroke-linecap:butt;stroke-linejoin:miter;stroke-opacity:1'
        inputLineStyle = inputLineStyleDefault
        
        helperLineStyleDefault = 'fill:none;stroke:#303030;stroke-width:2px;stroke-linecap:butt;stroke-linejoin:miter;stroke-opacity:1'
        helperLineStyle = helperLineStyleDefault
        #
        ET.register_namespace('', 'http://www.w3.org/2000/svg' )
        svg = ET.Element("{http://www.w3.org/2000/svg}svg")
        g = ET.SubElement(svg, "g")


        column_10 =  1
        column_20 = 120
        column_20_parameter = 80
            # parameter und Werteeingabe
        column_30 = 220
            # Linie Eingang
        column_40 = 340
            # Adapter
        column_50 = 460
            # output line    
        column_60 = 580
            # output values
        column_70 = 680
        column_80 = 800
        column_90 = 820
         
        # absolute current_height used so far
        current_height = 1 # top margin
        vSpacing = 20
        vGap = 4
        
        if debug_grid:
            # display a grid
            grid = {'c10' : column_10, 
                    'c20' : column_20, 
                    'c30' : column_30, 
                    'c40' : column_40, 
                    'c50' : column_50, 
                    'c60' : column_60, 
                    'c70' : column_70, 
                    'c80' : column_80, 
                    'c90' : column_90,
                    'c20p': column_20_parameter }
            
            for n in grid:
                x = grid[n]
                line_Val = ET.SubElement(g, "line")
                line_Val.set('x1', str(x) )
                line_Val.set('x2', str(x) )
                line_Val.set('y1', str(current_height) )
                line_Val.set('y2', str(current_height+vSpacing) )
                line_Val.set('style', helperLineStyle )
                
                self.textHelper( g,  text= n, 
                            x= x+5 ,
                            y= current_height +vSpacing,
                            style = boldTextStyle)
      
            current_height += vSpacing

        for _adapter in parentApplication.config.getAdapters():
            gg = ET.SubElement(g, "g")
            
            h = self.calculateHeight(_adapter)
            
            a = self.rectHelper(gg, _id='adapter' + '_' + _adapter.name,
                           x = column_40,
                           y = current_height, 
                           width=column_50-column_40,
                           height = h * vSpacing + vSpacing,
                           style = rectStyle)
            
            t = ET.SubElement(a, 'title')
            t.text=_adapter.description
    
            self.textHelper(gg,  text=_adapter.name, 
                            x= column_40 + 5,
                            y= current_height +  vSpacing,
                            style = boldTextStyle)
            
            # print the class name in multiple lines
            #
            class_segments = _adapter.className.split('.')
            
            for i in range( len(class_segments) ):
                s = class_segments[i]
                self.textHelper(gg,  text=s, 
                                x= column_40 + 5,
                                y= current_height +  (i+2) * vSpacing,
                                style = textStyle)
            
            if isinstance(_adapter,  adapter.adapters.GPIOAdapter):
                gi = 1
                for gpio in _adapter.gpios:
                    
                    atext = ''
                    if gpio.alias == None:
                        atext = "[{num:02d}] {name:s}".format(name=gpio.port, num=gpio.portNumber)
                    else:
                        atext = "[{num:02d}] {name:s} ({alias:s})".format(name=gpio.port, num=gpio.portNumber, alias=gpio.alias)
                        
                    self.textHelper(gg,  text=atext , 
                                    x= column_40 + 5,
                                    y= current_height +  2 * vSpacing + gi*vSpacing,
                                    style = gpioTextStyle)
                    gi += 1
                
            # leftside are the left side connectors
            leftSide = 0            
            for inp in _adapter.inputs:
                
                nCommand = 0
                for _ in range(len(inp.scratchNames)):
                    
                    _id_backanimation = idManager.getDisplayId_scratchInputCommand( inp.scratchNames[nCommand] )
                    _id_back = idManager.getId( )
                    _id_text = idManager.getId( )
                    # list_adapter_input_command .append([_id , _id + "_back"])
                    
                    jScript += self.createOnclickInputCommand(_id_text , inp.scratchNames[nCommand])
                    jScript += self.createOnclickInputCommand(_id_back , inp.scratchNames[nCommand])
                    
                    textInpBack = self.rectHelper(gg, _id=_id_back,
                           x = column_10,
                           y = current_height + (leftSide+0+nCommand) * vSpacing + vGap, 
                           width=column_20-column_10,
                           height = vSpacing-vGap,
                           style = commandBackgroundStyle)
                    
                    animate = ET.SubElement(textInpBack, "animate")
                    animate.set("id", _id_backanimation)
                    animate.set("attributeType", "CSS")
                    animate.set("attributeName", "fill") 
                    animate.set("from", "#ffffff")
                    animate.set("to", "#ff0000")
                    animate.set("dur", "0.3s");
                    
                    text_Inp_Cmd = self.textHelper(gg,  text=inp.scratchNames[nCommand], 
                                    _id = _id_text,
                                    x= column_10+5,
                                    y= current_height + (leftSide+1+nCommand) * vSpacing - vGap,
                                    style = textStyle)

                    line_Inp = ET.SubElement(gg, "line")
                    line_Inp.set('x1', str(column_10) )
                    line_Inp.set('x2', str(column_20+1) )
                    line_Inp.set('y1', str(current_height + (leftSide+1+nCommand) * vSpacing) )
                    line_Inp.set('y2', str(current_height + (leftSide+1+nCommand) * vSpacing) )
                    line_Inp.set('style', inputLineStyle )
                    
                    if nCommand > 0:
                        line_Inp = ET.SubElement(gg, "line")
                        line_Inp.set('x1', str(column_20) )
                        line_Inp.set('x2', str(column_20) )
                        line_Inp.set('y1', str(current_height + (leftSide+1+nCommand-1) * vSpacing) )
                        line_Inp.set('y2', str(current_height + (leftSide+1+nCommand-0) * vSpacing) )
                        line_Inp.set('style', inputLineStyle )
                        
                    nCommand += 1
                
                text_Inp = ET.SubElement(gg, "text")
                text_Inp.set('x', str(column_30+3))
                text_Inp.set('y', str(current_height + (leftSide+nCommand) * vSpacing - vGap))
                text_Inp.text = inp.name
                text_Inp.set('style', textStyle)
                
                line_Inp = ET.SubElement(gg, "line")
                line_Inp.set('x1', str(column_20) )
                line_Inp.set('x2', str(column_40) )
                line_Inp.set('y1', str(current_height + (leftSide+nCommand) * vSpacing) )
                line_Inp.set('y2', str(current_height + (leftSide+nCommand) * vSpacing) )
                line_Inp.set('style', inputLineStyle )
 
                leftSide += nCommand
                
            for val in _adapter.input_values:
                nValue = 0
                for _ in range(len(val.scratchNames)): 
                    _id_text = idManager.getDisplayId_scratchInputValue( val.scratchNames[nValue] )
                    _id = idManager.getId()
                
                    rect_Out = ET.SubElement(gg, "rect")
                    rect_Out.set('id', _id+'_back')
                    rect_Out.set('x', str(column_20))
                    rect_Out.set('y', str(current_height + (leftSide+0+nValue) * vSpacing + vGap ))
                    rect_Out.set('width', str(column_30-column_20))
                    rect_Out.set('height', str(vSpacing - vGap))
                    rect_Out.set('style', rectStyle)
                
                    textValue_Out = ET.SubElement(gg, "text")
                    textValue_Out.set('id', _id_text )
                    textValue_Out.set('x', str(column_20+4 ))
                    textValue_Out.set('y', str(current_height + (leftSide+1+nValue) * vSpacing - vGap))
                    textValue_Out.text = '?'
                    textValue_Out.set('style', textStyle)

                    jScript += self.createOnclickInputValue( _id_text, _id+'_back', _adapter.name, val.scratchNames[nValue])

                    text_Val = ET.SubElement(gg, "text")
                    text_Val.set('x', str(column_10))
                    text_Val.set('y', str(current_height + (leftSide+1+nValue) * vSpacing - vGap ))
                    text_Val.text = val.scratchNames[nValue]
                    text_Val.set('style', textStyle)
                    
                    line_Val = ET.SubElement(gg, "line")
                    line_Val.set('x1', str(column_10) )
                    line_Val.set('x2', str(column_20) )
                    line_Val.set('y1', str(current_height + (leftSide+1+nValue) * vSpacing) )
                    line_Val.set('y2', str(current_height + (leftSide+1+nValue) * vSpacing) )
                    line_Val.set('style', inputLineStyleDefault )
                    
                    nValue += 1

                text_Val = ET.SubElement(gg, "text")
                text_Val.set('x', str(column_30 +4 ))
                text_Val.set('y', str(current_height + (leftSide+0+nValue) * vSpacing - vGap))
                text_Val.text = val.name
                text_Val.set('style', textStyle)
                

                line_Val = ET.SubElement(gg, "line")
                line_Val.set('x1', str(column_30) )
                line_Val.set('x2', str(column_40) )
                line_Val.set('y1', str(current_height + (leftSide+0+nValue) * vSpacing) )
                line_Val.set('y2', str(current_height + (leftSide+0+nValue) * vSpacing) )
                line_Val.set('style', inputLineStyleDefault )
 
                leftSide += nValue
                
            for para in _adapter.parameters:
                
                text_Para = ET.SubElement(gg, "text")
                text_Para.set('x', str(column_20_parameter))
                text_Para.set('y', str(current_height + (leftSide+1) * vSpacing - vGap))
                text_Para.text = para + '= ' + _adapter.parameters[para]
                text_Para.set('style', textStyle)
                
                line_Para = ET.SubElement(gg, "line")
                line_Para.set('x1', str( column_20_parameter) )
                line_Para.set('x2', str( column_40) )
                line_Para.set('y1', str(current_height + (leftSide+1) * vSpacing) )
                line_Para.set('y2', str(current_height + (leftSide+1) * vSpacing) )
                line_Para.set('style', paraLineStyle )
 
                leftSide += 1
                
            # rightside are the output connectors    
            rightSide = 0   
            
            for out in _adapter.outputs:
                
                text_Out = ET.SubElement(gg, "text")
                text_Out.set('x', str(column_50+5))
                text_Out.set('y', str(current_height + (rightSide+1) * vSpacing - vGap))
                text_Out.text = out.name
                text_Out.set('style', textStyle)
                
                _id = 'adapter_' + _adapter.name + '_' + out.scratchNames[0]
                _id_backanimation = idManager.getDisplayId_scratchOutputCommand( out.scratchNames[0] )
                   
                jScript += self.createOnclickOutputCommand(_id + "_text", out.scratchNames[0])
                jScript += self.createOnclickOutputCommand(_id + "_back", out.scratchNames[0])
                
                textInpBack = ET.SubElement(gg, "rect")
                textInpBack.set('id', _id+ '_back' )
                textInpBack.set('x', str(column_70))
                textInpBack.set('y', str(current_height + (rightSide+0) * vSpacing +  vGap))
                textInpBack.set('width', str(column_80-column_70))
                textInpBack.set('height', str(vSpacing - vGap))
                textInpBack.set('style', commandBackgroundStyle)

                animate = ET.SubElement(textInpBack, "animate")
                animate.set("id", _id_backanimation)
                animate.set("attributeType", "CSS")
                animate.set("attributeName", "fill") 
                animate.set("from", "#ffffff")
                animate.set("to", "#ff0000")
                animate.set("dur", "0.3s");
                
                text_Command = ET.SubElement(gg, "text")
                text_Command.set('id', _id+ '_text' )
                text_Command.set('x', str(column_70+5))
                text_Command.set('y', str(current_height + (rightSide+1) * vSpacing-vGap))
                text_Command.text = out.scratchNames[0]
                text_Command.set('style', textStyle)

                
                line_Out = ET.SubElement(gg, "line")
                line_Out.set('x1', str(column_50) )
                line_Out.set('x2', str(column_80) )
                line_Out.set('y1', str(current_height + (rightSide+1) * vSpacing) )
                line_Out.set('y2', str(current_height + (rightSide+1) * vSpacing ))
                line_Out.set('style', inputLineStyleDefault )

                rightSide += 1
            
            for out in _adapter.output_values:
                _id = 'adapter_{an:s}_{on:s}'.format(an=_adapter.name, on=out.scratchNames[0])
                _id_text = idManager.getDisplayId_scratchOutputValue(out.scratchNames[0])
                
                text_Out = ET.SubElement(gg, "text")
                text_Out.set('x', str(column_50+5))
                text_Out.set('y', str(current_height + (rightSide+1) * vSpacing -vGap ))
                text_Out.text = out.name
                text_Out.set('style', textStyle)
                
                text_Command = ET.SubElement(gg, "text")
                text_Command.set('x', str(column_70+5 ))
                text_Command.set('y', str(current_height + (rightSide+1) * vSpacing - vGap))
                text_Command.text = out.scratchNames[0]
                text_Command.set('style', textStyle)
                
                line_Out = ET.SubElement(gg, "line")
                line_Out.set('x1', str(column_50) )
                line_Out.set('x2', str(column_80) )
                line_Out.set('y1', str(current_height + (rightSide+1) * vSpacing) )
                line_Out.set('y2', str(current_height + (rightSide+1) * vSpacing ))
                line_Out.set('style', inputLineStyleDefault )

                rect_Out = ET.SubElement(gg, "rect")
                rect_Out.set('id', _id + '_back')
                rect_Out.set('x', str(column_60))
                rect_Out.set('y', str(current_height + (rightSide+0) * vSpacing + vGap))
                rect_Out.set('width', str(column_70-column_60))
                rect_Out.set('height', str(vSpacing - vGap))
                rect_Out.set('style', rectStyle)

                # logger.debug("output_box id %s", _id)
                
                
                textValue_Out = ET.SubElement(gg, "text")
                textValue_Out.set('id', _id_text)
                textValue_Out.set('x', str(column_60+5))
                textValue_Out.set('y', str(current_height + (rightSide+1) * vSpacing -vGap))
                textValue_Out.text = '?'
                textValue_Out.set('style', textStyle)
                
                jScript += self.createOnclickOutputValue(_id+'_back', _id+'_back', _adapter.name, out.scratchNames[0])
                jScript += self.createOnclickOutputValue(_id_text, _id+'_back', _adapter.name, out.scratchNames[0])
 
                rightSide += 1
            
            # keep track of current_height, add some gap between adapters.    
            current_height += h*vSpacing + vSpacing + 10
                     
        svg.set('width', str(column_90+1))
        svg.set('height', str(current_height+20))
        
        svgText  = ET.tostring(svg)
        logger.debug(svgText)

        #
        # javascript, tooltips
        #
        
        for _adapter in parentApplication.config.getAdapters():
            adapterId = 'adapter_{an:s}'.format(an=_adapter.name )
            

        jScript += '   var websocket = new WebSocket("ws://" + window.location.host + "/ws");'
        jScript += """
           websocket.onmessage = function(e){
                var server_message = e.data;
                console.log(server_message);
                jObj = JSON.parse(server_message); 
                """
        # --------------------------
        jScript += "if ( jObj.command == 'scratch_input_command' ){ "
        #
        # add the scratch command to id-mappers
        res = idManager.getDisplayId_scratchInputCommandSummary()
        for sn in res.keys():
            jScript += "        if ( jObj.name == \"{name:s}\" ){{\n".format( name=sn )
            for ln in res[sn]:
                jScript += "             document.getElementById( \"{id:s}\" ).beginElement();\n".format(id=ln)
            jScript += "        }\n"   
        jScript +="""         
           }"""
        # --------------------------
           
        jScript +="""      if ( jObj.command == 'scratch_input_value' ){
           """
        #
        # add the scratch command to id-mappers
        res = idManager.getDisplayId_scratchInputValueSummary()
        for sn in res.keys():
            jScript += "        if ( jObj.name == \"{name:s}\" ){{\n".format( name=sn )
            for ln in res[sn]:
                jScript += "             document.getElementById( \"{id:s}\" ).textContent = jObj.value;\n".format(id=ln)
            jScript += "        }\n"   
        jScript +="""         
                }
        """
        # --------------------------
        jScript +="""      if ( jObj.command == 'scratch_output_command' ){
           """
        #
        # add the scratch command to id-mappers
        res = idManager.getDisplayId_scratchOutputCommandSummary()
        for sn in res.keys():
            jScript += "        if ( jObj.name == \"{name:s}\" ){{\n".format( name=sn )
            for ln in res[sn]:
                jScript += "             document.getElementById( \"{id:s}\" ).beginElement();\n".format(id=ln)
            jScript += "        }\n"   
        jScript +="""         
                }
        """
        jScript +="""      if ( jObj.command == 'scratch_output_value' ){
           """
        #
        # add the scratch command to id-mappers
        res = idManager.getDisplayId_scratchOutputValueSummary()
        for sn in res.keys():
            jScript += "        if ( jObj.name == \"{name:s}\" ){{\n".format( name=sn )
            for ln in res[sn]:
                jScript += "             document.getElementById( \"{id:s}\" ).textContent = jObj.value;\n".format(id=ln)
            jScript += "        }\n"   
        jScript +="""         
                }
        """
        
            
        jScript += """   }
           
           websocket.onopen = function(){
               console.log('Connection open!');
               document.getElementById("status.host").textContent ='Connection to scratchClient open';
               document.getElementById("status.host").style.background = 'green';
               
           }
           
           websocket.onclose = function(){
               console.log('Connection closed');
               document.getElementById("status.host").textContent ='Connection to scratchClient closed';
                document.getElementById("status.host").style.background = 'red';
           }
        """

        jScript += '// click on an object (send event)' + '\n'
        jScript += 'function click_Command (evt, id, command, scratch){' + '\n'
        jScript += '    try {' + '\n'
        jScript += '        message = JSON.stringify( { id:id, command:command, scratch:scratch }); '
        jScript += '        websocket.send(message);' + '\n'
        jScript += '    }   catch(err) {' + '\n'
        jScript += '        console.log( err.message );' + '\n'
        jScript += '    }' + '\n'
        jScript += '}' + '\n'
        #
        #
        #    
        
#         jScript += 'function displayBroadcastEvent( id)' + '\n'
#         jScript += '{' + '\n'
#         jScript += '    document.getElementById( id ).beginElement();' + '\n'
#         jScript += '}' + '\n'
# 
#         jScript += 'function displayUpdateEvent( id, value)' + '\n'
#         jScript += '{' + '\n'
#         jScript += '    document.getElementById( id ).textContent = value;' + '\n'
#         jScript += '}' + '\n'

        #
        # Output Values, need an editor popup
        #

        html_preamble = """
            <!-- this is target structure -->
            
            <div id="DIV.float"        style="position:absolute;left:80px;top:80px;width:160px;height:60px;display:none;background-color:#f8f8f8;font-family:sans-serif;font-size:10;border-width=2px;border-color=x200000;border:3px coral solid;">
                <div id="HEADER.float" style="position:absolute;left:3px;top:3px;font-family:sans-serif;font-size:14;font-weight:bold;">header</div>
                <img id="qwInput" onclick="close_floatInput();" src="/files/icon/16x16_Delete.jpg" 
                                       style="position:absolute;right:3px;top:3px;background-color:#FFFFFF;"/>
                <input id='I00.float' size="24" 
                                       style="position:absolute;left:3px;bottom:3px;font-family:sans-serif;font-size:12;fontWeight:normal;" />
            </div>
            
            
            <div id='status.host' style="position:fixed;width:240;current_height=20; left:0;bottom:0;background-color:#FFDDDD;font-family:sans-serif;font-size:12">
                 
            </div>
            """
            
        if debug:
            #
            # place mouse coordinates to screen.    
            jScript += """
            function readMouseMove(e){
                var result_x = document.getElementById('x_result');
                var result_y = document.getElementById('y_result');
                result_x.innerHTML = e.clientX;
                result_y.innerHTML = e.clientY;
            }
            document.onmousemove = readMouseMove;
            """
            html_preamble += """   
                <div id='status.host' style="position:fixed;width:240;current_height=20; left:240;bottom:0;background-color:#ffffff;font-family:sans-serif;font-size:12">
                     <div id='x_result' style="position:absolute;width:100;current_height=20; left:20;bottom:0;" >X</div>
                     <div id='y_result' style="position:absolute;width:100;current_height=20; left:120;bottom:0;" >Y</div>
                </div>
            """

        html_preamble += """
            <div style="position:absolute;width:280;right:0;top:0;background-color:#FFDDDD;font-family:sans-serif;font-size:12">
                 <ul id ='eventListId'  />
            </div>
        """
        
        jScript += """
                function close_floatInput(evt){
                    var tstObj = document.getElementById("DIV.float"); 
                    tstObj.style.display = 'none';
                }
                function searchKeyPressInput_float(e){
                    
                    var textObj = document.getElementById("I00.float"); 
                    
                    console.log("command " + pupup_state.command);
                    console.log("name    " + pupup_state.name);
                    console.log("value   " + textObj.value);
                    
                    // look for window.event in case event isn't passed in
                    if (typeof e == 'undefined' && window.event) { e = window.event; }
                    if (e.keyCode == 13)
                    {
                        message = JSON.stringify( { command: pupup_state.command, scratch:pupup_state.name, value:textObj.value } )
                        websocket.send ( message)  
                        textObj.value = ''
                    }
                } 
                function click_Value_input_float(evt, svgid, command, value_name){
                    console.log("command " + command);
                    console.log("name    " + value_name);
                    console.log("svgid   " + svgid);
                    
                    var svgText = document.getElementById(svgid);
                    var bbox = svgText.getBBox();
    
                    var divSvg = document.getElementById("DIV.svg");
                    
                                        
                    // var x = divSvg.offsetLeft + bbox.x;
                    // var y = divSvg.offsetHeight + bbox.y;
                    var x = evt.clientX;
                    var y = evt.clientY +window.scrollY +20;
                    
                    var tstObj = document.getElementById("DIV.float"); 
                    
                    tstObj.style.left= x;
                    tstObj.style.top = y;
                    tstObj.style.display = 'initial';
                    
                    var tstHeader = document.getElementById("HEADER.float"); 
                    tstHeader.textContent = value_name;
                    
                    console.log("bbox.x " + bbox.x);
                    console.log("bbox.y " + bbox.y);
                    
                    console.log("divSvg.offsetLeft   " + divSvg.offsetLeft   );
                    console.log("divSvg.offsetHeight " + divSvg.offsetHeight );
                    
                    pupup_state = { command: command, name: value_name};
                    
                    var textObj = document.getElementById("I00.float"); 
                    textObj.onkeypress = function(e){ searchKeyPressInput_float( e ); };
                    
                }
                """
                
        logger.debug (jScript)
        context = {'description'  : description,
                   'html_preamble': html_preamble, 
                   'svg'          : svgText ,
                   'jScript'      : jScript ,
                   'configName'   : xmlConfigName }
        
        return self.render_response('html/adapters.html', context)
            
            
class FileProvider(BaseHandler):
    
    def file(self, path=''):
        if debug:
            print('FileProvider path', path)        
            print('FileProvider configuration', configuration)
        
        _dir =  configuration['webapp2_static']['static_file_path']
        
        srcfile = posixpath.normpath(posixpath.join(_dir, path))
        if debug:
            print('FileProvider srcfile', srcfile)
        
        if os.path.exists(srcfile):
            try:
                mime = mimetypes.guess_type(srcfile)
                if logger.isEnabledFor(logging.DEBUG):
                    logger.debug(srcfile)
                    logger.debug( mime )
                cherrypy.response.headers['Content-Type']=  mime[0]
                
                f = open(srcfile, 'rb')
                fd = f.read()
                f.close()
                
                return fd

            except:
                logger.warn('not found %s', srcfile)
                raise cherrypy.HTTPError(404)
                        
        else:
            logger.warn('not found %s: %s', path, srcfile)
            raise cherrypy.HTTPError(404)

class AdapterAnimationWebSocket(WebSocket):
    
    def __init__(self, sock, protocols=None, extensions=None, environ=None, heartbeat_freq=None):
        WebSocket.__init__(self, sock, protocols, extensions, environ, heartbeat_freq)
        logger.info("AdapterAnimationWebSocket.init")
        
    def received_message(self, message):
        """
        Automatically sends back the provided ``message`` to
        its originating endpoint.
        """
        if debug:
            print(message.data)
        msg = json.loads(message.data)
        
        if msg['command'] == 'input.command':
            publishSubscribe.Pub.publish('scratch.input.command.{name:s}'.format(name=msg['scratch']), { 'name':msg['scratch'] } ) 
            
        if msg['command'] == 'output.command':
            publishSubscribe.Pub.publish('scratch.output.command.{name:s}'.format(name=msg['scratch']), { 'name':msg['scratch'] } ) 
           
        if msg['command'] == 'input.value':
            publishSubscribe.Pub.publish('scratch.input.value.{name:s}'.format(name=msg['scratch']), { 'name':msg['scratch'], 'value':msg['value'] } ) 
            
        if msg['command'] == 'output.value':
            publishSubscribe.Pub.publish('scratch.output.value.{name:s}'.format(name=msg['scratch']), { 'name':msg['scratch'], 'value':msg['value'] } ) 
        
    def opened(self):
        for _adapter in parentApplication.config.getAdapters():
            for inp in _adapter.inputs:
                for scratchName in inp.scratchNames:
                    publishSubscribe.Pub.subscribe("scratch.input.command.{name:s}".format(name=scratchName), self.inputCommand  )
            for inp in _adapter.input_values:
                for scratchName in inp.scratchNames:
                    publishSubscribe.Pub.subscribe("scratch.input.value.{name:s}".format(name=scratchName), self.inputValue )
            
            for out in _adapter.outputs:
                for scratchName in out.scratchNames:
                    publishSubscribe.Pub.subscribe("scratch.output.command.{name:s}".format(name=scratchName), self.outputCommand )
            for out in _adapter.output_values:
                for scratchName in out.scratchNames:
                    publishSubscribe.Pub.subscribe("scratch.output.value.{name:s}".format(name=scratchName), self.outputValue )
        
    def closed(self, code, reason=None):
        for _adapter in parentApplication.config.getAdapters():
            for inp in _adapter.inputs:
                for scratchName in inp.scratchNames:
                    publishSubscribe.Pub.unsubscribe("scratch.input.command.{name:s}".format(name=scratchName), self.inputCommand  )
            for inp in _adapter.input_values:
                for scratchName in inp.scratchNames:
                    publishSubscribe.Pub.unsubscribe("scratch.input.value.{name:s}".format(name=scratchName), self.inputValue )
            
            for out in _adapter.outputs:
                for scratchName in out.scratchNames:
                    publishSubscribe.Pub.unsubscribe("scratch.output.command.{name:s}".format(name=scratchName), self.outputCommand )
            for out in _adapter.output_values:
                for scratchName in out.scratchNames:
                    publishSubscribe.Pub.unsubscribe("scratch.output.value.{name:s}".format(name=scratchName), self.outputValue )        
        
    def inputValue(self, message):
        message['command'] = 'scratch_input_value'
        if not self.terminated:
            try:
                self.send ( json.dumps(message), False )
                if debug:
                    print "inputValue " + json.dumps(message)
            except Exception:
                pass
        
    def inputCommand(self, message):
        message['command'] = 'scratch_input_command'
        if not self.terminated:
            try:
                self.send ( json.dumps(message), False )
                if debug:
                    print "inputCommand " + json.dumps(message)
            except Exception:
                pass
                
    def outputValue(self, message):
        message['command'] = 'scratch_output_value'
        if not self.terminated:
            try:
                self.send ( json.dumps(message), False )
                if debug:
                    print "outputValue " + json.dumps(message)
            except Exception:
                pass
            
    def outputCommand(self, message):
        message['command'] = 'scratch_output_command'
        if not self.terminated:
            try:
                self.send ( json.dumps(message), False )
                if debug:
                    print "outputCommand " + json.dumps(message)
            except Exception:
                pass
                
                     
class AnimationHandler(object):
    def ws(self):
        # you can access the class instance through
        handler = cherrypy.request.ws_handler
        
        

class ValueHandler:
    def __init__(self):
        # eventHandler.register('ValueHandler', 'ValueHandler', self)
        pass
    
    def input(self, adapter, command, value):
        logger.debug("input  value called %s, %s, %s", adapter, command, value)
        # eventHandler.resolveValue(self, adapter, command, value, qualifier='input')
        return "no_return"

        return ""

    def output(self, adapter, command, value):
        logger.debug("output value called %s, %s, %s", adapter, command, value)
        # eventHandler.resolveValue(self, adapter, command, value, qualifier='output')
        return ""

parentApplication = None
remote = False


class DefaultWebsocketHandler(object):
    def ws(self):
        # you can access the class instance through
        _ = cherrypy.request.ws_handler



class ServerThread(threading.Thread):
    
    server = None
    remote = None
    config = None
    
    _stop = None
    _running = None
    
    def __init__(self, parent = None, remote = False, config = None):
        # print("next: ServerThread, init")
        threading.Thread.__init__(self, name="GUIServerThread")
        
        self._stop = threading.Event()
        self._running = threading.Event()
        
        self.config = config
        self.guiRemote = remote
        global parentApplication
        parentApplication = parent
        # print("next: ServerThread, init finished")
        
        #
        # make dispatcher and config available on class level. This allows 'plugin' mechanism
        #
        self.dispatcher = cherrypy.dispatch.RoutesDispatcher()
        self.conf = {'/': {'request.dispatch': self.dispatcher }
                    }
        self.main = ScratchClientMain()
        
    def websocketPlugin(self, name, route, pluginWebSocket):
        """enable insertion of webSocket-connection"""
        logger.debug("configure websocket plugin for " + name)
        self.dispatcher.connect(name=name   , route=route, controller=DefaultWebsocketHandler()  , action='ws')    
        self.conf[route] =   {
                'tools.websocket.on': True,
                'tools.websocket.handler_cls': pluginWebSocket
            }  
    
    def htmlPlugin (self, name, htmlpath, comment=''):
        """enable insertion of html-connection""" 
        self.main.addPath ( name, htmlpath, comment )
       
    def start(self):
        """Start adapter Thread"""
        # import pdb; pdb.set_trace() 

        self._stop.clear()
        self._running.clear()
        
        threading.Thread.start(self)
        
        if True:
            self._running.wait(60)
        else:
            time.sleep(20);
        # print("next: start finished")

    def stop(self):
        logger.debug("ServerThread, stop cherryPy")
        self._stop.set()
        #eventDistributor.stop()
        cherrypy.engine.exit()

    def stopped(self):
        return self._stop.isSet()

    def run(self):
        if debug:
            print("next: ServerThread run()")
          
        logger.debug("ServerThread thread started")
        
        if True:
            self.dispatcher.mapper.explicit = False
            
            
            if logger.isEnabledFor(logging.DEBUG):
                pass

            if logger.isEnabledFor(logging.INFO):
                cherrypy.config.update({'log.screen': False,
                            'log.access_file': '',
                            'log.error_file': ''})
            
            
            cherrypy.engine.autoreload_on = False
            
            cherrypy.engine.autoreload.unsubscribe()
            cherrypy.engine.timeout_monitor.unsubscribe()
            #cherrypy.engine.autoreload.frequency = 300
            # cherrypy.config.update({'engine.autoreload.on': False})
            cherrypy.engine.signals.subscribe()
            
            #cherrypy.engine.timeout_monitor.unsubscribe()
            
            
            config = ConfigHandler(self.config)
            command = CommandHandler()
            adapters = AdaptersHandler()

            value = ValueHandler()
            fileHandler = FileProvider()
            
            
            self.dispatcher.connect(name='main'      , route='/'                 , controller=self.main    , action='get')
            self.dispatcher.connect(name='config'    , route='/config'           , controller=config  , action='get')
            
            self.dispatcher.connect(name='command'   , route='/command/input'    , controller=command , action='postInput')
            self.dispatcher.connect(name='command'   , route='/command/output'   , controller=command , action='postOutput')

            self.dispatcher.connect(name='value_in'  , route='/value/input'      , controller=value   , action='input')
            self.dispatcher.connect(name='value_out' , route='/value/output'     , controller=value   , action='output')

            self.dispatcher.connect(name='adapters'  , route='/adapters'         , controller=adapters, action='get')
            # serverEvent-Requests
            #dispatcher.connect(name='events'    , route='/events'           , controller=event   , action='event')

            self.dispatcher.connect(name='files'     , route='/files/{path:.*?}' , controller=fileHandler, action='file')
            
   
            #
            # enable websocket, if the libs are available.
            #
            
            WebSocketPlugin(cherrypy.engine).subscribe()
            cherrypy.tools.websocket = WebSocketTool()
            
            animationHandler = AnimationHandler()
            
            self.dispatcher.connect(name='adapterAnimation'   , route='/ws'    , controller=animationHandler  , action='ws')
            # self.dispatcher.connect(name='adapterAnimation'   , route='/pendel', controller=pendelHandler  , action='ws')
                 
       
            self.conf['/ws'] =   {
                'tools.websocket.on': True,
                'tools.websocket.handler_cls': AdapterAnimationWebSocket
            }      
#             self.conf['/pendel'] =   {
#                 'tools.websocket.on': True,
#                 'tools.websocket.handler_cls': PendelWebSocket
#             }      
           
            if self.guiRemote:
                cherrypy.config.update( { 'server.socket_port':  8080,
                                          'server.socket_host': '0.0.0.0' 
                                        } )
            else:
                cherrypy.config.update( { 'server.socket_port':  8080,
                                          'server.socket_host': '127.0.0.1'
                                        } )
        
            cherrypy.tree.mount(root=None, config=self.conf)
            
            cherrypy.engine.start()
            self._running.set()
            
            cherrypy.engine.block()
        
        if logger.isEnabledFor(logging.DEBUG):    
            logger.debug("ServerThread thread stopped")
        else:
            print("ServerThread thread stopped")
    
    def cherrypy_exithandler(self):
        logger.error("cherrypy exit")
            
    def registerCommandResolver(self, _commandResolver):
        global commandResolver
        commandResolver = _commandResolver

