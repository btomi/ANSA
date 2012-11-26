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

#include "RIPngRouting.h"

#include "IPv6InterfaceData.h"
#include "IPv6ControlInfo.h"

Define_Module(RIPngRouting);

RIPngRouting::RIPngRouting()
{
    regularUpdateTimer = NULL;
    triggeredUpdateTimer = NULL;
}

RIPngRouting::~RIPngRouting()
{
    deleteTimer(regularUpdateTimer);
    deleteTimer(triggeredUpdateTimer);

    removeAllRoutingTableEntries();
    removeAllEnabledInterfaces();
}

void RIPngRouting::removeAllRoutingTableEntries()
{
    RoutingTableIt it;
    RIPng::RoutingTableEntry *routingTableEntry;

    for (it = routingTable.begin(); it != routingTable.end(); it++)
    {
        routingTableEntry = (*it).second;
        deleteTimer(routingTableEntry->getGCTimer());
        deleteTimer(routingTableEntry->getTimer());
        delete routingTableEntry;
    }

    routingTable.clear();
}

void RIPngRouting::showRoutingTable()
{
    RoutingTableIt it;
    RIPng::RoutingTableEntry *routingTableEntry;

    ev << routerText << endl;

    for (it = routingTable.begin(); it != routingTable.end(); it++)
    {
        routingTableEntry = (*it).second;
        ev << routingTableEntry->info() << endl;
    }
}

void RIPngRouting::initialize(int stage)
{
    if (stage != 3)
        return;

    // get the hostname
    cModule *containingMod = findContainingNode(this);
    if (!containingMod)
        hostName = "";
    else
        hostName = containingMod->getFullName();

    routerText = " (Router " + hostName + ") ";

    bSendTriggeredUpdateMessage = false;
    bBlockTriggeredUpdateMessage = false;

    // access to the routing and interface table
    rt = RoutingTable6Access().get();
    ift = InterfaceTableAccess().get();
    // subscribe for changes in the device
    nb = NotificationBoardAccess().get();
    nb->subscribe(this, NF_INTERFACE_STATE_CHANGED);
    nb->subscribe(this, NF_IPv6_ROUTE_DELETED);

    numSent = 0;
    numReceived = 0;
    WATCH(numSent);
    WATCH(numReceived);

    const char *RIPngAddressString = par("RIPngAddress");
    RIPngAddress = IPv6Address(RIPngAddressString);
    RIPngPort = par("RIPngPort");

    connNetworkMetric = par("connectedNetworkMetric");
    infinityMetric = par("infinityMetric");

    routeTimeout = par("routeTimeout").doubleValue();
    routeGarbageCollectionTimeout = par("routeGarbageCollectionTimeout").doubleValue();
    regularUpdateTimeout = par("regularUpdateInterval").doubleValue();

    // get deviceId
    deviceId = par("deviceId");

    // read the RIPng process configuration
    const char *RIPngConfigFileName = par("configFile");
    if (!loadConfigFromXML(RIPngConfigFileName))
        error("Error reading RIPng configuration from %s file", RIPngConfigFileName);

    globalSocket.setOutputGate(gate("udpOut"));

    // start REGULAR UPDATE TIMER
    regularUpdateTimer = createAndStartTimer(GENERAL_UPDATE, regularUpdateTimeout);
    triggeredUpdateTimer = createTimer(TRIGGERED_UPDATE);

    sendAllRoutesRequest();
}

