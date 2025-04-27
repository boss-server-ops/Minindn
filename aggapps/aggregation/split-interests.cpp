#include "split-interests.hpp"
#include "../pipeline/data-fetcher.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>

#include <iostream>

namespace ndn::chunks
{

    SplitInterests::SplitInterests(std::vector<std::reference_wrapper<Face>> faces, const Options &opts, Aggregator *aggregator)
        : m_options(opts), m_faces(std::move(faces)), m_aggregator(aggregator)
    {
        // ensure at least one Face is provided
        if (m_faces.empty())
        {
            throw std::invalid_argument("At least one Face must be provided");
        }
    }

    SplitInterests::~SplitInterests() = default;

    void
    SplitInterests::run(const Name &versionedName, DataCallback dataCb, FailureCallback failureCb)
    {
        spdlog::debug("SplitInterests::run() called");
        BOOST_ASSERT(m_options.disableVersionDiscovery ||
                     (!versionedName.empty() && versionedName[-1].isVersion()));
        BOOST_ASSERT(dataCb != nullptr);

        m_prefix = versionedName;
        m_onData = std::move(dataCb);
        m_onFailure = std::move(failureCb);

        // record the start time of the splits
        m_startTime = time::steady_clock::now();

        // record the first timestamp
        m_timeStamp = time::steady_clock::now();
        std::lock_guard<std::mutex> lock(m_receivedMutex);
        m_received = new size_t(0);
        m_flowController = m_aggregator->getFlowController();
        doRun();
    }

    void
    SplitInterests::cancel()
    {
        if (m_isStopping)
            return;

        m_isStopping = true;
        doCancel();
    }

    bool
    SplitInterests::allSplitReceived() const
    {

        if (m_aggregator == nullptr)
        {
            spdlog::warn("Aggregator is null in allSplitReceived check");
            return false;
        }

        size_t childCount = m_aggregator->getChildInterestNames().size();

        if (childCount == 0)
        {
            spdlog::warn("No child nodes found in Aggregator");
            return false;
        }

        spdlog::debug("Checking completion: received {} flows out of {} child nodes",
                      m_nReceivedFlow, childCount);

        return m_nReceivedFlow >= static_cast<int64_t>(childCount);
    }

    uint64_t
    SplitInterests::getNextSplitNo()
    {
        return m_nextSplitNo++;
    }

    int64_t
    SplitInterests::getReceivedSplit()
    {
        return m_nReceived;
    }

    uint64_t *SplitInterests::getReceived()
    {
        std::lock_guard<std::mutex> lock(m_receivedMutex);
        return m_received;
    }

    void
    SplitInterests::receivedSplitincrement()
    {
        m_nReceivedFlow++;
    }

    void
    SplitInterests::onData(std::map<uint64_t, std::shared_ptr<const Data>> &data)
    {
        m_onData(data);

        // obtain information from the first data packet
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

                    spdlog::info("Processing data from node {}, chunk {}", nodeName, chunkNumber);

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

        if (allSplitReceived())
        {
            printSummary();
            cancel();
        }
    }

    void
    SplitInterests::printSummary() const
    {
        std::cerr << "All splits received" << std::endl;
        spdlog::info("All splits received");
    }

    std::string
    SplitInterests::formatThroughput(double throughput)
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
