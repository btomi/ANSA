// Copyright (C) 2013 Brno University of Technology (http://nes.fit.vutbr.cz/ansa)
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
/**
 * @file AnsaRoutingTable.cc
 * @date 25.1.2013
 * @author Tomas Prochazka (mailto:xproch21@stud.fit.vutbr.cz), Vladimir Vesely (mailto:ivesely@fit.vutbr.cz)
 * @brief Extended RoutingTable with new features for PIM
 */

#include <algorithm>

#include "AnsaRoutingTable.h"
#include "IPv4InterfaceData.h"
#include "AnsaInterfaceEntry.h"

Define_Module(AnsaRoutingTable);

/** Defined in RoutingTable.cc */
std::ostream& operator<<(std::ostream& os, const IPv4Route& e);

/** Printout of structure AnsaIPv4MulticastRoute (one route in table). */
std::ostream& operator<<(std::ostream& os, const AnsaIPv4MulticastRoute& e)
{
    os << e.info();
    return os;
};

// to create ANSAIPv4Routes in updateNetmaskRoutes()
IPv4Route *AnsaRoutingTable::createNewRoute()
{
    return new ANSAIPv4Route();
}

/**
 * UPDATE DISPLAY STRING
 *
 * Update string under multicast table icon - number of multicast routes.
 */
void AnsaRoutingTable::updateDisplayString()
{
    if (!ev.isGUI())
        return;

    char buf[80];
    sprintf(buf, "%d routes\n%d multicast routes", getNumRoutes(), getNumMulticastRoutes());

    getDisplayString().setTagArg("t", 0, buf);
}



/**
 * INITIALIZE
 *
 * The method initializes Multicast Routing Table module. It get access to all needed objects.
 *
 * @param stage Stage of initialization.
 */
void AnsaRoutingTable::initialize(int stage)
{
    if (stage==0)
    {
        RoutingTable::initialize(stage);
    }
    else if (stage==3)
    {
        // routerID selection must be after stage==2 when network autoconfiguration
        // assigns interface addresses
        configureRouterId();

        // We don't want to call this method:
        // 1. adds IPv4Route instead of ANSAIPv4Route (method is not overridden)
        // 2. directly connected routes are added in the deviceConfigurator
        //updateNetmaskRoutes();

        //printRoutingTable();
    }
}


/**
 * ADD ROUTE
 *
 * Function check new multicast table entry and then add new entry to multicast table.
 *
 * @param entry New entry about new multicast group.
 * @see MulticastIPRoute
 * @see updateDisplayString()
 */
void AnsaRoutingTable::addMulticastRoute(AnsaIPv4MulticastRoute *entry)
{
    Enter_Method("addMulticastRoute(...)");

    // check for null multicast group address
    if (entry->getMulticastGroup().isUnspecified())
        error("addMulticastRoute(): multicast group address cannot be NULL");

    // check that group address is multicast address
    if (!entry->getMulticastGroup().isMulticast())
        error("addMulticastRoute(): group address is not multicast address");

    // check for source or RP address
    if (entry->getOrigin().isUnspecified() && entry->getRP().isUnspecified())
        error("addMulticastRoute(): source or RP address has to be specified");

    // check that the incoming interface exists
    //FIXME for PIM-SM is needed unspecified next hop (0.0.0.0)
    //if (!entry->getInIntPtr() || entry->getInIntNextHop().isUnspecified())
        //error("addMulticastRoute(): incoming interface has to be specified");
    //if (!entry->getInIntPtr())
        //error("addMulticastRoute(): incoming interface has to be specified");

    // add to tables
    internalAddMulticastRoute(entry);

    updateDisplayString();
}



/**
 * DELETE ROUTE
 *
 * Function check new multicast table entry and then add new entry to multicast table.
 *
 * @param entry Multicast entry which should be deleted from multicast table.
 * @return False if entry was not found in table. True if entry was deleted.
 * @see MulticastIPRoute
 * @see updateDisplayString()
 */
bool AnsaRoutingTable::deleteMulticastRoute(AnsaIPv4MulticastRoute *entry)
{
    Enter_Method("deleteMulticastRoute(...)");

    // if entry was found, it can be deleted
    if (internalRemoveMulticastRoute(entry))
    {
        // first delete all timers assigned to route
        if (entry->getSrt() != NULL)
        {
            cancelEvent(entry->getSrt());
            delete entry->getSrt();
        }
        if (entry->getGrt() != NULL)
        {
            cancelEvent(entry->getGrt());
            delete entry->getGrt();
        }
        if (entry->getSat())
        {
            cancelEvent(entry->getSat());
            delete entry->getSat();
        }
        if (entry->getKat())
        {
            cancelEvent(entry->getKat());
            delete entry->getKat();
        }
        if (entry->getEt())
        {
            cancelEvent(entry->getEt());
            delete entry->getEt();
        }
        if (entry->getJt())
        {
            cancelEvent(entry->getJt());
            delete entry->getJt();
        }
        if (entry->getPpt())
        {
            cancelEvent(entry->getPpt());
            delete entry->getPpt();
        }
        // delete timers from outgoing interfaces
        for (unsigned int j = 0;j < entry->getNumOutInterfaces(); j++)
        {
            AnsaIPv4MulticastRoute::AnsaOutInterface *outInterface = entry->getAnsaOutInterface(j);
            if (outInterface->pruneTimer != NULL)
            {
                cancelEvent(outInterface->pruneTimer);
                delete outInterface->pruneTimer;
            }
        }

        // delete route
        delete entry;
        updateDisplayString();
        return true;
    }
    return false;
}

