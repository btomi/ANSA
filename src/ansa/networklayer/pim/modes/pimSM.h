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
 * @file pimSM.h
 * @date 29.10.2011
 * @author: Veronika Rybova, Tomas Prochazka (mailto:xproch21@stud.fit.vutbr.cz), Vladimir Vesely (mailto:ivesely@fit.vutbr.cz)
 * @brief File implements PIM sparse mode.
 * @details Implementation will be done in the future according to RFC4601.
 */

#ifndef PIMSM_H
#define PIMSM_H

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

/**
 * @brief Class implements PIM-SM (sparse mode).
 */
class pimSM : public cSimpleModule, protected INotifiable
{
    private:
        AnsaRoutingTable            *rt;            /**< Pointer to routing table. */
        IInterfaceTable             *ift;           /**< Pointer to interface table. */
        NotificationBoard           *nb;            /**< Pointer to notification table. */
        PimInterfaceTable           *pimIft;        /**< Pointer to table of PIM interfaces. */
        PimNeighborTable            *pimNbt;        /**< Pointer to table of PIM neighbors. */

        void receiveChangeNotification(int category, const cPolymorphic *details);


    protected:
        std::string RPAddress;
        std::string SPTthreshold;

    public:
        //PIM-SM clear implementation
        void setRPAddress(std::string address);
        std::string getRPAddress () {return RPAddress;}

	protected:
		virtual int numInitStages() const  {return 5;}
		virtual void handleMessage(cMessage *msg);
		virtual void initialize(int stage);
};

#endif
