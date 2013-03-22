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
 * @file TRILL.cc
 * @author Marcel Marek (mailto:xscrew02@gmail.com), Vladimir Vesely (mailto:ivesely@fit.vutbr.cz)
 * @date 7.3.2013
 * @brief
 * @detail
 * @todo
 */

#include "TRILL.h"
#include "ISIS.h"


Define_Module(TRILL);
TRILL::TRILL(){

}

TRILL::~TRILL(){

}

MACAddress TRILL::getBridgeAddress() {
    Enter_Method_Silent();
    return bridgeAddress;
}


void TRILL::initialize(int stage) {

    if (stage == 0) {
        portCount = gateSize("lowerLayerOut");

        /* because ASSERT in handleIncomingFrame() */
        int i = 0;
        char msgname[16];
        sprintf(msgname, "switchMsg %d", i);
        currentMsg = new cMessage(msgname, i);

        cModule * tmp_rBMACTable = getParentModule()->getSubmodule("RBMACTable");
        rbMACTable = check_and_cast<RBMACTable *>(tmp_rBMACTable);

        cModule * tmp_rBVLANTable = getParentModule()->getSubmodule("rBVLANTable");
        vlanTable = check_and_cast<RBVLANTable *>(tmp_rBVLANTable);

//        cModule * tmp_stp = getParentModule()->getSubmodule("stp");
//        spanningTree = check_and_cast<Stp *>(tmp_stp);

        cModule * tmp_isis = getParentModule()->getSubmodule("isis");
        isis = check_and_cast<ISIS *>(tmp_isis);

        bridgeGroupAddress = MACAddress("01-80-C2-00-00-00");
        //TODO two modes, MAC per rBridge vs. MAC per interface
        bridgeAddress = MACAddress::generateAutoAddress();

        WATCH(bridgeAddress);
    }
}

void TRILL::handleMessage(cMessage *msg) {

    if (!msg->isSelfMessage()) {
        tFrameDescriptor frame;

        if (strcmp(msg->getArrivalGate()->getName(), "stpIn") == 0) {
            /* Messages from STP process */
            dispatchBPDU(msg, msg->getArrivalGate()->getIndex());
            return;

        } else if (strcmp(msg->getArrivalGate()->getName(), "ifIn") == 0) {
            /* Messages from network */
            if (reception(frame, msg) == true) {
                //EV << frame;
                //error("BLE BLE");
                relay(frame);
            }
            delete frame.payload;
        }
    } else {
        // Self message signal used to indicate a frame has finished processing
        processFrame(msg);
    }
    delete msg;
}

bool TRILL::reception(TRILL::tFrameDescriptor& frame, cMessage *msg) {
    /* ACTIVE TOPOLOGY ENFORCEMENT AFTER CLASSIFICATION AND INGRESS
     * because of learning state operation, MAC record can be learned
     * after classification and ingress, if learning is enabled on
     * that port.
     */
  //CLASSIFICATION & INGRESS CALL
    int rPort = msg->getArrivalGate()->getIndex();

    cMessage * tmp = msg;
    ASSERT(tmp);

    // Classify frame, and unpack to frame descriptor
    if (dynamic_cast<AnsaEtherFrame *>(tmp)){
        AnsaEtherFrame * taggedFrame = (AnsaEtherFrame *) tmp;
        if (ingress(frame, taggedFrame, rPort) == false) {
            return false;
        }
    } else if (dynamic_cast<EthernetIIFrame *>(tmp)) {
        EthernetIIFrame * untaggedFrame = (EthernetIIFrame *) tmp;
        ingress(frame, untaggedFrame, rPort); // discarding forbidden PortVID is in relay
    }

    return true;
}