void RIPngRouting::receiveChangeNotification(int category, const cObject *details)
{
   // TODO:
   return;
   // ignore notifications during initialization
   if (simulation.getContextType() == CTX_INITIALIZE)
       return;

   Enter_Method_Silent();
   printNotificationBanner(category, details);

   if (category == NF_INTERFACE_STATE_CHANGED)
   {
       InterfaceEntry *interfaceEntry = check_and_cast<InterfaceEntry*>(details);
       int interfaceEntryId = interfaceEntry->getInterfaceId();

       // an interface went down
       if (interfaceEntry->isDown())
       {
           // delete interface from ripng interfaces
           int size = getEnabledInterfacesCount();
           RIPng::Interface* ripngInterface;

           for (int i = 0; i < size; i++)
           {
               ripngInterface = getEnabledInterface(i);
               if (ripngInterface->getId() == interfaceEntryId)
               {
                   removeEnabledInterface(i);
                   break;
               }
           }

           // delete associated routes from ripng routing table
           RoutingTableIt it;
           for (it = routingTable.begin(); it != routingTable.end(); )
           {
               if ((*it).second->getInterfaceId() == interfaceEntryId)
                   removeRoutingTableEntry(it++);
               else
                   ++it;
           }
       }
       // TODO:
       // an interface went up
       // new network on an interface
   }
   // TODO:
   /*else if (category == NF_IPv6_ROUTE_DELETED)
   {
       RIPng::RoutingTableEntry *RIPngRoute = dynamic_cast<RIPng::RoutingTableEntry *>(details);
       if (RIPngRoute != NULL)
       // it's notification about RIPng route - ignore (we don't need it)
           return;
       IPv6Route *route = check_and_cast<IPv6Route *>(details);

       // if route from other routing protocol was deleted, check "RIPng routing table"
   }*/
}

bool RIPngRouting::loadConfigFromXML(const char *configFileName)
{
    // get router's config
    cXMLElement *routerNode = xmlParser::GetDevice("Router", deviceId, configFileName);
    if (routerNode == NULL)
    {
        ev << "No RIPng configuration found for this device" << routerText << endl;
        return true;
    }

    // interfaces config
    cXMLElement *interface;
    RIPng::Interface *ripngInterface;
    std::string RIPngInterfaceStatus;
    std::string RIPngInterfacePassiveStatus;
    std::string RIPngInterfaceSplitHorizon;

      //get first router's interface
      interface = xmlParser::GetInterface(NULL, routerNode);
      while (interface != NULL)
      {// process all interfaces in config file
          const char *interfaceName = interface->getAttribute("name");
          RIPngInterfaceStatus = getInterfaceRIPngStatus(interface);
          if (RIPngInterfaceStatus == "enable")
          {
              ripngInterface = enableRIPngOnInterface(interfaceName);
              // add prefixes from int to the RIPng routing table
              addPrefixesFromInterfaceToRT(interface);

              RIPngInterfacePassiveStatus = getRIPngInterfacePassiveStatus(interface);
              if (RIPngInterfacePassiveStatus == "enable")
              {// set the interface as passive (interface is "active" by default)
                  setInterfacePassiveStatus(ripngInterface, true);
              }

              RIPngInterfaceSplitHorizon = getRIPngInterfacePassiveStatus(interface);
              if (RIPngInterfaceSplitHorizon == "disable")
              {// disable Split Horizon on the interface (Split Horizon is enabled by default)
                  setInterfaceSplitHorizon(ripngInterface, false);
              }
          }

          // process next interface
          interface = xmlParser::GetInterface(interface, NULL);
      }

    return true;
}

const char *RIPngRouting::getInterfaceRIPngStatus(cXMLElement *interface)
{
    if (interface == NULL)
        error("Error reading RIPng interface status, interface can't be NULL.");

    // get interface RIPng status
    const char *cRIPngStatus;
    cXMLElement* RIPngStatus = interface->getElementByPath("RIPngInterfaceStatus");
    if (RIPngStatus == NULL)
        // RIPngInterface status is not set
        return "disable";

    cRIPngStatus = RIPngStatus->getNodeValue();
    if (cRIPngStatus == NULL)
        // RIPngInterface status is set, but with no value
        return "disable";

    return cRIPngStatus;
}

const char *RIPngRouting::getRIPngInterfacePassiveStatus(cXMLElement *interface)
{
    if (interface == NULL)
        error("Error reading RIPng passive interface status, interface can't be NULL.");

    const char *cPassiveStatus;
    cXMLElement* passiveStatus = interface->getElementByPath("RIPngPassiveInterface");
    if (passiveStatus == NULL)
        // RIPngPassiveInterface status is not set
        return "disable";

    cPassiveStatus = passiveStatus->getNodeValue();
    if (cPassiveStatus == NULL)
        // RIPngPassiveInterface status is set, but with no value
        return "disable";

    return cPassiveStatus;
}

