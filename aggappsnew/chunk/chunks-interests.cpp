#include "chunks-interests.hpp"
#include "../pipeline/data-fetcher.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>

#include <iostream>

namespace ndn::chunks
{

    ChunksInterests::ChunksInterests(Face &face, const Options &opts)
        : m_options(opts), m_face(face)
    {
    }

    ChunksInterests::~ChunksInterests() = default;

    void
    ChunksInterests::run(const Name &versionedName)
    {
        BOOST_ASSERT(m_options.disableVersionDiscovery ||
                     (!versionedName.empty() && versionedName[-1].isVersion()));

        m_prefix = versionedName;

        // record the start time of the chunks
        m_startTime = time::steady_clock::now();

        // record the first timestamp
        m_timeStamp = time::steady_clock::now();

        m_received = new size_t(0);
        doRun();
    }

    void
    ChunksInterests::cancel()
    {
        if (m_isStopping)
            return;

        m_isStopping = true;
        doCancel();
    }

    bool
    ChunksInterests::allChunksReceived() const
    {
        std::cerr << "m_nReceived: " << m_nReceived << " m_options.TotalChunksNumber: " << m_options.TotalChunksNumber << std::endl;
        return m_nReceived == m_options.TotalChunksNumber;
    }

    uint64_t
    ChunksInterests::getNextChunkNo()
    {
        return m_nextChunkNo++;
    }

    int64_t
    ChunksInterests::getReceivedChunks()
    {
        return m_nReceived;
    }

    uint64_t *ChunksInterests::getReceived()
    {
        return m_received;
    }

    void
    ChunksInterests::receivedChunkincrement()
    {
        m_nReceived++;
    }

    void
    ChunksInterests::onData(std::map<uint64_t, std::shared_ptr<const Data>> &data)
    {
        m_nReceived++;
        m_onData(data);
        // m_receivedSize += data.getContent().value_size();
        if (allChunksReceived())
        {

            printSummary();
        }
    }

    void
    ChunksInterests::printSummary() const
    {
        std::cerr << "All chunks received" << std::endl;
        spdlog::info("All chunks received");
    }

    std::string
    ChunksInterests::formatThroughput(double throughput)
    {
        int pow = 0;
        while (throughput >= 1000.0 && pow < 4)
        {
            throughput /= 1000.0;
            pow++;
        }
        switch (pow)
        {
        case 0:
            return std::to_string(throughput) + " bit/s";
        case 1:
            return std::to_string(throughput) + " kbit/s";
        case 2:
            return std::to_string(throughput) + " Mbit/s";
        case 3:
            return std::to_string(throughput) + " Gbit/s";
        case 4:
            return std::to_string(throughput) + " Tbit/s";
        }
        return "";
    }

} // namespace ndn::chunks
