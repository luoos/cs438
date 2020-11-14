#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <unordered_set>
#include <iostream>
#include <sstream>
#include <fstream>
#include <climits>
#include <vector>
#include "graphdata.h"

#define DELETE_CODE -999

/**
 * Structure that holds return objects needed for dijkstra path construction
 * predecessors are stored in map as <Key: node, Value: predecessor> 
 * distances to each node are stored as <Key: node, Value: distance>
 */
struct linkStateInfo {
    // predecessors
    std::unordered_map<size_t, size_t> preds;
    // distances
    std::unordered_map<size_t, int> dists;
};

/**
 * Initialize all data structures before running dijkstra iteratiosn
 * input source - node we want to find all shortest paths to
 * input graph - pointer to our graph topology
 * input finishedNodes - set of all nodes finished inspected
 * input dists - map of all shortest path costs from source to node
 * input preds - map of all immediate predecessors of nodes for shortest path
 */ 
void initializeDijkstra(size_t source, 
                        GraphData* graph, 
                        std::unordered_set<size_t>& finishedNodes, 
                        std::unordered_map<size_t, int>& dists,
                        std::unordered_map<size_t, size_t>& preds) {
    finishedNodes.insert(source);
    std::set<size_t> sourceNeighbors = graph->getNeighbors(source);
    const std::set<size_t> vertices = graph->getVertices();
    for (size_t vertex : vertices) {
        if (vertex == source) {
            continue;
        }
        if (sourceNeighbors.find(vertex) != sourceNeighbors.end()) {
            size_t cost = (size_t) graph->getEdgeWeight(source, vertex);
            dists.insert(std::pair<size_t, int>(vertex, cost));
            preds.insert(std::pair<size_t, size_t>(vertex, source));
        } else {
            dists.insert(std::pair<size_t, int>(vertex, INT_MAX));
        }
    }
    preds.insert(std::pair<size_t, size_t>(source, source));
    dists.insert(std::pair<size_t, int>(source, 0));    
}

/**
 * finds the smallest distance node not in finished set
 * input vertices - set of all vertices in graph
 * input finishedNodes - unordered set of all finished nodes
 * input dists - map of all nodes and their current shortest distance to source
 * output - unfinished node with smallest current path distance
 */ 
size_t findSmallestUnfinishedNode(const std::set<size_t>& vertices, 
                                    const std::unordered_set<size_t>& finishedNodes, 
                                    const std::unordered_map<size_t, int>& dists) {
    size_t smallestNode = 0;
    int smallestDist = -1;
    for (size_t vertex : vertices) {
        if (finishedNodes.find(vertex) == finishedNodes.end()) {
            // initialize the smallestNode and smallestDist values
            if (smallestDist == -1) {
                smallestNode = vertex;
                smallestDist = dists.at(vertex);
            } 
            // update only if the new distance is smaller
            if (smallestDist > dists.at(vertex)) {
                smallestNode = vertex;
                smallestDist = dists.at(vertex);
            }            
        }
    }
    return smallestNode;
}

/**
 * Checks if both nodes are infinite distance, if so then do not compare and skip updating distance
 * input newDist - current distance of smallest unfinished node
 * input neighbor - neighbor of smallest unfinished node
 * input dists - map of all nodes and their current smallest distances to source
 * output - if both nodes have infinite distance as smallest current path length
 */ 
bool isBothNodesInfiniteDist(int newDist, int currNeighborDist) {
    if (newDist == INT_MAX && currNeighborDist == INT_MAX) {
        return true;
    }
    return false;
}

/**
 * Checks if newDist is equal to neighbor node's current distance and does not break tie
 * input smallestNode - current smallest unfinished node to tets
 * input newDist - new distance of smallest unfinished node to test if should update
 * input neighbor - neighbor we are testing if we should update
 * input preds - map of nodes and their immediate predecessors in current shortest path
 * input dists - map of nodes and their current shortest path distances
 * output - whether new distance will break the tie or not
 */ 
bool isNewDistEqualButNotBreakTie(size_t smallestNode,
                            int newDist,
                            int currNeighborDist,
                            size_t neighbor,
                            std::unordered_map<size_t, size_t>& preds) {
    // tiebreak
    if (preds.find(neighbor) != preds.end() && 
        preds.at(neighbor) <= smallestNode && 
        newDist == currNeighborDist) {
        return true;
    }
    return false;
}

