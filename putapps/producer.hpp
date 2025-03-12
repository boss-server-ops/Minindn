/*
This file is based on the part of ndn-tools chunks
*/
#ifndef IMAgg_Producer_HPP
#define IMAgg_Producer_HPP

#include <ndn-cxx/face.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <spdlog/spdlog.h>
#include "InputGenerator.hpp"

#ifdef UNIT_TEST
#define PUBLIC_WITH_TESTS_ELSE_PRIVATE public
#else
#define PUBLIC_WITH_TESTS_ELSE_PRIVATE private
#endif

namespace ndn::chunks
{
    class Producer : noncopyable
    {
    public:
        struct Options
        {
            // Todo: Provided by ndn-tools and need to modify and read from config file
            security::SigningInfo signingInfo;
            time::milliseconds freshnessPeriod = 10_s;
            size_t maxSegmentSize = 8000;
            bool isQuiet = false;
            bool isVerbose = false;
            bool wantShowVersion = false;
        };

        /**
         * @brief Create the producer.
         * @param prefix prefix used to publish data; if the last component is not a valid
         *               version number, the current system time is used as version number.
         */
        Producer(const Name &prefix, Face &face, KeyChain &keyChain,
                 const Options &opts, uint64_t chunkNumber, InputGenerator &input, uint64_t totalChunkNumber);

        /**
         * @brief Run the producer.
         */
        void
        run();

        /**
         * @brief Segment the input stream and store the segments.
         * @param chunknumber the chunk number of the input stream
         * @param is the input stream of one chunk
         */
        void
        segmentationChunk(uint64_t chunkNumber);

    private:
        // /**
        //  * @brief Respond with a metadata packet containing the versioned content name.
        //  */
        // void
        // processDiscoveryInterest(const Interest &interest);

        /**
         * @brief Respond with the requested segment of content.
         */
        void
        processSegmentInterest(const Interest &interest);

        /**
         * @brief Get the agg tree structure
         */
        void
        processInitializaionInterest(const Interest &interest);
        PUBLIC_WITH_TESTS_ELSE_PRIVATE : std::unordered_map<uint64_t, std::vector<std::shared_ptr<Data>>> m_store;

    private:
        Name m_prefix;
        Name m_chunkedPrefix;
        Name m_initialPrefix;
        Face &m_face;
        KeyChain &m_keyChain;
        const Options m_options;
        uint64_t m_totalChunkNumber;
        // Below is the new data structure for IMAgg
        InputGenerator &m_input;
        std::unordered_map<uint64_t, uint64_t> m_nSentSegments;
        bool isini = false;

    public:
        spdlog::logger *logger;
    };
} // namespace ndn::chunks

#endif // IMAgg_Producer_HPP