#ifndef NDN_APP_H
#define NDN_APP_H

#include <ndn-cxx/face.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/encoding/block.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <memory>
#include <set>
#include <map>
#include <vector>
#include <string>
#include <iostream>

class App
{
public:
    /**
     * @brief Default constructor
     */
    App();
    virtual ~App();

    /**
     * @brief Get application ID (ID of applications face)
     * @return Application ID
     */
    uint32_t GetId() const;

    /**
     * @brief Method that will be called every time new Interest arrives
     * @param interest The received Interest packet
     */
    virtual void OnInterest(const ndn::Interest &interest);

    /**
     * @brief Method that will be called every time new Data arrives
     * @param data The received Data packet
     */
    virtual void OnData(const ndn::Data &data);

    /**
     * @brief Method that will be called every time new Nack arrives
     * @param nack The received Nack packet
     */
    virtual void OnNack(const ndn::lp::Nack &nack);

    /**
     * @brief Construct the aggregation tree
     */
    void ConstructAggregationTree();

protected:
    /**
     * @brief Called at time specified by Start
     */
    virtual void StartApplication();

    /**
     * @brief Called at time specified by Stop
     */
    virtual void StopApplication();

    /**
     * @brief Return all child nodes for given map and parent node
     * @param key Parent node
     * @param treeMap Given mapping
     * @return Set of child nodes
     */
    std::set<std::string> findLeafNodes(const std::string &key, const std::map<std::string, std::vector<std::string>> &treeMap);

    /**
     * @brief Return a mapping (key: child node, value: leaf nodes connected at the lower tier - producers)
     * @param key Parent node
     * @param treeMap Input mapping
     * @return Mapping of child nodes to leaf nodes
     */
    std::map<std::string, std::set<std::string>> getLeafNodes(const std::string &key, const std::map<std::string, std::vector<std::string>> &treeMap);

    /**
     * @brief Return round index
     * @param roundVec Vector consists all related nodes (aggregator) in one iteration
     * @param target Target node
     * @return Index of the round
     */
    int findRoundIndex(const std::vector<std::vector<std::string>> &roundVec, const std::string &target);

    /**
     * @brief Check whether this folder exists, if not, create it
     * @param path Path to the directory
     */
    void CheckDirectoryExist(const std::string &path);

    /**
     * @brief Open and clear the file
     * @param filename Name of the file to open
     */
    void OpenFile(const std::string &filename);

    // New design for tree topology to get child node info
    std::map<std::string, std::vector<std::string>> m_linkInfo;

    // Define log directory
    std::string folderPath = "logs";
    std::string throughput_recorder = folderPath + "/throughput.txt"; // "totalInterestThroughput", "totalDataThroughput", "total time"

    bool m_active; ///< @brief Flag to indicate that application is active (set by StartApplication and StopApplication)
    std::shared_ptr<ndn::Face> m_face;
    uint32_t m_appId;

    // Logger
    std::shared_ptr<spdlog::logger> m_logger;
};

#endif // NDN_APP_H