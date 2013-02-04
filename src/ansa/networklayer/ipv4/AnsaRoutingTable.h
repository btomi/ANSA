/**
 * @file AnsaRoutingTable.h
 * @date 25.1.2013
 * @author: Veronika Rybova, Tomas Prochazka
 * @brief File implements Routing table with features for PIM.
 */

#ifndef ANSAROUTINGTABLE_H_
#define ANSAROUTINGTABLE_H_

#include "RoutingTable.h"
#include "AnsaIPv4Route.h"
#include "IInterfaceTable.h"
#include "InterfaceTableAccess.h"
#include "NotificationBoard.h"

/**
 * AnsaRouteVector represents multicast table. It is list of AnsaMulticast routes.
 */
typedef std::vector<AnsaIPv4MulticastRoute *> routeVector;

class INET_API AnsaRoutingTable : public RoutingTable {

    protected:
        routeVector multicastRoutes;                        /**< Multicast routing table based on AnsaIPv4MulticastRoute which is inherited from IPv4MulticastRoute. */
        std::vector<std::string> showMRoute;                /**< Output of multicast routing table, same as Cisco mroute. */

    protected:
        // displays summary above the icon
        virtual void updateDisplayString();
        void initialize(int stage);

    public:
      AnsaRoutingTable(){};
      virtual ~AnsaRoutingTable();

    public:
      //rozsireni routing table
      virtual AnsaIPv4MulticastRoute *getRouteFor(IPv4Address group, IPv4Address source);
      virtual std::vector<AnsaIPv4MulticastRoute*> getRouteFor(IPv4Address group);
      virtual std::vector<AnsaIPv4MulticastRoute*> getRoutesForSource(IPv4Address source);
      void generateShowIPMroute();

      int getNumRoutes() const;
      virtual AnsaIPv4MulticastRoute *getMulticastRoute(int k) const;

      virtual void addMulticastRoute(const AnsaIPv4MulticastRoute *entry);
      virtual bool deleteMulticastRoute(const AnsaIPv4MulticastRoute *entry);

};

#endif /* ANSAROUTINGTABLE_H_ */
