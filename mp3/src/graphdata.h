#ifndef GRAPHDATA_H
#define GRAPHDATA_H

#include <fstream>
#include <set>
#include <unordered_map>
#include <algorithm>

/**
 * Object that converts topology file data into graph structure
 * vertices are stored as unsigned integers in a set
 * edges are stored in form <source, <dest, cost>>
 */
class GraphData {
private:
    std::set<size_t> vertices;
    std::unordered_map<size_t, std::set<size_t>> edges;
	std::unordered_map<size_t, std::unordered_map<size_t, size_t>> edgeWeights;
public:
	/**
	 * Constructor for getting graph data
	 * input vertices - set of node vertices
	 * input edges - edges stored as <source, [dests]>
	 * input edgeWeights - edge weights stored as <source, <dest, int>>, the source < dest always
	 */
	GraphData(std::set<size_t> vertices, 
				std::unordered_map<size_t, std::set<size_t>> edges,
				std::unordered_map<size_t, std::unordered_map<size_t, size_t>> edgeWeights);

	GraphData();

	/**
	 * Gets the neighbors of a graph given a source
	 * input source - node we are looking for the neighbors of
	 * output - set of neighbors of the source or empty set if it doesn't exist
	 */
	std::set<size_t> getNeighbors(size_t source);

	/**
	 * Gets the edge weight of a specified edge
	 * input source - source node of edge
	 * input dest - dest node of edge
	 * output - edge weight of graph or -1 if that edge does not exist
	 */
	int getEdgeWeight(size_t source, size_t dest);

	/**
	 * insert an edge into the graph 
	 * input source - source node of edge
	 * input dest - dest node of edge
	 * input cost - cost of the edge
	 */
	void insertEdge(size_t source, size_t dest, size_t cost);

	/**
	 * update an edge in the graph
	 * input source - source node of edge
	 * input dest - dest node of edge
	 * input cost - cost of edge
	 * output - true if updated, false if not exist
	 */
	bool updateEdge(size_t source, size_t dest, size_t cost);

	/**
	 * delete an edge from the graph
	 * input source - source node of edge
	 * input dest - dest node of edge
	 * output - true if deleted, false if not exist
	 */
	bool deleteEdge(size_t source, size_t dest);

	/**
	 * Gets the vertices of the graph
	 * output - set of vertices in graph
	 */
	std::set<size_t> getVertices();

	/**
	 * Gets all the edges of the graph
	 * output - map of every node and its edges 
	 */
    std::unordered_map<size_t, std::set<size_t>> getEdges();

	/**
	 * Gets all the edge weights of the graph
	 * output - map of every node and its edge weights
	 */
    std::unordered_map<size_t, std::unordered_map<size_t, size_t>> getEdgeWeights();
};

/**
 * parse topology file into our graph data structure
 * input fname - file name of the graph topology to parse
 * output pointer to a graphData structure
 */
GraphData* parseGraphData(std::string fname);

#endif