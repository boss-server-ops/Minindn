#include "producer.hpp"

#include <ndn-cxx/metadata-object.hpp>
#include <ndn-cxx/util/segmenter.hpp>
#include <iostream>

#include <boost/lexical_cast.hpp>
namespace ndn::chunks
{
    Producer::Producer(const Name &prefix, Face &face, KeyChain &keyChain,
                       const Options &opts, uint64_t chunkNumber, InputGenerator &input, uint64_t totalChunkNumber)
        : m_face(face), m_keyChain(keyChain), m_options(opts), m_totalChunkNumber(totalChunkNumber), m_input(input)
    {
        spdlog::debug("Producer::Producer()");

        m_prefix = prefix;
        m_chunkedPrefix = Name(m_prefix).append(std::to_string(chunkNumber));
        m_initialPrefix = Name(m_prefix).append("init");
        if (!m_options.isQuiet)
        {
            std::cerr << "Loading input ...\n";
            spdlog::info("Loading input ...");
        }
        // Segmenter segmenter(m_keyChain, m_options.signingInfo);
        // // All the data packets are segmented and stored in m_store
        // m_store[chunkNumber] = segmenter.segment(is, m_chunkedPrefix, m_options.maxSegmentSize, m_options.freshnessPeriod);
        // register m_prefix without Interest handler
        m_face.registerPrefix(m_prefix, nullptr, [this](const Name &prefix, const auto &reason)
                              {            
            spdlog::error("ERROR: Failed to register prefix '{}'({})", prefix.toUri(), boost::lexical_cast<std::string>(reason));
            m_face.shutdown(); });

        spdlog::debug("m_initialPrefix is {}", m_initialPrefix.toUri());
        face.setInterestFilter(m_initialPrefix, [this](const auto &, const auto &interest)
                               { processInitializaionInterest(interest); });
        // match Interests whose name starts with m_chunkedPrefix
        spdlog::debug("m_chunkedPrefix is {}", m_chunkedPrefix.toUri());
        face.setInterestFilter(m_prefix, [this](const auto &, const auto &interest)
                               { processSegmentInterest(interest); });

        if (!m_options.isQuiet)
        {
            std::cerr << "Published " << m_store[chunkNumber].size() << " Data packet" << (m_store[chunkNumber].size() > 1 ? "s" : "")
                      << " with prefix " << m_chunkedPrefix << "\n";
            spdlog::info("Published {} Data packet(s) with prefix {}", m_store[chunkNumber].size(), m_chunkedPrefix.toUri());
        }
    }

    void
    Producer::run()
    {
        spdlog::debug("Producer::run()");
        m_face.processEvents();
    }

    void
    Producer::processSegmentInterest(const Interest &interest)
    {
        if (isini)
        {
            isini = false;
            return;
        }
        spdlog::debug("Producer::processSegmentInterest()");
        if (m_options.isVerbose)
        {
            std::cerr << "Interest: " << interest << "\n";
            spdlog::info("Interest: {}", interest.getName().toUri());
        }
        const Name &name = interest.getName();
        uint64_t chunkNo = std::stoi(name[-2].toUri());
        spdlog::debug("chunkNo is {}", chunkNo);

        if (m_store[chunkNo].empty())
        {
            // spdlog::debug("temporarily no data");
            // return;
            segmentationChunk(chunkNo);
        }
        BOOST_ASSERT(!m_store[chunkNo].empty());

        std::shared_ptr<Data> data;

        if (name.size() == m_chunkedPrefix.size() + 1 && name[-1].isSegment())
        {
            const auto segmentNo = static_cast<size_t>(interest.getName()[-1].toSegment());
            // specific segment retrieval
            if (segmentNo < m_store[chunkNo].size())
            {
                data = m_store[chunkNo][segmentNo];
                m_nSentSegments[chunkNo]++;
            }
        }
        else if (interest.matchesData(*m_store[chunkNo][0]))
        {

            // unspecified version or segment number, return first segment
            data = m_store[chunkNo][0];
            m_nSentSegments[chunkNo] = 1;
        }

        if (data != nullptr)
        {
            if (m_options.isVerbose)
            {
                std::cerr << "Data: " << *data << "\n";
                spdlog::info("Data: {}", (*data).getName().toUri());
            }
            m_face.put(*data);

            // check all the segments are sent
            const Name &dataName = data->getName();
            if (dataName.size() > m_chunkedPrefix.size() && dataName[-1].isSegment())
            {
                uint64_t sentSegments = m_nSentSegments[chunkNo];
                auto it = m_store.find(chunkNo);
                if (it != m_store.end())
                {
                    size_t totalSegments = it->second.size();
                    if (sentSegments == totalSegments)
                    {
                        m_store.erase(chunkNo);
                        spdlog::debug("Cleared chunk {} after sending {} segments ", chunkNo, sentSegments);
                    }
                }
            }
        }
        else
        {
            if (m_options.isVerbose)
            {
                std::cerr << "Interest cannot be satisfied, sending Nack\n";
                spdlog::info("Interest cannot be satisfied, sending Nack");
            }
            m_face.put(lp::Nack(interest));
        }
    }

    void Producer::processInitializaionInterest(const Interest &interest)
    {
        spdlog::info("Received initialization interest: {}", interest.getName().toUri());

        auto data = make_shared<Data>(interest.getName());
        data->setFreshnessPeriod(time::seconds(10));
        data->setContent(makeStringBlock(tlv::Content, "Get initial interest"));

        m_keyChain.sign(*data);

        m_face.put(*data);

        spdlog::info("Sent initialization data: {}", data->getName().toUri());
        isini = true;
    }

    void
    Producer::segmentationChunk(uint64_t chunkNumber)
    {
        std::unique_ptr<std::istream> is = m_input.getChunk(chunkNumber);
        m_chunkedPrefix = Name(m_prefix).append(std::to_string(chunkNumber));
        if (!m_options.isQuiet)
        {
            std::cerr << "Loading input ...\n";
            spdlog::info("Loading input ...");
        }
        Segmenter segmenter(m_keyChain, m_options.signingInfo);
        // All the data packets are segmented and stored in m_store
        m_store[chunkNumber] = segmenter.segment(*is, m_chunkedPrefix, m_options.maxSegmentSize, m_options.freshnessPeriod);
        if (!m_options.isQuiet)
        {
            std::cerr << "Published " << m_store[chunkNumber].size() << " Data packet" << (m_store[chunkNumber].size() > 1 ? "s" : "")
                      << " with prefix " << m_chunkedPrefix << "\n";
            spdlog::info("Published {} Data packet(s) with prefix {}", m_store[chunkNumber].size(), m_chunkedPrefix.toUri());
        }
    }

} // namespace ndn::chunks