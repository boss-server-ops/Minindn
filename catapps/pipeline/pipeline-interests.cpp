#include "pipeline-interests.hpp"
#include "data-fetcher.hpp"
#include "../chunk/chunks-interests-adaptive.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>

#include <iostream>

namespace ndn::chunks
{

  PipelineInterests::PipelineInterests(Face &face, const Options &opts, ChunksInterestsAdaptive *chunker)
      : m_options(opts), m_face(face), m_chunker(chunker)
  {
  }

  PipelineInterests::~PipelineInterests() = default;

  void
  PipelineInterests::run(const Name &versionedName, DataCallback dataCb, FailureCallback failureCb)
  {
    spdlog::debug("PipelineInterests::run()");
    BOOST_ASSERT(m_options.disableVersionDiscovery ||
                 (!versionedName.empty() && versionedName[-1].isVersion()));
    BOOST_ASSERT(dataCb != nullptr);

    m_prefix = versionedName;
    m_onData = std::move(dataCb);
    m_onFailure = std::move(failureCb);

    // record the start time of the pipeline
    m_startTime = time::steady_clock::now();

    doRun();
  }

  void
  PipelineInterests::cancel()
  {
    if (m_isStopping)
      return;

    m_isStopping = true;
    doCancel();
  }

  bool
  PipelineInterests::allSegmentsReceived() const
  {
    return m_nReceived > 0 &&
           m_hasFinalBlockId &&
           static_cast<uint64_t>(m_nReceived - 1) >= m_lastSegmentNo;
  }

  uint64_t
  PipelineInterests::getNextSegmentNo()
  {
    return m_nextSegmentNo++;
  }

  void
  PipelineInterests::onData(const Data &data)
  {
    m_nReceived++;
    m_receivedSize += data.getContent().value_size();
    m_onData(data);
  }

  void
  PipelineInterests::onFailure(const std::string &reason)
  {
    spdlog::error("PipelineInterests::onFailure(): {}", reason);
    if (m_isStopping)
      return;

    cancel();

    if (m_onFailure)
    {
      boost::asio::post(m_face.getIoContext(), [this, reason]
                        { m_onFailure(reason); });
    }
  }

  void
  PipelineInterests::printOptions() const
  {
    std::cerr << "Pipeline parameters:\n"
              << "\tRequest fresh content = " << (m_options.mustBeFresh ? "yes" : "no") << "\n"
              << "\tInterest lifetime = " << m_options.interestLifetime << "\n"
              << "\tMax retries on timeout or Nack = " << (m_options.maxRetriesOnTimeoutOrNack == DataFetcher::MAX_RETRIES_INFINITE ? "infinite" : std::to_string(m_options.maxRetriesOnTimeoutOrNack)) << "\n";
    spdlog::info("Pipeline parameters:\n"
                 "\tRequest fresh content = {}\n"
                 "\tInterest lifetime = {}\n"
                 "\tMax retries on timeout or Nack = {}",
                 (m_options.mustBeFresh ? "yes" : "no"),
                 m_options.interestLifetime.count(),
                 (m_options.maxRetriesOnTimeoutOrNack == DataFetcher::MAX_RETRIES_INFINITE ? "infinite" : std::to_string(m_options.maxRetriesOnTimeoutOrNack)));
  }

  void
  PipelineInterests::printSummary() const
  {
    uint64_t chunkNo = std::stoi(m_prefix[-1].toUri());
    using namespace ndn::time;
    duration<double, seconds::period> timeElapsed = steady_clock::now() - getStartTime();
    double throughput = 8 * m_receivedSize / timeElapsed.count();

    std::cerr << "\n\nAll segments of chunk" << chunkNo << " of flow " << m_prefix[0].toUri() << " have been received.\n"
              << "Time elapsed: " << timeElapsed << "\n"
              << "Segments received: " << m_nReceived << "\n"
              << "Transferred size: " << m_receivedSize / 1e3 << " kB" << "\n"
              << "Goodput: " << formatThroughput(throughput) << "\n";
    spdlog::info("All segments of chunk {} of flow {} have been received.\n"
                 "Time elapsed: {}\n"
                 "Segments received: {}\n"
                 "Transferred size: {} kB\n"
                 "Goodput: {}",
                 chunkNo, m_prefix[0].toUri(), timeElapsed.count(), m_nReceived, m_receivedSize / 1e3, formatThroughput(throughput));
  }

  std::string
  PipelineInterests::formatThroughput(double throughput)
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

  ChunksInterestsAdaptive *PipelineInterests::getChunker() const
  {
    return m_chunker;
  }
  void PipelineInterests::setChunker(ChunksInterestsAdaptive *chunker)
  {
    m_chunker = chunker;
  }

  Options PipelineInterests::getOptions() const
  {
    return m_options;
  }

} // namespace ndn::chunks