const char *RIPngRouting::getRIPngInterfaceSplitHorizon(cXMLElement *interface)
{
    if (interface == NULL)
        error("Error reading RIPng Split Horizon on interface, interface can't be NULL.");

    const char *cSplitHorizon;
    cXMLElement* SplitHorizon = interface->getElementByPath("RIPngSplitHorizon");
    if (SplitHorizon == NULL)
        // RIPngSplitHorizon status is not set
        return "enable";

    cSplitHorizon = SplitHorizon->getNodeValue();
    if (cSplitHorizon == NULL)
        // RIPngSplitHorizon status is set, but with no value
        return "enable";

    return cSplitHorizon;
}

RIPng::Interface *RIPngRouting::enableRIPngOnInterface(const char *interfaceName)
{
    ev << "Enabling RIPng on " << interfaceName << routerText << endl;

    InterfaceEntry *interface = ift->getInterfaceByName(interfaceName);
    int interfaceId = interface->getInterfaceId();
    RIPng::Interface *RIPngInterface = new RIPng::Interface(interfaceId);
    // add interface to local RIPng interface table
    addEnabledInterface(RIPngInterface);

    return RIPngInterface;
}

void RIPngRouting::setInterfacePassiveStatus(RIPng::Interface *RIPngInterface, bool status)
{
    if (status == true)
        ev << "Setting RIPng passive interface (interface id: " << RIPngInterface->getId() << ")." << routerText << endl;

    if (status)
        RIPngInterface->enablePassive();
    else
        RIPngInterface->disablePassive();
}

void RIPngRouting::setInterfaceSplitHorizon(RIPng::Interface *RIPngInterface, bool status)
{
    if (status)
        RIPngInterface->enableSplitHorizon();
    else
        RIPngInterface->disableSplitHorizon();
}

void RIPngRouting::addPrefixesFromInterfaceToRT(cXMLElement *interface)
{
    const char *interfaceName = interface->getAttribute("name");
    InterfaceEntry *interfaceEntry = ift->getInterfaceByName(interfaceName);
    int interfaceId = interfaceEntry->getInterfaceId();

    RIPng::RoutingTableEntry *route;

    // process next interface
    cXMLElement *eIpv6address;
    std::string sIpv6address;
    IPv6Address ipv6address;
    int prefixLen;
        // get first ipv6 address
        eIpv6address = xmlParser::GetIPv6Address(NULL, interface);
        while (eIpv6address != NULL)
        {// process all ipv6 addresses on the interface
            sIpv6address = eIpv6address->getNodeValue();

            // check if it's a valid IPv6 address string with prefix and get prefix
            if (!ipv6address.tryParseAddrWithPrefix(sIpv6address.c_str(), prefixLen))
            {
               throw cRuntimeError("Unable to set IPv6 network address %s for static route", sIpv6address.c_str());
            }

            if (!ipv6address.isLinkLocal() && !ipv6address.isMulticast())
            {
                // make directly connected route
                route = new RIPng::RoutingTableEntry(ipv6address.getPrefix(prefixLen), prefixLen);
                route->setInterfaceId(interfaceId);
                route->setNextHop(IPv6Address::UNSPECIFIED_ADDRESS);  // means directly connected network
                route->setMetric(connNetworkMetric);
                addRoutingTableEntry(route, false);

                // directly connected routes do not need a route timer
                // directly connected routes are added to the "global routing table" by deviceConfigurator
            }

            eIpv6address = xmlParser::GetIPv6Address(eIpv6address, NULL);
        }
}

UDPSocket *RIPngRouting::createAndSetSocketForInt(RIPng::Interface* interface)
{
    UDPSocket *socket = new UDPSocket();
    socket->setOutputGate(gate("udpOut"));
    socket->bind(ift->getInterfaceById(interface->getId())->ipv6Data()->getLinkLocalAddress(), RIPngPort);

    int timeToLive = par("timeToLive");
    if (timeToLive != -1)
        socket->setTimeToLive(timeToLive);

    return socket;
}

