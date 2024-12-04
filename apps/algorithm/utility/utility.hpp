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



namespace Utility{

    // Comparator for priority queue
    struct Compare {
        bool operator()(const std::pair<std::string, int>& p1, const std::pair<std::string, int>& p2) {
            return p1.second > p2.second;
        }
    };

    std::vector <std::string> getContextInfo(std::string filename);

    std::vector <std::string> deleteNodes(std::vector <std::string> deletedList, std::vector <std::string> oldList);

    std::unordered_map<std::string, std::vector<std::pair<std::string, int>>> initializeGraph(std::string filename);

    int findLinkCost(const std::string& start, const std::string& end, const std::unordered_map<std::string, std::vector<std::pair<std::string, int>>> graph);

    std::vector<std::string> getProducers(std::string filename);

    int countProducers(std::string filename);

    std::map<std::string, std::map<std::string, int>> GetAllLinkCost(std::string filename);

};