void TRILL::relay(TRILL::tFrameDescriptor& frame) {

    // BPDU Handling
    if (frame.dest == bridgeGroupAddress) {
        deliverBPDU(frame);
        return;
    }

     /* Dropping forbidden PortVID */
    if (frame.VID == 0) {
        return;
    }

    // BROADCAST ??
    if (frame.dest.isBroadcast()) {
        frame.portList = vlanTable->getPorts(frame.VID);
        if (frame.portList.size() == 0) {
            return;
        }

    } else {
        RBVLANTable::tVIDPort tmpPort;
        tmpPort.action = RBVLANTable::REMOVE;

        RBMACTable::tPortList tmpPortList = rbMACTable->getPorts(frame.dest);

        if (tmpPortList.size() == 0) { // not known -> bcast
            frame.portList = vlanTable->getPorts(frame.VID);
        } else {
            for (unsigned int i = 0; i < tmpPortList.size(); i++) {
                tmpPort.port = tmpPortList.at(i);
                frame.portList.push_back(tmpPort);
            }
        }
    }

    //EV << frame;
    //error("BLE BLE");


    // LEARNING (explained in reception())
    learn(frame);
    // ACTIVE TOPOLOGY ENFORCEMENT (explained in reception())
//    if (spanningTree->forwarding(frame.rPort, frame.VID) == true) {
        // EGRESS
        egress(frame);
        // SEND
        dispatch(frame);
//    }
}

bool TRILL::ingress(TRILL::tFrameDescriptor& tmp, EthernetIIFrame *frame, int rPort) {
    // Info from recepted frame
    tmp.payload = frame->decapsulate();
    tmp.name.insert(0, frame->getName());
    tmp.rPort = rPort;
    tmp.src = frame->getSrc();
    tmp.dest = frame->getDest();
    tmp.etherType = frame->getEtherType();

    // VLAN Assign
    tmp.VID = vlanTable->getVID(rPort);

    if (tmp.VID == 0) {
        return false;
    }


    return true;
}

bool TRILL::ingress(TRILL::tFrameDescriptor& tmp, AnsaEtherFrame *frame, int rPort) {
    // Info from recepted frame
    tmp.payload = frame->decapsulate();
    tmp.name.insert(0, frame->getName());
    tmp.rPort = rPort;
    tmp.VID = frame->getVlan();
    tmp.src = frame->getSrc();
    tmp.dest = frame->getDest();
    tmp.etherType = frame->getEtherType();

    // VLAN Allowed
    if (vlanTable->isAllowed(frame->getVlan(), rPort) == false) {
        return false;
    }

    return true;
}

void TRILL::egress(TRILL::tFrameDescriptor& frame) {
  // MINIMIZE OUT PORTS
  // SET TAG ACTIONS

    RBVLANTable::tVIDPortList tmp = frame.portList;
    frame.portList.clear();

    for (unsigned int i = 0; i < tmp.size(); i++) {
        if (vlanTable->isAllowed(frame.VID, tmp.at(i).port) == true && tmp.at(i).port != frame.rPort) {
            frame.portList.push_back(tmp.at(i));
        }
    }

}

void TRILL::dispatch(TRILL::tFrameDescriptor& frame) {

    EthernetIIFrame * untaggedFrame = new EthernetIIFrame(frame.name.c_str());
    AnsaEtherFrame * taggedFrame = new AnsaEtherFrame(frame.name.c_str());

    taggedFrame->setKind(frame.payload->getKind());
    taggedFrame->setSrc(frame.src);
    taggedFrame->setDest(frame.dest);
    taggedFrame->setByteLength(ETHER_MAC_FRAME_BYTES);
    taggedFrame->setVlan(frame.VID);
    taggedFrame->setEtherType(frame.etherType);

    taggedFrame->encapsulate(frame.payload->dup());
    if (taggedFrame->getByteLength() < MIN_ETHERNET_FRAME_BYTES) {
        taggedFrame->setByteLength(MIN_ETHERNET_FRAME_BYTES); // "padding"
    }

    untaggedFrame->setKind(frame.payload->getKind());
    untaggedFrame->setSrc(frame.src);
    untaggedFrame->setDest(frame.dest);
    untaggedFrame->setByteLength(ETHER_MAC_FRAME_BYTES);
    untaggedFrame->setEtherType(frame.etherType);

    untaggedFrame->encapsulate(frame.payload->dup());
    if (untaggedFrame->getByteLength() < MIN_ETHERNET_FRAME_BYTES) {
        untaggedFrame->setByteLength(MIN_ETHERNET_FRAME_BYTES); // "padding"
    }
    RBVLANTable::tVIDPortList::iterator it;
    for (it = frame.portList.begin(); it != frame.portList.end(); it++) {
        if (it->port >= portCount) {
            continue;
        }
//        if (spanningTree->forwarding(it->port, frame.VID) == false) {
//            continue;
//        }
        if (it->action == RBVLANTable::INCLUDE) {
            send(taggedFrame->dup(), "lowerLayerOut", it->port);
        } else {
            send(untaggedFrame->dup(), "lowerLayerOut", it->port);
        }
    }

    delete taggedFrame;
    delete untaggedFrame;
    return;
}
/*
 * ByGate means that the input interface is identified by gateId
 */

