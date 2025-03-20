#include "aggtree.hpp"

// Read topology file and parse
void AggTree::readTopology(const string &filename)
{
    ifstream file(filename);
    string line;
    bool inNodesSection = false;
    bool inLinksSection = false;

    while (getline(file, line))
    {
        // Trim whitespace from the line
        line.erase(0, line.find_first_not_of(" \t\n\r\f\v"));
        line.erase(line.find_last_not_of(" \t\n\r\f\v") + 1);

        // Skip empty lines and comments
        if (line.empty() || line[0] == '#')
        {
            continue;
        }

        // Check for section headers
        if (line == "[nodes]")
        {
            inNodesSection = true;
            inLinksSection = false;
            continue;
        }
        else if (line == "[links]")
        {
            inNodesSection = false;
            inLinksSection = true;
            continue;
        }

        // Parse nodes section
        if (inNodesSection)
        {
            istringstream iss(line);
            string parent, child;
            if (getline(iss, parent, ':') && getline(iss, child, '_'))
            {
                topology[parent].name = parent;
                // No children to add in nodes section
            }
        }

        // Parse links section
        if (inLinksSection)
        {
            istringstream iss(line);
            string parent, child;
            if (getline(iss, parent, ':') && getline(iss, child, ' '))
            {
                topology[parent].name = parent;
                topology[parent].children.push_back(child);
                topology[child].name = child;
            }
        }
    }
}

// Recursively find all paths
void AggTree::findPaths(const string &node, vector<string> currentPath)
{
    if (topology[node].children.empty())
    {
        paths.push_back(currentPath);
        return;
    }
    for (const string &child : topology[node].children)
    {
        currentPath.push_back(child);
        findPaths(child, currentPath);
        currentPath.pop_back();
    }
}

// Get tree topology
void AggTree::getTreeTopology(const string &filename, const string &root)
{
    readTopology(filename);
    rootChildCount = topology[root].children.size();
    std::cerr << "Root child count: " << rootChildCount << std::endl;
    vector<string> currentPath;
    currentPath.push_back(root);
    findPaths(root, currentPath);
    generateInterestNames();
}

// Generate interest names for each path and store in member variable

void AggTree::generateInterestNames()
{
    interestNames.clear();

    // Process each first-level component (direct children of the root)
    for (const string &firstLevelNode : topology["con0"].children)
    {
        Name interestName;
        interestName.append(firstLevelNode); // First component is the node name itself

        // Create a map of each node to its direct children
        std::map<string, std::vector<string>> directChildrenMap;

        // Fill the map with node relationships from our paths
        for (const auto &path : paths)
        {
            if (path.size() > 1 && path[1] == firstLevelNode)
            {
                // Process each parent-child relationship in the path
                for (size_t i = 1; i < path.size() - 1; ++i)
                {
                    directChildrenMap[path[i]].push_back(path[i + 1]);
                }
            }
        }

        // Remove duplicate children
        for (auto &entry : directChildrenMap)
        {
            std::sort(entry.second.begin(), entry.second.end());
            entry.second.erase(std::unique(entry.second.begin(), entry.second.end()), entry.second.end());
        }

        // Check if this node has any children
        auto it = directChildrenMap.find(firstLevelNode);
        if (it != directChildrenMap.end() && !it->second.empty())
        {
            // Build structure strings for each direct child
            std::vector<std::string> childStructures;
            for (const auto &childNode : it->second)
            {
                std::string childStructure = buildHierarchyString(childNode, directChildrenMap);
                childStructures.push_back(childStructure);
            }

            // Join the child structures with '+' and add as second component
            std::string combinedStructure;
            for (size_t i = 0; i < childStructures.size(); ++i)
            {
                if (i > 0)
                    combinedStructure += "+";
                combinedStructure += childStructures[i];
            }

            interestName.append(combinedStructure);
        }

        interestNames.push_back(interestName);
    }
}

// Helper function to build the hierarchy string
std::string AggTree::buildHierarchyString(const string &node, const std::map<string, std::vector<string>> &childrenMap)
{
    auto it = childrenMap.find(node);
    if (it == childrenMap.end() || it->second.empty())
    {
        return node; // No children
    }

    std::string result = node;

    // Only add brackets if there are children
    result += "(";

    bool firstChild = true;
    for (const auto &child : it->second)
    {
        if (!firstChild)
        {
            result += "+"; // Separate siblings with '+'
        }
        result += buildHierarchyString(child, childrenMap); // Recursively process children
        firstChild = false;
    }

    result += ")";

    return result;
}

std::vector<std::string> AggTree::getDirectChildren(const std::string &nodeName) const
{
    auto it = topology.find(nodeName);
    if (it != topology.end())
    {
        return it->second.children;
    }
    return {};
}

// int main()
// {
//     AggTree tree;

//     // Get tree topology
//     tree.getTreeTopology("../../topologies/Linetest.conf", "con0");

//     // Output direct children of con0
//     cout << "Direct children of con0: ";
//     for (const string &child : tree.topology["con0"].children)
//     {
//         cout << child << " ";
//     }
//     cout << endl;

//     // Output all paths from con0 to leaf nodes
//     cout << "Complete paths from con0 to leaf nodes:" << endl;
//     for (const vector<string> &path : tree.paths)
//     {
//         for (const string &node : path)
//         {
//             cout << node << " ";
//         }
//         cout << endl;
//     }

//     // Generate interest names for each path
//     tree.generateInterestNames();
//     cout << "Generated Interest Names:" << endl;
//     cout << "Generated Interest Names:" << endl;
//     for (const auto &interestName : tree.interestNames)
//     {
//         // Decode the URI to display the original characters
//         string decodedUri = interestName.toUri();

//         // Decode common URL-encoded characters
//         size_t pos = 0;

//         // Decode pipe character
//         while ((pos = decodedUri.find("%7C", pos)) != string::npos)
//         {
//             decodedUri.replace(pos, 3, "|");
//             pos += 1;
//         }

//         // Decode left parenthesis
//         pos = 0;
//         while ((pos = decodedUri.find("%28", pos)) != string::npos)
//         {
//             decodedUri.replace(pos, 3, "(");
//             pos += 1;
//         }

//         // Decode right parenthesis
//         pos = 0;
//         while ((pos = decodedUri.find("%29", pos)) != string::npos)
//         {
//             decodedUri.replace(pos, 3, ")");
//             pos += 1;
//         }

//         // Decode plus sign
//         pos = 0;
//         while ((pos = decodedUri.find("%2B", pos)) != string::npos)
//         {
//             decodedUri.replace(pos, 3, "+");
//             pos += 1;
//         }

//         cout << decodedUri << endl;
//     }

//     return 0;
// }