void RIPngRouting::handleMessage(cMessage *msg)
{
    if (msg->isSelfMessage())
    {// timers
        handleTimer(check_and_cast<RIPngTimer*> (msg));
    }
    else if (msg->getKind() == UDP_I_DATA)
    {// process incoming message
        handleMessage(check_and_cast<RIPngMessage*> (msg));
    }
    else if (msg->getKind() == UDP_I_ERROR)
    {
        ev << "Ignoring UDP error report" << endl;
        delete msg;
    }
    else
    {
        error("Unrecognized message (%s)%s", msg->getClassName(), msg->getName());
    }

    if (ev.isGUI())
    {
        char buf[40];
        sprintf(buf, "rcvd: %d pks\nsent: %d pks", numReceived, numSent);
        getDisplayString().setTagArg("t", 0, buf);
    }
}

RIPngMessage *RIPngRouting::createMessage()
{
    char msgName[32] = "RIPngMessage";

    RIPngMessage *msg = new RIPngMessage(msgName);
    return msg;
}

void RIPngRouting::sendMessage(RIPngMessage *msg, IPv6Address &addr, int port, unsigned long enabledInterfaceIndex, bool globalSourceAddress)
{
    ASSERT(enabledInterfaceIndex < getEnabledInterfacesCount());
    int outInterface = getEnabledInterface(enabledInterfaceIndex)->getId();
    if (globalSourceAddress)
    {// "uses" global-unicast address as the source address
        globalSocket.sendTo(msg, addr, port, outInterface);
    }
    else
    {
        sockets[enabledInterfaceIndex]->sendTo(msg, addr, port, outInterface);
    }

    numSent++;
}

void RIPngRouting::addRoutingTableEntryToGlobalRT(RIPng::RoutingTableEntry* entry)
{
    // TODO: check if the route does not exist than compare their AD metrics
    RIPng::RoutingTableEntry *newEntry = new RIPng::RoutingTableEntry(*entry);
    rt->addRoutingProtocolRoute(newEntry);
}

void RIPngRouting::removeRoutingTableEntryFromGlobalRT(RIPng::RoutingTableEntry* entry)
{
    unsigned long i, numRoutes = rt->getNumRoutes();
    for (i = 0; i < numRoutes; i++)
    {
        IPv6Route *testEntry = rt->getRoute(i);
        if (testEntry->getDestPrefix() == entry->getDestPrefix())
        { // the corresponding route from "global routing table" to the entry from "RIPng routing table"
            RIPng::RoutingTableEntry* ripngEntry = dynamic_cast<RIPng::RoutingTableEntry*>(testEntry);
            if (ripngEntry != NULL)
            { // route is from RIPng process
                rt->removeRoute(testEntry);
                break;
            }
        }
    }
}

//-- INPUT PROCESSING
void RIPngRouting::handleMessage(RIPngMessage *msg)
{
    EV << "Received packet: " << UDPSocket::getReceivedPacketInfo(msg) << endl;

    int version = msg->getVersion();
    if (version != 1)
        EV << "This implementation of RIPng does not support version '" << version << "' of this protocol." << endl;

    int command = msg->getCommand();
    if (command == RIPngRequest)
    {
        handleRequest(msg);
    }
    else if (command == RIPngResponse)
    {
        handleResponse(msg);
    }

    delete msg;

    numReceived++;
}

//-- RESPONSE PROCESSING
void RIPngRouting::handleResponse(RIPngMessage *response)
{
    if (!checkMessageValidity(response))
        return;

    UDPDataIndication *controlInfo = check_and_cast<UDPDataIndication *>(response->getControlInfo());
    int interfaceId = controlInfo->getInterfaceId();
    IPv6Address sourceAddr = controlInfo->getSrcAddr().get6();

    processRTEs(response, interfaceId, sourceAddr);
}

bool RIPngRouting::checkMessageValidity(RIPngMessage *response)
{
    UDPDataIndication *controlInfo = check_and_cast<UDPDataIndication *>(response->getControlInfo());

    // is from RIPng port
    if (controlInfo->getSrcPort() != RIPngPort)
        return false;

    // source addr. is link-local
    IPv6Address sourceAddr = controlInfo->getSrcAddr().get6();
    if (!sourceAddr.isLinkLocal())
        return false;

    // source addr. is not from this device
    if (rt->isLocalAddress(sourceAddr))
        return false;

    // hop-count is 255
    if (controlInfo->getTtl() != 255)
        return false;

    return true;
}

