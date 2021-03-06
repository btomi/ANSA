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
* @file RIPngInterface.cc
* @author Jiri Trhlik (mailto:), Vladimir Vesely (mailto:ivesely@fit.vutbr.cz)
* @brief
* @detail
*/

#ifndef RIPNGROUTING_H_
#define RIPNGROUTING_H_

#include <omnetpp.h>
#include "UDPSocket.h"
#include "IPv6Address.h"
#include "ANSARoutingTable6.h"
#include "ANSARoutingTable6Access.h"
#include "IInterfaceTable.h"
#include "InterfaceTableAccess.h"
#include "NotificationBoard.h"

#include "RIPngInterface.h"
#include "RIPngRoutingTableEntry.h"
#include "xmlParser.h"

#include "UDPControlInfo_m.h"
#include "RIPngMessage_m.h"
#include "RIPngTimer_m.h"

/**
 *  Represents routing protocol RIPng. RIPng uses UDP and communicates
 *  using UDPSocket.
 */
class RIPngRouting : public cSimpleModule, protected INotifiable
{
  public:
    RIPngRouting();
    virtual ~RIPngRouting();

  protected:
    const char  *deviceId;   ///< Id of the device which contains this routing process.
    std::string hostName;    ///< Device name from the network topology.
    std::string routerText;  ///< Text with hostName for ev printing.

    int         connNetworkMetric;
    int         infinityMetric;
    simtime_t   routeTimeout;
    simtime_t   routeGarbageCollectionTimeout;
    simtime_t   regularUpdateTimeout;
    IPv6Address RIPngAddress;
    int         RIPngPort;
    int         adminDist;

    RIPngTimer *regularUpdateTimer;
    RIPngTimer *triggeredUpdateTimer;

    // statistics
    int numRoutes;

    bool bSendTriggeredUpdateMessage;
    bool bBlockTriggeredUpdateMessage;

    IInterfaceTable*                                ift;                ///< Provides access to the interface table.
    ANSARoutingTable6*                              rt;                 ///< Provides access to the IPv6 routing table.
    NotificationBoard*                              nb;                 ///< Provides access to the notification board
    std::vector<RIPng::Interface*>                  enabledInterfaces;  ///< Interfaces which has allowed RIPng
    std::vector<UDPSocket*>                         sockets;            ///< UDP Socket for every RIPng interface
    UDPSocket                                       globalSocket;       ///< Socket for "send" messages with global unicast address as a source

    typedef std::vector<RIPng::RoutingTableEntry*> RoutingTable;
    RoutingTable routingTable;                                          ///< The RIPng routing table.

  public:
    int getConnNetworkMetric() { return connNetworkMetric; }
    int getInfinityMetric() { return infinityMetric; }
    simtime_t getRouteTimeout() { return routeTimeout; }
    simtime_t getRouteGarbageCollectionTimeout() { return routeGarbageCollectionTimeout; }
    simtime_t getRegularUpdateTimeout() { return regularUpdateTimeout; }
    IPv6Address getRIPngAddress() { return RIPngAddress; }
    int getRIPngPort() { return RIPngPort; }
    int getAdminDist() { return adminDist; }

    //-- RIPNG ROUTING TABLE METHODS
    typedef RoutingTable::iterator RoutingTableIt;
    unsigned long                   getRoutingTableEntryCount() const { return routingTable.size(); }
    RIPng::RoutingTableEntry*       getRoutingTableEntry(const IPv6Address &prefix, int prefixLength);
    void                            addRoutingTableEntry(RIPng::RoutingTableEntry* entry, bool createTimers = true);
    void                            removeRoutingTableEntry(IPv6Address &prefix, int prefixLength);
    void                            removeRoutingTableEntry(RoutingTableIt it);

    /**
     * Update routing table entry from rte.
     * @param routingTableEntry [in] pointer to the routing table entry (in the RIPng routingTable) to update
     * @param rte [in] rte from the update message
     * @param sourceIntId [in] interface where was (the update message with) the rte received
     * @param sourceAddr [in] source of the received rte
     */
    void                            updateRoutingTableEntry(RIPng::RoutingTableEntry *routingTableEntry, RIPngRTE &rte, int sourceIntId, IPv6Address &sourceAddr);
    void                            removeAllRoutingTableEntries();

    //-- RIPNG INTERFACES METHODS
    unsigned long            getEnabledInterfacesCount() const  { return enabledInterfaces.size(); }
    RIPng::Interface*        getEnabledInterface(unsigned long i)  { return enabledInterfaces[i]; }
    const RIPng::Interface*  getEnabledInterface(unsigned long i) const  { return enabledInterfaces[i]; }
    int                      getEnabledInterfaceIndexById(int id);
    void                     addEnabledInterface(RIPng::Interface *interface);
    void                     removeEnabledInterface(unsigned long i);
    void                     removeAllEnabledInterfaces();

