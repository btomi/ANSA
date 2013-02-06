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
 * @file RoutingTableXmlParser.h
 * @date 21.10.2011
 * @author Tomas Prochazka (mailto:xproch21@stud.fit.vutbr.cz), Vladimir Vesely (mailto:ivesely@fit.vutbr.cz)
 * @brief Routing table parser
 * @details File contain methods for parsing xml with configuration
 */


#ifndef __INET_ROUTINGTABLEXMLPARSER_H
#define __INET_ROUTINGTABLEXMLPARSER_H

#include <omnetpp.h>
#include "AnsaRoutingTable.h"

/**
 * Parses a routing table file into a routing table.
 */
class INET_API RoutingTableXmlParser
{
  protected:
    IInterfaceTable *ift;
    IRoutingTable *rt;

  public:
    /**
     * Constructor
     */
    RoutingTableXmlParser(IInterfaceTable *ift, AnsaRoutingTable *rt);

    /**
     * Read Routing Table file; return 0 on success, -1 on error
     */
    virtual bool readRoutingTableFromXml (const char *filename, const char *RouterId);
    
    
    void readInterfaceFromXml(cXMLElement* Node);
    void readStaticRouteFromXml(cXMLElement* Node); 
};


#endif

