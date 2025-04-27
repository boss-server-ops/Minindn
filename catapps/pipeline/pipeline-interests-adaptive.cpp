#include "pipeline-interests-adaptive.hpp"
#include "data-fetcher.hpp"
#include "../chunk/chunks-interests-adaptive.hpp"

#include <boost/lexical_cast.hpp>
#include <iomanip>
#include <iostream>

namespace ndn::chunks
{

  PipelineInterestsAdaptive::PipelineInterestsAdaptive(Face &face,
                                                       RttEstimatorWithStats &rttEstimator,
                                                       const Options &opts, ChunksInterestsAdaptive *chunker)
      : PipelineInterests(face, opts, chunker), m_rttEstimator(rttEstimator), m_scheduler(m_face.getIoContext())
  {
  }

  PipelineInterestsAdaptive::~PipelineInterestsAdaptive()
  {
    cancel();
  }

  void
  PipelineInterestsAdaptive::doRun()
  {
    spdlog::info("PipelineInterestsAdaptive::doRun()");
    if (allSegmentsReceived())
    {
      cancel();
      if (!m_options.isQuiet)
      {
        printSummary();
      }
      return;
    }

    // schedule the event to check retransmission timer
    m_checkRtoEvent = m_scheduler.schedule(m_options.rtoCheckInterval, [this]
                                           { checkRto(); });

    schedulePackets();
  }

  void
  PipelineInterestsAdaptive::doCancel()
  {
    spdlog::debug("PipelineInterestsAdaptive::doCancel() in chunumber {}", m_prefix.get(-1).toUri());
    m_checkRtoEvent.cancel();
    m_segmentInfo.clear();
  }

  void
  PipelineInterestsAdaptive::checkRto()
  {
    if (isStopping())
      return;

    bool hasTimeout = false;
    uint64_t highTimeoutSeg = 0;

    for (auto &entry : m_segmentInfo)
    {
      SegmentInfo &segInfo = entry.second;
      if (segInfo.state != SegmentState::InRetxQueue)
      { // skip segments already in the retx queue
        auto timeElapsed = time::steady_clock::now() - segInfo.timeSent;
        if (timeElapsed > segInfo.rto)
        { // timer expired?
          m_nTimeouts++;
          hasTimeout = true;
          highTimeoutSeg = std::max(highTimeoutSeg, entry.first);
          spdlog::debug("enqueue happened from checkRto");
          enqueueForRetransmission(entry.first);
        }
      }
    }

    if (hasTimeout)
    {
      recordTimeout(highTimeoutSeg);
      if (!(m_chunker->getSplitinterest()->m_flowController->shouldPauseFlow(m_prefix.get(0).toUri())))
      {
        schedulePackets();
      }
      else
      {
        spdlog::debug("should pause flow");
      }
    }

    // schedule the next check after predefined interval
    m_checkRtoEvent = m_scheduler.schedule(m_options.rtoCheckInterval, [this]
                                           { checkRto(); });
  }

