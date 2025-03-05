#include "chunks-interests.hpp"
#include "data-fetcher.hpp"

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
    ChunksInterests::run(const Name &versionedName, DataCallback dataCb, FailureCallback failureCb)
    {
        BOOST_ASSERT(m_options.disableVersionDiscovery ||
                     (!versionedName.empty() && versionedName[-1].isVersion()));
        BOOST_ASSERT(dataCb != nullptr);

        m_prefix = versionedName;
        m_onData = std::move(dataCb);
        m_onFailure = std::move(failureCb);

        // record the start time of the chunks
        m_startTime = time::steady_clock::now();

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

    // TODO: logic is wrong
    bool
    ChunksInterests::allChunksReceived() const
    {
        return m_nextChunkNo >= m_options.TotalChunksNumber;
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

    void
    ChunksInterests::receivedChunkincrement()
    {
        m_nReceived++;
    }

    // void
    // ChunksInterests::onData(const Data &data)
    // {
    //     m_nReceived++;
    //     m_receivedSize += data.getContent().value_size();

    //     m_onData(data);
    // }

    // void
    // ChunksInterests::onFailure(const std::string &reason)
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
    // ChunksInterests::printOptions() const
    // {
    //     std::cerr << "Chunks parameters:\n"
    //               << "\tRequest fresh content = " << (m_options.mustBeFresh ? "yes" : "no") << "\n"
    //               << "\tInterest lifetime = " << m_options.interestLifetime << "\n"
    //               << "\tMax retries on timeout or Nack = " << (m_options.maxRetriesOnTimeoutOrNack == DataFetcher::MAX_RETRIES_INFINITE ? "infinite" : std::to_string(m_options.maxRetriesOnTimeoutOrNack)) << "\n";
    //     spdlog::info("Chunks parameters:\n"
    //                  "\tRequest fresh content = {}\n"
    //                  "\tInterest lifetime = {}\n"
    //                  "\tMax retries on timeout or Nack = {}",
    //                  (m_options.mustBeFresh ? "yes" : "no"),
    //                  m_options.interestLifetime,
    //                  (m_options.maxRetriesOnTimeoutOrNack == DataFetcher::MAX_RETRIES_INFINITE ? "infinite" : std::to_string(m_options.maxRetriesOnTimeoutOrNack)));
    // }

    void
    ChunksInterests::printSummary() const
    {
        std::cerr << "All chunks received" << std::endl;
        spdlog::info("All chunks received");
    }

    // std::string
    // ChunksInterests::formatThroughput(double throughput)
    // {
    //     int pow = 0;
    //     while (throughput >= 1000.0 && pow < 4)
    //     {
    //         throughput /= 1000.0;
    //         pow++;
    //     }
    //     switch (pow)
    //     {
    //     case 0:
    //         return std::to_string(throughput) + " bit/s";
    //     case 1:
    //         return std::to_string(throughput) + " kbit/s";
    //     case 2:
    //         return std::to_string(throughput) + " Mbit/s";
    //     case 3:
    //         return std::to_string(throughput) + " Gbit/s";
    //     case 4:
    //         return std::to_string(throughput) + " Tbit/s";
    //     }
    //     return "";
    // }

} // namespace ndn::chunks