bool shouldUpdateDist(int newDist, 
                        int currNeighborDist,
                        size_t smallestNode,
                        size_t neighbor,
                        std::unordered_map<size_t, size_t>& preds) {
    // ignore if both have infinite distance or is equal and does not break tie
    if (isBothNodesInfiniteDist(newDist, currNeighborDist) || 
        isNewDistEqualButNotBreakTie(smallestNode, newDist, currNeighborDist, neighbor, preds)) {
        return false;
    }
    return true;
}

/**
 * Performs Dijkstra's algorithm on graph given a source node
 * input graph - Graph object we are performing dijkstra on
 * input source - source node we want to calculate distances from
 * output - Dijkstra info of distances to source and predecessors of each node 
 */
linkStateInfo dijkstra(GraphData* graph, size_t source) {
    std::unordered_map<size_t, int> dists;
    std::unordered_map<size_t, size_t> preds;
    std::unordered_set<size_t> finishedNodes;
    // initialize data
    initializeDijkstra(source, graph, finishedNodes, dists, preds);
    // loop through graph
    const std::set<size_t> vertices = graph->getVertices();
    while (finishedNodes.size() < vertices.size()) {
        size_t smallestNode = findSmallestUnfinishedNode(vertices, finishedNodes, dists);
        finishedNodes.insert(smallestNode);

        for (size_t neighbor : graph->getNeighbors(smallestNode)) {
            // make sure INT_MAX does not wrap around to negative number
            int newDist = dists.at(smallestNode) == INT_MAX ? 
                            INT_MAX : dists.at(smallestNode) + graph->getEdgeWeight(smallestNode, neighbor);

            int currNeighborDist = dists.at(neighbor);
            if (newDist <= currNeighborDist) {
                if (shouldUpdateDist(newDist, currNeighborDist, smallestNode, neighbor, preds)) {
                    dists.at(neighbor) = std::min(currNeighborDist, newDist);
                    if (preds.find(neighbor) == preds.end()) {
                        preds.insert(std::pair<size_t, size_t>(neighbor, smallestNode));
                    } else {
                        preds.at(neighbor) = smallestNode;
                    }             
                }
            }
        }
    }

    struct linkStateInfo dijkstraData;
    dijkstraData.dists = dists;
    dijkstraData.preds = preds;
    return dijkstraData;
}

/**
 * Sends the forwarding table for each source node on graph to output file
 * input outfile - file stream we are sending our output to
 * input source - source node of the forwarding table
 * input vertices - all the vertices in the graph
 * input pathInfo - structure of dijkstra predecessors and distances to reconstruct paths
 */ 
void outputTable(std::ofstream& outputfile, size_t source, std::set<size_t> vertices, struct linkStateInfo& pathInfo) {
    // output the forwarding table
    for (size_t dest : vertices) {
        //std::cout << "check: " << dest;
        if (pathInfo.preds.find(dest) != pathInfo.preds.end()) {
            //std::cout << " table: " << dest;
            // derive nexthop
            size_t nexthop = dest;
            while (source != pathInfo.preds.at(nexthop)) {
                nexthop = pathInfo.preds.at(nexthop);
            }
            outputfile << dest << " " << nexthop << " " << pathInfo.dists.at(dest) << "\n"; 
        }
    }
}

/**
 * Sends message output for each source and dest to output file
 * input outfile - file stream we are sending our output to
 * input source - source node of message
 * input dest - dest node of message
 * input message - message portion of messagefile
 * input info - structure of dijkstra predecessors and distances to reconstruct paths
 */ 
void outputMessage(std::ofstream& outputfile, size_t source, size_t dest, std::string message, linkStateInfo& info) {
    // if unreachable output infinite cost and unreachable hops
    if (info.preds.find(dest) == info.preds.end()) {
        outputfile << "from " << source << " to " << dest << " cost infinite hops unreachable message " << message << "\n";
        return;
    }
    // if reachable get its cost and path
    int cost = info.dists.at(dest);
    std::vector<size_t> path;
    size_t nexthop = dest;
    while (source != nexthop) {
        nexthop = info.preds.at(nexthop);
        path.push_back(nexthop);
    }
    // build output
    outputfile << "from " << source << " to " << dest << " cost " << cost << " hops ";
    // construct path hops
    for (int i = path.size() - 1; i >= 0; i--) {
        outputfile << path[i] << " ";
    }
    outputfile << "message " << message << "\n";
}

/**
 * create forwarding tables given dijkstra info and output them
 * input graph - parsed graph from topology file
 * input dijkstraMap - map holding all dijkstra info for each source node
 * input outputfile - reference to output file stream we write to
 */ 
