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
// 

/**
 * @file TRILL.h
 * @author Marcel Marek (mailto:xscrew02@gmail.com), Vladimir Vesely (mailto:ivesely@fit.vutbr.cz)
 * @date 7.3.2013
 * @brief Based on Zdenek Kraus's AnsaSwitchCore
 * @detail
 * @todo
 */

#ifndef TRILL_H_
#define TRILL_H_


#include <omnetpp.h>
#include <csimplemodule.h>

#include <map>
#include <string>

#include "NotificationBoard.h"
#include "MACAddress.h"
#include "Ethernet.h"

#include "EtherFrame_m.h"
#include "AnsaEtherFrame_m.h"

#include "macTable.h"
#include "vlanTable.h"
#include "stp/stp.h"
//#include "ISIS.h"
class ISIS;

class TRILL : public cSimpleModule
{
    public:
        TRILL();
        virtual ~TRILL();


        /* switch core frame descriptor for internal representation unpacked frame */
        typedef struct s_frame_descriptor {
          cPacket * payload; // relaying message
          int VID; // frame's VLAN ID
          int rPort; // reception port
          VLANTable::tVIDPortList portList; // suggested(then applied) list of destination ports
          MACAddress src; // source MAC address of the original frame
          MACAddress dest; // destination MAC address of the original frame
          int etherType; // EtherType from EthernetIIFrame
          std::string name; // for simulation frame name
        } tFrameDescriptor;

        MACAddress getBridgeAddress();

      private:
        //number of nicknames, default 1, may be 1 - 256
        //unsigned 16bit int -> desired number of distribution trees to be calculated
        //                      desired number of dist. trees for advertising
        //maximum number of dist. trees RBridge can compute
        //two lists of nicknames ... (specify later)
        //ageing timer see 802.1Q-2005
        //forward delay see 802.1Q-2005
        //unsigned 8 int - confidence of learned {MAC, VLAN, local port}
        //                                       {MAC, VLAN, remote RBridge}
        // defaults to 0x20, can be configured from 0x00 to 0xFE

        //u_int16 desired minimum inter-RBridge MTU. defaults 1470 which is minimum valid value. any lower advertised value is ignored
        //number of failed MTU-probes before condluding that particular MTU is not supported. defaults 3, can be 1 to 255


      protected:
      MACAddress bridgeGroupAddress;
      MACAddress bridgeAddress;

      MACTable * addrTable;
      VLANTable * vlanTable;
      Stp * spanningTree;
      ISIS* isis;

      cMessage * currentMsg;

      int portCount;

      virtual void initialize(int stage);
      virtual int numInitStages() const {return 1;}
      virtual void handleMessage(cMessage * msg);
      virtual void finish();

      bool reception(tFrameDescriptor& frame, cMessage *msg);
      void relay(tFrameDescriptor& frame);
      void dispatch(tFrameDescriptor& frame);

      bool ingress(tFrameDescriptor& tmp, EthernetIIFrame *frame, int rPort);
      bool ingress(tFrameDescriptor& tmp, AnsaEtherFrame *frame, int rPort);
      void egress(tFrameDescriptor& frame);

      void learn(tFrameDescriptor& frame);

      void processFrame(cMessage * msg);
      void handleIncomingFrame(EthernetIIFrame *frame);

      void handleAndDispatchFrame(EthernetIIFrame *frame, int inputport);
      void broadcastFrame(EthernetIIFrame *frame, int inputport);

      void sinkMsg(cMessage *msg);
      void sinkDupMsg(cMessage *msg);

      void dispatchBPDU(cMessage *msg, int port);
      void deliverBPDU(tFrameDescriptor& frame);

     // void tagMsg(int _vlan);
      AnsaEtherFrame * tagMsg(EthernetIIFrame * _frame, int _vlan);
      //void untagMsg();
      EthernetIIFrame * untagMsg(AnsaEtherFrame * _frame);



    };



    inline std::ostream& operator<<(std::ostream& os, const TRILL::tFrameDescriptor f) {
        os << "---FRAME DESCRIPTOR---" << std::endl;

        os << " Name: " << f.name << std::endl;
        os << "  Src: " << f.src << std::endl;
        os << " Dest: " << f.dest << std::endl;
        os << "  VID: " << f.VID << std::endl;
        os << "rPort: " << f.rPort << std::endl;
        os << "pList: " << f.portList << std::endl;
        os << "eType: " << f.etherType << std::endl;
        os << "pLoad: " << f.payload << std::endl;

        os << "---END OF DESCRIPTOR---" << std::endl;
        return os;
    }






    #endif /* TRILL_H_ */
