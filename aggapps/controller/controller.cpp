#include "controller.hpp"
#include <algorithm>
#include <numeric>
#include <set>
#include <stdexcept>
#include <ndn-cxx/encoding/block.hpp>
#include <ndn-cxx/util/sha256.hpp>

namespace ndn::chunks
{

    FlowController::FlowController(const std::string &configPath, const std::vector<std::string> &nodeNames)
        : m_nodeNames(nodeNames), m_numFaces(nodeNames.size()),
          m_lastProcessedChunk(-1), m_tableSize(10), m_maxBufferedChunks(100) // Default values
    {
        namespace pt = boost::property_tree;

        try
        {
            pt::ptree tree;
            pt::read_ini(configPath, tree);

            // Read table-size (maximum allowed difference between flows)
            m_tableSize = tree.get<uint64_t>("General.table-size", 10);

            // Read max buffered chunks setting
            m_maxBufferedChunks = tree.get<uint64_t>("General.max-buffered-chunks", 100);

            if (m_nodeNames.empty())
            {
                throw std::runtime_error("No node names provided: must have at least 1 node");
            }

            spdlog::info("FlowController initialized with {} child nodes, table size: {}, max buffered chunks: {}",
                         m_nodeNames.size(), m_tableSize, m_maxBufferedChunks);

            // Initialize buffers and flow control structures for all nodes
            for (const auto &nodeName : m_nodeNames)
            {
                m_nodeBuffers[nodeName] = {};
                m_highestChunkPerNode[nodeName] = 0;
                m_pausedFlows[nodeName] = false;
                spdlog::debug("Initialized flow control for node: {}", nodeName);
            }
        }
        catch (const pt::ini_parser_error &e)
        {
            spdlog::error("Failed to parse config file: {}", e.what());
            throw;
        }
        catch (const std::exception &e)
        {
            spdlog::error("FlowController initialization error: {}", e.what());
            throw;
        }
    }

    FlowController::~FlowController()
    {
        try
        {
            // Ensure all remaining data is processed
            processAvailableChunks();
        }
        catch (const std::exception &e)
        {
            spdlog::error("Error during FlowController destruction: {}", e.what());
        }
    }

    void FlowController::addChunk(const std::string &nodeName, uint64_t chunkNumber, const DataChunk &dataChunk)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Verify if this node is being monitored
        if (m_nodeBuffers.find(nodeName) == m_nodeBuffers.end())
        {
            spdlog::warn("Received chunk from unknown node: {}", nodeName);
            return;
        }

        // Check if we have already received this chunk from this node
        auto &nodeBuffer = m_nodeBuffers[nodeName];
        if (nodeBuffer.find(chunkNumber) != nodeBuffer.end())
        {
            spdlog::debug("Duplicate chunk {} from node {}", chunkNumber, nodeName);
            return;
        }

        // Check if the chunk is valid
        if (dataChunk.empty())
        {
            spdlog::warn("Received empty chunk {} from node {}", chunkNumber, nodeName);
            return;
        }

        // Store the data chunk
        nodeBuffer[chunkNumber] = dataChunk;

        size_t totalSize = 0;
        for (const auto &[_, data] : dataChunk)
        {
            totalSize += data->getContent().value_size();
        }

        spdlog::debug("Received chunk {} from node {}, size: {} bytes, segments: {}",
                      chunkNumber, nodeName, totalSize, dataChunk.size());

        // Update highest chunk number for this node
        if (chunkNumber > m_highestChunkPerNode[nodeName])
        {
            m_highestChunkPerNode[nodeName] = chunkNumber;

            // Update flow control status based on this new highest chunk
            updateFlowControlStatus(nodeName, chunkNumber);
        }

        // Update completion count
        if (m_chunkCompletionCount.find(chunkNumber) == m_chunkCompletionCount.end())
        {
            m_chunkCompletionCount[chunkNumber] = 0;
        }
        m_chunkCompletionCount[chunkNumber]++;

