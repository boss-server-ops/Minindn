#pragma once
#ifndef IMAgg_Aggregator_HPP
#define IMAgg_Aggregator_HPP

#include <ndn-cxx/face.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <spdlog/spdlog.h>
#include <ndn-cxx/util/scheduler.hpp>
#include <ndn-cxx/util/rtt-estimator.hpp>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>

#include "../pipeline/pipeliner.hpp"
#include "../pipeline/options.hpp"
using ndn::util::RttEstimatorWithStats;

#ifdef UNIT_TEST
#define PUBLIC_WITH_TESTS_ELSE_PRIVATE public
#else
#define PUBLIC_WITH_TESTS_ELSE_PRIVATE private
#endif

namespace ndn::chunks
{
    class FlowController;
    class ChunksInterestsAdaptive;
    class Request;

    // Structure to store information about each child node
    struct ChildNodeInfo
    {
        std::string name;         // Name of the direct child node
        std::string substructure; // Substructure containing child node's children (if any)
        Name interestName;        // Interest name to be sent to this child node
    };

    class Aggregator : noncopyable
    {
    public:
        struct PutOptions
        {
            // Todo: Provided by ndn-tools and need to modify and read from config file
            security::SigningInfo signingInfo;
            time::milliseconds freshnessPeriod = 10_s;
            size_t maxSegmentSize = 4096;
            bool isQuiet = false;
            bool isVerbose = false;
            bool wantShowVersion = false;
        };

        /**
         * @brief Create the aggregator.
         * @param prefix prefix used to publish data; if the last component is not a valid
         *               version number, the current system time is used as version number.
         */
        Aggregator(const Name &prefix, KeyChain &keyChain,
                   const PutOptions &opts, uint64_t chunkNumber, uint64_t totalChunkNumber);

        ~Aggregator();

        /**
         * @brief Run the aggregator.
         */
        void run();

        /**
         * @brief Stop the aggregator and all its threads.
         */
        void stop();

        /**
         * @brief Send interests to child nodes
         * @param interest The original interest with segmentation
         */
        void sendInterest(const Interest &interest);

        /**
         * @brief Process the received interest and send the first intial interest
         * @param interest The received interest
         */
        void sendInitialInterest(const Interest &interest);

        void onInitialData(const Interest &interest, const Data &data, const std::string &childName);
        void onInitialNack(const Interest &interest, const lp::Nack &nack, const std::string &childName);
        void onInitialTimeout(const Interest &interest, const std::string &childName);

        /**
         * @brief Get the interest names for all child nodes with corresponding face indices
         * @return Vector of pairs containing interest name and suggested face index
         */
        std::vector<std::pair<Name, size_t>> getChildInterestNames() const;

        /**
         * @brief Get the flow controller used by this aggregator
         * @return Pointer to the flow controller, or nullptr if not initialized
         */
        std::shared_ptr<FlowController> getFlowController() const;

    private:
        /**
         * @brief Initialize aggregator with parsed child node information from the interest
         * @param interest The interest containing hierarchical structure information
         */
        void initializeFromInterest(const Interest &interest);

        /**
         * @brief Initialize the RTT estimator with the provided options.
         */
        void initializeRttEstimator();

        /**
         * @brief Initialize the options of cat.
         */
        void initializeCatOptions();

        /**
         * @brief Thread function for processing the parent face events
         */
        void parentFaceThread();

        /**
         * @brief Thread function for processing the child face events
         */
        void childFaceThread();

        /**
         * @brief Thread function for processing interests from the queue
         */
        void processInterestQueue();

        /**
         * @brief Thread function for processing Chunkers from the queue
         */
        void processChunkerQueue();

        /**
         * @brief Process a segment interest and handle initial setup or data retrieval
         * @param interest The interest to process
         */
        void processSegmentInterest(const Interest &interest);

        /**
         * @brief Parse the hierarchical structure of child nodes from interest
         * @param interest Interest with encoded child node structure
         * @return Vector of ChildNodeInfo structures
         */
        std::vector<ChildNodeInfo> parseChildNodes(const Interest &interest);

        /**
         * @brief Respond to the original interest.
         */
        void respondToOriginalInterest();

        /**
         * @brief Store the original interest for response
         * @param interest The original interest
         */
        void storeOriginalInterest(const Interest &interest);

        /**
         * @brief Segment a processed chunk into NDN data packets
         * @param chunkNumber The chunk number to segment
         * @param interest The interest to respond to
         */
        void segmentationChunk(uint64_t chunkNumber, const Interest &interest);

        /**
         * @brief Respond to the interest with the requested content.
         * @param interest The interest to respond to
         */
        void respondToInterest(const Interest &interest);

    private:
        Name m_prefix;
        Name m_initialPrefix;
        KeyChain &m_keyChain;
        const PutOptions m_options;
        Options m_catoptions;
        uint64_t m_totalChunkNumber;

        // 两个Face实例
        std::unique_ptr<Face> m_parentFace; // 用于与父节点通信
        std::unique_ptr<Face> m_childFace;  // 用于与子节点通信

        // 线程相关成员
        std::thread m_parentThread;
        std::thread m_childThread;
        std::thread m_processingThread;
        std::thread m_chunkerThread;
        std::atomic<bool> m_isRunning{false};

        // 线程安全的队列
        std::queue<Interest> m_interestQueue;
        std::mutex m_interestQueueMutex;
        std::condition_variable m_interestQueueCV;

        std::queue<std::pair<std::string, Name>> m_chunkerQueue;
        std::mutex m_chunkerQueueMutex;
        std::condition_variable m_chunkerQueueCV;

        // 其他成员变量
        std::unordered_map<uint64_t, std::vector<std::shared_ptr<Data>>> m_store;
        std::unordered_map<uint64_t, uint64_t> m_nSentSegments;
        std::vector<std::string> m_childNodes;
        std::vector<ChildNodeInfo> m_childNodeInfos;
        std::map<std::string, bool> m_initializedNodes;
        std::map<uint64_t, bool> m_isprocessing;
        std::map<std::string, std::string> m_initializationResponses;
        Interest m_originalInterest;
        bool m_hasOriginalInterest = false;
        std::shared_ptr<FlowController> m_flowController;
        size_t m_numFaces = 2; // 固定为2个face
        Name m_chunkedPrefix;

        // 线程安全的数据结构
        std::mutex m_storeMutex;
        std::mutex m_processingMutex;
        std::mutex m_initializationMutex;

        // Chunker和RTT估计器
        std::map<std::string, std::unique_ptr<ChunksInterestsAdaptive>> m_childChunker;
        std::map<std::string, std::unique_ptr<RttEstimatorWithStats>> m_rttEstimators;
        std::shared_ptr<util::RttEstimator::Options> m_rttEstOptions;
        RttEstimatorWithStats m_rttEstimator;

        // Scheduler
        Scheduler m_parentScheduler;
        Scheduler m_childScheduler;
        std::unordered_map<std::string, scheduler::ScopedEventId> m_respondEvents;

    public:
        // 为了解决ChunksInterestsAdaptive中引用m_face的问题
        Face &m_face = *m_childFace; // 提供兼容接口，指向子面
        spdlog::logger *logger;
    };
} // namespace ndn::chunks

#endif // IMAgg_Aggregator_HPP