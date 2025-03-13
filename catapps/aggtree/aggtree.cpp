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
    vector<string> currentPath;
    currentPath.push_back(root);
    findPaths(root, currentPath);
    generateInterestNames();
}

// Generate interest names for each path and store in member variable
void AggTree::generateInterestNames()
{
    interestNames.clear();
    unordered_map<string, vector<string>> aggregatedPaths;

    // Aggregate paths by their first component after the root
    for (const auto &path : paths)
    {
        if (path.size() > 1)
        {
            string firstComponent = path[1];
            string aggregatedPath;
            for (size_t i = 2; i < path.size(); ++i)
            {
                aggregatedPath += path[i];
                if (i < path.size() - 1)
                {
                    aggregatedPath += "/";
                }
            }
            aggregatedPaths[firstComponent].push_back(aggregatedPath);
        }
    }

    // Generate interest names
    for (const auto &entry : aggregatedPaths)
    {
        Name interestName;
        interestName.append(entry.first);
        string combinedPaths;
        for (const auto &path : entry.second)
        {
            if (!combinedPaths.empty())
            {
                combinedPaths += "|";
            }
            combinedPaths += path;
        }
        interestName.append(combinedPaths);
        interestNames.push_back(interestName);
    }
}

// int main()
// {
//     AggTree tree;

//     // Get tree topology
//     tree.getTreeTopology("../../topologies/Customtest.conf", "con0");

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
//     for (const auto &interestName : tree.interestNames)
//     {
//         // Decode the URI to display the original characters
//         string decodedUri = interestName.toUri();
//         size_t pos = 0;
//         while ((pos = decodedUri.find("%7C", pos)) != string::npos)
//         {
//             decodedUri.replace(pos, 3, "|");
//             pos += 1;
//         }
//         cout << decodedUri << endl;
//     }

//     return 0;
// }