#ifndef NDN_FLOW_CONTROLLER_HPP
#define NDN_FLOW_CONTROLLER_HPP

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <fstream>
#include <iostream>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <mutex>
#include <spdlog/spdlog.h>
#include <ndn-cxx/data.hpp>
#include "../aggtree/aggtree.hpp"

namespace ndn::chunks
{
    // 原始的 DataChunk 类型，保持 map 结构
    using DataChunk = std::map<uint64_t, std::shared_ptr<const Data>>;

    class FlowController
    {
    public:
        /**
         * @brief Construct a new Flow Controller
         * @param configPath Path to the configuration file
         * @param output Output stream where processed data will be written
         * @param nodeNames Vector of node names to monitor
         */
        FlowController(const std::string &configPath, std::ostream &output,
                       const std::vector<std::string> &nodeNames);

        /**
         * @brief Create a Flow Controller to monitor all direct children of a specified root node in AggTree
         * @param configPath Path to the configuration file
         * @param output Output stream where processed data will be written
         * @param tree AggTree object
         * @param rootNodeName Name of the root node
         * @return A new FlowController instance
         */
        static std::unique_ptr<FlowController> createFromAggTree(
            const std::string &configPath,
            std::ostream &output,
            const AggTree &tree,
            const std::string &rootNodeName);
        /**
         * @brief Destructor, processes all remaining data
         */
        ~FlowController();

        /**
         * @brief Add a data chunk to the buffer of the corresponding node
         * @param nodeName Source node name
         * @param chunkNumber Chunk sequence number
         * @param dataChunk Data packet
         */
        void addChunk(const std::string &nodeName, uint64_t chunkNumber, const DataChunk &dataChunk);

        /**
         * @brief Get the number of child nodes
         * @return Number of faces/nodes
         */
        size_t getNumFaces() const { return m_nodeNames.size(); }

        /**
         * @brief Get all monitored node names
         * @return Vector of node names
         */
        const std::vector<std::string> &getNodeNames() const { return m_nodeNames; }

        /**
         * @brief Process all chunks that can be merged
         * Merges chunks when all nodes have provided data for the same chunk number
         */
        void processAvailableChunks();

        /**
         * @brief Check if a specific flow should be paused due to being too far ahead
         * @param nodeName Name of the node to check
         * @return True if the flow should be paused (is too far ahead of others)
         */
        bool shouldPauseFlow(const std::string &nodeName) const;

        /**
         * @brief Reset the paused status of a flow
         * @param nodeName Name of the node to reset
         */
        void resetPauseStatus(const std::string &nodeName);

    private:
        /**
         * @brief Merge multiple data chunks into a single averaged chunk
         * @param chunkNumber Chunk sequence number
         * @return Merged/averaged data chunk
         */
        DataChunk averageChunks(uint64_t chunkNumber);

        /**
         * @brief Check if a specific chunk has been provided by all nodes
         * @param chunkNumber Chunk sequence number
         * @return True if all nodes have provided data for this chunk
         */
        bool isChunkComplete(uint64_t chunkNumber) const;

        /**
         * @brief Update flow control status based on chunk number differences
         * Pauses flows that are too far ahead of the slowest flow
         * @param nodeName The node that just received a chunk
         * @param chunkNumber The received chunk number
         */
        void updateFlowControlStatus(const std::string &nodeName, uint64_t chunkNumber);

        /**
         * @brief Average multiple Data objects into a single Data object
         * @param dataObjects Vector of Data objects to average
         * @return A new averaged Data object
         */
        DataChunk averageDataObjects(const std::vector<DataChunk> &dataObjects);

        /**
         * @brief Write a data chunk to output stream
         * @param chunk The data chunk to write
         */
        void writeChunkToOutput(const DataChunk &chunk);

    private:
        std::vector<std::string> m_nodeNames; // List of node names being monitored
        size_t m_numFaces;                    // Number of child nodes (convenience)
        std::ostream &m_output;               // Reference to output stream

        // Buffer for each node [nodeName][chunkNumber] -> DataChunk
        std::map<std::string, std::map<uint64_t, DataChunk>> m_nodeBuffers;

        // Tracks the number of nodes that have provided data for each chunk
        std::map<uint64_t, size_t> m_chunkCompletionCount;

        // The highest chunk number that has been processed, used for ordered output
        uint64_t m_lastProcessedChunk;

        // Maximum allowed difference in highest received chunk numbers between flows
        uint64_t m_tableSize;

        // Highest received chunk number for each node
        std::map<std::string, uint64_t> m_highestChunkPerNode;

        // Indicates if a flow should be paused due to being too far ahead
        std::map<std::string, bool> m_pausedFlows;

        // Mutex to protect data structures
        mutable std::mutex m_mutex;
    };

} // namespace ndn::chunks

#endif // NDN_FLOW_CONTROLLER_HPP