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
        if (!prefix.empty() && prefix[-1].isVersion())
        {
            m_prefix = prefix.getPrefix(-1);
            m_versionedPrefix = prefix;
        }
        else
        {
            m_prefix = prefix;
            // m_versionedPrefix = Name(m_prefix).appendVersion();
            m_versionedPrefix = Name(m_prefix).append(std::to_string(chunkNumber));
        }
        if (!m_options.isQuiet)
        {
            std::cerr << "Loading input ...\n";
            spdlog::info("Loading input ...");
        }
        Segmenter segmenter(m_keyChain, m_options.signingInfo);
        // All the data packets are segmented and stored in m_store
        m_store = segmenter.segment(is, m_versionedPrefix, m_options.maxSegmentSize, m_options.freshnessPeriod);
        // register m_prefix without Interest handler
        m_face.registerPrefix(m_prefix, nullptr, [this](const Name &prefix, const auto &reason)
                              {            
            spdlog::error("ERROR: Failed to register prefix '{}'({})", prefix.toUri(), boost::lexical_cast<std::string>(reason));
            m_face.shutdown(); });
        // match Interests whose name starts with m_versionedPrefix
        face.setInterestFilter(m_versionedPrefix, [this](const auto &, const auto &interest)
                               { processSegmentInterest(interest); });
        // match Interests whose name is exactly m_prefix
        // face.setInterestFilter(InterestFilter(m_prefix, ""), [this](const auto &, const auto &interest)
        //                        { processSegmentInterest(interest); });
        // face.setInterestFilter(m_prefix, [this](const auto &, const auto &interest)
        //                        { processSegmentInterest(interest); });
        // match discovery Interests
        auto discoveryName = MetadataObject::makeDiscoveryInterest(m_prefix).getName();
        face.setInterestFilter(discoveryName, [this](const auto &, const auto &interest)
                               { processDiscoveryInterest(interest); });

        if (m_options.wantShowVersion)
        {
            std::cout << m_versionedPrefix[-1] << "\n";
            spdlog::info("Versioned Prefix: {}", m_versionedPrefix[-1].toNumber());
        }
        if (!m_options.isQuiet)
        {
            std::cerr << "Published " << m_store.size() << " Data packet" << (m_store.size() > 1 ? "s" : "")
                      << " with prefix " << m_versionedPrefix << "\n";
            spdlog::info("Published {} Data packet(s) with prefix {}", m_store.size(), m_versionedPrefix.toUri());
        }
    }

    void
    Producer::run()
    {
        m_face.processEvents();
    }

    void
    Producer::processDiscoveryInterest(const Interest &interest)
    {
        if (m_options.isVerbose)
        {
            std::cerr << "Discovery Interest: " << interest << "\n";
            spdlog::error("Discovery Interest: {}", interest.getName().toUri());
        }

        if (!interest.getCanBePrefix())
        {
            if (m_options.isVerbose)
            {
                std::cerr << "Discovery Interest lacks CanBePrefix, sending Nack\n";
            }
            m_face.put(lp::Nack(interest));
            return;
        }

        MetadataObject mobject;
        mobject.setVersionedName(m_versionedPrefix);

        // make a metadata packet based on the received discovery Interest name
        // interest.getName() is used as the name of the metadata packet to be sent
        auto mdata = mobject.makeData(interest.getName(), m_keyChain, m_options.signingInfo);

        if (m_options.isVerbose)
        {
            std::cerr << "Sending metadata: " << mdata << "\n";
            spdlog::info("Sending metadata: {}", mdata.getName().toUri());
        }

        m_face.put(mdata);
    }
    void
    Producer::processSegmentInterest(const Interest &interest)
    {
        BOOST_ASSERT(!m_store.empty());

        if (m_options.isVerbose)
        {
            std::cerr << "Interest: " << interest << "\n";
            spdlog::info("Interest: {}", interest.getName().toUri());
        }

        const Name &name = interest.getName();
        std::shared_ptr<Data> data;

        if (name.size() == m_versionedPrefix.size() + 1 && name[-1].isSegment())
        {
            const auto segmentNo = static_cast<size_t>(interest.getName()[-1].toSegment());
            // specific segment retrieval
            if (segmentNo < m_store.size())
            {
                data = m_store[segmentNo];
            }
        }
        else if (interest.matchesData(*m_store[0]))
        {
            // unspecified version or segment number, return first segment
            data = m_store[0];
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

} // namespace ndn::chunks