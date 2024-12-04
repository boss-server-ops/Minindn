#include <iostream>
#include <vector>
#include <cmath> // For ceil
#include <numeric> // For std::accumulate
#include <cassert> // For assert
#include <fstream>
#include <map>
#include <queue>
#include <sstream>
#include <string>
#include <limits>
#include <unordered_map>
#include <climits>
#include <set>
#include <algorithm>

class AggregationTree {
public:
    AggregationTree(std::string file);
    virtual ~AggregationTree(){};

    std::string findCH(std::vector<std::string> clusterNodes, std::vector<std::string> clusterHeadCandidate, std::string client);

    bool aggregationTreeConstruction(std::vector<std::string> dataPointNames, int C);

    // Global variables
    std::unordered_map<std::string, std::vector<std::pair<std::string, int>>> graph;
    //std::string filename = "src/ndnSIM/examples/topologies/DataCenterTopology.txt";
    std::string filename;
    std::vector<std::string> fullList;
    std::vector<std::string> CHList;
    std::string globalClient = "con0";
    std::map<std::string, std::vector<std::string>> aggregationAllocation;
    std::vector<std::vector<std::string>> noCHTree;
    std::map<std::string, std::map<std::string, int>> linkCostMatrix;

};