  void
  PipelineInterestsAdaptive::sendInterest(uint64_t segNo, bool isRetransmission)
  {
    if (isStopping())
      return;

    if (m_hasFinalBlockId && segNo > m_lastSegmentNo)
      return;

    if (!isRetransmission && m_hasFailure)
      return;

    spdlog::info("Send interest for segment #{}", segNo);

    if (m_options.isVerbose)
    {
      std::cerr << (isRetransmission ? "Retransmitting" : "Requesting")
                << " segment #" << segNo << "\n";
      spdlog::info((isRetransmission ? "Retransmitting" : "Requesting"),
                   " segment #{}", segNo);
    }

    if (isRetransmission)
    {
      // keep track of retx count for this segment
      auto ret = m_retxCount.emplace(segNo, 1);
      if (!ret.second)
      { // not the first retransmission
        m_retxCount[segNo] += 1;
        if (m_options.maxRetriesOnTimeoutOrNack != DataFetcher::MAX_RETRIES_INFINITE &&
            m_retxCount[segNo] > m_options.maxRetriesOnTimeoutOrNack)
        {
          return handleFail(segNo, "Reached the maximum number of retries (" +
                                       std::to_string(m_options.maxRetriesOnTimeoutOrNack) +
                                       ") while retrieving segment #" + std::to_string(segNo));
        }

        if (m_options.isVerbose)
        {
          std::cerr << "# of retries for segment #" << segNo
                    << " is " << m_retxCount[segNo] << "\n";
          spdlog::info("# of retries for segment #{} is {}", segNo, m_retxCount[segNo]);
        }
      }
    }

    auto interest = Interest()
                        .setName(Name(m_prefix).appendSegment(segNo))
                        .setMustBeFresh(m_options.mustBeFresh)
                        .setInterestLifetime(m_options.interestLifetime);

    SegmentInfo &segInfo = m_segmentInfo[segNo];
    segInfo.interestHdl = m_face.expressInterest(interest,
                                                 FORWARD_TO_MEM_FN(handleData),
                                                 FORWARD_TO_MEM_FN(handleNack),
                                                 FORWARD_TO_MEM_FN(handleLifetimeExpiration));
    spdlog::debug("Interest name: {}", interest.getName().toUri());
    segInfo.timeSent = time::steady_clock::now();
    segInfo.rto = m_rttEstimator.getEstimatedRto();
    spdlog::debug("In flight increment from sendInterest,m_infight is {},real m_inflight is {} in chunknumber {}", m_chunker->safe_getInFlight(), m_nInFlight, m_prefix.get(-1).toUri());
    m_chunker->safe_InFlightIncrement();
    m_nInFlight++;
    m_nSent++;

    if (isRetransmission)
    {
      segInfo.state = SegmentState::Retransmitted;
      m_nRetransmitted++;
    }
    else
    {
      m_highInterest = segNo;
      segInfo.state = SegmentState::FirstTimeSent;
    }
  }

  void
  PipelineInterestsAdaptive::schedulePackets()
  {
    spdlog::debug("Pipeline schedule packets");
    spdlog::debug("In flight: {} from {},real m_ninflight {} in chunknumber {}", m_chunker->safe_getInFlight(), m_prefix.get(0).toUri(), m_nInFlight, m_prefix.get(-1).toUri());
    spdlog::debug("Window size: {} from {}", m_chunker->safe_getWindowSize(), m_prefix.get(0).toUri());
    BOOST_ASSERT(m_chunker->safe_getInFlight() >= 0);
    BOOST_ASSERT(m_nInFlight >= 0);
    auto availableWindowSize = static_cast<int64_t>(m_chunker->safe_getWindowSize()) - m_chunker->safe_getInFlight();
    spdlog::debug("Available window size: {}", availableWindowSize);
    while (availableWindowSize > 0)
    {

      spdlog::debug("Available window size: {}", availableWindowSize);
      if (!m_retxQueue.empty())
      { // do retransmission first
        uint64_t retxSegNo = m_retxQueue.front();
        m_retxQueue.pop();
        if (m_segmentInfo.count(retxSegNo) == 0)
        {
          m_nSkippedRetx++;
          continue;
        }
        // the segment is still in the map, that means it needs to be retransmitted
        sendInterest(retxSegNo, true);
      }
      else
      { // send next segment
        spdlog::debug("Send next segment not retransmission");
        sendInterest(getNextSegmentNo(), false);
      }
      availableWindowSize--;
    }
    // spdlog::debug("The inflight of segment is {}", m_nInFlight);
    if (m_nInFlight == 0)
    {
      if (m_scheduleEvent)
      {
        m_scheduleEvent.cancel();
      }
      spdlog::debug("schedule beacause of inflight is 0");

      m_scheduleEvent = m_scheduler.schedule(time::milliseconds(0), [this]
                                             { schedulePackets(); });
    }
    // to avoid other flows interruping the current flow

    wait();
  }

