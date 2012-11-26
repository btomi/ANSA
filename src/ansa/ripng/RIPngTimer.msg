//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

cplusplus {{
#include "IPv6Address.h"
}}

class noncobject IPv6Address;

enum RIPngTimerType
{
    GENERAL_UPDATE = 0;
    TRIGGERED_UPDATE = 1;
    ROUTE_TIMEOUT = 2;
    ROUTE_GARBAGE_COLECTION_TIMEOUT = 3;
}

//
// RIPng Timers
// For General Update Message,
// Route Timeout and
// Garbage-Collection Time.
//
packet RIPngTimer 
{
    char timerKind enum(RIPngTimerType) = GENERAL_UPDATE;
    IPv6Address IPv6Prefix;                                //meaning in the case of ROUTE_TIMEOUT and ROUTE_GARBAGE_COLECTION_TIMEOUT
}
