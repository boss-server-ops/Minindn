#include "utility.hpp"
#include <iostream>
#include <vector>
#include <cmath> // For ceil
#include <numeric> // For std::accumulate
#include <cassert> // For assert
#include <algorithm>
#include <fstream>
#include <map>
#include <queue>
#include <sstream>
#include <string>
#include <limits>
#include <unordered_map>
#include <climits>
#include <stdexcept>







// Start of node list management, used for aggregation tree construction
// Return nodes except "con" and "forwarder"
std::vector <std::string> Utility::getContextInfo(std::string filename) {
    std::ifstream file(filename);
    std::vector <std::string> nodes;

    if (!file.is_open()) {
        std::cerr << "Fail to open file: " << filename << std::endl;
        return nodes;
    }

    std::string line;
    bool linkSection = false;

    while (getline(file, line)) {
        if (line == "router") {
            linkSection = true;
            continue;
        } else if (line == "link") {
            break;
        }

        if (linkSection && !line.empty() && line.find("forwarder") != 0 && line.find("con") != 0) {
            std::istringstream iss(line);
            std::string nodeName;
            iss >> nodeName;
            nodes.push_back(nodeName);
        }
    }

    return nodes;
}

std::vector <std::string> Utility::deleteNodes(std::vector <std::string> deletedList, std::vector <std::string> oldList) {
    std::vector <std::string> newList = oldList;

    auto newEnd = std::remove_if(newList.begin(), newList.end(),
                                 [&deletedList](const std::string &element) {
                                     return std::find(deletedList.begin(), deletedList.end(), element) !=
                                            deletedList.end();
                                 });

    newList.erase(newEnd, newList.end());

    return newList;
}

// End of node list management



// Start of link cost computation, used for all cases
// Define a custom type for easier readability
//typedef std::pair<std::string, std::string> NodePair;

// Function to initialize the adjacency list from the file
std::unordered_map<std::string, std::vector<std::pair<std::string, int>>> Utility::initializeGraph(std::string filename) {
    std::unordered_map<std::string, std::vector<std::pair<std::string, int>>> graph;

    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return graph;
    }

    std::string line;
    bool linkSection = false;

    while (getline(file, line)) {
        if (line == "link") {
            linkSection = true;
            continue;
        }

        if (linkSection && !line.empty()) {
            std::istringstream iss(line);
            std::string node1, node2;
            std::string speed; // Placeholder for speed
            int cost;
            std::string delay; // Placeholder for delay
            int priority; // Placeholder for priority

            iss >> node1 >> node2 >> speed >> cost >> delay >> priority;

            // Store the connections in an adjacency list
            graph[node1].push_back(std::make_pair(node2, cost));
            graph[node2].push_back(std::make_pair(node1, cost));
        }
    }

    file.close();
    return graph;
}


// Function to find the minimum link cost between any two nodes using Dijkstra's algorithm
int Utility::findLinkCost(const std::string& start, const std::string& end, const std::unordered_map<std::string, std::vector<std::pair<std::string, int>>> graph) {
    if (graph.find(start) == graph.end() || graph.find(end) == graph.end())
        return -1; // Nodes are not present in the graph

    std::priority_queue<std::pair<std::string, int>, std::vector<std::pair<std::string, int>>, Compare> pq;
    std::unordered_map<std::string, int> distances;

    // Initialize distances to maximum
    for (const auto& node : graph) {
        distances[node.first] = INT_MAX;
    }

    // Start from the source node
    pq.push({start, 0});
    distances[start] = 0;

    while (!pq.empty()) {
        auto current = pq.top();
        pq.pop();

        std::string currentNode = current.first;
        int currentCost = current.second;

        // Shortest path to end node found
        if (currentNode == end) {
            return currentCost;
        }

        // Traverse all adjacent nodes
        for (const auto& neighbor : graph.at(currentNode)) {
            std::string nextNode = neighbor.first;
            int nextCost = neighbor.second;
            int newCost = currentCost + nextCost;

            // Check if a cheaper path is found
            if (newCost < distances[nextNode]) {
                distances[nextNode] = newCost;
                pq.push({nextNode, newCost});
            }
        }
    }

    std::cout << "Error happened, no route is found!!!!!!!!!!!" << std::endl;
    return -1; // No path found
}



// Initialize the nodes from the bottom for first iteration, i.e. get all producers
std::vector<std::string> Utility::getProducers(std::string filename) {
    std::ifstream file(filename);
    std::vector<std::string> proNodes;
    std::string line;
    bool inRouterSection = false;

    if (!file) {
        std::cerr << "Unable to open file: " << filename << std::endl;
        return proNodes; // Return empty if file cannot be opened
    }

    while (std::getline(file, line)) {
        // Trim whitespace and check if the line is a section header
        line.erase(0, line.find_first_not_of(" \t\n\r\f\v")); // Trim leading whitespace
        if (line == "router") {
            inRouterSection = true;
        } else if (line == "link") {
            break; // Exit if we reach the "link" section
        } else if (inRouterSection) {
            // Check if the line starts with "pro"
            if (line.substr(0, 3) == "pro") {
                proNodes.push_back(line);
            }
        }
    }
    file.close();
    return proNodes;

}

// Compute the number of producers, used for compute model average on consumer
int Utility::countProducers(std::string filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open the file: " << filename << std::endl;
        return -1; // Return -1 or any specific error code to indicate failure
    }

    std::string line;
    bool inRouterSection = false;
    int producerCount = 0;

    // Read the file content line by line
    while (std::getline(file, line)) {
        // Trim the line from both sides (optional, depends on input cleanliness)
        line.erase(line.find_last_not_of(" \n\r\t") + 1);
        line.erase(0, line.find_first_not_of(" \n\r\t"));

        // Check for the starting point of the router section
        if (line == "router") {
            inRouterSection = true;
            continue;
        }

        // Check for the end of the router section (only "link" marks the end)
        if (line == "link") {
            break;  // Exit the loop as we are past the relevant router section
        }

        // Count the producer lines if we are within the router section
        if (inRouterSection && line.substr(0, 3) == "pro") {
            producerCount++;
        }
    }

    file.close(); // Close the file after reading
    return producerCount;
}

std::map<std::string, std::map<std::string, int>> Utility::GetAllLinkCost(std::string filename)
{
    std::map<std::string, std::map<std::string, int>> linkCostMatrix;
    std::vector<std::string> nodeList;
    std::unordered_map<std::string, std::vector<std::pair<std::string, int>>> graph;

    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Fail to open file." << filename << std::endl;
        return linkCostMatrix;
    }
    std::string line;
    bool isRouterSection = false;

    while (std::getline(file, line)) {
        line.erase(0, line.find_first_not_of(" \t\n\r\f\v")); // Trim leading whitespace
        if (line == "router") {
            isRouterSection = true;
        } else if (line == "link") {
            break;
        } else if (isRouterSection) {
            if (line.substr(0, 3) == "pro" || line.substr(0, 3) == "agg" || line.substr(0, 3) == "con")
                nodeList.push_back(line);
        }
    }

    // Get graph for further computation
    graph = initializeGraph(filename);

    // Compute link cost matrix
    for (const auto& node1: nodeList) {
        for (const auto& node2 : nodeList) {
            if (node1 == node2) {
                linkCostMatrix[node1][node2] = 0;
            } else if (linkCostMatrix[node1].find(node2) == linkCostMatrix[node1].end()) {
                int cost = findLinkCost(node1, node2, graph);
                linkCostMatrix[node1][node2] = cost;
                linkCostMatrix[node2][node1] = cost;
            }
        }
    }
    return linkCostMatrix;
}