  void
  PipelineInterestsAdaptive::wait()
  {
    spdlog::info("Pipeline wait and m_hasFinalBlockId is {}", m_hasFinalBlockId);
    spdlog::info("m_nSent is {} and m_nRetransimitted is {} and m_lastSegmentNo is{}", m_nSent, m_nRetransmitted, m_lastSegmentNo);
    if (m_waitEvent)
    {
      m_waitEvent.cancel();
    }
    if (!m_hasFinalBlockId || (m_hasFinalBlockId && ((m_nSent - m_nRetransmitted) <= m_lastSegmentNo)))
    {
      if (!(m_chunker->getSplitinterest()->m_flowController->shouldPauseFlow(m_prefix.get(0).toUri())))
      {
        m_waitEvent = m_scheduler.schedule(time::milliseconds(0), [this]
                                           { schedulePackets(); });
      }
      else
      {
        spdlog::debug("should pause flow");
        m_waitEvent = m_scheduler.schedule(time::milliseconds(0), [this]
                                           { wait(); });
      }
    }
    else
    {
      m_canschedulenext = true;
    }
  }

  void
  PipelineInterestsAdaptive::handleData(const Interest &interest, const Data &data)
  {
    spdlog::info("Received data for interest {}", interest.getName().toUri());
    if (isStopping())
      return;

    // Interest was expressed with CanBePrefix=false
    BOOST_ASSERT(data.getName().equals(interest.getName()));

    auto &received = *(m_chunker->getReceived());
    std::lock_guard<std::mutex> lock(m_chunker->getSplitinterest()->getMutex());
    received += data.getContent().value_size();
    spdlog::debug("Received {} bytes, total received: {}", data.getContent().value_size(), received);
    if (!m_hasFinalBlockId && data.getFinalBlock())
    {
      m_lastSegmentNo = data.getFinalBlock()->toSegment();
      m_hasFinalBlockId = true;
      cancelInFlightSegmentsGreaterThan(m_lastSegmentNo);
      if (m_hasFailure && m_lastSegmentNo >= m_failedSegNo)
      {
        // previously failed segment is part of the content
        return onFailure(m_failureReason);
      }
      else
      {
        m_hasFailure = false;
      }
    }

    uint64_t recvSegNo = getSegmentFromPacket(data);
    auto segIt = m_segmentInfo.find(recvSegNo);
    if (segIt == m_segmentInfo.end())
    {
      return; // ignore already-received segment
    }

    SegmentInfo &segInfo = segIt->second;
    time::nanoseconds rtt = time::steady_clock::now() - segInfo.timeSent;
    if (m_options.isVerbose)
    {
      std::cerr << "Received segment #" << recvSegNo
                << ", rtt=" << rtt.count() / 1e6 << "ms"
                << ", rto=" << segInfo.rto.count() / 1e6 << "ms\n";
      spdlog::info("Received segment #{}: rtt={}ms, rto={}ms", recvSegNo, rtt.count() / 1e6, segInfo.rto.count() / 1e6);
    }

    m_highData = std::max(m_highData, recvSegNo);

    // for segments in retx queue, we must not decrement m_nInFlight
    // because it was already decremented when the segment timed out
    if (segInfo.state != SegmentState::InRetxQueue)
    {

      spdlog::debug("In flight decrement from handleData,m_infight is {},real m_inflight is {} in chunknumber {}", m_chunker->safe_getInFlight(), m_nInFlight, m_prefix.get(-1).toUri());
      m_chunker->safe_InFlightDecrement();
      m_nInFlight--;
    }

    // upon finding congestion mark, decrease the window size
    // without retransmitting any packet
    if (data.getCongestionMark() > 0)
    {
      m_nCongMarks++;
      if (!m_options.ignoreCongMarks)
      {
        if (m_options.disableCwa || m_highData > m_recPoint)
        {
          m_recPoint = m_highInterest; // react to only one congestion event (timeout or congestion mark)
                                       // per RTT (conservative window adaptation)
          m_nMarkDecr++;
          decreaseWindow();

          if (m_options.isVerbose)
          {
            std::cerr << "Received congestion mark, value = " << data.getCongestionMark()
                      << ", new cwnd = " << m_chunker->safe_getWindowSize() << "\n";
            spdlog::info("Received congestion mark, value = {}, new cwnd = {}", data.getCongestionMark(), m_chunker->safe_getWindowSize());
          }
        }
      }
      else
      {
        increaseWindow();
      }
    }
    else
    {
      increaseWindow();
    }
    onData(data);
    // do not sample RTT for retransmitted segments
    if ((segInfo.state == SegmentState::FirstTimeSent ||
         segInfo.state == SegmentState::InRetxQueue) &&
        m_retxCount.count(recvSegNo) == 0)
    {
      auto nExpectedSamples = std::max<int64_t>((m_nInFlight + 1) >> 1, 1);
      BOOST_ASSERT(nExpectedSamples > 0);
      m_rttEstimator.addMeasurement(rtt, static_cast<size_t>(nExpectedSamples));
      afterRttMeasurement({recvSegNo, rtt,
                           m_rttEstimator.getSmoothedRtt(),
                           m_rttEstimator.getRttVariation(),
                           m_rttEstimator.getEstimatedRto()});
    }

    // remove the entry associated with the received segment
    m_segmentInfo.erase(segIt);

    if (allSegmentsReceived())
    {
      cancel();
      if (!m_options.isQuiet)
      {
        printSummary();
      }
    }
    else
    {
      if (!(m_chunker->getSplitinterest()->m_flowController->shouldPauseFlow(m_prefix.get(0).toUri())))
      {
        schedulePackets();
      }
      else
      {
        spdlog::debug("should pause flow");
      }
    }
  }

