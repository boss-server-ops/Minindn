#ifndef IMAGG_AGGTREE_HPP
#define IMAGG_AGGTREE_HPP

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <string>
#include <ndn-cxx/interest.hpp>

using namespace std;
using namespace ndn;

// Define data structure
struct Node
{
    string name;
    vector<string> children;
};

// Define AggTree class
class AggTree
{
public:
    // Constructor
    AggTree() = default;

    // Public methods
    void readTopology(const string &filename);
    void findPaths(const string &node, vector<string> currentPath);
    void getTreeTopology(const string &filename, const string &root);
    void generateInterestNames();
    std::string buildHierarchyString(const string &node, const std::map<string, std::vector<string>> &childrenMap);
    std::vector<std::string> getDirectChildren(const std::string &nodeName) const;

    // Public members
    unordered_map<string, Node> topology;
    vector<vector<string>> paths;
    vector<Name> interestNames;
    size_t rootChildCount = 0;
};

#endif // IMAGG_AGGTREE_HPP