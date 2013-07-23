// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, see <http://www.gnu.org/licenses/>.
//

/**
 * @file ANSARoutingTable6.h
 * @date 21.5.2013
 * @author Jiri Trhlik (mailto:jiritm@gmail.com), Vladimir Vesely (mailto:ivesely@fit.vutbr.cz)
 * @brief Extended RoutingTable6
 * @details Adds administrative distance, fixes routing table cache, IPv4-like routes updates
 */

#ifndef __INET_ANSA_ANSAROUTINGTABLE6_H
#define __INET_ANSA_ANSAROUTINGTABLE6_H

#include "RoutingTable6.h"

class IInterfaceTable;
class InterfaceEntry;
class ANSARoutingTable6;

/**
 * Changed info() format of base IPvRoute.
 * Also added method to change the route without sending notifications.
 * @see IPv6Route
 */
class ANSAIPv6Route : public IPv6Route
{
  protected:
    IInterfaceTable *ift;     ///< cached pointer

    void changedSilent(int fieldCode);

  public:
    ANSAIPv6Route(IPv6Address destPrefix, int length, RouteSrc src);

    ANSARoutingTable6 *getANSARoutingTable6();

    virtual std::string info() const;
    virtual std::string detailedInfo() const;
    virtual const char *getRouteSrcName() const;

    const char *getInterfaceName() const;

    /**
     * Silent versions of the setters. Used if more than one route information is changed.
     * Silent versions do not fire "route changed notification".
     */
    virtual void setAdminDistSilent(RouteAdminDist adminDist)  { if (adminDist != _adminDist) { _adminDist = adminDist; changedSilent(F_ADMINDIST);} }
    virtual void setRoutingProtocolSourceSilent(RoutingProtocolSource routingProtocolSource) {  if (routingProtocolSource != _routingProtocolSource) { _routingProtocolSource = routingProtocolSource; changedSilent(F_ROUTINGPROTSOURCE);} }
    virtual void setInterfaceIdSilent(int interfaceId)  { if (interfaceId != _interfaceID) { _interfaceID = interfaceId; changedSilent(F_IFACE);} }
    virtual void setNextHopSilent(const IPv6Address& nextHop)  {if (nextHop != _nextHop) { _nextHop = nextHop; changedSilent(F_NEXTHOP);} }
    virtual void setMetricSilent(int metric)  { if (metric != _metric) { _metric = metric; changedSilent(F_METRIC);} }
};

/**
 * Extends RoutingTable6 by administrative distance.
 * TODO: watch this class and changes in INET! - especially RoutingTable6 class
 * @see RoutingTable6
 */
class ANSARoutingTable6 : public RoutingTable6
{
  public:
    ANSARoutingTable6();
    virtual ~ANSARoutingTable6();

    virtual void addOrUpdateOnLinkPrefix(const IPv6Address& destPrefix, int prefixLength,
                                 int interfaceId, simtime_t expiryTime);

    virtual void addOrUpdateOwnAdvPrefix(const IPv6Address& destPrefix, int prefixLength,
                                 int interfaceId, simtime_t expiryTime);

    virtual void addStaticRoute(const IPv6Address& destPrefix, int prefixLength,
                        unsigned int interfaceId, const IPv6Address& nextHop,
                        int metric = 0);

    virtual void addDefaultRoute(const IPv6Address& raSrcAddr, unsigned int ifID,
        simtime_t routerLifetime);

    virtual void addRoutingProtocolRoute(ANSAIPv6Route *route);

    /**
     * Same as routeChanged, except route changed notification is not fired.
     * @see routeChanged
     */
    virtual void routeChangedSilent(ANSAIPv6Route *entry, int fieldCode);

    /**
     * Must be reimplemented because of cache handling.
     */
    virtual void receiveChangeNotification(int category, const cObject *details);
};

#endif

