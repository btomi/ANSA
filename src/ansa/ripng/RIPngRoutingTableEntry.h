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

#ifndef ROUTINGTABLEENTRY_H_
#define ROUTINGTABLEENTRY_H_

#include "RoutingTable6.h"

#include "RIPngTimer_m.h"

namespace RIPng
{

/**
 *  Just extends IPv6 route for RIPng.
 */
class RoutingTableEntry : public IPv6Route
{
  public:
    RoutingTableEntry(IPv6Address destPrefix, int length);
    RoutingTableEntry(const RoutingTableEntry& entry);
    virtual ~RoutingTableEntry();

  protected:
    RIPngTimer *timeout;          ///< Pointer for the Route timeout timer
    RIPngTimer *GCTimeout;        ///< Pointer for the Route Garbage-Collection Timer
    bool changeFlag;              ///< The Route Changed Flag
    unsigned short int routeTag;  ///< The Route routeTag

  public:
    bool isChangeFlagSet() { return changeFlag; }
    void setChangeFlag() { changeFlag = true; }
    void clearChangeFlag() { changeFlag = false; }

    RIPngTimer  *getTimer() { return timeout; }
    RIPngTimer  *getGCTimer() { return GCTimeout; }
    unsigned short int getRouteTag() { return routeTag; }

    void              setTimer(RIPngTimer *t) { timeout = t; }
    void              setGCTimer(RIPngTimer *t) { GCTimeout = t; }
    void              setRouteTag(unsigned short int tag) { routeTag = tag;}
};

} /* namespace RIPng */

#endif /* ROUTINGTABLEENTRY_H_ */