  public:
    void showRoutingTable();

  protected:
    //-- GENERAL METHODS
    /**
     *  Creates a RIPng message with no data.
     *  @return pointer to the new RIPng message.
     */
    RIPngMessage *createMessage();

    /**
     * Creates and sets UDPSocket
     * @param interface [in] interface for which is constructed socket
     * @return pointer to the newly created socket
     */
    UDPSocket *createAndSetSocketForInt(RIPng::Interface* interface);

  public:
    /**
     * Enables RIPng on the interface (creates new RIPng::Interface and adds it to the "RIPng interface table").
     * @param interfaceName [in] The interface name on which to enable RIPng.
     * @return created interface
     */
    RIPng::Interface *enableRIPngOnInterface(const char *interfaceName);
    void setInterfacePassiveStatus(RIPng::Interface *RIPngInterface, bool status);
    void setInterfaceSplitHorizon(RIPng::Interface *RIPngInterface, bool status);
    void setInterfacePoisonReverse(RIPng::Interface *RIPngInterface, bool status);

  protected:
    /**
     *  Adds RIPng Routing Table Entry to the "global outing table", setting the new route.
     *  The function also checks if the route exists and if so, the administrative
     *  distances are compared.
     *  @param entry [in] Routing Table Entry
     */
    void addRoutingTableEntryToGlobalRT(RIPng::RoutingTableEntry* entry);

    /**
     * Removes RIPng Routing Table Entry from the "global routing table".
     * Before removing the function checks if the RIPng route exists in the "global routing table"
     * (because another routing protocol could add the same route with lower AD).
     * @param entry [in] Routing Table Entry
     */
    void removeRoutingTableEntryFromGlobalRT(RIPng::RoutingTableEntry* entry);

    //-- OUTPUT PROCESSING
    /**
     * Creates and sends regular update messages out of every RIPng (non-passive) interface.
     * @param addr [in] destination address of the message
     * @param port [in] destination port of the message
     */
    void sendRegularUpdateMessage();

    /**
     *  Creates and sends triggered update messages -
     *  only routes with the change flag set are included in the message.
     *  The message is sent to the RIPng mutlicast address.
     */
    void sendTriggeredUpdateMessage();

    void sendDelayedTriggeredUpdateMessage();

    /**
     * Make update message for RIPng interface (using Split Horizon if is enabled on the interface).
     * @param interface [in] interface from enabledInterfaces
     * @param changed [in] if only routes with the changed flag set should be included in the message
     * @return pointer to the new update message, if there is no rtes to send returns NULL (e.g., because of Split Horizon)
     */
    RIPngMessage *makeUpdateMessageForInterface(RIPng::Interface *interface, bool changed);

    /**
     *  This method is used by sendRegularUpdateMessage() and sendTriggeredUpdateMessage()
     *  Link-local address of the interface is used as the source address in the message.
     *  @param msg [in] message to be send
     *  @param addr [in] destination address
     *  @param port [in] destination port
     *  @param enabledInterfaceIndex [in] from which RIPng interface should be message sent
     *  @param globalSourceAddress [in] a global-unicast address should be used as the source address of the message (or: source address is left for lower layers)
     *  @see sendRegularUpdateMessage()
     *  @see sendTriggeredUpdateMessage()
     */
    void sendMessage(RIPngMessage *msg, IPv6Address &addr, int port, unsigned long enabledInterfaceIndex, bool globalSourceAddress);

    /**
     *  Sends request for all routes to RIPng address and port
     */
    void sendAllRoutesRequest();

    /**
     *  Clear Route Change Flag for all Routing Table Entries.
     */
    void clearRouteChangeFlags();

    /**
     * Fills in the RIPngRTE vector with Routing Table Entries from the RIPng routing table
     * this method is used for creating RIPng (update) messages
     * @param rtes [out] vector to be filled in
     * @param interfaceId [in] if this parameter is not NULL, then split horizon (with poison reverse) is used based on the interface parameters
     *                         otherwise all RTEs are included
     * @param onlyChanged [in] if this parameter is true, then only routes with routeChangeFlag are included
     */
    void getRTEs(std::vector<RIPngRTE> &rtes, RIPng::Interface *interface, bool onlyChanged = false);

    /**
     *  Creates and returns new RTE base on the routingTableEntry
     *  @param routingTableEntry [in] Routing Table Entry
     *  @return created RIPngRTE object
     */
    RIPngRTE makeRTEFromRoutingTableEntry(RIPng::RoutingTableEntry *routingTableEntry);