void constructAndOutputForwardingTables(GraphData* graph, 
                                    std::unordered_map<size_t, linkStateInfo>& dijkstraMap, 
                                    std::ofstream& outputfile) {
    const std::set<size_t> vertices = graph->getVertices();
    for (size_t vertex : vertices) {
        // use dijkstra for each node to get forwarding table 
        struct linkStateInfo pathInfo = dijkstra(graph, vertex);
        dijkstraMap.insert(std::pair<size_t, linkStateInfo>(vertex, pathInfo));
        // output the forwarding table
        outputTable(outputfile, vertex, vertices, dijkstraMap.at(vertex));
    }
}

/**
 * construct output messages and send them out
 * input messagefile - input file stream that gets each message line
 * input dijkstraMap - map holding all dijkstra info for each source node
 * input outputfile - reference to output file stream we write to
 */ 
void constructAndOutputMessages(std::ifstream& messagefile, 
                                std::unordered_map<size_t, linkStateInfo>& dijkstraMap,
                                std::ofstream& outputfile) {
    std::string line;
    size_t message_source;
    size_t message_dest;
    std::string message;
    while (std::getline(messagefile, line)) {
        std::stringstream ss(line);
        ss >> message_source >> message_dest;
        int afterFirstSpaceInd = line.find(' ') + 1;
        int afterSecSpaceInd = line.find(' ', afterFirstSpaceInd) + 1;
        message = line.substr(afterSecSpaceInd);
        struct linkStateInfo info = dijkstraMap.at(message_source);
        outputMessage(outputfile, message_source, message_dest, message, info);
    }
}

/**
 * Given changes from the changes file, determine what edit will be made to graph topology
 * input changes_source - source node of edge that will be changed
 * input changes_dest - dest node of edge that will be change
 * input changes_cost - new cost of source to dest edge (if -999, then edge is deleted)
 * input graph - current graph topology
 */ 
void editGraphTopology(size_t changes_source, size_t changes_dest, int changes_cost, GraphData* graph) {
    // edit the graph topology
    if (changes_cost == DELETE_CODE) {
        graph->deleteEdge(changes_source, changes_dest);
    } else if (graph->getEdgeWeight(changes_source, changes_dest) != -1) { // update edge
        graph->updateEdge(changes_source, changes_dest, changes_cost);
    } else { // add edge
        graph->insertEdge(changes_source, changes_dest, changes_cost);
    }
}

/**
 * Parse changes from changesfile and perform changes and output changed graph info
 * input graph - current graph topology
 * input dijkstraMap - map holding all dijkstra info for each source node
 * input changesfile - input file stream that gets each change to graph
 * input messagefile - input file stream that gets each message line
 * input messagefilename - name of the message file
 * input outputfile - reference to output file stream we write to
 */ 
void performChangesAndOutput(GraphData* graph,
                                std::unordered_map<size_t, linkStateInfo>& dijkstraMap,
                                std::ifstream& changesfile,
                                std::ifstream& messagefile,
                                std::string messagefilename,
                                std::ofstream& outputfile) {
    size_t changes_source;
    size_t changes_dest;
    int changes_cost;
    while (changesfile >> changes_source >> changes_dest >> changes_cost) {
        // edit the graph topology
        editGraphTopology(changes_source, changes_dest, changes_cost, graph);
        // loop through every node (from 1 to N)
        dijkstraMap.clear();
        constructAndOutputForwardingTables(graph, dijkstraMap, outputfile);
        // loop through each message
        messagefile.clear();
        messagefile.open(messagefilename);
        constructAndOutputMessages(messagefile, dijkstraMap, outputfile);
        messagefile.close();
    }
}

int main(int argc, char** argv) {
    //printf("Number of arguments: %d", argc);
    if (argc != 4) {
        printf("Usage: ./linkstate topofile messagefile changesfile\n");
        return -1;
    }
    std::string topofileName = argv[1];
    std::string messagefileName = argv[2];
    std::string changesfileName = argv[3];
    // open output file
    std::ofstream outputfile;
    outputfile.open("output.txt");
    // parse graph data and find forwarding tables
    GraphData* topoGraph = parseGraphData(topofileName);
    std::unordered_map<size_t, linkStateInfo> dijkstraMap;
    constructAndOutputForwardingTables(topoGraph, dijkstraMap, outputfile);
    // output messages
    std::ifstream messagefile;
    messagefile.open(messagefileName);
    constructAndOutputMessages(messagefile, dijkstraMap, outputfile);
    messagefile.close();
    // loop through changes file
    std::ifstream changesfile;
    changesfile.open(changesfileName);
    performChangesAndOutput(topoGraph, dijkstraMap, changesfile, messagefile, messagefileName, outputfile);
    changesfile.close();
    // deallocate memory and terminate
    delete topoGraph;
    return 0;
}
