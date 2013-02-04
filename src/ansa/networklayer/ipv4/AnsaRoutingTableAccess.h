/**
 * @file MulticastRoutingTableAccess.h
 * @brief File contains implementation of access class.
 * @brief 17.3.2012
 * @author Veronika Rybova
 */

#ifndef ANSAROUTINGTABLEACCESS_H_
#define ANSAROUTINGTABLEACCESS_H_

#include <omnetpp.h>
#include "ModuleAccess.h"
#include "AnsaRoutingTable.h"

/**
 * @brief Class gives access to the MulticastRoutingTable.
 */
class INET_API AnsaRoutingTableAccess : public ModuleAccess<AnsaRoutingTable>
{
    public:
        AnsaRoutingTableAccess() : ModuleAccess<AnsaRoutingTable>("routingTable") {}
};

#endif /* ANSAROUTINGTABLEACCESS_H_ */
