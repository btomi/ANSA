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
 * @file AnsaIPv4.cc
 * @date 10.10.2011
 * @author Veronika Rybova, Vladimir Vesely (mailto:ivesely@fit.vutbr.cz)
 * @brief IPv4 implementation with changes for multicast
 * @details File contains reimplementation of some methods in IP class,
 * which can work also with multicast data and multicast
 */


#include <omnetpp.h>
#include "AnsaIPv4.h"

Define_Module(AnsaIPv4);


/**
 * INITIALIZE
 *
 * The method initialize ale structures (tables) which will use.
 *
 * @param stage Stage of initialization.
 */
void AnsaIPv4::initialize(int stage)
{
    QueueBase::initialize();

    ift = InterfaceTableAccess().get();
    rt = AnsaRoutingTableAccess().get();
    nb = NotificationBoardAccess().get();

    queueOutGate = gate("queueOut");

    defaultTimeToLive = par("timeToLive");
    defaultMCTimeToLive = par("multicastTimeToLive");
    fragmentTimeoutTime = par("fragmentTimeout");
    forceBroadcast = par("forceBroadcast");
    mapping.parseProtocolMapping(par("protocolMapping"));

    curFragmentId = 0;
    lastCheckTime = 0;
    fragbuf.init(icmpAccess.get());

    numMulticast = numLocalDeliver = numDropped = numUnroutable = numForwarded = 0;

    WATCH(numMulticast);
    WATCH(numLocalDeliver);
    WATCH(numDropped);
    WATCH(numUnroutable);
    WATCH(numForwarded);

    // by default no MANET routing
    manetRouting = false;

}


void AnsaIPv4::handlePacketFromNetwork(IPv4Datagram *datagram, InterfaceEntry *fromIE)
{
    ASSERT(datagram);
    ASSERT(fromIE);

    //
    // "Prerouting"
    //

    // check for header biterror
    if (datagram->hasBitError())
    {
        // probability of bit error in header = size of header / size of total message
        // (ignore bit error if in payload)
        double relativeHeaderLength = datagram->getHeaderLength() / (double)datagram->getByteLength();
        if (dblrand() <= relativeHeaderLength)
        {
            EV << "bit error found, sending ICMP_PARAMETER_PROBLEM\n";
            icmpAccess.get()->sendErrorMessage(datagram, ICMP_PARAMETER_PROBLEM, 0);
            return;
        }
    }

    // remove control info, but keep the one on the last fragment of DSR and MANET datagrams
    if (datagram->getTransportProtocol()!=IP_PROT_DSR && datagram->getTransportProtocol()!=IP_PROT_MANET
            && !datagram->getDestAddress().isMulticast() && datagram->getTransportProtocol()!=IP_PROT_PIM)
    {
        delete datagram->removeControlInfo();
    }
    else if (datagram->getMoreFragments())
        delete datagram->removeControlInfo(); // delete all control message except the last


    //MYWORK Add all neccessery info to the IP Control Info for future use.
    if (datagram->getDestAddress().isMulticast() || datagram->getTransportProtocol() == IP_PROT_PIM)
    {
        IPv4ControlInfo *ctrl = (IPv4ControlInfo*)(datagram->removeControlInfo());
        ctrl->setSrcAddr(datagram->getSrcAddress());
        ctrl->setDestAddr(datagram->getDestAddress());
        ctrl->setInterfaceId(getSourceInterfaceFrom(datagram)->getInterfaceId());
        datagram->setControlInfo(ctrl);
    }

    // route packet
    IPv4Address &destAddr = datagram->getDestAddress();

    EV << "Received datagram `" << datagram->getName() << "' with dest=" << destAddr << "\n";

    if (fromIE->isLoopback())
    {
        reassembleAndDeliver(datagram);
    }
    else if (destAddr.isMulticast())
    {
        // check for local delivery
        // Note: multicast routers will receive IGMP datagrams even if their interface is not joined to the group
        if (fromIE->ipv4Data()->isMemberOfMulticastGroup(destAddr) ||
                (rt->isMulticastForwardingEnabled() && datagram->getTransportProtocol() == IP_PROT_IGMP))
            reassembleAndDeliver(datagram->dup());

        //PIM-DM send PIM packet to PIM module
        if (datagram->getTransportProtocol() == IP_PROT_PIM)
        {
            cPacket *packet = decapsulate(datagram);
            send(packet, "transportOut", mapping.getOutputGateForProtocol(IP_PROT_PIM));
            return;
        }

        // don't forward if IP forwarding is off, or if dest address is link-scope
        if (!rt->isIPForwardingEnabled() || destAddr.isLinkLocalMulticast())
            delete datagram;
        else if (datagram->getTimeToLive() == 0)
        {
            EV << "TTL reached 0, dropping datagram.\n";
            delete datagram;
        }
        else
            //forwardMulticastPacket(datagram, fromIE);
            routeMulticastPacket(datagram, NULL, getSourceInterfaceFrom(datagram));
    }
    else
    {
#ifdef WITH_MANET
        if (manetRouting)
            sendRouteUpdateMessageToManet(datagram);
#endif
        InterfaceEntry *broadcastIE = NULL;

        // check for local delivery; we must accept also packets coming from the interfaces that
        // do not yet have an IP address assigned. This happens during DHCP requests.
        if (rt->isLocalAddress(destAddr) || fromIE->ipv4Data()->getIPAddress().isUnspecified())
        {
            reassembleAndDeliver(datagram);
        }
        else if (destAddr.isLimitedBroadcastAddress() || (broadcastIE=rt->findInterfaceByLocalBroadcastAddress(destAddr)))
        {
            // broadcast datagram on the target subnet if we are a router
            if (broadcastIE && fromIE != broadcastIE && rt->isIPForwardingEnabled())
                fragmentAndSend(datagram->dup(), broadcastIE, IPv4Address::ALLONES_ADDRESS);

            EV << "Broadcast received\n";
            reassembleAndDeliver(datagram);
        }
        else if (!rt->isIPForwardingEnabled())
        {
            EV << "forwarding off, dropping packet\n";
            numDropped++;
            delete datagram;
        }
        else
            routeUnicastPacket(datagram, NULL/*destIE*/, IPv4Address::UNSPECIFIED_ADDRESS);
    }
}

