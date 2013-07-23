// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this program; if not, see <http://www.gnu.org/licenses/>.
//

/**
 * @file ANSARoutingTable6.cc
 * @date 21.5.2013
 * @author Jiri Trhlik (mailto:jiritm@gmail.com), Vladimir Vesely (mailto:ivesely@fit.vutbr.cz)
 * @brief Extended RoutingTable6
 * @details Adds administrative distance, fixes routing table cache, IPv4-like routes updates
 */

#include <algorithm>

#include "opp_utils.h"

#include "ANSARoutingTable6.h"

#include "IPv6InterfaceData.h"
#include "InterfaceTableAccess.h"

#include "IPv6TunnelingAccess.h"

Define_Module(ANSARoutingTable6);

ANSAIPv6Route::ANSAIPv6Route(IPv6Address destPrefix, int length, RouteSrc src)
    : IPv6Route(destPrefix, length, src)
{
    ift = InterfaceTableAccess().get();
}

std::string ANSAIPv6Route::info() const
{
    std::stringstream out;

    out << getRouteSrcName();
    out << " ";
    if (getDestPrefix().isUnspecified())
        out << "::";
    else
        out << getDestPrefix();
    out << "/" << getPrefixLength();
    out << " [" << getAdminDist() << "/" << getMetric() << "]";
    out << " via ";
    if (getNextHop() == IPv6Address::UNSPECIFIED_ADDRESS)
        out << "::";
    else
        out << getNextHop();
    out << ", " << getInterfaceName();
    if (getExpiryTime()>0)
        out << " exp:" << getExpiryTime();

    return out.str();
}

std::string ANSAIPv6Route::detailedInfo() const
{
    return std::string();
}

const char *ANSAIPv6Route::getRouteSrcName() const
{
    switch (getSrc())
    {
        case FROM_RA:
            return "ND";  //Neighbor Discovery

        case OWN_ADV_PREFIX:
            return "C";

        case STATIC:
            if (getNextHop() == IPv6Address::UNSPECIFIED_ADDRESS)
                return "C";
            return "S";

        case ROUTING_PROT:
            switch(getRoutingProtocolSource())
            {
                case pRIP: return "R";
                case pBGP: return "B";
                case pISIS1: return "I1";
                case pISIS2: return "I2";
                case pISISinterarea: return "IA";
                case pISISsum: return "IS";
                case pOSPFintra: return "O";
                case pOSPFinter: return "OI";
                case pOSPFext1: return "OE1";
                case pOSPFext2: return "OE2";
                case pOSPFNSSAext1: return "ON1";
                case pOSPFNSSAext2: return "ON2";
                case pEIGRP: return "D";
                case pEIGRPext: return "EX";
                default: return "?";
            }

        default:
            return "?";
    }
}

const char *ANSAIPv6Route::getInterfaceName() const
{
    ASSERT(ift);
    InterfaceEntry *interface = ift->getInterfaceById(_interfaceID);
    return interface ? interface->getName() : "";
}

void ANSAIPv6Route::changedSilent(int fieldCode)
{
    ANSARoutingTable6* rt = dynamic_cast<ANSARoutingTable6*>(_rt);
    if (rt)
        rt->routeChangedSilent(this, fieldCode);
}

ANSARoutingTable6::ANSARoutingTable6()
{
}

ANSARoutingTable6::~ANSARoutingTable6()
{
}

IPv6Route *ANSARoutingTable6::createNewRoute(IPv6Address destPrefix, int prefixLength, IPv6Route::RouteSrc src)
{
    return new ANSAIPv6Route(destPrefix, prefixLength, src);
}

void ANSARoutingTable6::routeChangedSilent(ANSAIPv6Route *entry, int fieldCode)
{
    ASSERT(entry != NULL);

    /*if (fieldCode==ANSAIPv6Route::F_NEXTHOP || fieldCode==ANSAIPv6Route::F_METRIC || fieldCode==ANSAIPv6Route::F_IFACE
            || fieldCode==ANSAIPv6Route::F_ADMINDIST || fieldCode==ANSAIPv6Route::F_ROUTINGPROTSOURCE
            )*/

    /*XXX: this deletes some cache entries we want to keep, but the node MUST update
     the Destination Cache in such a way that all entries will use the latest
     route information.*/
    if (fieldCode==ANSAIPv6Route::F_NEXTHOP || fieldCode==ANSAIPv6Route::F_IFACE)
        purgeDestCache();

    updateDisplayString();
}