  void
  PipelineInterestsAdaptive::handleNack(const Interest &interest, const lp::Nack &nack)
  {
    if (isStopping())
      return;

    if (m_options.isVerbose)
    {
      std::cerr << "Received Nack with reason " << nack.getReason()
                << " for Interest " << interest << "\n";
      spdlog::info("Received Nack with reason {} for Interest {}", boost::lexical_cast<std::string>(nack.getReason()), interest.getName().toUri());
    }

    uint64_t segNo = getSegmentFromPacket(interest);

    switch (nack.getReason())
    {
    case lp::NackReason::DUPLICATE:
      // ignore duplicates
      break;
    case lp::NackReason::CONGESTION:
      // treated the same as timeout for now
      spdlog::debug("enqueue happened from handleNack");
      enqueueForRetransmission(segNo);
      recordTimeout(segNo);
      if (!(m_chunker->getSplitinterest()->m_flowController->shouldPauseFlow(m_prefix.get(0).toUri())))
      {
        schedulePackets();
      }
      else
      {
        spdlog::debug("should pause flow");
      }
      break;
    default:
      handleFail(segNo, "Could not retrieve data for " + interest.getName().toUri() +
                            ", reason: " + boost::lexical_cast<std::string>(nack.getReason()));
      break;
    }
  }

  void
  PipelineInterestsAdaptive::handleLifetimeExpiration(const Interest &interest)
  {
    if (isStopping())
      return;
    uint64_t segNo = getSegmentFromPacket(interest);
    if (m_segmentInfo.at(segNo).state == SegmentState::InRetxQueue)
    {
      spdlog::debug("handleLifetimeExpiration, the segment is already in retx queue");
      return;
    }
    m_nTimeouts++;

    spdlog::debug("enqueue happened from handleLifetimeExpiration");
    enqueueForRetransmission(segNo);
    recordTimeout(segNo);
    if (!(m_chunker->getSplitinterest()->m_flowController->shouldPauseFlow(m_prefix.get(0).toUri())))
    {
      schedulePackets();
    }
    else
    {
      spdlog::debug("should pause flow");
    }
  }

  void
  PipelineInterestsAdaptive::recordTimeout(uint64_t segNo)
  {
    if (m_options.disableCwa || segNo > m_recPoint)
    {
      // interests that are still outstanding during a timeout event
      // should not trigger another window decrease later (bug #5202)
      m_recPoint = m_highInterest;

      decreaseWindow();
      spdlog::info("Timeout event, new cwnd = {}", m_chunker->safe_getWindowSize());
      m_rttEstimator.backoffRto();
      m_nLossDecr++;

      if (m_options.isVerbose)
      {
        std::cerr << "Packet loss event, new cwnd = " << m_chunker->safe_getWindowSize()
                  << ", ssthresh = " << m_chunker->safe_getSsthresh() << "\n";
        spdlog::info("Packet loss event, new cwnd = {}, ssthresh = {}", m_chunker->safe_getWindowSize(), m_chunker->safe_getSsthresh());
      }
    }
  }

