#include "producer.hpp"

#include <ndn-cxx/metadata-object.hpp>
#include <ndn-cxx/util/segmenter.hpp>
#include <iostream>

#include <boost/lexical_cast.hpp>
namespace ndn::chunks
{
    Producer::Producer(const Name &prefix, Face &face, KeyChain &keyChain, std::istream &is,
                       const Options &opts, uint64_t chunkNumber)
        : m_face(face), m_keyChain(keyChain), m_options(opts)
    {
        spdlog::debug("Producer::Producer()");

        m_prefix = prefix;
        m_chunkedPrefix = Name(m_prefix).append(std::to_string(chunkNumber));
        if (!m_options.isQuiet)
        {
            std::cerr << "Loading input ...\n";
            spdlog::info("Loading input ...");
        }
        Segmenter segmenter(m_keyChain, m_options.signingInfo);
        // All the data packets are segmented and stored in m_store
        m_store[chunkNumber] = segmenter.segment(is, m_chunkedPrefix, m_options.maxSegmentSize, m_options.freshnessPeriod);
        // register m_prefix without Interest handler
        m_face.registerPrefix(m_prefix, nullptr, [this](const Name &prefix, const auto &reason)
                              {            
            spdlog::error("ERROR: Failed to register prefix '{}'({})", prefix.toUri(), boost::lexical_cast<std::string>(reason));
            m_face.shutdown(); });
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
            spdlog::debug("temporarily no data");
            return;
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
            }
        }
        else if (interest.matchesData(*m_store[chunkNo][0]))
        {
            // unspecified version or segment number, return first segment
            data = m_store[chunkNo][0];
        }

        if (data != nullptr)
        {
            if (m_options.isVerbose)
            {
                std::cerr << "Data: " << *data << "\n";
                spdlog::info("Data: {}", (*data).getName().toUri());
            }
            m_face.put(*data);
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

    void Producer::segmentationChunk(uint64_t chunkNumber, std::istream &is)
    {
        m_chunkedPrefix = Name(m_prefix).append(std::to_string(chunkNumber));
        if (!m_options.isQuiet)
        {
            std::cerr << "Loading input ...\n";
            spdlog::info("Loading input ...");
        }
        Segmenter segmenter(m_keyChain, m_options.signingInfo);
        // All the data packets are segmented and stored in m_store
        m_store[chunkNumber] = segmenter.segment(is, m_chunkedPrefix, m_options.maxSegmentSize, m_options.freshnessPeriod);
        if (!m_options.isQuiet)
        {
            std::cerr << "Published " << m_store[chunkNumber].size() << " Data packet" << (m_store[chunkNumber].size() > 1 ? "s" : "")
                      << " with prefix " << m_chunkedPrefix << "\n";
            spdlog::info("Published {} Data packet(s) with prefix {}", m_store[chunkNumber].size(), m_chunkedPrefix.toUri());
        }
    }

} // namespace ndn::chunks