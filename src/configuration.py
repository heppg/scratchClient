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
#
#
import xml.etree.ElementTree as ET
import inspect
import sys

import errorManager
import publishSubscribe

debug = False

    
class GPIORegistry:
    """Dynamic mapping from port names to GPIO-Numbers"""
    gpios = {}

    def __init__(self, portmappingFilename ):
        if True:
            try:
                tree = ET.parse(portmappingFilename)
                
            except ET.ParseError as e:
                errorManager.append("error reading gpio config file '{name:s}'".format(name=portmappingFilename)+'\n' + 
                                                 '{error:s}'.format(error=str(e)) + '\n' +
                                                 'Correct xml file and restart.')
                return
            except IOError as e:
                errorManager.append("error reading config file '{name:s}', '{error:s}'".format(name=self.portmappingFilename, error=str(e)))
                return
            root = tree.getroot()
            #
            # config files may evolve over time. Check the version tag for this.
            #
            version=None
            if 'version' in root.attrib:
                version = root.attrib['version']
    
            if version == '1.0':
                pass
            else:
                print("configuration, unknown 'version' attribute {version:s}.".format(version=version))
                sys.exit(10)
                
            self.gpios = {}
            
            for child in root:
                if debug:
                    print ('configure search modules', child.tag, child.attrib)
                
                if 'port' == child.tag:
                    portNumber = child.attrib['gpioNumber']
                    name = child.attrib['name']
                    self.gpios[name] = int( portNumber )
            
    def getPort(self, portName):
        if portName in self.gpios:
            return self.gpios[portName]
        errorManager.append("gpio name in config file not known: %s", portName)
        return None
        
allEverGpios = None

class AdapterSetting:
    name = None
    className = None

    def __init__(self, name, classname):
        self.name = name
        self.classname = classname

class InputSetting:
    name = None
    scratchNames = None
    def __init__(self, name):
        self.name = name
        self.scratchNames= []

class OutputSetting:
    name = None
    scratchNames = None
    
    def __init__(self, name):
        self.name = name
        self.scratchNames= []

class GpioSetting:
    """gpio configuration settings, data only. Direction, pullup, initial state (low,high)"""
    
    pull = None
    dir = None
    default = None
    
    def __init__(self):
        pass
    
    def __str__(self):
        return "GpioSetting[dir={dir:s}, pull={pull:s}, default={default:s}]".format(dir=self.dir, pull=self.pull, default=self.default)

class GpioConfiguration:
    """gpio configuration, data only"""
    
    port = None   
    portNumber = None 
    #
    # Alias-Namen, optional
    #    
    alias = None 
    default_setting = None
    active_setting = None
    
    def __init__( self ):
        self.port = None
    
    def __str__(self):
        return "GpioConfiguration[port={port:s}, portNumber={portNumber:d}]".format(port=self.port, portNumber=self.portNumber)
    
    def getPort(self):
        return self.portNumber    

class GPIOManager:
    """wrapper class for a GPIOManager"""
    
    delegateGPIOManager = None
    
    def __init__(self, lib='RPi.GPIO'):
        importCommand = "import gpio.{p:s}".format(p=lib.replace('.', '_'))
        className = 'gpio.' + lib.replace('.', '_')+ '.GPIOManager()'

        classNameCommand = className 
        #print("importCommand ", importCommand)
        #print("classNameCommand ", classNameCommand)
        try:
            exec( importCommand)
            self.delegateGPIOManager = eval(classNameCommand)
        except ImportError as e:
            print("problems instantiating GPIO-Manager Delegation to ", lib, e)
            sys.exit(4)
          
    def getSimulation(self):
        self.delegateGPIOManager.getSimulation()
        
    def setActive(self, state):
        self.delegateGPIOManager.setActive(state)

    def setGPIOActive(self, gpioConfiguration, state):
        #print("GPIOManager, setGPIOActive")
        self.delegateGPIOManager.setGPIOActive(gpioConfiguration, state)
        
    def low(self, gpio):
        self.delegateGPIOManager.low(gpio)    

    def high(self, gpio):
        self.delegateGPIOManager.high(gpio)
    
    def direction_in(self, gpio):
        self.delegateGPIOManager.direction_in(gpio)
        
    def direction_out(self, gpio):
        self.delegateGPIOManager.direction_out(gpio)
    
    def get(self, gpio):
        return self.delegateGPIOManager.get(gpio)

    def startPWM(self, gpio, frequency, value):
        self.delegateGPIOManager.startPWM(gpio, frequency, value)
                
    def setPWMDutyCycle(self, gpio, value):
        self.delegateGPIOManager.setPWMDutyCycle(gpio, value)

    def resetPWM(self, gpio):
        self.delegateGPIOManager.resetPWM(gpio)