  void
  PipelineInterestsAdaptive::enqueueForRetransmission(uint64_t segNo)
  {
    spdlog::debug("in flight is {} from {} and m_ninflight is {} in chunumber {} with segNo {}", m_chunker->safe_getInFlight(), m_prefix.get(0).toUri(), m_nInFlight, m_prefix.get(-1).toUri(), segNo);
    BOOST_ASSERT(m_chunker->safe_getInFlight() > 0);
    BOOST_ASSERT(m_nInFlight > 0);
    spdlog::debug("inflight decrement from enqueueForRetransmission,m_infight is {},real m_ninflight is{} in chunknumber {}", m_chunker->safe_getInFlight(), m_nInFlight, m_prefix.get(-1).toUri());
    m_chunker->safe_InFlightDecrement();
    m_nInFlight--;
    m_retxQueue.push(segNo);
    m_segmentInfo.at(segNo).state = SegmentState::InRetxQueue;
    if (m_nInFlight == 0)
    {
      uint64_t retxSegNo = m_retxQueue.front();
      m_retxQueue.pop();
      if (m_segmentInfo.count(retxSegNo) == 0)
      {
        return;
      }
      // the segment is still in the map, that means it needs to be retransmitted
      sendInterest(retxSegNo, true);
    }
  }

  void
  PipelineInterestsAdaptive::handleFail(uint64_t segNo, const std::string &reason)
  {
    spdlog::error("Failed to retrieve segment #{}: {}", segNo, reason);
    if (isStopping())
      return;

    // if the failed segment is definitely part of the content, raise a fatal error
    if (m_hasFinalBlockId && segNo <= m_lastSegmentNo)
      return onFailure(reason);

    if (!m_hasFinalBlockId)
    {
      m_segmentInfo.erase(segNo);
      spdlog::debug("inflight decrement from handleFail,m_infight is {},real m_inflight is {} in chunknumber {}", m_chunker->safe_getInFlight(), m_nInFlight, m_prefix.get(-1).toUri());
      m_chunker->safe_InFlightDecrement();
      m_nInFlight--;

      if (m_segmentInfo.empty())
      {
        onFailure("Fetching terminated but no final segment number has been found");
      }
      else
      {
        cancelInFlightSegmentsGreaterThan(segNo);
        m_hasFailure = true;
        m_failedSegNo = segNo;
        m_failureReason = reason;
      }
    }
  }

  void
  PipelineInterestsAdaptive::cancelInFlightSegmentsGreaterThan(uint64_t segNo)
  {
    spdlog::debug("cancelInFlightSegmentsGreaterThan {}", segNo);
    for (auto it = m_segmentInfo.begin(); it != m_segmentInfo.end();)
    {
      // cancel fetching all segments that follow
      if (it->first > segNo)
      {
        spdlog::warn("Inflight decrement from cancelInFlightSegmentsGreaterThan,m_infight is {},real m_inflight is {} in chunknumber {} and segNo is {}", m_chunker->safe_getInFlight(), m_nInFlight, m_prefix.get(-1).toUri(), it->first);
        if (it->second.state != SegmentState::InRetxQueue)
        {
          m_chunker->safe_InFlightDecrement();
          m_nInFlight--;
        }
        it = m_segmentInfo.erase(it);

        // m_chunker->safe_InFlightDecrement();
        // m_nInFlight--;
      }
      else
      {
        spdlog::warn("cancelInFlightSegmentsGreaterThan,m_infight is {},real m_inflight is {} in chunknumber {} and segNo is {}", m_chunker->safe_getInFlight(), m_nInFlight, m_prefix.get(-1).toUri(), it->first);
        ++it;
      }
    }
  }

