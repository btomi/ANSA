/**
 * @file pimDM.h
 * @date 29.10.2011
 * @author: Veronika Rybova
 * @brief File implements PIM dense mode.
 * @details Implementation according to RFC3973.
 */

#ifndef HLIDAC_PIMDM
#define HLIDAC_PIMDM

#include <omnetpp.h>
#include "PIMPacket_m.h"
#include "PIMTimer_m.h"
#include "InterfaceTableAccess.h"
#include "AnsaRoutingTableAccess.h"
#include "NotificationBoard.h"
#include "NotifierConsts.h"
#include "PimNeighborTable.h"
#include "PimInterfaceTable.h"
#include "IPv4ControlInfo.h"
#include "IPv4InterfaceData.h"
#include "AnsaIPv4Route.h"



#define PT 180.0						/**< Prune Timer = 180s (3min). */
#define GRT 3.0							/**< Graft Retry Timer = 3s. */
#define SAT 210.0						/**< Source Active Timer = 210s, Cisco has 180s, after that, route is flushed */
#define SRT 60.0						/**< State Refresh Timer = 60s. */

/**
 * @brief Class implements PIM-DM (dense mode).
 */
class pimDM : public cSimpleModule, protected INotifiable
{
	private:
		AnsaRoutingTable           	*rt;           	/**< Pointer to routing table. */
		//MulticastRoutingTable 		*mrt;			/**< Pointer to multicast routing table. */
	    IInterfaceTable         	*ift;          	/**< Pointer to interface table. */
	    NotificationBoard 			*nb; 		   	/**< Pointer to notification table. */
	    PimInterfaceTable			*pimIft;		/**< Pointer to table of PIM interfaces. */
	    PimNeighborTable			*pimNbt;		/**< Pointer to table of PIM neighbors. */

	    // process events
	    void receiveChangeNotification(int category, const cPolymorphic *details);
	    void newMulticast(AnsaIPv4MulticastRoute *newRoute);
	    void newMulticastAddr(addRemoveAddr *members);
	    void oldMulticastAddr(addRemoveAddr *members);
	    void dataOnPruned(IPv4Address destAddr, IPv4Address srcAddr);
	    void dataOnNonRpf(IPv4Address group, IPv4Address source, int intId);
	    void dataOnRpf(AnsaIPv4MulticastRoute *route);
	    void rpfIntChange(AnsaIPv4MulticastRoute *route);

	    // process timers
	    void processPIMTimer(PIMTimer *timer);
	    void processPruneTimer(PIMpt * timer);
	    void processGraftRetryTimer(PIMgrt *timer);
	    void processSourceActiveTimer(PIMsat * timer);
	    void processStateRefreshTimer(PIMsrt * timer);

	    // create timers
	    PIMpt* createPruneTimer(IPv4Address source, IPv4Address group, int intId, int holdTime);
	    PIMgrt* createGraftRetryTimer(IPv4Address source, IPv4Address group);
	    PIMsat* createSourceActiveTimer(IPv4Address source, IPv4Address group);
	    PIMsrt* createStateRefreshTimer(IPv4Address source, IPv4Address group);

	    // process PIM packets
	    void processPIMPkt(PIMPacket *pkt);
	    void processJoinPruneGraftPacket(PIMJoinPrune *pkt, PIMPacketType type);
	    void processPrunePacket(AnsaIPv4MulticastRoute *route, int intId, int holdTime);
	    void processGraftPacket(IPv4Address source, IPv4Address group, IPv4Address sender, int intId);
	    void processGraftAckPacket(AnsaIPv4MulticastRoute *route);
	    void processStateRefreshPacket(PIMStateRefresh *pkt);

	    //create PIM packets
	    void sendPimJoinPrune(IPv4Address nextHop, IPv4Address src, IPv4Address grp, int intId);
	    void sendPimGraft(IPv4Address nextHop, IPv4Address src, IPv4Address grp, int intId);
	    void sendPimGraftAck(PIMGraftAck *msg);
	    void sendPimStateRefresh(IPv4Address originator, IPv4Address src, IPv4Address grp, int intId, bool P);



	protected:
		virtual int numInitStages() const  {return 5;}
		virtual void handleMessage(cMessage *msg);
		virtual void initialize(int stage);
};

#endif