/**
 * GET ROUTE FOR
 *
 * The method returns one route from multicast routing table for given group and source IP addresses.
 *
 * @param group IP address of multicast group.
 * @param source IP address of multicast source.
 * @return Pointer to found multicast route.
 * @see getRoute()
 */
AnsaIPv4MulticastRoute *AnsaRoutingTable::getRouteFor(IPv4Address group, IPv4Address source)
{
    Enter_Method("getMulticastRoutesFor(%x, %x)", group.getInt(), source.getInt()); // note: str().c_str() too slow here here
    EV << "MulticastRoutingTable::getRouteFor - group = " << group << ", source = " << source << endl;

    // search in multicast routing table
    AnsaIPv4MulticastRoute *route = NULL;

    int n = getNumMulticastRoutes();
    int i;
    // go through all multicast routes
    for (i = 0; i < n; i++)
    {
        route = dynamic_cast<AnsaIPv4MulticastRoute*>(getMulticastRoute(i));
        if (route && route->getMulticastGroup() == group && route->getOrigin() == source)
            break;
    }

    if (i == n)
        return NULL;
    return route;
}

/**
 * GET ROUTE FOR
 *
 * The method returns all routes from multicast routing table for given multicast group.
 *
 * @param group IP address of multicast group.
 * @return Vecotr of pointers to routes in multicast table.
 * @see getRoute()
 */
std::vector<AnsaIPv4MulticastRoute*> AnsaRoutingTable::getRouteFor(IPv4Address group)
{
    Enter_Method("getMulticastRoutesFor(%x)", group.getInt()); // note: str().c_str() too slow here here
    EV << "MulticastRoutingTable::getRouteFor - address = " << group << endl;
    std::vector<AnsaIPv4MulticastRoute*> routes;

    // search in multicast table
    int n = getNumMulticastRoutes();
    for (int i = 0; i < n; i++)
    {
        AnsaIPv4MulticastRoute *route = dynamic_cast<AnsaIPv4MulticastRoute*>(getMulticastRoute(i));
        if (route && route->getMulticastGroup() == group)
            routes.push_back(route);
    }

    return routes;
}

/**
 * GET ROUTES FOR SOURCES
 *
 * The method returns all routes from multicast routing table for given source.
 *
 * @param source IP address of multicast source.
 * @return Vector of found multicast routes.
 * @see getNetwork()
 */
std::vector<AnsaIPv4MulticastRoute*> AnsaRoutingTable::getRoutesForSource(IPv4Address source)
{
    Enter_Method("getRoutesForSource(%x)", source.getInt()); // note: str().c_str() too slow here here
    EV << "MulticastRoutingTable::getRoutesForSource - source = " << source << endl;
    std::vector<AnsaIPv4MulticastRoute*> routes;

    // search in multicast table
    int n = getNumMulticastRoutes();
    int i;
    for (i = 0; i < n; i++)
    {
        //FIXME works only for classfull adresses (function getNetwork) !!!!
        AnsaIPv4MulticastRoute *route = dynamic_cast<AnsaIPv4MulticastRoute*>(getMulticastRoute(i));
        if (route && route->getOrigin().getNetwork().getInt() == source.getInt())
            routes.push_back(route);
    }
    return routes;
}

bool AnsaRoutingTable::isLocalAddress(const IPv4Address& dest) const
{
    Enter_Method("isLocalAddress A (%u.%u.%u.%u)", dest.getDByte(0), dest.getDByte(1), dest.getDByte(2), dest.getDByte(3)); // note: str().c_str() too slow here

    if (localAddresses.empty())
    {
        // collect interface addresses if not yet done
        for (int i=0; i<ift->getNumInterfaces(); i++)
        {
            IPv4Address interfaceAddr = ift->getInterface(i)->ipv4Data()->getIPAddress();
            localAddresses.insert(interfaceAddr);
        }
    }

    for (int i=0; i<ift->getNumInterfaces(); i++)
    {
        AnsaInterfaceEntry* ieVF = dynamic_cast<AnsaInterfaceEntry *>(ift->getInterface(i));
        if (ieVF && ieVF->hasIPAddress(dest))
            return true;
    }

    AddressSet::iterator it = localAddresses.find(dest);
    return it!=localAddresses.end();
}

InterfaceEntry *AnsaRoutingTable::getInterfaceByAddress(const IPv4Address& addr) const
{
    Enter_Method("getInterfaceByAddress(%u.%u.%u.%u)", addr.getDByte(0), addr.getDByte(1), addr.getDByte(2), addr.getDByte(3)); // note: str().c_str() too slow here

    if (addr.isUnspecified())
        return NULL;
    for (int i=0; i<ift->getNumInterfaces(); ++i)
    {
        InterfaceEntry* ie = ift->getInterface(i);
        if (ie->ipv4Data()->getIPAddress()==addr)
            return ie;

        if (!ie->isLoopback() && dynamic_cast<AnsaInterfaceEntry *>(ie))
        {
            AnsaInterfaceEntry* ieVF = dynamic_cast<AnsaInterfaceEntry *>(ie);
            if (ieVF->hasIPAddress(addr))
                return ie;

        }
    }
    return NULL;
}

