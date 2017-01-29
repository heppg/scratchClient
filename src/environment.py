# -*- coding: utf-8 -*-
# --------------------------------------------------------------------------------------------
# Implementation of scratch Remote Sensor Protocol Client
#
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
# some of the class instances need to be known in the overall system.
# e.g. while configuring adapters, the gui instance needs to be known
# this is accomplished by environment dict
#

modules = {}

def append( name, module ):
    modules[name] =  module
    
def has_key(name):
    return modules.has_key(name) 

def get(name):
    return modules[name]