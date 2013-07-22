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
 * @file AnsaRoutingTable.h
 * @date 25.1.2013
 * @author Tomas Prochazka (mailto:xproch21@stud.fit.vutbr.cz), Vladimir Vesely (mailto:ivesely@fit.vutbr.cz)
 * @brief Extended RoutingTable with new features for PIM
 */

#ifndef ANSAROUTINGTABLE_H_
#define ANSAROUTINGTABLE_H_

#include "RoutingTable.h"
#include "AnsaIPv4Route.h"
#include "IInterfaceTable.h"
#include "InterfaceTableAccess.h"
#include "NotificationBoard.h"

class INET_API AnsaRoutingTable : public RoutingTable {

    protected:
        // displays summary above the icon
        virtual void updateDisplayString();
        virtual void initialize(int stage);

    public:
      AnsaRoutingTable(){};
      virtual ~AnsaRoutingTable(){};

    public:

      /**
       * reimplemented - adds ANSAIPv4Routes instead of IPv4Route
       */
      virtual void updateNetmaskRoutes();

      //rozsireni routing table
      virtual AnsaIPv4MulticastRoute *getRouteFor(IPv4Address group, IPv4Address source);
      virtual std::vector<AnsaIPv4MulticastRoute*> getRouteFor(IPv4Address group);
      virtual std::vector<AnsaIPv4MulticastRoute*> getRoutesForSource(IPv4Address source);

      virtual void addMulticastRoute(AnsaIPv4MulticastRoute *entry);
      virtual bool deleteMulticastRoute(AnsaIPv4MulticastRoute *entry);

      virtual bool isLocalAddress(const IPv4Address& dest) const;
      virtual InterfaceEntry *getInterfaceByAddress(const IPv4Address& addr) const;
};

#endif /* ANSAROUTINGTABLE_H_ */
