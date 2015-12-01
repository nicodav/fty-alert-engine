/*  =========================================================================
    alert-agent - ALERT-AGENT wrapper

    Copyright (C) 2014 - 2015 Eaton                                        
                                                                           
    This program is free software; you can redistribute it and/or modify   
    it under the terms of the GNU General Public License as published by   
    the Free Software Foundation; either version 2 of the License, or      
    (at your option) any later version.                                    
                                                                           
    This program is distributed in the hope that it will be useful,        
    but WITHOUT ANY WARRANTY; without even the implied warranty of         
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          
    GNU General Public License for more details.                           
                                                                           
    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.            

################################################################################
#  THIS FILE IS 100% GENERATED BY ZPROJECT; DO NOT EDIT EXCEPT EXPERIMENTALLY  #
#  Please refer to the README for information about making permanent changes.  #
################################################################################
    =========================================================================
*/

#ifndef ALERT_AGENT_LIBRARY_H_INCLUDED
#define ALERT_AGENT_LIBRARY_H_INCLUDED

//  Set up environment for the application

//  External dependencies
#include <malamute.h>
#include <biosproto.h>
#include <lua.h>
#include <cxxtools.h>

//  ALERT-AGENT version macros for compile-time API detection

#define ALERT_AGENT_VERSION_MAJOR 0
#define ALERT_AGENT_VERSION_MINOR 1
#define ALERT_AGENT_VERSION_PATCH 0

#define ALERT_AGENT_MAKE_VERSION(major, minor, patch) \
    ((major) * 10000 + (minor) * 100 + (patch))
#define ALERT_AGENT_VERSION \
    ALERT_AGENT_MAKE_VERSION(ALERT_AGENT_VERSION_MAJOR, ALERT_AGENT_VERSION_MINOR, ALERT_AGENT_VERSION_PATCH)

#if defined (__WINDOWS__)
#   if defined LIBALERT_AGENT_STATIC
#       define ALERT_AGENT_EXPORT
#   elif defined LIBALERT_AGENT_EXPORTS
#       define ALERT_AGENT_EXPORT __declspec(dllexport)
#   else
#       define ALERT_AGENT_EXPORT __declspec(dllimport)
#   endif
#else
#   define ALERT_AGENT_EXPORT
#endif

//  Opaque class structures to allow forward references


//  Public API classes

#endif
/*
################################################################################
#  THIS FILE IS 100% GENERATED BY ZPROJECT; DO NOT EDIT EXCEPT EXPERIMENTALLY  #
#  Please refer to the README for information about making permanent changes.  #
################################################################################
*/
