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
 * @author Veronika Rybova,Tomas Prochazka (mailto:xproch21@stud.fit.vutbr.cz), Vladimir Vesely (mailto:ivesely@fit.vutbr.cz)
 * @brief IPv4 implementation with changes for multicast
 * @details File contains reimplementation of some methods in IP class,
 * which can work also with multicast data and multicast
 */


#include <omnetpp.h>
#include "AnsaIPv4.h"

#include "AnsaIPv4RoutingDecision_m.h"

Define_Module(AnsaIPv4);


/**
 * INITIALIZE
 *
 * The method initialize ale structures (tables) which will use.
 *
 * @param stage Stage of initialization.
 */
void AnsaIPv4::initialize()
{
    IPv4::initialize();

    nb = NotificationBoardAccess().get();
    pimIft = PimInterfaceTableAccess().getIfExists();               // For recognizing PIM mode
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

        //FIXME temporary hack to catch "IGMP" for initialization PIM Join/Prune (*,G) PIM-SM
        if (datagram->getTransportProtocol() == IP_PROT_IGMP)
        {
            EV << "AnsaIPv4::handlePacketFromNetwork - IGMP packet received" << endl;

            if (fromIE->ipv4Data()->hasMulticastListener(datagram->getDestAddress()))
                fromIE->ipv4Data()->removeMulticastListener(datagram->getDestAddress());
            else
                fromIE->ipv4Data()->addMulticastListener(datagram->getDestAddress());

            delete datagram;
            return;
        }

        //PIM send PIM packet to PIM module
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
            routeMulticastPacket(datagram, getSourceInterfaceFrom(datagram));
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
                IPv4::fragmentAndSend(datagram->dup(), broadcastIE, IPv4Address::ALLONES_ADDRESS);

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
        {
            routeUnicastPacket(datagram, NULL/*destIE*/, IPv4Address::UNSPECIFIED_ADDRESS);
        }
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
void AnsaIPv4::routeMulticastPacket(IPv4Datagram *datagram, InterfaceEntry *fromIE)
{
    IPv4Address destAddr = datagram->getDestAddress();
    IPv4Address srcAddr = datagram->getSrcAddress();
    IPv4ControlInfo *ctrl = (IPv4ControlInfo *) datagram->getControlInfo();
    EV << "Routing multicast datagram `" << datagram->getName() << "' with dest=" << destAddr << "\n";
    AnsaRoutingTable *rt = check_and_cast<AnsaRoutingTable*>(dynamic_cast<cObject*>(this->rt));
    AnsaIPv4MulticastRoute *route = rt->getRouteFor(destAddr, srcAddr);
    AnsaIPv4MulticastRoute *routeG = rt->getRouteFor(destAddr, IPv4Address::UNSPECIFIED_ADDRESS);

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

    //PIM mode specific behavior
    //FIXME better is outgoing interface
    PIMmode intfMode = pimIft->getInterfaceByIntID(fromIE->getInterfaceId())->getMode();

    // multicast group is not in multicast routing table and has to be added
    if (route == NULL && routeG == NULL)
    {
        EV << "AnsaIP::routeMulticastPacket - Multicast route does not exist, try to add." << endl;
        nb->fireChangeNotification(NF_IPv4_NEW_MULTICAST, ctrl);
        if (intfMode == Dense)
        {
            delete datagram->removeControlInfo();
            ctrl = NULL;
        }
        // read new record
        route = rt->getRouteFor(destAddr, srcAddr);
    }

    if (route == NULL && routeG == NULL)
    {
        EV << "Still do not exist." << endl;
        delete datagram;
        return;
    }

    //routing for selected mode
    if (intfMode == Dense)
        routePimDM(route, datagram, ctrl);
    if (intfMode == Sparse)
        routePimSM(route, routeG, datagram, ctrl);

    // refresh output in MRT
    rt->generateShowIPMroute();
    // only copies sent, delete original datagram
    delete datagram;
}

void AnsaIPv4::routePimDM (AnsaIPv4MulticastRoute *route, IPv4Datagram *datagram, IPv4ControlInfo *ctrl)
{
    IPv4Address destAddr = datagram->getDestAddress();

    nb->fireChangeNotification(NF_IPv4_DATA_ON_RPF, route);

    // data won't be sent because there is no outgoing interface and/or route is pruned
    if (route->getNumOutInterfaces() == 0 || route->isFlagSet(AnsaIPv4MulticastRoute::P))
    {
        EV << "Route does not have any outgoing interface or it is pruned." << endl;
        if(ctrl != NULL)
        {
            if (!route->isFlagSet(AnsaIPv4MulticastRoute::A))
                nb->fireChangeNotification(NF_IPv4_DATA_ON_PRUNED_INT, ctrl);
        }
        return;
    }

    // send packet to all outgoing interfaces of route (oilist)
    for (unsigned int i=0; i<route->getNumOutInterfaces(); i++)
    {
        // do not send to pruned interface
        AnsaIPv4MulticastRoute::AnsaOutInterface *outInterface = route->getAnsaOutInterface(i);
        if (outInterface && outInterface->forwarding == AnsaIPv4MulticastRoute::Pruned)
            continue;

        InterfaceEntry *destIE = route->getOutInterface(i)->getInterface();
        IPv4Datagram *datagramCopy = (IPv4Datagram *) datagram->dup();

        IPv4::fragmentAndSend(datagramCopy, destIE, destAddr);    // send
    }
}

void AnsaIPv4::routePimSM (AnsaIPv4MulticastRoute *route, AnsaIPv4MulticastRoute *routeG, IPv4Datagram *datagram, IPv4ControlInfo *ctrl)
{

    IPv4Address destAddr = datagram->getDestAddress();
    InterfaceEntry *rpfInt = rt->getInterfaceForDestAddr(datagram->getSrcAddress());
    AnsaIPv4MulticastRoute *routePtr = NULL;
    InterfaceEntry *intToRP = NULL;

    //if (S,G) route exist, prefer (S,G) olist
    if (routeG)
    {
        //outInt = routeG->getOutInt();
        routePtr = routeG;
        nb->fireChangeNotification(NF_IPv4_DATA_ON_RPF_PIMSM, routeG);
    }
    if (route)
    {
        //outInt = route->getOutInt();
        routePtr = route;
        intToRP = rt->getInterfaceForDestAddr(routePtr->getRP());
        nb->fireChangeNotification(NF_IPv4_DATA_ON_RPF_PIMSM, route);
    }

    // send packet to all outgoing interfaces of route (oilist)
    for (unsigned int i=0; i<routePtr->getNumOutInterfaces(); i++)
    {
        AnsaIPv4MulticastRoute::AnsaOutInterface *outInterface = routePtr->getAnsaOutInterface(i);
        // do not send to pruned interface
        if (outInterface && outInterface->forwarding == AnsaIPv4MulticastRoute::Pruned)
            continue;
        // RPF check before datagram sending
        if (outInterface->intId == rpfInt->getInterfaceId())
            continue;
        InterfaceEntry *destIE = routePtr->getOutInterface(i)->getInterface();
        IPv4Datagram *datagramCopy = (IPv4Datagram *) datagram->dup();

        IPv4::fragmentAndSend(datagramCopy, destIE, destAddr);
    }

    if (route && route->isFlagSet(AnsaIPv4MulticastRoute::F)
            && route->isFlagSet(AnsaIPv4MulticastRoute::P)
            && (route->getRegStatus(intToRP->getInterfaceId()) == AnsaIPv4MulticastRoute::Join))
        nb->fireChangeNotification(NF_IPv4_MDATA_REGISTER, ctrl);
}


void AnsaIPv4::handleMessageFromHL(cPacket *msg)
{
    // if no interface exists, do not send datagram
    if (ift->getNumInterfaces() == 0)
    {
        EV << "No interfaces exist, dropping packet\n";
        numDropped++;
        delete msg;
        return;
    }

    // encapsulate and send
    IPv4Datagram *datagram = dynamic_cast<IPv4Datagram *>(msg);
    IPv4ControlInfo *controlInfo = NULL;
    //FIXME dubious code, remove? how can the HL tell IP whether it wants tunneling or forwarding?? --Andras
    if (datagram) // if HL sends an IPv4Datagram, route the packet
    {
        // Dsr routing, Dsr is a HL protocol and send IPv4Datagram
        if (datagram->getTransportProtocol()==IP_PROT_DSR)
        {
            controlInfo = check_and_cast<IPv4ControlInfo*>(datagram->removeControlInfo());
        }
    }
    else
    {
        // encapsulate
        controlInfo = check_and_cast<IPv4ControlInfo*>(msg->removeControlInfo());
        datagram = encapsulate(msg, controlInfo);
    }

    // extract requested interface and next hop
    InterfaceEntry *destIE = NULL;
    IPv4Address nextHopAddress = IPv4Address::UNSPECIFIED_ADDRESS;
    int vforwarder = -1;
    bool multicastLoop = true;

    if (controlInfo!=NULL)
    {
        destIE = ift->getInterfaceById(controlInfo->getInterfaceId());
        nextHopAddress = controlInfo->getNextHopAddr();
        multicastLoop = controlInfo->getMulticastLoop();

        if (destIE && dynamic_cast<AnsaInterfaceEntry *>(destIE))
        {
            AnsaInterfaceEntry* ieVF = dynamic_cast<AnsaInterfaceEntry *>(destIE);
            vforwarder = ieVF->getVirtualForwarderId(controlInfo->getMacSrc());
        }
    }

    delete controlInfo;

    // send
    IPv4Address &destAddr = datagram->getDestAddress();

    EV << "Sending datagram `" << datagram->getName() << "' with dest=" << destAddr << "\n";

    if (datagram->getDestAddress().isMulticast())
    {
        destIE = determineOutgoingInterfaceForMulticastDatagram(datagram, destIE);

        // loop back a copy
        if (multicastLoop && (!destIE || !destIE->isLoopback()))
        {
            InterfaceEntry *loopbackIF = ift->getFirstLoopbackInterface();
            if (loopbackIF)
                fragmentAndSend(datagram->dup(), loopbackIF, destAddr, vforwarder);
        }

        if (destIE)
        {
            numMulticast++;
            fragmentAndSend(datagram, destIE, destAddr, vforwarder);
        }
        else
        {
            EV << "No multicast interface, packet dropped\n";
            numUnroutable++;
            delete datagram;
        }
    }
    else // unicast and broadcast
    {
#ifdef WITH_MANET
        if (manetRouting)
            sendRouteUpdateMessageToManet(datagram);
#endif
        // check for local delivery
        if (rt->isLocalAddress(destAddr))
        {
            EV << "local delivery\n";
            if (destIE)
                EV << "datagram destination address is local, ignoring destination interface specified in the control info\n";

            destIE = ift->getFirstLoopbackInterface();
            ASSERT(destIE);
            fragmentAndSend(datagram, destIE, destAddr, vforwarder);
        }
        else if (destAddr.isLimitedBroadcastAddress() || rt->isLocalBroadcastAddress(destAddr))
            routeLocalBroadcastPacket(datagram, destIE);
        else
            routeUnicastPacket(datagram, destIE, nextHopAddress);
    }
}


void AnsaIPv4::routeUnicastPacket(IPv4Datagram *datagram, InterfaceEntry *destIE, IPv4Address destNextHopAddr)
{
    IPv4Address destAddr = datagram->getDestAddress();

    EV << "Routing datagram `" << datagram->getName() << "' with dest=" << destAddr << ": ";

    IPv4Address nextHopAddr;
    // if output port was explicitly requested, use that, otherwise use IPv4 routing
    if (destIE)
    {
        EV << "using manually specified output interface " << destIE->getName() << "\n";
        // and nextHopAddr remains unspecified
        if (manetRouting && !destNextHopAddr.isUnspecified())
           nextHopAddr = destNextHopAddr;  // Manet DSR routing explicit route
        // special case ICMP reply
        else if (destIE->isBroadcast())
        {
            // if the interface is broadcast we must search the next hop
            const IPv4Route *re = rt->findBestMatchingRoute(destAddr);
            if (re && (re->getSource() != IPv4Route::MANET || re->getDestination()==destAddr) &&
                    re->getInterface() == destIE)
                nextHopAddr = re->getGateway();
        }
    }
    else
    {
        // use IPv4 routing (lookup in routing table)
        //    FIXME MANET routes should use 255.255.255.255 netmask,
        //          to eliminate the equality check below.
        const IPv4Route *re = rt->findBestMatchingRoute(destAddr);
        if (re && (re->getSource() != IPv4Route::MANET || re->getDestination() == destAddr))
        {
            destIE = re->getInterface();
            nextHopAddr = re->getGateway();
        }
    }

    if (!destIE) // no route found
    {
#ifdef WITH_MANET
            if (manetRouting)
               sendNoRouteMessageToManet(datagram);
            else
            {
#endif
                EV << "unroutable, sending ICMP_DESTINATION_UNREACHABLE\n";
                numUnroutable++;
                icmpAccess.get()->sendErrorMessage(datagram, ICMP_DESTINATION_UNREACHABLE, 0);
#ifdef WITH_MANET
            }
#endif
    }
    else // fragment and send
    {
        int vforwarder = -1;
        if (dynamic_cast<AnsaInterfaceEntry *>(destIE))
        {
            vforwarder = dynamic_cast<AnsaInterfaceEntry *>(destIE)->getVirtualForwarderId(datagram->getSrcAddress());
        }

        EV << "output interface is " << destIE->getName() << ", next-hop address: " << nextHopAddr << "\n";
        numForwarded++;
        fragmentAndSend(datagram, destIE, nextHopAddr, vforwarder);
    }
}

void AnsaIPv4::sendDatagramToOutput(IPv4Datagram *datagram, InterfaceEntry *ie, IPv4Address nextHopAddr, int vforwarderId)
{
    if (vforwarderId == -1)
    {
        IPv4::sendDatagramToOutput(datagram,ie, nextHopAddr);
        return;
    }

    AnsaInterfaceEntry* ieVF = dynamic_cast<AnsaInterfaceEntry *>(ie);
    delete datagram->removeControlInfo();
    AnsaIPv4RoutingDecision *routingDecision = new AnsaIPv4RoutingDecision();
    routingDecision->setInterfaceId(ieVF->getInterfaceId());
    routingDecision->setNextHopAddr(nextHopAddr);
    routingDecision->setVforwarderId(vforwarderId);
    datagram->setControlInfo(routingDecision);
    send(datagram, queueOutGate);
}

void AnsaIPv4::fragmentAndSend(IPv4Datagram *datagram, InterfaceEntry *ie, IPv4Address nextHopAddr, int vforwarder)
{
    // fill in source address
    if (datagram->getSrcAddress().isUnspecified())
        datagram->setSrcAddress(ie->ipv4Data()->getIPAddress());

    // hop counter decrement; but not if it will be locally delivered
    if (!ie->isLoopback())
        datagram->setTimeToLive(datagram->getTimeToLive()-1);

    // hop counter check
    if (datagram->getTimeToLive() < 0)
    {
        // drop datagram, destruction responsibility in ICMP
        EV << "datagram TTL reached zero, sending ICMP_TIME_EXCEEDED\n";
        icmpAccess.get()->sendErrorMessage(datagram, ICMP_TIME_EXCEEDED, 0);
        numDropped++;
        return;
    }

    int mtu = ie->getMTU();

    // check if datagram does not require fragmentation
    if (datagram->getByteLength() <= mtu)
    {
        sendDatagramToOutput(datagram, ie, nextHopAddr, vforwarder);
        return;
    }

    // if "don't fragment" bit is set, throw datagram away and send ICMP error message
    if (datagram->getDontFragment())
    {
        EV << "datagram larger than MTU and don't fragment bit set, sending ICMP_DESTINATION_UNREACHABLE\n";
        icmpAccess.get()->sendErrorMessage(datagram, ICMP_DESTINATION_UNREACHABLE,
                                                     ICMP_FRAGMENTATION_ERROR_CODE);
        numDropped++;
        return;
    }

    // optimization: do not fragment and reassemble on the loopback interface
    if (ie->isLoopback())
    {
        sendDatagramToOutput(datagram, ie, nextHopAddr, vforwarder);
        return;
    }

    // FIXME some IP options should not be copied into each fragment, check their COPY bit
    int headerLength = datagram->getHeaderLength();
    int payloadLength = datagram->getByteLength() - headerLength;
    int fragmentLength = ((mtu - headerLength) / 8) * 8; // payload only (without header)
    int offsetBase = datagram->getFragmentOffset();

    int noOfFragments = (payloadLength + fragmentLength - 1)/ fragmentLength;
    EV << "Breaking datagram into " << noOfFragments << " fragments\n";

    // create and send fragments
    std::string fragMsgName = datagram->getName();
    fragMsgName += "-frag";

    for (int offset=0; offset < payloadLength; offset+=fragmentLength)
    {
        bool lastFragment = (offset+fragmentLength >= payloadLength);
        // length equal to fragmentLength, except for last fragment;
        int thisFragmentLength = lastFragment ? payloadLength - offset : fragmentLength;

        // FIXME is it ok that full encapsulated packet travels in every datagram fragment?
        // should better travel in the last fragment only. Cf. with reassembly code!
        IPv4Datagram *fragment = (IPv4Datagram *) datagram->dup();
        fragment->setName(fragMsgName.c_str());

        // "more fragments" bit is unchanged in the last fragment, otherwise true
        if (!lastFragment)
            fragment->setMoreFragments(true);

        fragment->setByteLength(headerLength + thisFragmentLength);
        fragment->setFragmentOffset(offsetBase + offset);

        sendDatagramToOutput(fragment, ie, nextHopAddr, vforwarder);
    }

    delete datagram;
}