class AdapterMethods:
    """helper class to investigate on adapter methods"""
    
    moduleMethods = None
    
    def __init__(self, module):
        self.moduleMethods = inspect.getmembers(module, inspect.ismethod )
    
    def hasMethod(self, name):
        for x in self.moduleMethods:
            if x[0] == name:
                return True
        return False
    

class ConfigManager:
    filename = None
    tree = None
            
    configDelegate = None
    
    def __init__(self, filename):
        self.filename = filename
        pass
    
    def configure(self):

        try:
            self.tree = ET.parse(self.filename)
        except ET.ParseError as e:
            errorManager.append("error reading config file '{name:s}'".format(name=self.filename)+'\n' + 
                                             '{error:s}'.format(error=str(e)) + '\n' +
                                             'Correct xml file and restart.')
            return
        except IOError as e:
            errorManager.append("error reading config file '{name:s}', '{error:s}'".format(name=self.filename, error=str(e)))
            return
        root = self.tree.getroot()
        #
        # config files may evolve over time. Check the version tag for this.
        #
        version=None
        if 'version' in root.attrib:
            version = root.attrib['version']
        else:
            print("configuration, no 'version' attribute available.")
            print("assume version=1.0.")
            version='1.0'

        if version == '1.0':
            self.configDelegate=ConfigManager_1_0(self.tree)
        else:
            print("configuration, unknown 'version' attribute {version:s}.".format(version=version))
            sys.exit(10)
            
        self.configDelegate.configure()

    # def setCommandResolver (self, commandResolver):
    #     self.configDelegate.setCommandResolver ( commandResolver)
        
    # def configureGuiServer (self, guiServer):
    #     self.configDelegate.configureGuiServer(guiServer)
                           
                                                       
    def configureCommandResolver (self, scratchSender):
        if self.configDelegate == None:
            return
        self.configDelegate.configureCommandResolver(scratchSender)

    def getAdapters(self):
        if self.configDelegate == None:
            return []
        return self.configDelegate.getAdapters()
    
    def getDescription(self):
        return self.configDelegate.description
    
    def check(self):
        """check for config errors"""
        if self.configDelegate == None:
            return
        self.configDelegate.check()
    
    def registerRegisterGuiOnCommandResolver(self, gui, scratchSender):
        self.configDelegate.registerGuiOnCommandResolver(gui, scratchSender)