/**
 * ROUTE MULTICAST PACKET
 *
 * Extension of method routeMulticastPacket() from class IP. The method checks if data come
 * to RPF interface, if not it sends notification. Multicast data which are sent by this router
 * and has given outgoing interface are sent directly (PIM messages). The method finds route for
 * group. If there is no route, it will be added. Then packet is copied and sent to all outgoing
 * interfaces in route.
 *
 * All part which I added are signed by MYWORK tag.
 *
 * @param datagram Pointer to incoming datagram.
 * @param destIE Pointer to outgoing interface.
 * @param fromIE Pointer to incoming interface.
 * @see routeMulticastPacket()
 */
void AnsaIPv4::routeMulticastPacket(IPv4Datagram *datagram, InterfaceEntry *destIE, InterfaceEntry *fromIE)
{
    IPv4Address destAddr = datagram->getDestAddress();
    IPv4Address srcAddr = datagram->getSrcAddress();
    IPv4ControlInfo *ctrl = (IPv4ControlInfo *) datagram->getControlInfo();
    EV << "Routing multicast datagram `" << datagram->getName() << "' with dest=" << destAddr << "\n";
    AnsaIPv4MulticastRoute *route = rt->getRouteFor(destAddr, srcAddr);

    numMulticast++;

    // Process datagram only if sent locally or arrived on the shortest
    // route (provided routing table already contains srcAddr) = RPF interface;
    // otherwise discard and continue.
    InterfaceEntry *rpfInt = rt->getInterfaceForDestAddr(datagram->getSrcAddress());
    if (fromIE!=NULL && rpfInt!=NULL && fromIE!=rpfInt)
    {
        //MYWORK RPF interface has changed
        /*if (route != NULL && (route->getInIntId() != rpfInt->getInterfaceId()))
        {
            EV << "RPF interface has changed" << endl;
            nb->fireChangeNotification(NF_IPv4_RPF_CHANGE, route);
        }*/
        //MYWORK Data come to non-RPF interface
        if (!rt->isLocalMulticastAddress(destAddr) && !destAddr.isLinkLocalMulticast())
        {
            EV << "Data on non-RPF interface" << endl;
            nb->fireChangeNotification(NF_IPv4_DATA_ON_NONRPF, ctrl);
            return;
        }
        else
        {
            // FIXME count dropped
            EV << "Packet dropped." << endl;
            delete datagram;
            return;
        }
    }

    //MYWORK for local traffic to given destination (PIM messages)
    if (fromIE == NULL && destIE != NULL)
    {
        IPv4Datagram *datagramCopy = (IPv4Datagram *) datagram->dup();
        datagramCopy->setSrcAddress(destIE->ipv4Data()->getIPAddress());
        fragmentAndSend(datagramCopy, destIE, destAddr);

        delete datagram;
        return;
    }

    // if received from the network...
    if (fromIE!=NULL)
    {
        EV << "Packet was received from the network..." << endl;
        // check for local delivery (multicast assigned to any interface)
        if (rt->isLocalMulticastAddress(destAddr))
        {
            EV << "isLocalMulticastAddress." << endl;
            IPv4Datagram *datagramCopy = (IPv4Datagram *) datagram->dup();

            // FIXME code from the MPLS model: set packet dest address to routerId
            datagramCopy->setDestAddress(rt->getRouterId());
            reassembleAndDeliver(datagramCopy);
        }

        // don't forward if IP forwarding is off
        if (!rt->isIPForwardingEnabled())
        {
            EV << "IP forwarding is off." << endl;
            delete datagram;
            return;
        }

        // don't forward if dest address is link-scope
        // address is in the range 224.0.0.0 to 224.0.0.255
        if (destAddr.isLinkLocalMulticast())
        {
            EV << "isLinkLocalMulticast." << endl;
            delete datagram;
            return;
        }
    }

//MYWORK(to the end) now: routing
    EV << "AnsaIPv4::routeMulticastPacket - Multicast routing." << endl;

    // multicast group is not in multicast routing table and has to be added
    if (route == NULL)
    {
        EV << "AnsaIP::routeMulticastPacket - Multicast route does not exist, try to add." << endl;
        nb->fireChangeNotification(NF_IPv4_NEW_MULTICAST, ctrl);
        delete datagram->removeControlInfo();
        ctrl = NULL;
        // read new record
        route = rt->getRouteFor(destAddr, srcAddr);
    }

    if (route == NULL)
    {
        EV << "Still do not exist." << endl;
        delete datagram;
        return;
    }

    nb->fireChangeNotification(NF_IPv4_DATA_ON_RPF, route);

    // data won't be sent because there is no outgoing interface and/or route is pruned
    InterfaceVector outInt = route->getOutInt();
    if (outInt.size() == 0 || route->isFlagSet(P))
    {
        EV << "Route does not have any outgoing interface or it is pruned." << endl;
        if(ctrl != NULL)
        {
            if (!route->isFlagSet(A))
                nb->fireChangeNotification(NF_IPv4_DATA_ON_PRUNED_INT, ctrl);
        }
        delete datagram;
        return;
    }

    // send packet to all outgoing interfaces of route (oilist)
    for (unsigned int i=0; i<outInt.size(); i++)
    {
        // do not send to pruned interface
        if (outInt[i].forwarding == Pruned)
            continue;

        InterfaceEntry *destIE = outInt[i].intPtr;
        IPv4Datagram *datagramCopy = (IPv4Datagram *) datagram->dup();

        // set datagram source address if not yet set
        if (datagramCopy->getSrcAddress().isUnspecified())
            datagramCopy->setSrcAddress(destIE->ipv4Data()->getIPAddress());

        // send
        fragmentAndSend(datagramCopy, destIE, destAddr);
    }

    // only copies sent, delete original datagram
    delete datagram;
}