  void
  PipelineInterestsAdaptive::printOptions() const
  {
    PipelineInterests::printOptions();
    std::cerr
        << "\tInitial congestion window size = " << m_options.initCwnd << "\n"
        << "\tInitial slow start threshold = " << m_options.initSsthresh << "\n"
        << "\tAdditive increase step = " << m_options.aiStep << "\n"
        << "\tMultiplicative decrease factor = " << m_options.mdCoef << "\n"
        << "\tRTO check interval = " << m_options.rtoCheckInterval << "\n"
        << "\tReact to congestion marks = " << (m_options.ignoreCongMarks ? "no" : "yes") << "\n"
        << "\tConservative window adaptation = " << (m_options.disableCwa ? "no" : "yes") << "\n"
        << "\tResetting window to " << (m_options.resetCwndToInit ? "initial value" : "ssthresh") << " upon loss event\n";
    spdlog::info("\tInitial congestion window size = {}", m_options.initCwnd,
                 "\tInitial slow start threshold = {}", m_options.initSsthresh,
                 "\tAdditive increase step = {}", m_options.aiStep,
                 "\tMultiplicative decrease factor = {}", m_options.mdCoef,
                 "\tRTO check interval = {}", m_options.rtoCheckInterval.count(),
                 "\tReact to congestion marks = {}", (m_options.ignoreCongMarks ? "no" : "yes"),
                 "\tConservative window adaptation = {}", (m_options.disableCwa ? "no" : "yes"),
                 "\tResetting window to {}", (m_options.resetCwndToInit ? "initial value" : "ssthresh"));
  }

  void
  PipelineInterestsAdaptive::printSummary() const
  {
    PipelineInterests::printSummary();
    std::cerr << "Congestion marks: " << m_nCongMarks << " (caused " << m_nMarkDecr << " window decreases)\n"
              << "Timeouts: " << m_nTimeouts << " (caused " << m_nLossDecr << " window decreases)\n"
              << "Retransmitted segments: " << m_nRetransmitted
              << " (" << (m_nSent == 0 ? 0 : (m_nRetransmitted * 100.0 / m_nSent)) << "%)"
              << ", skipped: " << m_nSkippedRetx << "\n"
              << "RTT ";
    spdlog::info("Congestion marks: {} (caused {} window decreases)", m_nCongMarks, m_nMarkDecr,
                 "Timeouts: {} (caused {} window decreases)", m_nTimeouts, m_nLossDecr,
                 "Retransmitted segments: {} ({}%)", m_nRetransmitted, (m_nSent == 0 ? 0 : (m_nRetransmitted * 100.0 / m_nSent)),
                 "skipped: {}", m_nSkippedRetx);

    if (m_rttEstimator.getMinRtt() == time::nanoseconds::max() ||
        m_rttEstimator.getMaxRtt() == time::nanoseconds::min())
    {
      std::cerr << "stats unavailable\n";
      spdlog::info("stats unavailable");
    }
    else
    {
      std::cerr << "min/avg/max = " << std::fixed << std::setprecision(3)
                << m_rttEstimator.getMinRtt().count() / 1e6 << "/"
                << m_rttEstimator.getAvgRtt().count() / 1e6 << "/"
                << m_rttEstimator.getMaxRtt().count() / 1e6 << " ms\n";
      spdlog::info("min/avg/max = {}/{}/{} ms", m_rttEstimator.getMinRtt().count() / 1e6,
                   m_rttEstimator.getAvgRtt().count() / 1e6, m_rttEstimator.getMaxRtt().count() / 1e6);
    }
  }

  std::ostream &
  operator<<(std::ostream &os, SegmentState state)
  {
    switch (state)
    {
    case SegmentState::FirstTimeSent:
      os << "FirstTimeSent";
      break;
    case SegmentState::InRetxQueue:
      os << "InRetxQueue";
      break;
    case SegmentState::Retransmitted:
      os << "Retransmitted";
      break;
    }
    return os;
  }

} // namespace ndn::chunks