bool TRILL::isAllowedByGate(int vlanID, int gateIndex){

    return vlanTable->isAllowed(vlanID, gateIndex);

}

void TRILL::learn(AnsaEtherFrame *frame){
    //remember the last parameter is not interfaceIndex, but gateId
    rbMACTable->update(frame->getSrc(), frame->getVlan(), frame->getArrivalGateId());

}

void TRILL::learn(EthernetIIFrame *frame){
    //remember the last parameter is not interfaceIndex, but gateId
    /*TODO instead of frame->getVlan() do something like:
     * find out if tagAction ::REMOVE is set to this port
     *  if yes -> use some default vlanId
     *  if not -> drop frame?
     */
    rbMACTable->update(frame->getSrc(), 1, frame->getArrivalGateId());

}

void TRILL::learn(TRILL::tFrameDescriptor& frame) {
//    if (spanningTree->learning(frame.rPort, frame.VID) == true) {
      rbMACTable->update(frame.src, frame.rPort);
//    }
}



void TRILL::handleIncomingFrame(EthernetIIFrame *frame) {
    // If buffer not full, insert payload frame into buffer and process the frame in parallel.
    cMessage *msg = this->currentMsg;
    ASSERT(msg->getContextPointer()==NULL);
    msg->setContextPointer(frame);
    scheduleAt(simTime(), msg);
    return;
}

void TRILL::processFrame(cMessage *msg) {
    EthernetIIFrame *frame = (EthernetIIFrame *) msg->getContextPointer();
    ASSERT(frame);
    msg->setContextPointer(NULL);
    int inputport = frame->getArrivalGate()->getIndex();

    handleAndDispatchFrame(frame, inputport);
    return;
}

void TRILL::handleAndDispatchFrame(EthernetIIFrame *frame, int inputport) {
    this->rbMACTable->update(frame->getSrc(), inputport);

/*
     AnsaEtherFrame * tmp;
     tmp = this->tagMsg(frame, 1);
     newframe = this->untagMsg(tmp);
*/

    // handle broadcast frames first
    if (frame->getDest().isBroadcast()) {
        EV<< "Broadcasting broadcast frame " << frame << endl;
        broadcastFrame(frame, inputport);
        return;
    }

    // Finds output port of destination address and sends to output port
    // if not found then broadcasts to all other ports instead
    MACTable::tPortList portList = rbMACTable->getPorts(frame->getDest());

    if (portList.size() == 0) {
        EV << "Dest address " << frame->getDest() << " unknown, broadcasting frame " << frame << endl;
        broadcastFrame(frame, inputport);
    } else {
        for (unsigned int i = 0; i < portList.size(); i++) {
            if (inputport != portList.at(i)) {
                EV << "Sending frame " << frame << " with dest address " << frame->getDest() << " to port " << portList.at(i) << endl;
                send(frame->dup(), "lowerLayerOut", portList.at(i));
            }
        }
        delete frame;
    }

    return;
}

void TRILL::broadcastFrame(EthernetIIFrame *frame, int inputport) {
    for (int i = 0; i < this->portCount; ++i) {
        if (i != inputport) {
            send((EthernetIIFrame*) frame->dup(), "lowerLayerOut", i);
        }
    }
    delete frame;

    return;
}

void TRILL::sinkMsg(cMessage *msg) {
    send(msg, "toSink");
    return;
}


void TRILL::sinkDupMsg(cMessage *msg) {
    send(msg->dup(), "toSink");
    return;
}

