#include "split-interests.hpp"
#include "../pipeline/data-fetcher.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>

#include <iostream>

namespace ndn::chunks
{

    SplitInterests::SplitInterests(std::vector<std::reference_wrapper<Face>> faces, const Options &opts)
        : m_options(opts), m_faces(std::move(faces))
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

        m_received = new size_t(0);
        m_aggTree.getTreeTopology(m_options.topoFile, "con0");
        m_outputFile.open(m_options.outputFile, std::ios::binary);
        m_flowController = FlowController::createFromAggTree("../experiments/conconfig.ini", m_outputFile, m_aggTree, "con0");
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
        return m_nReceivedFlow == m_aggTree.rootChildCount;
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
                std::string chunkNumberStr = dataName[1].toUri();
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

        if (allSplitReceived())
        {
            printSummary();
            cancel();
        }
    }

    // void
    // SplitInterests::onFailure(const std::string &reason)
    // {
    //     if (m_isStopping)
    //         return;

    //     cancel();

    //     if (m_onFailure)
    //     {
    //         boost::asio::post(m_face.getIoContext(), [this, reason]
    //                           { m_onFailure(reason); });
    //     }
    // }

    // void
    // SplitInterests::printOptions() const
    // {
    //     std::cerr << "Split parameters:\n"
    //               << "\tRequest fresh content = " << (m_options.mustBeFresh ? "yes" : "no") << "\n"
    //               << "\tInterest lifetime = " << m_options.interestLifetime << "\n"
    //               << "\tMax retries on timeout or Nack = " << (m_options.maxRetriesOnTimeoutOrNack == DataFetcher::MAX_RETRIES_INFINITE ? "infinite" : std::to_string(m_options.maxRetriesOnTimeoutOrNack)) << "\n";
    //     spdlog::info("Split parameters:\n"
    //                  "\tRequest fresh content = {}\n"
    //                  "\tInterest lifetime = {}\n"
    //                  "\tMax retries on timeout or Nack = {}",
    //                  (m_options.mustBeFresh ? "yes" : "no"),
    //                  m_options.interestLifetime,
    //                  (m_options.maxRetriesOnTimeoutOrNack == DataFetcher::MAX_RETRIES_INFINITE ? "infinite" : std::to_string(m_options.maxRetriesOnTimeoutOrNack)));
    // }

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
