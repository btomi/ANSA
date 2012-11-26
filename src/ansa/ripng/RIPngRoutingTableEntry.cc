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

#include "RIPngRoutingTableEntry.h"

#include "omnetpp.h"

namespace RIPng
{

RoutingTableEntry::RoutingTableEntry(IPv6Address destPrefix, int length) :
    IPv6Route(destPrefix, length, IPv6Route::ROUTING_PROT),
    changeFlag(false),
    routeTag(0)
{
    setTimer(NULL);
    setGCTimer(NULL);
}

RoutingTableEntry::RoutingTableEntry(const RoutingTableEntry& entry) :
    IPv6Route(entry.getDestPrefix(), entry.getPrefixLength(), IPv6Route::ROUTING_PROT),
    changeFlag(false),
    routeTag(0)
{
    setTimer(NULL);
    setGCTimer(NULL);
    setNextHop(entry.getNextHop());
    setInterfaceId(entry.getInterfaceId());
    setMetric(entry.getMetric());
}

RoutingTableEntry::~RoutingTableEntry()
{
}

} /* namespace RIPng */