void TRILL::dispatchBPDU(cMessage *msg, int port) {
    if (port >= this->portCount || port < 0) {
        return;
    }

    EthernetIIFrame * untaggedFrame = new EthernetIIFrame(msg->getName());

    untaggedFrame->setKind(msg->getKind());
    untaggedFrame->setSrc(bridgeAddress);
    untaggedFrame->setDest(bridgeGroupAddress);
    untaggedFrame->setByteLength(ETHER_MAC_FRAME_BYTES);
    untaggedFrame->setEtherType(-1);

    untaggedFrame->encapsulate((cPacket *)msg);
    if (untaggedFrame->getByteLength() < MIN_ETHERNET_FRAME_BYTES) {
        untaggedFrame->setByteLength(MIN_ETHERNET_FRAME_BYTES); // "padding"
    }


    send((EthernetIIFrame*) untaggedFrame, "lowerLayerOut", port);
}

void TRILL::deliverBPDU(TRILL::tFrameDescriptor& frame) {
    send(frame.payload->dup(), "stpOut", frame.rPort);
}

void TRILL::finish() {
    cancelAndDelete(this->currentMsg);
    return;
}
/*
 void TRILL::tagMsg(int _vlan) {

 EthernetIIFrame * tmp = check_and_cast<EthernetIIFrame *> (this->currentMsg);

 cPacket * payload = tmp->decapsulate();
 AnsaEtherFrame * frame = new AnsaEtherFrame(payload->getName());

 frame->setSrc(tmp->getSrc());  // if blank, will be filled in by MAC
 frame->setDest(tmp->getDest());
 frame->setByteLength(ETHER_MAC_FRAME_BYTES);
 frame->setVlan(_vlan);

 frame->encapsulate(tmp);
 if (frame->getByteLength() < MIN_ETHERNET_FRAME) {
 frame->setByteLength(MIN_ETHERNET_FRAME);  // "padding"
 }
 this->currentMsg = frame;
 return;
 }*/


AnsaEtherFrame * TRILL::tagMsg(EthernetIIFrame * _frame, int _vlan) {

    EthernetIIFrame * tmp = _frame;

    cPacket * payload = tmp->decapsulate();
    AnsaEtherFrame * frame = new AnsaEtherFrame(payload->getName());

    frame->setSrc(tmp->getSrc());
    frame->setDest(tmp->getDest());
    frame->setByteLength(ETHER_MAC_FRAME_BYTES);
    frame->setVlan(_vlan);
    frame->setEtherType(tmp->getEtherType());

    frame->encapsulate(payload);
    if (frame->getByteLength() < MIN_ETHERNET_FRAME_BYTES) {
        frame->setByteLength(MIN_ETHERNET_FRAME_BYTES); // "padding"
    }

    return frame;
}

EthernetIIFrame * TRILL::untagMsg(AnsaEtherFrame * _frame) {

    AnsaEtherFrame * tmp = _frame;

    cPacket * payload = tmp->decapsulate();
    EthernetIIFrame * frame = new EthernetIIFrame(payload->getName());

    frame->setSrc(tmp->getSrc()); // if blank, will be filled in by MAC
    frame->setDest(tmp->getDest());
    frame->setByteLength(ETHER_MAC_FRAME_BYTES);
    frame->setEtherType(tmp->getEtherType());

    frame->encapsulate(payload);
    if (frame->getByteLength() < MIN_ETHERNET_FRAME_BYTES) {
        frame->setByteLength(MIN_ETHERNET_FRAME_BYTES); // "padding"
    }
    return frame;
}
/*

 void TRILL::untagMsg() {

 AnsaEtherFrame * tmp = check_and_cast<AnsaEtherFrame *> (this->currentMsg);

 cPacket * payload = tmp->decapsulate();
 EthernetIIFrame * frame = new EthernetIIFrame(payload->getName());

 frame->setSrc(tmp->getSrc());  // if blank, will be filled in by MAC
 frame->setDest(tmp->getDest());
 frame->setByteLength(ETHER_MAC_FRAME_BYTES);

 frame->encapsulate(tmp);
 if (frame->getByteLength() < MIN_ETHERNET_FRAME) {
 frame->setByteLength(MIN_ETHERNET_FRAME);  // "padding"
 }
 this->currentMsg = frame;
 return;
 }

*/
