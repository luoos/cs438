/*
 * Author: Jun Luo
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <map>
#include <unordered_map>
#include <fstream>
#include <deque>
#include <vector>
#include <tuple>
#include <sstream>

#define INFINI 19997997
#define NOHOP -1

using namespace std;

// message wrap the distance vector sent by owner to its neighbors
class Message {
    public:
    int ownerId_;
    vector<int> neighborIds_;
    map<int, int> disVector_;

    Message(int ownerId, vector<int> neighborIds, map<int, int> disVector) {
        ownerId_ = ownerId;
        neighborIds_ = neighborIds;
        disVector_ = disVector;
    }
};

class Node;

class NetworkEmulator {
    private:

    unordered_map<int, Node*> nodes_;
    int minId_, maxId_;
    deque<Message> messageQueue_;  // message to update distance vector
    deque<tuple<int, int, int> > changes_;  // <node1, node2, newCost>
    vector<tuple<int, int, string> > packets_;  // <src, dest, message>

    public:
    Node* getNode(int id);
    void setUpNodes();
    void writeAllForwardingTables(ofstream *fpOut);
    void disconnectNode(int nodeId);
    void applyMessage(Message mesg);
    void sendMessage(Message mesg);
    void converge();
    void addSinglePacket(tuple<int, int, string> packet);
    void addSingleChange(tuple<int, int, int> change);
    void applyChange(tuple<int, int, int> change);
    vector<int> findPath(int srcId, int destId);
    void writePacketRoute(ofstream *fpOut);
    void working(ofstream *fpOut);
};

class Node {
    private:

    int selfId_;
    int minId_, maxId_;
    NetworkEmulator *emulator_;

    // map of distance vectors of all known nodes, from the perspective of self node
    // key: nodeId, value: distance vector of this key node
    unordered_map<int, map<int, int> > disVectors_;

    // key: neighbor Id, value: cost to this neighbor
    map<int, int> costToNeighbors_;

    // key: destination node Id, value: nexthop node Id
    unordered_map<int, int> forwardingTable_;

    int costFromNeighborToDestination(int neighborId, int desId) {
        if (neighborId == desId) {
            return 0;
        } else if (disVectors_.find(neighborId) != disVectors_.end() &&
                   disVectors_[neighborId].find(desId) != disVectors_[neighborId].end()) {
            return disVectors_[neighborId][desId];
        }
        return INFINI;
    }

    // return pair<nextHop, minCost>
    pair<int, int> getMinCostToDestination(int desId) {
        if (desId == selfId_) {
            return make_pair(selfId_, 0);
        }
        int minCost = INFINI;
        int nextHop = NOHOP;
        for (auto &entry : costToNeighbors_) {
            int cost = entry.second + costFromNeighborToDestination(entry.first, desId);
            if (cost < minCost) {
                minCost = cost;
                nextHop = entry.first;
            }
        }
        return make_pair(nextHop, minCost);
    }

    vector<int> getNeighborIds() {
        vector<int> neighborIds;
        for (auto &entry : costToNeighbors_) {
            neighborIds.push_back(entry.first);
        }
        return neighborIds;
    }

    public:

    Node(int id, NetworkEmulator *emulator) {
        selfId_ = id;
        emulator_ = emulator;
        map<int, int> selfDisVector;
        disVectors_[selfId_] = selfDisVector;
    }

    void setNeighbor(int neighborId, int cost) {
        if (cost == -999) {  // special int means no direct link
            costToNeighbors_.erase(neighborId);
            disVectors_[selfId_][neighborId] = INFINI;
            forwardingTable_[neighborId] = NOHOP;
        } else {
            costToNeighbors_[neighborId] = cost;
        }
    }

    void updateIdRange(int minId, int maxId) {
        minId_ = minId;
        maxId_ = maxId;
        for (int nodeId = minId; nodeId <= maxId; nodeId++) {
            disVectors_[selfId_][nodeId] = INFINI;
            forwardingTable_[nodeId] = NOHOP;
        }
    }

    bool updateSelfDistanceVector() {
        // return true if the self distance vector changes, otherwise return false
        bool hasChange = false;
        for (int desId = minId_; desId <= maxId_; desId++) {
            pair<int, int> pair = getMinCostToDestination(desId);
            int nextHop = pair.first;
            int minCost = pair.second;
            if (disVectors_[selfId_][desId] != minCost) {
                forwardingTable_[desId] = nextHop;
                disVectors_[selfId_][desId] = minCost;
                hasChange = true;
            }
        }
        return hasChange;
    }

    void sendSelfVectorToNeighbors() {
        Message mesg(selfId_, getNeighborIds(), disVectors_[selfId_]);
        emulator_->sendMessage(mesg);
    }

    void recvNewDisVector(int nodeId, map<int, int> vectors) {
        disVectors_[nodeId] = vectors;
        bool hasChange = updateSelfDistanceVector();
        if (hasChange) {
            sendSelfVectorToNeighbors();
        }
    }

    void writeForwardingTable(ofstream *fpOut) {
        for (int destId = minId_; destId <= maxId_; destId++) {
            int cost = disVectors_[selfId_][destId];
            int nextHop = forwardingTable_[destId];
            if (nextHop == NOHOP) {
                continue;
            }
            *fpOut << destId << " " << nextHop << " " << cost << endl;
        }
    }

    int forward(int destId) {
        // return the next hop id
        return forwardingTable_[destId];
    }

    int getCost(int destId) {
        return disVectors_[selfId_][destId];
    }

    void disconnectNode(int disconnectId) {
        if (selfId_ == disconnectId) return;

        for (int nodeId = minId_; nodeId <= maxId_; nodeId++) {
            disVectors_[nodeId][disconnectId] = INFINI;
        }
        forwardingTable_[disconnectId] = NOHOP;
    }
};

Node* NetworkEmulator::getNode(int id) {
    if (nodes_.find(id) == nodes_.end()) {
        Node *newNode = new Node(id, this);
        nodes_.insert(pair<int, Node*>(id, newNode));
        return newNode;
    }
    return nodes_[id];
}

void NetworkEmulator::setUpNodes() {
    // update Id range
    int minId = INFINI, maxId = -1;
    for (auto &entry : nodes_) {
        if (entry.first < minId) {
            minId = entry.first;
        }
        if (entry.first > maxId) {
            maxId = entry.first;
        }
    }
    minId_ = minId;
    maxId_ = maxId;
    for (auto &entry : nodes_) {
        entry.second->updateIdRange(minId, maxId);
    }

    // init nodes' distance vector
    for (auto &entry : nodes_) {
        entry.second->updateSelfDistanceVector();
    }
}

void NetworkEmulator::writeAllForwardingTables(ofstream *fpOut) {
    for (int nodeId = minId_; nodeId <= maxId_; nodeId++) {
        nodes_[nodeId]->writeForwardingTable(fpOut);
    }
}

void NetworkEmulator::disconnectNode(int disconnectId) {
    for (int nodeId = minId_; nodeId <= maxId_; nodeId++) {
        nodes_[nodeId]->disconnectNode(disconnectId);
    }
    for (int i = 0; i < messageQueue_.size(); i++) {
        messageQueue_[i].disVector_[disconnectId] = INFINI;
    }
}

void NetworkEmulator::applyMessage(Message mesg) {
    for (int nodeId : mesg.neighborIds_) {
        nodes_[nodeId]->recvNewDisVector(mesg.ownerId_, mesg.disVector_);
    }
    if (mesg.neighborIds_.size() == 0) {
        disconnectNode(mesg.ownerId_);
    }
}

void NetworkEmulator::sendMessage(Message mesg) {
    messageQueue_.push_back(mesg);
}

void NetworkEmulator::converge() {
    for (auto &entry : nodes_) {
        entry.second->updateSelfDistanceVector();
        entry.second->sendSelfVectorToNeighbors();
    }
    while (messageQueue_.size() > 0) {
        applyMessage(messageQueue_[0]);
        messageQueue_.pop_front();
    }
}

void NetworkEmulator::addSinglePacket(tuple<int, int, string> packet) {
    packets_.push_back(packet);
}

void NetworkEmulator::addSingleChange(tuple<int, int, int> change) {
    changes_.push_back(change);
}

void NetworkEmulator::applyChange(tuple<int, int, int> change) {
    int node1 = get<0>(change);
    int node2 = get<1>(change);
    int cost  = get<2>(change);
    nodes_[node1]->setNeighbor(node2, cost);
    nodes_[node2]->setNeighbor(node1, cost);
}

vector<int> NetworkEmulator::findPath(int srcId, int destId) {
    vector<int> path;
    int nextHop = srcId;
    while (nextHop != destId && nextHop != NOHOP) {
        path.push_back(nextHop);
        nextHop = nodes_[nextHop]->forward(destId);
    }
    return path;
}

void NetworkEmulator::writePacketRoute(ofstream *fpOut) {
    for (auto &it : packets_) {
        int srcId = get<0>(it), destId = get<1>(it);
        int cost = nodes_[srcId]->getCost(destId);
        string message = get<2>(it);
        vector<int> path = findPath(srcId, destId);
        *fpOut << "from " << srcId << " to " << destId << " cost ";
        if (cost == INFINI) {
            *fpOut << "infinite hops unreachable ";
        } else {
            *fpOut << cost << " hops ";
            for (int id : path) {
                *fpOut << id << " ";
            }
        }
        *fpOut << "message " << message << endl;
    }
}

void NetworkEmulator::working(ofstream *fpOut) {
    converge();
    writeAllForwardingTables(fpOut);
    writePacketRoute(fpOut);

    while (changes_.size() > 0) {
        applyChange(changes_[0]);
        changes_.pop_front();
        converge();
        writeAllForwardingTables(fpOut);
        writePacketRoute(fpOut);
    }
}

NetworkEmulator parseTopologyAndCreateEmulator(string topologyFileName) {
    int sourceId, destId, cost;
    NetworkEmulator emulator;

    ifstream topologyFile(topologyFileName);
    while (topologyFile >> sourceId >> destId >> cost) {
        Node *sourceNode = emulator.getNode(sourceId);
        Node *destNode = emulator.getNode(destId);
        sourceNode->setNeighbor(destId, cost);
        destNode->setNeighbor(sourceId, cost);
    }
    topologyFile.close();
    emulator.setUpNodes();

    return emulator;
}

void parseChageFile(string changeFileName, NetworkEmulator *emulator) {
    int node1, node2, cost;
    ifstream changesFile(changeFileName);
    while (changesFile >> node1 >> node2 >> cost) {
        emulator->addSingleChange(make_tuple(node1, node2, cost));
    }
    changesFile.close();
}

void parseMessageFile(string messageFileName, NetworkEmulator *emulator) {
    ifstream messageFile(messageFileName);
    string line;
    int srcId, destId;
    string message;
    while (getline(messageFile, line)) {
        stringstream ss(line);
        ss >> srcId >> destId;
        getline(ss, message);
        message = message.substr(1);  // there is a whitespace at the beginning
        emulator->addSinglePacket(make_tuple(srcId, destId, message));
    }
    messageFile.close();
}

int main(int argc, char** argv) {
    if (argc != 4) {
        printf("Usage: ./distvec topofile messagefile changesfile\n");
        return -1;
    }

    ofstream *fpOut = new ofstream("output.txt");
    NetworkEmulator emulator = parseTopologyAndCreateEmulator(argv[1]);
    parseChageFile(argv[3], &emulator);
    parseMessageFile(argv[2], &emulator);
    emulator.working(fpOut);
    fpOut->close();
    delete fpOut;

    return 0;
}

