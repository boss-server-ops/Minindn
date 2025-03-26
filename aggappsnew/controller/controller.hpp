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

#include "../aggregation/aggregator.hpp"

namespace ndn::chunks
{
    using DataChunk = std::map<uint64_t, std::shared_ptr<const Data>>;
    struct ChildNodeInfo;
    class FlowController
    {
    public:
        /**
         * @brief Construct a new Flow Controller
         * @param configPath Path to the configuration file
         * @param nodeNames Vector of node names to monitor
         */
        FlowController(const std::string &configPath, const std::vector<std::string> &nodeNames);

        /**
         * @brief Create a Flow Controller from parsed child node information
         * @param configPath Path to the configuration file
         * @param childNodeInfos Vector of child node information structures
         * @return A new FlowController instance
         */
        static std::unique_ptr<FlowController> createFromChildNodeInfos(
            const std::string &configPath,
            const std::vector<ChildNodeInfo> &childNodeInfos);

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

        /**
         * @brief Check if a specific chunk has been processed and is available in buffer
         * @param chunkNumber Chunk number to check
         * @return True if the chunk is available in processed state
         */
        bool isChunkProcessed(uint64_t chunkNumber) const;

        /**
         * @brief Get a processed chunk as an input stream
         * @param chunkNumber Chunk number to retrieve
         * @return Pointer to an input stream containing the chunk data, or nullptr if not available
         */
        std::istream *getProcessedChunkAsStream(uint64_t chunkNumber);

        /**
         * @brief Clear any cached stream data
         * Useful to free memory when streams are no longer needed
         */
        void clearStreamCache();

        /**
         * @brief Get the highest processed chunk number
         * @return The highest chunk number that has been fully processed
         */
        uint64_t getLastProcessedChunk() const { return m_lastProcessedChunk; }

        /**
         * @brief Get the highest received chunk number for a specific node
         * @param nodeName Name of the node to query
         * @return The highest chunk number received from that node
         */
        uint64_t getHighestChunkForNode(const std::string &nodeName) const;
        /**
         * @brief Remove a processed chunk from the buffer
         * @param chunkNumber Chunk number to remove
         */
        void removeProcessedChunk(uint64_t chunkNumber);

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
         * @brief Store a processed chunk in the buffer
         * @param chunkNumber Chunk number
         * @param chunk The processed data chunk
         */
        void storeProcessedChunk(uint64_t chunkNumber, const DataChunk &chunk);

    private:
        std::vector<std::string> m_nodeNames; // List of node names being monitored
        size_t m_numFaces;                    // Number of child nodes (convenience)

        // Buffer for each node [nodeName][chunkNumber] -> DataChunk
        std::map<std::string, std::map<uint64_t, DataChunk>> m_nodeBuffers;

        // Buffer for storing processed (averaged) chunks
        std::map<uint64_t, DataChunk> m_processedChunks;

        // Tracks the number of nodes that have provided data for each chunk
        std::map<uint64_t, size_t> m_chunkCompletionCount;

        // The highest chunk number that has been processed, used for ordered output
        uint64_t m_lastProcessedChunk;

        // Maximum allowed difference in highest received chunk numbers between flows
        uint64_t m_tableSize;

        // Maximum number of processed chunks to keep in buffer
        uint64_t m_maxBufferedChunks;

        // Highest received chunk number for each node
        std::map<std::string, uint64_t> m_highestChunkPerNode;

        // Indicates if a flow should be paused due to being too far ahead
        std::map<std::string, bool> m_pausedFlows;

        // Mutex to protect data structures
        mutable std::mutex m_mutex;

        // Cache for converted streams
        mutable std::map<uint64_t, std::unique_ptr<std::stringstream>> m_streamCache;
        /**
         * @brief Convert a DataChunk to a stringstream
         * @param chunk The DataChunk to convert
         * @return A populated stringstream containing serialized data
         */
        std::unique_ptr<std::stringstream> dataChunkToStream(const DataChunk &chunk) const;
    };

} // namespace ndn::chunks

#endif // NDN_FLOW_CONTROLLER_HPP