    //-- INPUT PROCESSING
    /**
     *  Handle a RIPng message
     *  @param msg [in] message to process
     */
    void handleMessage(RIPngMessage *msg);

    //-- RESPONSE PROCESSING
    /**
     * Handle a RIPng response message
     * @param response [in] a response to process
     * @param srcInd [in] interface ID on which the response was received
     * @param srcAddr [in] source address of the response
     * @see processMessage()
     */
    void handleResponse(RIPngMessage *response, int srcInt, IPv6Address &srcAddr);

    /**
     *  Checks for response message validity as is described in RFC 2080
     *  @param responese [in] response message to check
     *  @return true if the response is valid, false otherwise
     */
    bool checkMessageValidity(RIPngMessage *response);

    /**
     *  Process all the RTEs in the response message
     *  @param response [in] the response message
     *  @param sourceIntId [in] interface ID from which the RTEs have been received
     *  @param sourceAddr [in] an IPv6 address of the source of the RTEs
     */
    void processRTEs(RIPngMessage *response, int sourceIntId, IPv6Address &sourceAddr);

    /**
     *  Process single RTE from the response message
     *  @param rte [in] the RTE to process
     *  @param sourceIntId [in] interface ID from which the RTE has been received
     *  @param sourceAddr [in] an IPv6 address of the source of the RTE
     */
    void processRTE(RIPngRTE &rte, int sourceIntId, IPv6Address &sourceAddr);

    /**
     *  Checks for the rte validity as is described in RFC 2080
     *  @param rte [in] rte to check
     *  @param sourceAddr [in] rte originator
     *  @return true if the rte is valid, false otherwise
     */
    bool checkAndLogRTE(RIPngRTE &rte, IPv6Address &sourceAddr);

    //-- REQUEST PROCESSING
    /**
     * Handle a RIPng request message
     * @param response [in] a request to process
     * @param srcPort [in] source port of the request
     * @param srcAddr [in] source address of the request
     * @param destAddr [in] destination address of the request
     * @param ripngIntInd [in] ripng interface index of the interface on which the request was received
     * @see processMessage()
     */
    void handleRequest(RIPngMessage *request, int srcPort, IPv6Address &srcAddr, IPv6Address &destAddr, unsigned long ripngIntInd);

    //-- TIMEOUTS
    /**
     * Creates new timer
     * @param timerKind [in] type of the timer @see RIPngTimer
     * @return Created RIPngTimer timer
     */
    RIPngTimer *createTimer(int timerKind);

    /**
     * Creates and starts new timer
     * @param timerKind [in] type of the timer @see RIPngTimer
     * @param timerLen [in] timer duration
     * @return Created RIPngTimer timer
     */
    RIPngTimer *createAndStartTimer(int timerKind, simtime_t timerLen);

    /**
     * Starts or resets timer
     * @param timer [in] timer
     * @param timerLen [in] timer duration
     */
    void resetTimer(RIPngTimer *timer, simtime_t timerLen);

    /**
     * Cancels timer
     * @param timer [in] timer
     */
    void cancelTimer(RIPngTimer *timer);

    /**
     * Deletes timer (timer will be canceled if is running)
     * @param timer [in] timer
     */
    void deleteTimer(RIPngTimer *timer);

    /**
     *  Handles self-messages (timers)
     *  @param msg [in] RIPngTimer message
     */
    void handleTimer(RIPngTimer *msg);

    /**
     *  Handles regular update timer, sends updates messages and schedules next reg. update
     */
    void handleRegularUpdateTimer();

    /**
     *  Handles triggered update timer, sends triggered update message, if the message is "in the queue"
     */
    void handleTriggeredUpdateTimer();

    /**
     *  Starts Route Deletion Process for the route with the given timer (called when the RTE timer expires).
     *  @param timer [in] Route Deletion Process will be started for the route with this timer
     */
    void startRouteDeletionProcess(RIPngTimer *timer);

    /**
     *  Starts Route Deletion Process for the route.
     *  This method do not stop route timer!
     *  @param timer [in] Route Deletion Process will be started for this route.
     */
    void startRouteDeletionProcess(RIPng::RoutingTableEntry *routingTableEntry);

    /**
     *  Deletes route from RIPng routing table, called when the RTE GCTimer expires.
     *  @param timer [in] Route which should be deleted is given by this timer (also garbage collection timer)
     */
    void deleteRoute(RIPngTimer *timer);

  protected:
    virtual int numInitStages() const {return 4;}
    virtual void initialize(int stage);
    virtual void handleMessage(cMessage *msg);
    virtual void receiveChangeNotification(int category, const cObject *details);
};

#endif /* RIPNGROUTING_H_ */