void RIPngRouting::processRTEs(RIPngMessage *response, int sourceIntId, IPv6Address &sourceAddr)
{
    unsigned int rtesSize = response->getRtesArraySize();
    RIPngRTE rte;

    // if startRouteDeletionProcess() was called we dont want to call sendTriggeredUpdateMessage() in that function,
    // so we'll call sendTriggeredUpdateMessage() after "all startRouteDeletionProcess()"
    bBlockTriggeredUpdateMessage = true;

    //Process every RTE
    for (unsigned int i = 0; i < rtesSize; i++)
    {
        rte = response->getRtes(i);
        processRTE(rte, sourceIntId, sourceAddr);
    }

    if (bSendTriggeredUpdateMessage)
    {
        sendTriggeredUpdateMessage();
    }

    bBlockTriggeredUpdateMessage = false;
}

void RIPngRouting::processRTE(RIPngRTE &rte, int sourceIntId, IPv6Address &sourceAddr)
{
    checkAndLogRTE(rte, sourceAddr);

    IPv6Address prefix = rte.getIPv6Prefix();
    int prefixLen = rte.getPrefixLen();
    // ?? prefix = prefix.getPrefix(prefixLen);

    // Check if a route with the prefix exists
    RIPng::RoutingTableEntry *routingTableEntry = getRoutingTableEntry(prefix);

    if (routingTableEntry != NULL)
    {// Update Routing Table Entry
        updateRoutingTableEntry(routingTableEntry, rte, sourceIntId, sourceAddr);
    }
    else
    {// Create and add new Routing Table Entry
        int metric = rte.getMetric();
        if (metric < infinityMetric)
            ++metric;

        RIPng::RoutingTableEntry *route = new RIPng::RoutingTableEntry(prefix, prefixLen);
        route->setInterfaceId(sourceIntId);
        route->setNextHop(sourceAddr);
        route->setMetric(metric);
        route->setChangeFlag();

        addRoutingTableEntry(route);
        addRoutingTableEntryToGlobalRT(route);

        bSendTriggeredUpdateMessage = true;
    }
}

bool RIPngRouting::checkAndLogRTE(RIPngRTE &rte, IPv6Address &sourceAddr)
{
    // prefix is valid (not multicast, link-local)
    // prefix len. is valid (0-128)
    // metric is valid (0-16)
    if (!rte.getIPv6Prefix().isGlobal() ||
         rte.getPrefixLen() > 128 ||
         rte.getMetric() > 16)
    {
        EV << "Bad RTE from: " << sourceAddr << endl;
        return false;
    }

    return true;
}

void RIPngRouting::updateRoutingTableEntry(RIPng::RoutingTableEntry *routingTableEntry, RIPngRTE &rte, int sourceIntId, IPv6Address &sourceAddr)
{
    const IPv6Address &nextHop = routingTableEntry->getNextHop();
    int newMetric = rte.getMetric();
    if (newMetric < infinityMetric)
        ++newMetric;

    if (nextHop == sourceAddr)
    {// RTE is from the same router
        RIPngTimer *routeTimer = routingTableEntry->getTimer();
        resetTimer(routeTimer, routeTimeout);

        if (newMetric != routingTableEntry->getMetric())
        {
            if (newMetric == infinityMetric)
            {
                // bSendTriggeredUpdateMessage is set in startRouteDeletionProcess() function
                cancelTimer(routeTimer);
                startRouteDeletionProcess(routingTableEntry);
            }
            else
            {
                // route is from the same router, always set the metric and the change flag
                routingTableEntry->setMetric(newMetric);
                routingTableEntry->setChangeFlag();

                bSendTriggeredUpdateMessage = true;
            }
        }
    }
    else
    {
        if (newMetric < routingTableEntry->getMetric())
        {
            routingTableEntry->setMetric(newMetric);
            routingTableEntry->setNextHop(sourceAddr);
            resetTimer(routingTableEntry->getTimer(), routeTimeout);
            // stop garbage collection timer if is running
            RIPngTimer *GCTimer = routingTableEntry->getGCTimer();
            ASSERT(GCTimer != NULL);
            if (GCTimer->isScheduled())
            {
                cancelTimer(GCTimer);
                // route was deleted from "global routing table" in the route deletion process, add it again
                addRoutingTableEntryToGlobalRT(routingTableEntry);
            }
            routingTableEntry->setChangeFlag();

            bSendTriggeredUpdateMessage = true;
        }
        // TODO: OPTIMALIZATION: else if (routeTimer is nearly expired for the routingTableEntry)
    }
}

