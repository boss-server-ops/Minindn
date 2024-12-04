#include "../include/AggregationTree.hpp"
#include "../include/regularized_k_means.hpp"
#include "../utility/utility.hpp"

#include <iostream>
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
#include <stdexcept>
#include <algorithm>
#include <unistd.h>
#include <vector>
#include <chrono>





AggregationTree::AggregationTree(std::string file){
    filename = file;
    fullList = Utility::getContextInfo(filename);
    CHList = fullList;
    linkCostMatrix = Utility::GetAllLinkCost(filename);
    //graph = Utility::initializeGraph(filename);
    //std::cout << "Finish initialization!" << std::endl;
}


std::string AggregationTree::findCH(std::vector<std::string> clusterNodes, std::vector<std::string> clusterHeadCandidate, std::string client) {

    std::string CH = client;
    // Initiate a large enough cost
    int leastCost = 1000;

    for (const auto& headCandidate : clusterHeadCandidate) {
        bool canBeCH = true;

        for (const auto& node : clusterNodes) {
            if (linkCostMatrix[node][client] < linkCostMatrix[node][headCandidate]) {
                canBeCH = false;
                break;
            }
        }

        // This candidate is closer to client
        long long totalCost = 0;
        if (canBeCH) {
            for (const auto& node : clusterNodes) {
                totalCost += linkCostMatrix[node][headCandidate];
            }
            int averageCost = static_cast<int>(totalCost / clusterNodes.size());

            if (averageCost < leastCost) {
                leastCost = averageCost;
                CH = headCandidate;
            }
        }
    }

    if (CH == client) {
        std::cerr << "No CH is found for current cluster!!!!!!!!!!!" << std::endl;
        return CH;
    } else {
        std::cout << "CH " << CH << " is chosen." << std::endl;
        return CH;
    }


}

// End of cluster head construction

bool AggregationTree::aggregationTreeConstruction(std::vector<std::string> dataPointNames, int C) {

    // Compute N
    int N = dataPointNames.size();

    // Compute the number of clusters k
    int numClusters = static_cast<int>(ceil(static_cast<double>(N) / C));

    // Create a vector to store cluster assignments
    std::vector<int> clusterAssignment(N);
    for (int i = 0; i < N; ++i) {
        clusterAssignment[i] = i % numClusters; // Cluster assignment based on modulus operation
    }

/*    // Output the cluster assignments
    std::cout << "Cluster initialization." << std::endl;
    std::cout << "There are " << numClusters << " clusters." << std::endl;
    for (int i = 0; i < N; ++i) {
        std::cout << "Data point " << dataPointNames[i] << " is in cluster " << clusterAssignment[i] << std::endl;
    }*/

    // Create a map of clusters to their data points, store data point's ID inside cluster's vector
    std::vector<std::vector<std::string>> clusters(numClusters);
    for (int i = 0; i < N; ++i) {
        clusters[clusterAssignment[i]].push_back(dataPointNames[i]);
    }

    // Start of balanced K-Means
    // Get the output at current layer (data point allocation for each cluster)
    //BalancedKMeans BKM;
    //std::vector<std::vector<std::string>> newCluster = BKM.balancedKMeans(N, C, numClusters, clusterAssignment, dataPointNames, clusters, linkCostMatrix);





    // BKM begins...
    //std::string file = "../data/ndn_test.csv"; // input file
    //std::string assignment_file = "assignments";
    //std::string cluster_center_file = "clusters";
    //std::string summary_file = "summary.txt";
    int runs = 1; // number of repeat
    //int k = 4; // number of clusters
    int threads = -1; // default, auto-detect
    bool no_warm_start = false; // must have warm start
    unsigned int seed = std::random_device{}(); // seed for initialization
    RegularizedKMeans::InitMethod init_method = RegularizedKMeans::InitMethod::kForgy;


    //auto data = ReadData(file);
    //auto start_time = std::chrono::high_resolution_clock::now();
    double result;
    KMeans* k_means;
    auto* rkm = new RegularizedKMeans(
            dataPointNames, numClusters,linkCostMatrix, init_method, !no_warm_start,
            threads, seed);
    result = rkm->SolveHard();
    k_means = rkm;
    //delete k_means;

    // BKM finish...

    std::vector<std::vector<std::string>> newCluster = k_means->clusters;

    // Construct the nodeList for CH allocation
    int i = 0;
    std::cout << "\nIterating new clusters." << std::endl;
    for (const auto& iteCluster: newCluster) {
        std::cout << "Cluster " << i << " contains the following nodes:" <<std::endl;
        for (const auto& iteNode: iteCluster) {
            CHList.erase(std::remove(CHList.begin(), CHList.end(), iteNode), CHList.end());
            std::cout << iteNode << " ";
        }
        std::cout << std::endl;
        ++i;
    }


    std::cout << "\nCurrent CH candidates: " << std::endl;
    for (const auto& item : CHList) {
        std::cout << item << std::endl;
    }

    // Start CH allocation
    std::vector<std::string> newDataPoints;
    std::cout << "\nStarting CH allocation." << std::endl;
    for (const auto& clusterNodes : newCluster) {
        std::string clusterHead = findCH(clusterNodes, CHList, globalClient);
         // "globalClient" doesn't exist in candidate list, if CH == globalClient, it means no CH is found
        if (clusterHead != globalClient) {
            CHList.erase(std::remove(CHList.begin(), CHList.end(), clusterHead), CHList.end());
            aggregationAllocation[clusterHead] = clusterNodes;
            newDataPoints.push_back(clusterHead); // If newDataPoints are more than C, perform tree construction for next layer
        }
        else {
            std::cout << "No cluster head found for current cluster, combine them into sub-tree." << std::endl;
            noCHTree.push_back(clusterNodes);
        }

    }

    std::cout << "\nThe rest CH candidates after CH allocation: " << std::endl;
    for (const auto& item : CHList) {
        std::cout << item << std::endl;
    }



    if (newDataPoints.size() < C){

        if (newDataPoints.empty()) // If all clusters can't find CH, allocate the first sub-tree to aggregationAllocation for first round, then iterate other sub-trees in later rounds
        {
            // Move the first sub-tree to "aggregationAllocation"
            const auto& firstSubTree = noCHTree[0];
            aggregationAllocation[globalClient] = firstSubTree;
            noCHTree.erase(noCHTree.begin());
            return true;
        } else // If some clusters can find CH, put them into aggregationAllocation for first round, iterate other sub-trees in later rounds
        {
            aggregationAllocation[globalClient] = newDataPoints;
            return true;
        }
    } else {
        aggregationTreeConstruction(newDataPoints, C);
    }
}
