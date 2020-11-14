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
    // loop through graph
    while (finishedNodes.size() < vertices.size()) {
        // find smallest node not in finishedNodes
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
        // add node to finishedNodes
        finishedNodes.insert(smallestNode);
        // update dists for all neighbors of node not in finishedNodes
        for (size_t neighbor : graph->getNeighbors(smallestNode)) {
            // make sure INT_MAX does not wrap around to negative number
            int newDist = dists.at(smallestNode) == INT_MAX ? 
                            INT_MAX : dists.at(smallestNode) + graph->getEdgeWeight(smallestNode, neighbor);
            // update if new distance is shorter than current
            // tiebreak by choosing path with smaller node id
            if (newDist <= dists.at(neighbor)) {
                // ignore if both have infinite distance
                if (newDist == INT_MAX &&  dists.at(neighbor) == INT_MAX) {
                    continue;
                }
                // tiebreak
                if (preds.find(neighbor) != preds.end() && 
                    preds.at(neighbor) <= smallestNode && 
                    newDist == dists.at(neighbor)) {
                    continue;
                }
                dists.at(neighbor) = std::min(dists.at(neighbor), newDist);

                if (preds.find(neighbor) == preds.end()) {
                    preds.insert(std::pair<size_t, size_t>(neighbor, smallestNode));
                } else {
                    preds.at(neighbor) = smallestNode;
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

int main(int argc, char** argv) {
    //printf("Number of arguments: %d", argc);
    if (argc != 4) {
        printf("Usage: ./linkstate topofile messagefile changesfile\n");
        return -1;
    }
    // FILE *fpOut;
    // fpOut = fopen("output.txt", "w");
    std::ofstream outputfile;
    outputfile.open("output.txt");
    // open topofile to get graph topology
    GraphData* topoGraph = parseGraphData(argv[1]);
    // hold every dijkstra data output in a map
    std::unordered_map<size_t, linkStateInfo> dijkstraMap;
    // loop through every node (from 1 to N)
    const std::set<size_t> vertices = topoGraph->getVertices();
    for (size_t vertex : vertices) {
        // use dijkstra for each node to get forwarding table 
        struct linkStateInfo pathInfo = dijkstra(topoGraph, vertex);
        dijkstraMap.insert(std::pair<size_t, linkStateInfo>(vertex, pathInfo));
        // output the forwarding table
        outputTable(outputfile, vertex, vertices, dijkstraMap.at(vertex));
    }

    // loop through each message line in 
    std::ifstream messagefile;
    messagefile.open(argv[2]);
    std::string line;
    size_t message_source;
    size_t message_dest;
    std::string message;
    while (std::getline(messagefile, line)) {
        std::stringstream ss(line);
        ss >> message_source >> message_dest;
        message = line.substr(4);
        struct linkStateInfo info = dijkstraMap.at(message_source);
        outputMessage(outputfile, message_source, message_dest, message, info);
    }
    messagefile.close();

    // loop through changes file
    std::ifstream changesfile;
    changesfile.open(argv[3]);
    size_t changes_source;
    size_t changes_dest;
    int changes_cost;
    while (changesfile >> changes_source >> changes_dest >> changes_cost) {
        // edit the graph topology
        if (changes_cost == DELETE_CODE) {
            topoGraph->deleteEdge(changes_source, changes_dest);
        } else if (topoGraph->getEdgeWeight(changes_source, changes_dest) != -1) { // update edge
            topoGraph->updateEdge(changes_source, changes_dest, changes_cost);
        } else { // add edge
            topoGraph->insertEdge(changes_source, changes_dest, changes_cost);
        }
        // loop through every node (from 1 to N)
        dijkstraMap.clear();
        for (size_t vertex : vertices) {
            // use dijkstra for each node to get forwarding table
            struct linkStateInfo pathInfo = dijkstra(topoGraph, vertex);
            dijkstraMap.insert(std::pair<size_t, linkStateInfo>(vertex, pathInfo));
            // output the forwarding table
            outputTable(outputfile, vertex, vertices, dijkstraMap.at(vertex));
        }
        // loop through each message
        messagefile.clear();
        messagefile.open(argv[2]);
        while (std::getline(messagefile, line)) {
            std::stringstream ss(line);
            ss >> message_source >> message_dest;
            message = line.substr(4);
            struct linkStateInfo info = dijkstraMap.at(message_source);
            // write to output each message
            outputMessage(outputfile, message_source, message_dest, message, info);
        }
        messagefile.close();
    }
    changesfile.close();

    // fclose(fpOut);
    delete topoGraph;
    return 0;
}
