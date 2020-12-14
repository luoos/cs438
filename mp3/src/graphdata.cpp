/*
 * Author: Carl Guo
 */
#include "graphdata.h"

GraphData::GraphData(std::set<size_t> vertices,
					std::unordered_map<size_t, std::set<size_t>> edges,
					std::unordered_map<size_t, std::unordered_map<size_t, size_t>> edgeWeights) {
	this->vertices = vertices;
	this->edges = edges;
	this->edgeWeights = edgeWeights;
}

GraphData::GraphData() {
	this->vertices = std::set<size_t>();
	this->edges = std::unordered_map<size_t, std::set<size_t>>();
	this->edgeWeights = std::unordered_map<size_t, std::unordered_map<size_t, size_t>>();
}

std::set<size_t> GraphData::getNeighbors(size_t source) {
	if (edges.find(source) != edges.end()) {
		return edges.at(source);
	}
	return std::set<size_t>();
}

int GraphData::getEdgeWeight(size_t source, size_t dest) {
	if (source == dest) {
		return 0;
	}

	if (source > dest) {
		std::swap(source, dest);
	}

	if (this->edgeWeights.find(source) != this->edgeWeights.end()) {
		if (this->edgeWeights.at(source).find(dest) != this->edgeWeights.at(source).end()) {
			return this->edgeWeights.at(source).at(dest);
		}
	}
	// if the edge doesn't exist return negative
	return -1;
}

void GraphData::insertEdge(size_t source, size_t dest, size_t cost) {
	// add new vertices to set
	this->vertices.insert(source);
	this->vertices.insert(dest);
	// add both to edges
	if (this->edges.find(source) != this->edges.end()) {
		this->edges.at(source).insert(dest);
	} else {
		std::set<size_t> neighbors;
		neighbors.insert(dest);
		this->edges.insert(std::pair<size_t, std::set<size_t>>(source, neighbors));
	}
	// add dest edge neighbor to map
	if (this->edges.find(dest) != edges.end()) {
		this->edges.at(dest).insert(source);
	} else {
		std::set<size_t> neighbors;
		neighbors.insert(source);
		this->edges.insert(std::pair<size_t, std::set<size_t>>(dest, neighbors));
	}
	// add to edgeWeight
	if (source > dest) {
		std::swap(source, dest);
	}
	if (this->edgeWeights.find(source) != this->edgeWeights.end()) {
		this->edgeWeights.at(source).insert(std::pair<size_t, size_t>(dest, cost));
	} else {
		std::unordered_map<size_t, size_t> destWithWeight;
		destWithWeight.insert(std::pair<size_t, size_t>(dest, cost));
		this->edgeWeights.insert(std::pair<size_t, std::unordered_map<size_t, size_t>>(source, destWithWeight));
	}
}

bool GraphData::updateEdge(size_t source, size_t dest, size_t cost) {
	// check if edge exists
	if (this->getEdgeWeight(source, dest) == -1) {
		return false;
	}
	// update edge weight
	if (source > dest) {
		std::swap(source, dest);
	}
	this->edgeWeights.at(source).at(dest) = cost;
	return true;
}

bool GraphData::deleteEdge(size_t source, size_t dest) {
	// check if edge exists
	if (this->getEdgeWeight(source, dest) == -1) {
		return false;
	}

	if (source > dest) {
		std::swap(source, dest);
	}
	// delete edge from edges
	this->edges.at(source).erase(dest);
	this->edges.at(dest).erase(source);
	// delete edge from edgeWeight
	this->edgeWeights.at(source).erase(dest);
	return true;
}

std::set<size_t> GraphData::getVertices() {
	return this->vertices;
}

std::unordered_map<size_t, std::set<size_t>> GraphData::getEdges() {
	return this->edges;
}

std::unordered_map<size_t, std::unordered_map<size_t, size_t>> GraphData::getEdgeWeights() {
	return this->edgeWeights;
}

GraphData* parseGraphData(std::string fname) {
    std::ifstream graphFile(fname);
    GraphData* graph = new GraphData();

    unsigned int source;
    unsigned int dest;
    unsigned int cost;
    // retrieve graph info line by line
    while (graphFile >> source >> dest >> cost) {
        graph->insertEdge(source, dest, cost);
    }
	graphFile.close();

    return graph;
}
