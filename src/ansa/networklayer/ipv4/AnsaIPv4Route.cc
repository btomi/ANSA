/**
 * @file AnsaIPv4Route.cc
 * @date 25.1.2013
 * @author: Veronika Rybova, Tomas Prochazka
 * @brief File implements new functions for PIM
 */

#include "AnsaIPv4Route.h"


AnsaIPv4MulticastRoute::AnsaIPv4MulticastRoute()
{
    inInt.intPtr = NULL;
    grt = NULL;
    sat = NULL;
    srt = NULL;

    this->setRoutingTable(NULL);
    this->setParent(NULL);
    this->setSource(MANUAL);
    this->setMetric(0);
}

std::string AnsaIPv4MulticastRoute::info() const
{
    std::stringstream out;
    out << "(";
    if (this->getOrigin().isUnspecified()) out << "*  "; else out << this->getOrigin() << ",  ";
    if (this->getMulticastGroup().isUnspecified()) out << "*  "; else out << this->getMulticastGroup() << "),  ";
    if (RP.isUnspecified()) out << "0.0.0.0"<< endl; else out << "RP is " << RP << endl;
    out << "Incoming interface: ";
    if(inInt.intPtr != NULL)
    {
        if (inInt.intPtr) out << inInt.intPtr->getName() << ", ";
        out << "RPF neighbor " << inInt.nextHop << endl;
        out << "Outgoing interface list:" << endl;
    }

    for (InterfaceVector::const_iterator i = outInt.begin(); i < outInt.end(); i++)
    {
        if ((*i).intPtr) out << (*i).intPtr->getName() << ", ";
        if (i->forwarding == Forward) out << "Forward/"; else out << "Pruned/";
        if (i->mode == Densemode) out << "Dense"; else out << "Sparse";
        out << endl;
    }

    return out.str();
}

bool AnsaIPv4MulticastRoute::isFlagSet(flag fl)
{
    for(unsigned int i = 0; i < flags.size(); i++)
    {
        if (flags[i] == fl)
            return true;
    }
    return false;
}

void AnsaIPv4MulticastRoute::addFlag(flag fl)
{
    if (!isFlagSet(fl))
        flags.push_back(fl);
}

void AnsaIPv4MulticastRoute::removeFlag(flag fl)
{
    for(unsigned int i = 0; i < flags.size(); i++)
    {
        if (flags[i] == fl)
        {
            flags.erase(flags.begin() + i);
            return;
        }
    }
}

int AnsaIPv4MulticastRoute::getOutIdByIntId(int intId)
{
    unsigned int i;
    for (i = 0; i < outInt.size(); i++)
    {
        if (outInt[i].intId == intId)
            break;
    }
    return i;
}

bool AnsaIPv4MulticastRoute::isOilistNull()
{
    bool olistNull = true;
    for (unsigned int i = 0; i < outInt.size(); i++)
    {
        if (outInt[i].forwarding == Forward)
        {
            olistNull = false;
            break;
        }
    }
    return olistNull;
}