//-- REQUEST PROCESSING
void RIPngRouting::handleRequest(RIPngMessage *request)
{
    UDPDataIndication *controlInfo = check_and_cast<UDPDataIndication *>(request->getControlInfo());
    IPv6Address sourceAddr = controlInfo->getSrcAddr().get6();
    int sourcePort = controlInfo->getSrcPort();
    int sourceInterfaceId = controlInfo->getInterfaceId();

    unsigned int rteNum = request->getRtesArraySize();
    std::vector<RIPngRTE> responseRtes;

    if (rteNum == 1)
    {// could be a request for all routes
        RIPngRTE &rte = request->getRtes(0);
        if (rte.getIPv6Prefix() == IPv6Address::UNSPECIFIED_ADDRESS &&
            rte.getPrefixLen() == 0 &&
            rte.getMetric() == infinityMetric &&
            rte.getRouteTag() == 0)
        {
            getRTEs(responseRtes, sourceInterfaceId);
        }
    }
    else
    {
        RIPng::RoutingTableEntry *routingTableEntry;
        for (unsigned int i = 0; i < rteNum; i++)
        {
            RIPngRTE rte = request->getRtes(i);
            routingTableEntry = getRoutingTableEntry(rte.getIPv6Prefix());

            if (routingTableEntry != NULL)
            {// match for the requested rte
                responseRtes.push_back(makeRTEFromRoutingTableEntry(routingTableEntry));
            }
            else
            {
                rte.setMetric(infinityMetric);
                responseRtes.push_back(rte);
            }
        }
    }

    int size = responseRtes.size();
    //if (size <= 0)
        //break;

    RIPngMessage *response = createMessage();
    response->setCommand(RIPngResponse);
    response->setRtesArraySize(size);
    // set RTEs to response
    for(int j = 0; j < size; j++)
        response->setRtes(j, responseRtes[j]);

    IPv6Address destAddr = controlInfo->getDestAddr().get6();
    int intInd = getEnabledInterfaceIndexById(sourceInterfaceId);
    if (destAddr == RIPngAddress && sourcePort == RIPngPort)
        sendMessage(response, sourceAddr, sourcePort, intInd, false);
    else
        sendMessage(response, sourceAddr, sourcePort, intInd, true);
}

//-- TIMEOUTS
RIPngTimer *RIPngRouting::createTimer(int timerKind)
{
    RIPngTimer *timer = new RIPngTimer();
    timer->setTimerKind(timerKind);

    return timer;
}

RIPngTimer *RIPngRouting::createAndStartTimer(int timerKind, simtime_t timerLen)
{
    RIPngTimer *timer = createTimer(timerKind);

    scheduleAt(simTime() + timerLen, timer);

    return timer;
}

void RIPngRouting::resetTimer(RIPngTimer *timer, simtime_t timerLen)
{
    ASSERT(timer != NULL);
    if (timer->isScheduled())
        cancelEvent(timer);

    scheduleAt(simTime() + timerLen, timer);
}

void RIPngRouting::cancelTimer(RIPngTimer *timer)
{
    if (timer != NULL)
    {
        if (timer->isScheduled())
            cancelEvent(timer);
    }
}

void RIPngRouting::deleteTimer(RIPngTimer *timer)
{
    if (timer != NULL)
    {
        if (timer->isScheduled())
            cancelEvent(timer);

        delete timer;
    }
}

void RIPngRouting::handleTimer(RIPngTimer *msg)
{
    int type = msg->getTimerKind();

    switch (type)
    {
        case GENERAL_UPDATE :
            handleRegularUpdateTimer();
            break;
        case TRIGGERED_UPDATE :
            handleTriggeredUpdateTimer();
            break;
        case ROUTE_TIMEOUT :
            startRouteDeletionProcess(msg);
            break;
        case ROUTE_GARBAGE_COLECTION_TIMEOUT :
            deleteRoute(msg);
            break;
        default:
            break;
    }
}

