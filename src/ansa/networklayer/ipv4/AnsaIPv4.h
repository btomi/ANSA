/**
 * @file AnsaIPv4.h
 * @brief File contains extension of class IP, which can work also with multicast data and multicast
 * @author Veronika Rybova
 * @date 21.10.2011
 */

#ifndef __INET_ANSAIPV4_H
#define __INET_ANSAIPV4_H

#include "INETDefs.h"

#include "QueueBase.h"
#include "InterfaceTableAccess.h"
#include "AnsaRoutingTable.h"
#include "ICMPAccess.h"
#include "IPv4ControlInfo.h"
#include "IPv4Datagram.h"
#include "IPv4FragBuf.h"
#include "ProtocolMap.h"

#ifdef WITH_MANET
#include "ControlManetRouting_m.h"
#endif

#include "ICMPMessage_m.h"
#include "IPv4InterfaceData.h"
#include "ARPPacket_m.h"
#include "IPv4.h"
#include "PimSplitter.h"
#include "AnsaRoutingTableAccess.h"


class ARPPacket;
class ICMPMessage;

//FIXME it shouldn't be there, but somewhere more globally
enum AnsaIPProtocolId
{
    IP_PROT_PIM = 103
};

/**
 * @brief Class is extension of the IP protocol implementation for multicast.
 * @details It extends class IP mainly by methods processing multicast stream.
 */
class INET_API AnsaIPv4 : public IPv4
{
    private:
        AnsaRoutingTable            *rt;
        NotificationBoard           *nb;

    protected:
        virtual void handlePacketFromNetwork(IPv4Datagram *datagram, InterfaceEntry *fromIE);
        virtual void routeMulticastPacket(IPv4Datagram *datagram, InterfaceEntry *destIE, InterfaceEntry *fromIE);

    public:
        AnsaIPv4() {}

    protected:
      virtual int numInitStages() const  {return 5;}
      virtual void initialize(int stage);
};


#endif
