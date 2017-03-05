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

errors = []
warnings = []

def append( err):
    errors.append(err)
    
def appendWarning( err):
    warnings.append(err)

def hasErrors():
    return len(errors)

def hasWarnings():
    return len(warnings)