void RIPngRouting::handleRegularUpdateTimer()
{
     // send regular update message
    sendRegularUpdateMessage();
    // plan next regular update
    resetTimer(regularUpdateTimer, regularUpdateTimeout);
}

void RIPngRouting::handleTriggeredUpdateTimer()
{
    if (bSendTriggeredUpdateMessage)
        sendTriggeredUpdateMessage();
}

void RIPngRouting::startRouteDeletionProcess(RIPngTimer *timer)
{
    IPv6Address &addr = timer->getIPv6Prefix();
    RIPng::RoutingTableEntry *routingTableEntry = getRoutingTableEntry(addr);

    startRouteDeletionProcess(routingTableEntry);
}

void RIPngRouting::startRouteDeletionProcess(RIPng::RoutingTableEntry *routingTableEntry)
{
    ASSERT(routingTableEntry != NULL);
    routingTableEntry->setMetric(infinityMetric);
    routingTableEntry->setChangeFlag();

    RIPngTimer *GCTimer = routingTableEntry->getGCTimer();
    // (re)set the timer
    resetTimer(GCTimer, routeGarbageCollectionTimeout);

    removeRoutingTableEntryFromGlobalRT(routingTableEntry);

    if (bBlockTriggeredUpdateMessage)
    // if response is processing and a rte with the metric of 16 was received, wait until all the RTEs are proccesed
        bSendTriggeredUpdateMessage = true;
    else
        sendDelayedTriggeredUpdateMessage();
}

void RIPngRouting::deleteRoute(RIPngTimer *timer)
{
    IPv6Address &addr = timer->getIPv6Prefix();

    removeRoutingTableEntry(addr);
}

//-- OUTPUT PROCESSING
void RIPngRouting::sendRegularUpdateMessage()
{
    int numInterfaces = getEnabledInterfacesCount();
    RIPng::Interface *interface;

    // sent update on every interface, where is enabled RIPng and that interface is not passive
    for (int i = 0; i < numInterfaces; i++)
    {
        interface = getEnabledInterface(i);
        if (interface->isPassive())
            // do not send updates out of the passive interface
            continue;

        RIPngMessage *msg = makeUpdateMessageForInterface(interface, false);
        if (msg != NULL)
        // no rtes to send
            sendMessage(msg, RIPngAddress, RIPngPort, i, false);
    }

    //reset Route Change Flags
    clearRouteChangeFlags();

    bSendTriggeredUpdateMessage = false;
}

void RIPngRouting::sendDelayedTriggeredUpdateMessage()
{
    // we are using delayed triggered update message because if one route went down,
    // more than one route (with the next hop using that unavailable route) timeout can expire in the same time -
    // this way we prevent to send multiple triggered update messages containing just one route update
    // for this we can use triggered update timer

    bSendTriggeredUpdateMessage = true;

    if (!triggeredUpdateTimer->isScheduled())
    {
        // random timeout from 1 to 5 seconds
        simtime_t triggeredUpdateTimeout = uniform(1, 5);
        resetTimer(triggeredUpdateTimer, triggeredUpdateTimeout);
    }
    // else - do nothing, a triggered update is already planned
}

void RIPngRouting::sendTriggeredUpdateMessage()
{
    if (triggeredUpdateTimer->isScheduled())
    // method will be called again when regularUpdateTimer expired
        return;

   // random timeout from 1 to 5 seconds
   simtime_t triggeredUpdateTimeout = uniform(1, 5);
   if (regularUpdateTimer->getArrivalTime() - simTime() < triggeredUpdateTimeout)  // regulardUpdateTimer can't be NULL
   // regular update message is going to be sent soon
       return;

    int numInterfaces = getEnabledInterfacesCount();
    RIPng::Interface *interface;

    // sent update on every interface, where is enabled RIPng and that interface is not passive
    for (int i = 0; i < numInterfaces; i++)
    {
        interface = getEnabledInterface(i);
        if (interface->isPassive())
            // do not send updates out of the passive interface
            continue;

        RIPngMessage *msg = makeUpdateMessageForInterface(interface, true);
        if (msg != NULL)
        // no rtes to send
            sendMessage(msg, RIPngAddress, RIPngPort, i, false);
    }

    //reset Route Change Flags
    clearRouteChangeFlags();

    bSendTriggeredUpdateMessage = false;

    resetTimer(triggeredUpdateTimer, uniform(1, 5));
}

