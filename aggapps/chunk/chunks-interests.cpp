#include "chunks-interests.hpp"
#include "../pipeline/data-fetcher.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>

#include <iostream>

namespace ndn::chunks
{

    ChunksInterests::ChunksInterests(Face &face, const Options &opts, Aggregator *aggregator)
        : m_options(opts), m_face(face), m_aggregator(aggregator)
    {
        m_flowController = m_aggregator->getFlowController();
    }

    ChunksInterests::~ChunksInterests() = default;

    void
    ChunksInterests::run(const Name &versionedName)
    {
        BOOST_ASSERT(m_options.disableVersionDiscovery ||
                     (!versionedName.empty() && versionedName[-1].isVersion()));

        m_prefix = versionedName;

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
        if (!data.empty())
        {
            auto &firstData = data.begin()->second;
            const Name &dataName = firstData->getName();

            //  extract node name and chunk number from the name
            if (dataName.size() >= 3)
            {
                std::string nodeName = dataName[0].toUri();

                // chunknumber is a form of string, need to convert
                std::string chunkNumberStr = dataName[-2].toUri();
                uint64_t chunkNumber = 0;
                try
                {
                    // remove possible URI-encoded characters
                    if (chunkNumberStr.front() == '/' || chunkNumberStr.front() == '%')
                    {
                        chunkNumberStr = chunkNumberStr.substr(1);
                    }
                    chunkNumber = std::stoull(chunkNumberStr);

                    spdlog::debug("Processing data from node {}, chunk {}", nodeName, chunkNumber);

                    m_flowController->addChunk(nodeName, chunkNumber, data);
                }
                catch (const std::exception &e)
                {
                    spdlog::error("Failed to parse chunk number '{}': {}", chunkNumberStr, e.what());
                }
            }
            else
            {
                spdlog::warn("Data name has incorrect format: {}", dataName.toUri());
            }
        }
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
