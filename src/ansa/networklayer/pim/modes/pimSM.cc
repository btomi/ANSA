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
 * @file pimSM.cc
 * @date 29.10.2011
 * @author: Veronika Rybova, Tomas Prochazka (mailto:xproch21@stud.fit.vutbr.cz), Vladimir Vesely (mailto:ivesely@fit.vutbr.cz)
 * @brief File implements PIM sparse mode.
 * @details Implementation will be done in the future according to RFC4601.
 */

#include "pimSM.h"
#include "deviceConfigurator.h"


Define_Module(pimSM);

void pimSM::handleMessage(cMessage *msg)
{
	EV << "PIMSM::handleMessage" << endl;

	// self message (timer)
	if (msg->isSelfMessage())
	{
	   EV << "PIMSM::handleMessage:Timer" << endl;
	   PIMTimer *timer = check_and_cast <PIMTimer *> (msg);
	}
	else if (dynamic_cast<PIMPacket *>(msg))
	{
	   EV << "PIMSM::handleMessage: PIM-SM packet" << endl;
	   PIMPacket *pkt = check_and_cast<PIMPacket *>(msg);
	   EV << "Verze: " << pkt->getVersion() << ", typ: " << pkt->getType() << endl;
	}
	else
	   EV << "PIMSM::handleMessage: Wrong message" << endl;
}

void pimSM::initialize(int stage)
{
    if (stage == 4)
    {
        // Pointer to routing tables, interface tables, notification board
        rt = AnsaRoutingTableAccess().get();
        ift = InterfaceTableAccess().get();
        nb = NotificationBoardAccess().get();
        pimIft = PimInterfaceTableAccess().get();
        pimNbt = PimNeighborTableAccess().get();

        // is PIM enabled?
        if (pimIft->getNumInterface() == 0)
        {
            EV << "PIM is NOT enabled on device " << endl;
            return;
        }

        // subscribe for notifications
        nb->subscribe(this, NF_IPv4_NEW_MULTICAST_DENSE);
        nb->subscribe(this, NF_IPv4_NEW_IGMP_ADDED);
        nb->subscribe(this, NF_IPv4_NEW_IGMP_REMOVED);
        nb->subscribe(this, NF_IPv4_DATA_ON_PRUNED_INT);
        nb->subscribe(this, NF_IPv4_DATA_ON_NONRPF);
        nb->subscribe(this, NF_IPv4_DATA_ON_RPF);
        //nb->subscribe(this, NF_IPv4_RPF_CHANGE);
        nb->subscribe(this, NF_IPv4_ROUTE_ADDED);
        nb->subscribe(this, NF_INTERFACE_STATE_CHANGED);

        DeviceConfigurator *devConf = ModuleAccess<DeviceConfigurator>("deviceConfigurator").get();
        devConf->loadPimGlobalConfig(this);

    }
}

void pimSM::setRPAddress(std::string address)
{
    if (address != "")
        RPAddress.append(address);
    else
        EV << "PIMSM::setRPAddress: empty RP address" << endl;
}

/**
 * RECEIVE CHANGE NOTIFICATION
 *
 * The method from class Notification Board is used to catch its events.
 *
 * @param category Category of notification.
 * @param details Additional information for notification.
 * @see newMulticast()
 * @see newMulticastAddr()
 */
void pimSM::receiveChangeNotification(int category, const cPolymorphic *details)
{
    // ignore notifications during initialize
    if (simulation.getContextType()==CTX_INITIALIZE)
        return;

    // PIM needs addition info for each notification
    if (details == NULL)
        return;

    Enter_Method_Silent();
    printNotificationBanner(category, details);
//    IPv4ControlInfo *ctrl;
//    AnsaIPv4MulticastRoute *route;
//    addRemoveAddr *members;
//
//    // according to category of event...
//    switch (category)
//    {
//        case NF_INTERFACE_STATE_CHANGED:
//            EV <<  "pimDM::INTERFACE CHANGE" << endl;
//            setUpInterface();
//            break;
//
//        // new multicast data appears in router
//        case NF_IPv4_NEW_MULTICAST_DENSE:
//            EV <<  "pimDM::receiveChangeNotification - NEW MULTICAST DENSE" << endl;
//            route = (AnsaIPv4MulticastRoute *)(details);
//            newMulticast(route);
//            break;
//
//        // configuration of interface changed, it means some change from IGMP, address were added.
//        case NF_IPv4_NEW_IGMP_ADDED:
//            EV << "pimDM::receiveChangeNotification - IGMP change - address were added." << endl;
//            members = (addRemoveAddr *) (details);
//            newMulticastAddr(members);
//            break;
//
//        // configuration of interface changed, it means some change from IGMP, address were removed.
//        case NF_IPv4_NEW_IGMP_REMOVED:
//            EV << "pimDM::receiveChangeNotification - IGMP change - address were removed." << endl;
//            members = (addRemoveAddr *) (details);
//            oldMulticastAddr(members);
//            break;
//
//        case NF_IPv4_DATA_ON_PRUNED_INT:
//            EV << "pimDM::receiveChangeNotification - Data appears on pruned interface." << endl;
//            ctrl = (IPv4ControlInfo *)(details);
//            dataOnPruned(ctrl->getDestAddr(), ctrl->getSrcAddr());
//            break;
//
//        // data come to non-RPF interface
//        case NF_IPv4_DATA_ON_NONRPF:
//            EV << "pimDM::receiveChangeNotification - Data appears on non-RPF interface." << endl;
//            ctrl = (IPv4ControlInfo *)(details);
//            dataOnNonRpf(ctrl->getDestAddr(), ctrl->getSrcAddr(), ctrl->getInterfaceId());
//            break;
//
//        // data come to RPF interface
//        case NF_IPv4_DATA_ON_RPF:
//            EV << "pimDM::receiveChangeNotification - Data appears on RPF interface." << endl;
//            route = (AnsaIPv4MulticastRoute *)(details);
//            dataOnRpf(route);
//            break;
//
//        // RPF interface has changed
//        case NF_IPv4_ROUTE_ADDED:
//            EV << "pimDM::receiveChangeNotification - RPF interface has changed." << endl;
//            IPv4Route *entry = (IPv4Route *) (details);
//            vector<AnsaIPv4MulticastRoute*> routes = rt->getRoutesForSource(entry->getDestination());
//            for (unsigned int i = 0; i < routes.size(); i++)
//                rpfIntChange(routes[i]);
//            break;
//    }
}