RIPngMessage *RIPngRouting::makeUpdateMessageForInterface(RIPng::Interface *interface, bool changed)
{
    int size;
    std::vector<RIPngRTE> rtes;
    int interfaceId;

    // if split horizon is disabled on the interface get all the RTEs (set interfaceId to -1)
    if (interface->isSplitHorizon())
        interfaceId = interface->getId();
    else
        interfaceId = -1;

    if (changed)
        getChangedRTEs(rtes, interfaceId);
    else
        getRTEs(rtes, interfaceId);

    size = rtes.size();
    if (size <= 0)
        // there's no RTE to send from this interface (no message will be created)
        return NULL;

    RIPngMessage *msg = createMessage();
    msg->setCommand(RIPngResponse);
    msg->setRtesArraySize(size);
    // set RTEs to response
    for(int j = 0; j < size; j++)
        msg->setRtes(j, rtes[j]);

    return msg;
}

void RIPngRouting::getRTEs(std::vector<RIPngRTE> &rtes, int interfaceId)
{
    RIPng::RoutingTableEntry *routingTableEntry;
    RoutingTableIt it;

    bool splitHorizon = (interfaceId == -1) ? false : true;

    for (it = routingTable.begin(); it != routingTable.end(); it++)
    {
        routingTableEntry = (*it).second;
        if (splitHorizon && (routingTableEntry->getInterfaceId() == interfaceId))
            // split horizon
            continue;

        RIPngRTE rte = makeRTEFromRoutingTableEntry(routingTableEntry);
        rtes.push_back(rte);
    }
}

void RIPngRouting::getChangedRTEs(std::vector<RIPngRTE> &rtes, int interfaceId)
{
    RIPng::RoutingTableEntry *routingTableEntry;
    RoutingTableIt it;

    bool splitHorizon = (interfaceId == -1) ? false : true;

    for (it = routingTable.begin(); it != routingTable.end(); it++)
    {
        routingTableEntry = (*it).second;
        if (splitHorizon && (routingTableEntry->getInterfaceId() == interfaceId))
            // split horizon
            continue;

        if (routingTableEntry->isChangeFlagSet())
        {
            RIPngRTE rte = makeRTEFromRoutingTableEntry(routingTableEntry);
            rtes.push_back(rte);
        }
    }
}

RIPngRTE RIPngRouting::makeRTEFromRoutingTableEntry(RIPng::RoutingTableEntry *routingTableEntry)
{
    RIPngRTE rte;
    // create RTE for message to neighbor
    rte.setPrefixLen(routingTableEntry->getPrefixLength());
    rte.setIPv6Prefix(routingTableEntry->getDestPrefix());
    rte.setMetric(routingTableEntry->getMetric());
    rte.setRouteTag(routingTableEntry->getRouteTag());

    return rte;
}

void RIPngRouting::clearRouteChangeFlags()
{
    RIPng::RoutingTableEntry *routingTableEntry;
    RoutingTableIt it;

    for (it = routingTable.begin(); it != routingTable.end(); it++)
    {
        routingTableEntry = (*it).second;
        routingTableEntry->clearChangeFlag();
    }
}

void RIPngRouting::sendAllRoutesRequest()
{
    int numInterfaces = getEnabledInterfacesCount();
    RIPng::Interface *interface;

    RIPngRTE rte = RIPngRTE();
    rte.setIPv6Prefix(IPv6Address()); // IPv6 Address ::0
    rte.setMetric(16);
    rte.setPrefixLen(0);
    rte.setRouteTag(0);

    // sent update on every interface, where is enabled RIPng and that interface is not passive
    for (int i = 0; i < numInterfaces; i++)
    {
        interface = getEnabledInterface(i);
        if (interface->isPassive())
            // do not send request out of the passive interface
            continue;

        RIPngMessage *msg = createMessage();

        msg->setCommand(RIPngRequest);
        msg->setRtesArraySize(1);
        msg->setRtes(0, rte);

        sendMessage(msg, RIPngAddress, RIPngPort, i, false);
    }
}
