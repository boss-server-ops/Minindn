#include "aggtree.hpp"
#include <stack>

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

// aggtree.cpp（关键修改部分）
void AggTree::generateInterestNames()
{
    interestNames.clear();

    // 第一步：构建节点到子路径的映射
    std::map<std::string, std::vector<std::vector<std::string>>> nodeSubpaths;
    for (const auto &path : paths)
    {
        if (path.size() < 2)
            continue;
        std::string firstNode = path[1];
        std::vector<std::string> subpath(path.begin() + 1, path.end());
        nodeSubpaths[firstNode].push_back(subpath);
    }

    // 第二步：处理每个第一层子节点
    for (const auto &child : topology[rootName].children)
    {
        // 构建节点关系树
        std::map<std::string, std::vector<std::string>> nodeChildren;
        for (const auto &subpath : nodeSubpaths[child])
        {
            for (size_t i = 0; i < subpath.size() - 1; ++i)
            {
                std::string current = subpath[i];
                std::string next = subpath[i + 1];
                auto &children = nodeChildren[current];
                if (std::find(children.begin(), children.end(), next) == children.end())
                {
                    children.push_back(next);
                }
            }
        }

        // 第三步：生成结构化字符串（无递归函数）
        Name interestName;
        interestName.append(child);

        if (!nodeChildren[child].empty())
        {
            std::string structure;
            std::stack<std::pair<std::string, int>> nodeStack; // <节点名, 子节点索引>
            nodeStack.push({child, 0});

            while (!nodeStack.empty())
            {
                auto [currentNode, childIdx] = nodeStack.top();
                nodeStack.pop();

                // 添加节点开括号
                if (childIdx == 0)
                {
                    structure += "(";
                }

                // 处理所有子节点
                const auto &children = nodeChildren[currentNode];
                for (; childIdx < children.size(); ++childIdx)
                {
                    const auto &childNode = children[childIdx];
                    structure += childNode;

                    // 如果子节点有后代，准备处理嵌套
                    if (nodeChildren.count(childNode))
                    {
                        nodeStack.push({currentNode, childIdx + 1}); // 保存当前进度
                        nodeStack.push({childNode, 0});              // 处理子节点
                        break;
                    }

                    // 添加分隔符或闭括号
                    if (childIdx != children.size() - 1)
                    {
                        structure += "+";
                    }
                    else
                    {
                        structure += ")";
                    }
                }
            }
            interestName.append(structure);
        }

        interestNames.push_back(interestName);
    }
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