class ConfigManager_1_0:
    
    version='1.0'
    tree = None
    
    gpios = []
    adapters = []
        
    # commandResolver = None
    
    def __init__(self, tree):
        self.tree = tree
        pass
    
    # def setCommandResolver (self, commandResolver):
    #     self.commandResolver = commandResolver
    
    def _check_gpio_unique(self):
        # collect all adapter's gpio and collct in on elist.
        # check fo runiqueness
        pass

    def _check_gpio_alias(self):
        """ check mandatory gpio-alias"""
        for adapter in self.adapters:
            if len(adapter.mandatoryAlias) > 0:
                ma = []
                for g in adapter.gpios:
                    if g.alias != None:
                        if g.alias in ma:
                            errorManager.append("adapter: {a:s} Alias not unique: {p:s}".format(a=adapter.name, p=g.alias))
                        else:
                            ma.append(g.alias)
                for m in adapter.mandatoryAlias:
                    if m in ma:
                        pass
                    else:
                        errorManager.append("adapter: {a:s} Alias missing: {p:s}".format(a=adapter.name, p=m))
                                        
    def _check_module_parameters(self):
        if debug:
            print("_check_module_parameters")
        for adapter in self.adapters:
            for p in adapter.mandatoryParameters:
                if debug:
                    print("check parameter ", p)
                if p not in  adapter.parameters:
                    errorManager.append("adapter: {a:s} mandatory parameter '{name:s}' missing in configuration.".format(a=adapter.name, name=p))

    def _check_module_input_output(self):
        if debug:
            print("_check_module_input_output")
        for adapter in self.adapters:
            for p in adapter.inputs:
                if debug:
                    print("check input ", p.name)
                for o in adapter.input_values:
                    if p.name == o.name:
                        errorManager.append("adapter: {a:s} input name '{name:s}' must not be used as input value name.".format(a=adapter.name, name=p.name))
                for o in adapter.outputs:
                    if p.name == o.name:
                        errorManager.append("adapter: {a:s} input name '{name:s}' must not be used as output name.".format(a=adapter.name, name=p.name))
                for o in adapter.output_values:
                    if p.name == o.name:
                        errorManager.append("adapter: {a:s} input name '{name:s}' must not be used as output value name.".format(a=adapter.name, name=p.name))
            for p in adapter.input_values:
                if debug:
                    print("check input_values ", p.name)
                for o in adapter.outputs:
                    if p.name == o.name:
                        errorManager.append("adapter: {a:s} input value name '{name:s}' must not be used as output name.".format(a=adapter.name, name=p.name))
                for o in adapter.output_values:
                    if p.name == o.name:
                        errorManager.append("adapter: {a:s} input value name '{name:s}' must not be used as output value name.".format(a=adapter.name, name=p.name))
            for p in adapter.outputs:
                if debug:
                    print("check outputs ", p.name)
                for o in adapter.output_values:
                    if p.name == o.name:
                        errorManager.append("adapter: {a:s} input value name '{name:s}' must not be used as output value name.".format(a=adapter.name, name=p.name))
    
    
    def _check_module_name(self):
        if debug:
            print("_check_module_name")
        names = []    
        for adapter in self.adapters:
            if adapter.name in names:
                errorManager.append("name used multiple times, use of unique names needed "+ adapter.name)
            else:    
                names.append(adapter.name)
        
    def check(self):
        """Validate configuration for errors from a system perspective:
        - gpio-ports are unique
        - modules names are unique
    """
        if debug:
            print("check")
        self._check_gpio_unique()
        self._check_gpio_alias()
        ## errorList += self._check_module_classes()
        self._check_module_input_output()
        self._check_module_parameters()
        self._check_module_name()
        
    def configureCommandResolver (self, scratchSender):
        for adapter in self.adapters:
            for _outputs in adapter.outputs:
                for command in _outputs.scratchNames:
                    publishSubscribe.Pub.subscribe("scratch.output.command.{name:s}".format(name=command), scratchSender.sendCommand)
                    
            for _outputs in adapter.output_values:
                for command in _outputs.scratchNames:
                    publishSubscribe.Pub.subscribe("scratch.output.value.{name:s}".format(name=command), scratchSender.sendValue)

    # def configureGuiServer (self, guiServer):
    #     for adapter in self.adapters:
    #         adapter.guiServer = guiServer
            
            
    def registerGuiOnCommandResolver(self, gui, scratchSender):
