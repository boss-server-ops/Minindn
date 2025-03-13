#include "split-interests.hpp"
#include "../pipeline/data-fetcher.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>

#include <iostream>

namespace ndn::chunks
{

    SplitInterests::SplitInterests(Face &face, Face &face2, const Options &opts)
        : m_options(opts), m_face(face), m_face2(face2)
    {
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
        return m_nReceived == m_aggTree.rootChildCount;
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
        m_nReceived++;
    }

    void
    SplitInterests::onData()
    {
        m_nReceived++;
        if (allSplitReceived())
        {
            printSummary();
            cancel();
        }
        m_onData();
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