        // If this chunk is now complete, try to process it
        if (m_chunkCompletionCount[chunkNumber] == m_nodeNames.size())
        {
            spdlog::debug("All nodes have provided chunk {}", chunkNumber);

            // If this is the next chunk in order, process it immediately
            // if (chunkNumber == m_lastProcessedChunk + 1)
            // {
            spdlog::debug("Processing chunk {} immediately", chunkNumber);
            auto averagedChunk = averageChunks(chunkNumber);

            // Store the processed chunk instead of writing to output
            storeProcessedChunk(chunkNumber, averagedChunk);

            // Update the last processed chunk number
            m_lastProcessedChunk = chunkNumber;

            // Remove this chunk from all node buffers
            for (auto &[name, buffer] : m_nodeBuffers)
            {
                buffer.erase(chunkNumber);
            }

            // Remove from completion count
            m_chunkCompletionCount.erase(chunkNumber);
            // }
        }
    }

    // Modified createFromChildNodeInfos method to use string collections directly
    std::unique_ptr<FlowController> FlowController::createFromChildNodeInfos(
        const std::string &configPath,
        const std::vector<ChildNodeInfo> &childNodeInfos)
    {
        // Extract node names from the ChildNodeInfo objects
        std::vector<std::string> childNodes;
        for (const auto &info : childNodeInfos)
        {
            childNodes.push_back(info.name);
        }

        if (childNodes.empty())
        {
            spdlog::warn("No child nodes provided");
        }
        else
        {
            spdlog::info("Found {} child nodes for flow controller", childNodes.size());
            for (const auto &node : childNodes)
            {
                spdlog::debug("Child node: {}", node);
            }
        }

        // Create a new FlowController instance
        return std::make_unique<FlowController>(configPath, childNodes);
    }

    bool FlowController::isChunkProcessed(uint64_t chunkNumber) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_processedChunks.find(chunkNumber) != m_processedChunks.end();
    }

    std::istream *FlowController::getProcessedChunkAsStream(uint64_t chunkNumber)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Check if we already have a stream for this chunk
        auto streamIt = m_streamCache.find(chunkNumber);
        if (streamIt != m_streamCache.end())
        {
            // Reset the stream position to the beginning
            streamIt->second->clear();
            streamIt->second->seekg(0);
            return streamIt->second.get();
        }

        // No cached stream, check if we have the processed chunk
        auto it = m_processedChunks.find(chunkNumber);
        if (it == m_processedChunks.end())
        {
            return nullptr; // Chunk not available
        }

        // Convert the DataChunk to a stream and cache it
        auto stream = dataChunkToStream(it->second);
        std::istream *result = stream.get();
        m_streamCache[chunkNumber] = std::move(stream);

        return result;
    }

    void FlowController::removeProcessedChunk(uint64_t chunkNumber)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_processedChunks.find(chunkNumber);
        if (it != m_processedChunks.end())
        {
            m_processedChunks.erase(it);
            spdlog::debug("Removed processed chunk {} from buffer", chunkNumber);
        }
    }
    void FlowController::clearStreamCache()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_streamCache.clear();
        spdlog::debug("Cleared stream cache");
    }

    std::unique_ptr<std::stringstream> FlowController::dataChunkToStream(const DataChunk &chunk) const
    {
        auto stream = std::make_unique<std::stringstream>();

        // For each segment in the chunk
        for (const auto &[_, data] : chunk)
        {
            // Get content bytes
            const auto &content = data->getContent();
            size_t contentSize = content.value_size();

            // Write content size
            // *stream << content << std::endl;

            // Write actual content bytes
            if (contentSize > 0)
            {
                stream->write(reinterpret_cast<const char *>(content.value()), contentSize);
            }
        }

        return stream;
    }

    uint64_t FlowController::getHighestChunkForNode(const std::string &nodeName) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_highestChunkPerNode.find(nodeName);
        if (it != m_highestChunkPerNode.end())
        {
            return it->second;
        }
        return 0; // Return 0 if no chunks received from this node
    }

    void FlowController::storeProcessedChunk(uint64_t chunkNumber, const DataChunk &chunk)
    {
        // Store the processed chunk in the buffer
        m_processedChunks[chunkNumber] = chunk;

        spdlog::debug("Stored processed chunk {} in buffer, size: {} segments",
                      chunkNumber, chunk.size());

        // Clean up old chunks if we exceed the maximum buffer size
        while (m_processedChunks.size() > m_maxBufferedChunks && !m_processedChunks.empty())
        {
            // Always remove the oldest chunk (lowest chunk number)
            auto oldestIt = m_processedChunks.begin();
            spdlog::debug("Removing old chunk {} from buffer to maintain size limit", oldestIt->first);
            m_processedChunks.erase(oldestIt);
        }
    }

    // Add other method implementations as needed...

    void FlowController::updateFlowControlStatus(const std::string &nodeName, uint64_t chunkNumber)
    {
        // Find the minimum highest chunk number across all nodes
        uint64_t minHighest = std::numeric_limits<uint64_t>::max();

        for (const auto &[name, highest] : m_highestChunkPerNode)
        {
            if (highest > 0)
            { // Only consider nodes that have received at least one chunk
                minHighest = std::min(minHighest, highest);
            }
        }

        // If minHighest is still the max value, it means we don't have data for all nodes yet
        if (minHighest == std::numeric_limits<uint64_t>::max())
        {
            return;
        }

        // For each node, check if it's too far ahead
        for (auto &[name, highest] : m_highestChunkPerNode)
        {
            // If this node is too far ahead of the slowest node
            if (highest > 0 && (highest - minHighest > m_tableSize))
            {
                // Mark this node as needing to be paused
                if (!m_pausedFlows[name])
                {
                    m_pausedFlows[name] = true;
                    spdlog::warn("Flow {} paused: ahead by {} chunks (exceeds limit {})",
                                 name, highest - minHighest, m_tableSize);
                }
            }
            // Otherwise, if it was paused but the gap has decreased, unpause it
            else if (m_pausedFlows[name])
            {
                m_pausedFlows[name] = false;
                spdlog::info("Flow {} resumed: ahead by {} chunks (within limit {})",
                             name, highest - minHighest, m_tableSize);
            }
        }
    }

    bool FlowController::shouldPauseFlow(const std::string &nodeName) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = m_pausedFlows.find(nodeName);
        if (it == m_pausedFlows.end())
        {
            spdlog::warn("Checked pause status for unknown node: {}", nodeName);
            return false;
        }

        return it->second;
    }

    void FlowController::resetPauseStatus(const std::string &nodeName)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = m_pausedFlows.find(nodeName);
        if (it == m_pausedFlows.end())
        {
            spdlog::warn("Attempted to reset pause status for unknown node: {}", nodeName);
            return;
        }

        it->second = false;
        spdlog::info("Manually reset pause status for flow {}", nodeName);
    }

    void FlowController::processAvailableChunks()
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        std::vector<uint64_t> completeChunks;
        for (const auto &[chunkNum, count] : m_chunkCompletionCount)
        {
            if (count == m_nodeNames.size())
            {
                completeChunks.push_back(chunkNum);
            }
        }

        // Sort by chunk number
        std::sort(completeChunks.begin(), completeChunks.end());

        for (uint64_t chunkNum : completeChunks)
        {
            spdlog::debug("Processing chunk {} in batch", chunkNum);
            auto averagedChunk = averageChunks(chunkNum);

            // Store the processed chunk instead of writing to output
            storeProcessedChunk(chunkNum, averagedChunk);

            // Update the last processed chunk number
            if (chunkNum > m_lastProcessedChunk)
            {
                m_lastProcessedChunk = chunkNum;
            }

            // Remove this chunk from all node buffers
            for (auto &[name, buffer] : m_nodeBuffers)
            {
                buffer.erase(chunkNum);
            }

            // Remove from completion count
            m_chunkCompletionCount.erase(chunkNum);
        }
    }

    bool FlowController::isChunkComplete(uint64_t chunkNumber) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = m_chunkCompletionCount.find(chunkNumber);
        if (it == m_chunkCompletionCount.end() || it->second != m_nodeNames.size())
        {
            return false;
        }

        // Ensure all nodes have data for this chunk
        for (const auto &[nodeName, nodeBuffers] : m_nodeBuffers)
        {
            auto chunkIt = nodeBuffers.find(chunkNumber);
            if (chunkIt == nodeBuffers.end() || chunkIt->second.empty())
            {
                return false;
            }
        }

        return true;
    }

    DataChunk FlowController::averageChunks(uint64_t chunkNumber)
    {
        std::vector<DataChunk> dataChunks;

        // Collect all data chunks for this chunk from all nodes
        for (const auto &[nodeName, nodeBuffers] : m_nodeBuffers)
        {
            auto chunkIt = nodeBuffers.find(chunkNumber);
            if (chunkIt != nodeBuffers.end() && !chunkIt->second.empty())
            {
                dataChunks.push_back(chunkIt->second);
            }
        }

        if (dataChunks.empty())
        {
            spdlog::warn("No data found for chunk {}", chunkNumber);
            return DataChunk{};
        }

        return averageDataObjects(dataChunks);
    }

    DataChunk FlowController::averageDataObjects(const std::vector<DataChunk> &dataChunks)
    {
        if (dataChunks.empty())
        {
            spdlog::warn("No data chunks to average");
            return DataChunk{};
        }

        if (dataChunks.size() == 1)
        {
            // No need to average if there's only one chunk
            return DataChunk(dataChunks[0]);
        }

        // 收集所有存在的子索引
        std::set<uint64_t> allSegments;
        for (const auto &chunk : dataChunks)
        {
            for (const auto &[segNo, _] : chunk)
            {
                allSegments.insert(segNo);
            }
        }

        DataChunk result;

        // 对每个子索引进行平均处理
        for (uint64_t segNo : allSegments)
        {
            std::vector<std::shared_ptr<const Data>> segmentData;

            // 从每个节点收集同一个子索引的数据
            for (const auto &chunk : dataChunks)
            {
                auto it = chunk.find(segNo);
                if (it != chunk.end() && it->second)
                {
                    segmentData.push_back(it->second);
                }
            }

            if (segmentData.empty())
                continue;

            if (segmentData.size() == 1)
            {
                // 只有一个数据源，直接使用
                result[segNo] = std::make_shared<Data>(*segmentData[0]);
                continue;
            }

            // 使用第一个数据包的名称
            Name dataName = segmentData[0]->getName();

            // 从所有数据包中提取内容
            std::vector<std::vector<uint8_t>> contents;
            size_t minSize = std::numeric_limits<size_t>::max();

            for (const auto &data : segmentData)
            {
                const Block &content = data->getContent();
                std::vector<uint8_t> contentBytes(content.value(), content.value() + content.value_size());
                contents.push_back(contentBytes);
                minSize = std::min(minSize, contentBytes.size());
            }

            if (minSize == 0)
            {
                result[segNo] = std::make_shared<Data>(*segmentData[0]);
                continue;
            }

            // 执行字节级平均
            std::vector<uint8_t> averagedContent(minSize);

            for (size_t i = 0; i < minSize; ++i)
            {
                uint64_t sum = 0;
                for (const auto &content : contents)
                {
                    sum += content[i];
                }
                averagedContent[i] = static_cast<uint8_t>(sum / contents.size());
            }

            // 创建新的数据包
            auto resultData = std::make_shared<Data>(dataName);
            resultData->setContent(make_shared<Buffer>(averagedContent.data(), averagedContent.size()));
            resultData->setFreshnessPeriod(segmentData[0]->getFreshnessPeriod());

            result[segNo] = resultData;
        }

        size_t totalSize = 0;
        for (const auto &[_, data] : result)
        {
            totalSize += data->getContent().value_size();
        }

        spdlog::debug("Averaged content from {} sources, total size: {} bytes, segments: {}",
                      dataChunks.size(), totalSize, result.size());

        return result;
    }
} // namespace ndn::chunks