#         for adapter in self.adapters:
#             for _outputs in adapter.outputs:
#                 for command in _outputs.scratchNames:
#                     
#                     publishSubscribe.Pub.subscribe("scratch.output.command.{name:s}".format(name=command), scratchSender.sendCommand)
#                     
#             for _outputs in adapter.output_values:
#                 for command in _outputs.scratchNames:
#                     publishSubscribe.Pub.subscribe("scratch.output.value.{name:s}".format(name=command), scratchSender.sendCommand)
        pass            
            
    def configure (self):
        root = self.tree.getroot()
        nAdapter = 0
        loggingContext = ''
        
        for child in root:
            if debug:
                print ('configure search modules', child.tag, child.attrib)
            found = False
            
            if 'description' == child.tag:
                self.description = child.text
                found = True
                
            if 'adapter' == child.tag:
                className = child.attrib['class']
                
                if 'name' in  child.attrib:
                    name = child.attrib['name']
                    loggingContext = "adapter '{a:s}'".format(a=name)
                else:
                    loggingContext = "adapter '[{a:s}]'".format(a=str(nAdapter))
                        
                sections = className.split('.')
                _path= '.'.join( sections[0: len(sections)-1] ) 
                _class = sections[len(sections)-1]
                
                if debug:
                    print ('path  = ',_path)
                    print ('class = ', _class)
                
                
                try:
                    iStatement = "import {p:s}".format(p=_path, c=_class)
                    if debug:
                        print ('import statement', iStatement)
                    
                    # exec( iStatement , globals(), locals() )
                    exec( iStatement )
                    #print ('import statement', "success")
                        
                except ImportError as e:
                    print ('----------', e)
                    errorManager.append("{lc:s}: no adapter class '{cn:s}' (ImportError)".format(lc=loggingContext, cn= className))
                    continue
                
                lAdapter = None
                try:
                    cStatement = "{p:s}.{c:s}()".format(p=_path, c=_class)
                    # print 'class statement'
                    if debug:
                        print ('cStatement ', cStatement)
                    
                    lAdapter = eval(cStatement, globals(), locals())
                    #print ('cStatement', "success", lAdapter) 
                    
                except AttributeError as e:
                    print ('----------', e)
                    errorManager.append("{lc:s}: no adapter class '{cn:s}' (AttributeError)".format(lc=loggingContext, cn= className))
                    continue
                # could be more elegant:  lAdapter.__class__
                # print( str( lAdapter.__class__ ))
                lAdapter.className= className
                    
                if debug:
                    print('configure adapter')
                
                self.adapterConfig(lAdapter, loggingContext, child)
                
                self.adapters.append( lAdapter)
                if debug:
                    print("adapter complete")
                        
                nAdapter += 1
                
                found = True

            if not(found):
                errorManager.append("{lc:s}: unknown tag below root '{cn:s}'".format(lc=loggingContext, cn= child.tag ))
                
    def getAdapters(self):
        return self.adapters
    
    
    def settingConfig(self, loggingContext, xmlSetting):
        
        setting  = GpioSetting()
        setting.dir = None
        if  'dir' in xmlSetting.attrib: 
            setting.dir = xmlSetting.attrib['dir'] 
        else:
            errorManager.append("{lc:s}, no dir-attribute.".format(lc=loggingContext))
            return
        
        if  'pull' in xmlSetting.attrib:
            setting.pull = xmlSetting.attrib['pull'] 
            
        if  'default' in xmlSetting.attrib:
            setting.default = xmlSetting.attrib['default'] 

        return setting
    
    def gpioConfig(self, loggingContext, tle):
        gpio = GpioConfiguration()
        gpio.port = tle.attrib['port']
        
        portName = gpio.port
        gpio.portNumber = allEverGpios.getPort(gpio.port)

        if  'alias' in tle.attrib:
            gpio.alias = tle.attrib['alias']
                            
        s = tle.find('default')
        if s != None:
            loggingContext2 = "{lc:s} gpio port '{p:s}', 'default'".format(lc=loggingContext, p=portName)
            gpio.default_setting = self.settingConfig(loggingContext2, s)
        else:
            loggingContext2 = "{lc:s} gpio port '{p:s}' ".format(lc=loggingContext, p=portName)
            errorManager.append("{lc:s}: gpio has no default setting.".format(lc=loggingContext2))    
        
        s = tle.find('active')
        if s != None:
            loggingContext2 = "{lc:s} gpio port '{p:s}', 'active'".format(lc=loggingContext, p=portName)
            gpio.active_setting = self.settingConfig( loggingContext2, s )
        else:
            loggingContext2 = "{lc:s} gpio port '{p:s}'".format(lc=loggingContext, p=portName)
            errorManager.append("{lc:s}: gpio has no active setting.".format(lc=loggingContext2))    
            
        return gpio

    def adapterConfig(self, adapter, loggingContext, child):
        
        if debug:
            print("adapterConfig", child.tag)
        if 'name' in child.attrib:
            # set name and force registration
            adapter.setName( child.attrib['name'] )
        else:
            errorManager.append('no name given for adapter config, class = ', child.attrib['class'])
        
        #
        # give the adapter a chance to dynamically create methods
        # if needed when method 'setXMLConfig' is available
        #
        moduleMethods = AdapterMethods(adapter)
        
        adapter_input_values = []
        adapter_inputs = []
        adapter_output_values = []
        adapter_outputs = []
        
        if moduleMethods.hasMethod("setXMLConfig"):
            adapter.setXMLConfig(child)
            # there could be methods dynamically added, so rerun AdapterMethods
            moduleMethods = AdapterMethods(adapter)
        
        if debug:
            for x in  moduleMethods.moduleMethods:
                print(x)

                
        for tle in child:
            
            if 'description' == tle.tag:
                adapter.description = tle.text
                
            elif 'gpio' == tle.tag:
                
                gpio = self.gpioConfig( loggingContext, tle )
                adapter.gpios.append( gpio )
                
            elif 'input' == tle.tag:
                
                if not ( 'name' in tle.attrib ):
                    errorManager.append("{lc:s}: no name attribute for input method".format(lc=loggingContext))
                    continue
                methodName = tle.attrib['name']
                
                if moduleMethods.hasMethod(methodName):
                    inp = InputSetting(methodName)
                    
                    foundBroadcast = False
                    for comm in tle:
                        if comm.tag == 'broadcast':
                            name = comm.attrib['name']
                            inp.scratchNames.append(name)
                            foundBroadcast = True
                        else:
                            errorManager.append("{lc:s}: input tag needs 'broadcast', but found {n:s}".format(lc=loggingContext, n=comm.tag))
                    if not (foundBroadcast):
                        errorManager.append("{lc:s}: input tag needs 'broadcast', but not found".format(lc=loggingContext ))
                                
                    adapter_inputs.append(inp)
                                
                else:
                    errorManager.append("{lc:s}, method name '{n:s}' for input method missing".format(lc=loggingContext, n=methodName))
                    continue        

            elif 'output' == tle.tag:
                
                if not ( 'name' in tle.attrib ):
                    errorManager.append("{lc:s} no name attribute for output method".format(lc=loggingContext))
                    continue
                methodName = tle.attrib['name']
                
                if moduleMethods.hasMethod(methodName):
                    out = OutputSetting(methodName)
                    foundBroadcast = False
                    for comm in tle:
                        if comm.tag == 'broadcast':
                            name = comm.attrib['name']
                            out.scratchNames.append( name)
                            foundBroadcast = True
                    if not (foundBroadcast):
                        errorManager.append("{lc:s}: output tag needs 'broadcast', but not found".format(lc=loggingContext ))
                    adapter_outputs.append(out)
                else:
                    errorManager.append("{lc:s} method name '{n:s}' for broadcast not available (check adapter python code)".format(lc=loggingContext, n=methodName))
                    continue        
            
            elif 'input_value' == tle.tag:
                
                if not ( 'name' in tle.attrib ):
                    errorManager.append("{lc:s}: no name attribute for input_value method".format(lc=loggingContext))
                    continue

                methodName = tle.attrib['name']
                
                if moduleMethods.hasMethod(methodName):
                    value = InputSetting(methodName)
                    adapter_input_values.append(value)

                    for comm in tle:
                        if comm.tag == 'variable':
                            n = comm.attrib['name']
                            value.scratchNames.append( n)
                        else:
                            errorManager.append("{lc:s}: input_value with unexpected child '{c:s}', variable expected".format(c=comm.tag, lc=loggingContext))
                            
                else:
                    if debug:
                        print ("these are the well known adapter methods", moduleMethods.moduleMethods)
                        
                    errorManager .append("{lc:s}: unknown input_value name '{command:s}' (check adapter python code)".format(lc=loggingContext, command=methodName))     

            elif 'output_value' == tle.tag:
                
                if not ( 'name' in tle.attrib ):
                    errorManager.append("{lc:s}: no name attribute for output_value method".format(lc=loggingContext))
                    continue
                methodName = tle.attrib['name']
                if moduleMethods.hasMethod(methodName):
                    value = OutputSetting(methodName)
                    adapter_output_values.append(value)
                    for comm in tle:
                        if comm.tag == 'sensor':
                            n = comm.attrib['name']
                            value.scratchNames.append( n)
                        else:
                            errorManager.append("{lc:s}: output_value with unexpected child '{c:s}', sensor expected".format(c=comm.tag, lc=loggingContext))
                else:
                    errorManager .append("{lc:s}: unknown output_value name '{command:s}' (check adapter python code)".format(lc=loggingContext, command=methodName))     
            
            elif 'parameter' == tle.tag:
               
                if not ( 'name' in tle.attrib ):
                    errorManager.append("{lc:s}: no name attribute for parameter".format(lc=loggingContext))
                    continue
                if not ( 'value' in tle.attrib ):
                    errorManager.append("{lc:s}: no value  attribute for parameter".format(lc=loggingContext))
                    continue
                if debug: 
                    print("parameter", tle.attrib[ 'name'], tle.attrib['value'])
                
                name  = str(tle.attrib['name'] )
                value = str(tle.attrib['value'] )
                adapter.parameters[name ] = value
            else:
                # if adapter provides a 'setXmlConfig-Method, pass the current node to the adapter and let it grab 
                # whatever needed.
                if moduleMethods.hasMethod("setXMLConfig"):
                    pass
                else:
                    errorManager.append("{lc:s}: unknown tag '{tag:s}'".format(lc=loggingContext, tag=tle.tag))
                     

        adapter.addInputs(adapter_inputs)
        adapter.addInputValues(adapter_input_values)
        
        adapter.addOutputs(adapter_outputs)
        adapter.addOutputValues(adapter_output_values)
                