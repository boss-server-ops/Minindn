
#pragma once
#ifndef IMAgg_Aggregator_HPP
#define IMAgg_Aggregator_HPP

#include <ndn-cxx/face.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <spdlog/spdlog.h>
#include <ndn-cxx/util/scheduler.hpp>

#include "../pipeline/pipeliner.hpp"

#ifdef UNIT_TEST
#define PUBLIC_WITH_TESTS_ELSE_PRIVATE public
#else
#define PUBLIC_WITH_TESTS_ELSE_PRIVATE private
#endif

namespace ndn::chunks
{
    class FlowController;
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
        struct Options
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
        Aggregator(const Name &prefix, Face &face, KeyChain &keyChain,
                   const Options &opts, uint64_t chunkNumber, uint64_t totalChunkNumber);

        ~Aggregator();
        /**
         * @brief Run the aggregator.
         */
        void
        run();

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
         * @brief Respond with the requested segment of content.
         */
        void processSegmentInterest(const Interest &interest);

        PUBLIC_WITH_TESTS_ELSE_PRIVATE : std::unordered_map<uint64_t, std::vector<std::shared_ptr<Data>>> m_store;

        /**
         * @brief Parse child nodes from the structured interest name
         * @param interest The received interest with hierarchical structure
         * @return Vector of child node structures with names and substructures
         * @example /agg0/agg1(pro0+pro1)+pro3 means agg0 has two children agg1 and pro3, agg1 has two children pro0 and pro1
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

        void segmentationChunk(uint64_t chunkNumber, const Interest &interest);

        /**
         * @brief Respond to the interest with the requested content.
         * @param interest The interest to respond to
         */
        void respondToInterest(const Interest &interest);

    private:
        Name m_prefix;
        Name m_initialPrefix;
        Face &m_face;
        KeyChain &m_keyChain;
        const Options m_options;
        uint64_t m_totalChunkNumber;
        // Below is the new data structure for IMAgg
        std::unordered_map<uint64_t, uint64_t> m_nSentSegments;
        bool isini = false;
        std::vector<std::string> m_childNodes;       // Legacy storage for child nodes
        std::vector<ChildNodeInfo> m_childNodeInfos; // Structured information about child nodes
        std::vector<std::unique_ptr<Face>> m_childFaces;
        std::vector<std::unique_ptr<Pipeliner>> m_pipeliners;
        std::vector<std::thread> m_pipelinerThreads;
        std::map<std::string, bool> m_initializedNodes;               // Tracks which nodes have responded
        std::map<std::string, std::string> m_initializationResponses; // Stores responses from each node
        Interest m_originalInterest;                                  // Keeps the original interest for response
        bool m_hasOriginalInterest = false;
        std::shared_ptr<FlowController> m_flowController;
        size_t m_numFaces = 1;
        Request *m_request = nullptr;
        ndn::Name m_chunkedPrefix;

        PUBLIC_WITH_TESTS_ELSE_PRIVATE : Scheduler m_scheduler; ///< one scheduler per Face
        std::unordered_map<std::string, scheduler::ScopedEventId> m_respondEvents;

    public:
        spdlog::logger *logger;
    };
} // namespace ndn::chunks

#endif // IMAgg_Aggregator_HPP