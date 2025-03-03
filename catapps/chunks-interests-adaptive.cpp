#include "chunks-interests-adaptive.hpp"
#include "data-fetcher.hpp"
#include "pipeliner.hpp"

#include <boost/lexical_cast.hpp>
#include <ndn-cxx/security/validator-null.hpp>
#include <iomanip>
#include <iostream>
#include <thread>

namespace ndn::chunks
{

    ChunksInterestsAdaptive::ChunksInterestsAdaptive(Face &face,
                                                     RttEstimatorWithStats &rttEstimator,
                                                     const Options &opts, PipelineInterests &pipeline, DiscoverVersion &discover)
        : ChunksInterests(face, opts), m_cwnd(m_options.initCwnd), m_ssthresh(m_options.initSsthresh), m_rttEstimator(rttEstimator), m_pipeline(&pipeline), m_discover((&discover))
    {
    }

    ChunksInterestsAdaptive::~ChunksInterestsAdaptive()
    {
        cancel();
    }

    void
    ChunksInterestsAdaptive::doRun()
    {
        if (allChunksReceived())
        {
            cancel();
            if (!m_options.isQuiet)
            {
                printSummary();
            }
            return;
        }

        // // schedule the event to check retransmission timer
        // m_checkRtoEvent = m_scheduler.schedule(m_options.rtoCheckInterval, [this]
        //                                        { checkRto(); });

        schedulePackets();
    }

    void
    ChunksInterestsAdaptive::doCancel()
    {
        // m_checkRtoEvent.cancel();
        m_chunkInfo.clear();
    }

    // void
    // ChunksInterestsAdaptive::checkRto()
    // {
    //     if (isStopping())
    //         return;

    //     bool hasTimeout = false;
    //     uint64_t highTimeoutChu = 0;

    //     for (auto &entry : m_chunkInfo)
    //     {
    //         ChunkInfo &chuInfo = entry.second;
    //         if (chuInfo.state != ChunkState::InRetxQueue)
    //         { // skip chunks already in the retx queue
    //             auto timeElapsed = time::steady_clock::now() - chuInfo.timeSent;
    //             if (timeElapsed > chuInfo.rto)
    //             { // timer expired?
    //                 m_nTimeouts++;
    //                 hasTimeout = true;
    //                 highTimeoutChu = std::max(highTimeoutChu, entry.first);
    //                 enqueueForRetransmission(entry.first);
    //             }
    //         }
    //     }

    //     if (hasTimeout)
    //     {
    //         recordTimeout(highTimeoutChu);
    //         schedulePackets();
    //     }

    //     // schedule the next check after predefined interval
    //     m_checkRtoEvent = m_scheduler.schedule(m_options.rtoCheckInterval, [this]
    //                                            { checkRto(); });
    // }

    void
    ChunksInterestsAdaptive::sendInterest(uint64_t chuNo)
    {
        if (isStopping())
            return;

        if (chuNo >= m_options.TotalChunksNumber)
            return;

        // if (!isRetransmission && m_hasFailure)
        //     return;

        if (m_options.isVerbose)
        {
            std::cerr << "Requesting"
                      << " chunk #" << chuNo << "\n";
            spdlog::info("Requesting",
                         " chunk #{}", chuNo);
        }

        // Set the chunk component but it is actually a number component
        // auto interest = Interest()
        //                     .setName(Name(m_prefix).appendNumber(chuNo))
        //                     .setMustBeFresh(m_options.mustBeFresh)
        //                     .setInterestLifetime(m_options.interestLifetime);

        // m_ChuPipeline[chuNo] = m_pipeline;
        // m_Chu
        ChunkInfo &chuInfo = m_chunkInfo[chuNo];
        Pipeliner *pipeliner = new Pipeliner(security::getAcceptAllValidator());
        chuInfo.pipeliner = pipeliner;
        pipeliner = nullptr;
        m_discover->setPrefix(Name(m_prefix).appendNumber(chuNo));
        m_pipeline->setChunker(this);
        std::thread([this, &chuInfo]()
                    { chuInfo.pipeliner->run(std::make_unique<DiscoverVersion>(*m_discover), std::make_unique<PipelineInterests>(*m_pipeline)); })
            .detach();
        // chuInfo.interestHdl = m_face.expressInterest(interest,
        //                                              FORWARD_TO_MEM_FN(handleData),
        //                                              FORWARD_TO_MEM_FN(handleNack),
        //                                              FORWARD_TO_MEM_FN(handleLifetimeExpiration));

        chuInfo.timeSent = time::steady_clock::now();
        // chuInfo.rto = m_rttEstimator.getEstimatedRto();

        m_nSent++;
        m_highInterest = chuNo;
    }

    void
    ChunksInterestsAdaptive::schedulePackets()
    {
        BOOST_ASSERT(safe_getInFlight() >= 0);
        auto availableWindowSize = static_cast<int64_t>(safe_getWindowSize()) - safe_getInFlight();

        while (availableWindowSize > 0)
        {
            sendInterest(getNextChunkNo());
            // After sendInterest, the availableWindowSize should change,so detect it.
            availableWindowSize = static_cast<int64_t>(safe_getWindowSize()) - safe_getInFlight();
        }
    }

    void ChunksInterestsAdaptive::safe_WindowIncrement(double value)
    {
        double current = m_cwnd.load(std::memory_order_relaxed);
        double desired;
        do
        {
            desired = current + value;
        } while (!m_cwnd.compare_exchange_weak(
            current, desired,
            std::memory_order_release,
            std::memory_order_relaxed));
    }
    void ChunksInterestsAdaptive::safe_WindowDecrement(double value)
    {
        double current = m_cwnd.load(std::memory_order_relaxed);
        double desired;
        do
        {
            desired = current - value;
        } while (!m_cwnd.compare_exchange_weak(
            current, desired,
            std::memory_order_release,
            std::memory_order_relaxed));
    }

    // void ChunksInterestsAdaptive::safe_SsthreshIncrement(double value)
    // {
    //     double current = m_ssthresh.load(std::memory_order_relaxed);
    //     double desired;
    //     do
    //     {
    //         desired = current + value;
    //     } while (!m_ssthresh.compare_exchange_weak(
    //         current, desired,
    //         std::memory_order_release,
    //         std::memory_order_relaxed));
    // }
    // void ChunksInterestsAdaptive::safe_SsthreshDecrement(double value)
    // {
    //     double current = m_ssthresh.load(std::memory_order_relaxed);
    //     double desired;
    //     do
    //     {
    //         desired = current - value;
    //     } while (!m_ssthresh.compare_exchange_weak(
    //         current, desired,
    //         std::memory_order_release,
    //         std::memory_order_relaxed));
    // }

    void ChunksInterestsAdaptive::safe_setSsthresh(double value)
    {
        m_ssthresh.store(value, std::memory_order_relaxed);
    }

    void ChunksInterestsAdaptive::safe_InFlightIncrement()
    {
        m_nInFlight.fetch_add(1, std::memory_order_relaxed);
    }
    void ChunksInterestsAdaptive::safe_InFlightDecrement()
    {
        m_nInFlight.fetch_sub(1, std::memory_order_relaxed);
    }

    double ChunksInterestsAdaptive::safe_getWindowSize()
    {
        return m_cwnd.load(std::memory_order_relaxed);
    }

    int64_t ChunksInterestsAdaptive::safe_getInFlight()
    {
        return m_nInFlight.load(std::memory_order_relaxed);
    }

    double ChunksInterestsAdaptive::safe_getSsthresh()
    {
        return m_ssthresh.load(std::memory_order_relaxed);
    }

    // void
    // ChunksInterestsAdaptive::handleData(const Interest &interest, const Data &data)
    // {
    //     if (isStopping())
    //         return;

    //     // Interest was expressed with CanBePrefix=false
    //     BOOST_ASSERT(data.getName().equals(interest.getName()));

    //     if (!m_hasFinalBlockId && data.getFinalBlock())
    //     {
    //         m_lastChunkNo = data.getFinalBlock()->toChunk();
    //         m_hasFinalBlockId = true;
    //         cancelInFlightChunksGreaterThan(m_lastChunkNo);
    //         if (m_hasFailure && m_lastChunkNo >= m_failedChuNo)
    //         {
    //             // previously failed chunk is part of the content
    //             return onFailure(m_failureReason);
    //         }
    //         else
    //         {
    //             m_hasFailure = false;
    //         }
    //     }

    //     uint64_t recvChuNo = getChunkFromPacket(data);
    //     auto chuIt = m_chunkInfo.find(recvChuNo);
    //     if (chuIt == m_chunkInfo.end())
    //     {
    //         return; // ignore already-received chunk
    //     }

    //     ChunkInfo &chuInfo = chuIt->second;
    //     time::nanoseconds rtt = time::steady_clock::now() - chuInfo.timeSent;
    //     if (m_options.isVerbose)
    //     {
    //         std::cerr << "Received chunk #" << recvChuNo
    //                   << ", rtt=" << rtt.count() / 1e6 << "ms"
    //                   << ", rto=" << chuInfo.rto.count() / 1e6 << "ms\n";
    //         spdlog::info("Received chunk #{}: rtt={}ms, rto={}ms", recvChuNo, rtt.count() / 1e6, chuInfo.rto.count() / 1e6);
    //     }

    //     m_highData = std::max(m_highData, recvChuNo);

    //     // for chunks in retx queue, we must not decrement m_nInFlight
    //     // because it was already decremented when the chunk timed out
    //     if (chuInfo.state != ChunkState::InRetxQueue)
    //     {
    //         m_nInFlight--;
    //     }

    //     // upon finding congestion mark, decrease the window size
    //     // without retransmitting any packet
    //     if (data.getCongestionMark() > 0)
    //     {
    //         m_nCongMarks++;
    //         if (!m_options.ignoreCongMarks)
    //         {
    //             if (m_options.disableCwa || m_highData > m_recPoint)
    //             {
    //                 m_recPoint = m_highInterest; // react to only one congestion event (timeout or congestion mark)
    //                                              // per RTT (conservative window adaptation)
    //                 m_nMarkDecr++;
    //                 decreaseWindow();

    //                 if (m_options.isVerbose)
    //                 {
    //                     std::cerr << "Received congestion mark, value = " << data.getCongestionMark()
    //                               << ", new cwnd = " << m_cwnd << "\n";
    //                     spdlog::info("Received congestion mark, value = {}, new cwnd = {}", data.getCongestionMark(), m_cwnd);
    //                 }
    //             }
    //         }
    //         else
    //         {
    //             increaseWindow();
    //         }
    //     }
    //     else
    //     {
    //         increaseWindow();
    //     }

    //     onData(data);

    //     // do not sample RTT for retransmitted chunks
    //     if ((chuInfo.state == ChunkState::FirstTimeSent ||
    //          chuInfo.state == ChunkState::InRetxQueue) &&
    //         m_retxCount.count(recvChuNo) == 0)
    //     {
    //         auto nExpectedSamples = std::max<int64_t>((m_nInFlight + 1) >> 1, 1);
    //         BOOST_ASSERT(nExpectedSamples > 0);
    //         m_rttEstimator.addMeasurement(rtt, static_cast<size_t>(nExpectedSamples));
    //         afterRttMeasurement({recvChuNo, rtt,
    //                              m_rttEstimator.getSmoothedRtt(),
    //                              m_rttEstimator.getRttVariation(),
    //                              m_rttEstimator.getEstimatedRto()});
    //     }

    //     // remove the entry associated with the received chunk
    //     m_chunkInfo.erase(chuIt);

    //     if (allChunksReceived())
    //     {
    //         cancel();
    //         if (!m_options.isQuiet)
    //         {
    //             printSummary();
    //         }
    //     }
    //     else
    //     {
    //         schedulePackets();
    //     }
    // }

    // void
    // ChunksInterestsAdaptive::handleNack(const Interest &interest, const lp::Nack &nack)
    // {
    //     if (isStopping())
    //         return;

    //     if (m_options.isVerbose)
    //     {
    //         std::cerr << "Received Nack with reason " << nack.getReason()
    //                   << " for Interest " << interest << "\n";
    //         spdlog::info("Received Nack with reason {} for Interest {}", boost::lexical_cast<std::string>(nack.getReason()), interest.getName().toUri());
    //     }

    //     uint64_t chuNo = getChunkFromPacket(interest);

    //     switch (nack.getReason())
    //     {
    //     case lp::NackReason::DUPLICATE:
    //         // ignore duplicates
    //         break;
    //     case lp::NackReason::CONGESTION:
    //         // treated the same as timeout for now
    //         enqueueForRetransmission(chuNo);
    //         recordTimeout(chuNo);
    //         schedulePackets();
    //         break;
    //     default:
    //         handleFail(chuNo, "Could not retrieve data for " + interest.getName().toUri() +
    //                               ", reason: " + boost::lexical_cast<std::string>(nack.getReason()));
    //         break;
    //     }
    // }

    // void
    // ChunksInterestsAdaptive::handleLifetimeExpiration(const Interest &interest)
    // {
    //     if (isStopping())
    //         return;

    //     m_nTimeouts++;

    //     uint64_t chuNo = getChunkFromPacket(interest);
    //     enqueueForRetransmission(chuNo);
    //     recordTimeout(chuNo);
    //     schedulePackets();
    // }

    // void
    // ChunksInterestsAdaptive::recordTimeout(uint64_t chuNo)
    // {
    //     if (m_options.disableCwa || chuNo > m_recPoint)
    //     {
    //         // interests that are still outstanding during a timeout event
    //         // should not trigger another window decrease later (bug #5202)
    //         m_recPoint = m_highInterest;

    //         decreaseWindow();
    //         m_rttEstimator.backoffRto();
    //         m_nLossDecr++;

    //         if (m_options.isVerbose)
    //         {
    //             std::cerr << "Packet loss event, new cwnd = " << m_cwnd
    //                       << ", ssthresh = " << m_ssthresh << "\n";
    //             spdlog::info("Packet loss event, new cwnd = {}, ssthresh = {}", m_cwnd, m_ssthresh);
    //         }
    //     }
    // }

    // void
    // ChunksInterestsAdaptive::enqueueForRetransmission(uint64_t chuNo)
    // {
    //     BOOST_ASSERT(m_nInFlight > 0);
    //     m_nInFlight--;
    //     m_retxQueue.push(chuNo);
    //     m_chunkInfo.at(chuNo).state = ChunkState::InRetxQueue;
    // }

    // void
    // ChunksInterestsAdaptive::handleFail(uint64_t chuNo, const std::string &reason)
    // {
    //     if (isStopping())
    //         return;

    //     // if the failed chunk is definitely part of the content, raise a fatal error
    //     if (m_hasFinalBlockId && chuNo <= m_lastChunkNo)
    //         return onFailure(reason);

    //     if (!m_hasFinalBlockId)
    //     {
    //         m_chunkInfo.erase(chuNo);
    //         m_nInFlight--;

    //         if (m_chunkInfo.empty())
    //         {
    //             onFailure("Fetching terminated but no final chunk number has been found");
    //         }
    //         else
    //         {
    //             cancelInFlightChunksGreaterThan(chuNo);
    //             m_hasFailure = true;
    //             m_failedChuNo = chuNo;
    //             m_failureReason = reason;
    //         }
    //     }
    // }

    // void
    // ChunksInterestsAdaptive::cancelInFlightChunksGreaterThan(uint64_t chuNo)
    // {
    //     for (auto it = m_chunkInfo.begin(); it != m_chunkInfo.end();)
    //     {
    //         // cancel fetching all chunks that follow
    //         if (it->first > chuNo)
    //         {
    //             it = m_chunkInfo.erase(it);
    //             m_nInFlight--;
    //         }
    //         else
    //         {
    //             ++it;
    //         }
    //     }
    // }

    // void
    // ChunksInterestsAdaptive::printOptions() const
    // {
    //     ChunksInterests::printOptions();
    //     std::cerr
    //         << "\tInitial congestion window size = " << m_options.initCwnd << "\n"
    //         << "\tInitial slow start threshold = " << m_options.initSsthresh << "\n"
    //         << "\tAdditive increase step = " << m_options.aiStep << "\n"
    //         << "\tMultiplicative decrease factor = " << m_options.mdCoef << "\n"
    //         << "\tRTO check interval = " << m_options.rtoCheckInterval << "\n"
    //         << "\tReact to congestion marks = " << (m_options.ignoreCongMarks ? "no" : "yes") << "\n"
    //         << "\tConservative window adaptation = " << (m_options.disableCwa ? "no" : "yes") << "\n"
    //         << "\tResetting window to " << (m_options.resetCwndToInit ? "initial value" : "ssthresh") << " upon loss event\n";
    //     spdlog::info("\tInitial congestion window size = {}", m_options.initCwnd,
    //                  "\tInitial slow start threshold = {}", m_options.initSsthresh,
    //                  "\tAdditive increase step = {}", m_options.aiStep,
    //                  "\tMultiplicative decrease factor = {}", m_options.mdCoef,
    //                  "\tRTO check interval = {}", m_options.rtoCheckInterval,
    //                  "\tReact to congestion marks = {}", (m_options.ignoreCongMarks ? "no" : "yes"),
    //                  "\tConservative window adaptation = {}", (m_options.disableCwa ? "no" : "yes"),
    //                  "\tResetting window to {}", (m_options.resetCwndToInit ? "initial value" : "ssthresh"));
    // }

    // void
    // ChunksInterestsAdaptive::printSummary() const
    // {
    //     ChunksInterests::printSummary();
    //     std::cerr << "Congestion marks: " << m_nCongMarks << " (caused " << m_nMarkDecr << " window decreases)\n"
    //               << "Timeouts: " << m_nTimeouts << " (caused " << m_nLossDecr << " window decreases)\n"
    //               << "Retransmitted chunks: " << m_nRetransmitted
    //               << " (" << (m_nSent == 0 ? 0 : (m_nRetransmitted * 100.0 / m_nSent)) << "%)"
    //               << ", skipped: " << m_nSkippedRetx << "\n"
    //               << "RTT ";
    //     spdlog::info("Congestion marks: {} (caused {} window decreases)", m_nCongMarks, m_nMarkDecr,
    //                  "Timeouts: {} (caused {} window decreases)", m_nTimeouts, m_nLossDecr,
    //                  "Retransmitted chunks: {} ({}%)", m_nRetransmitted, (m_nSent == 0 ? 0 : (m_nRetransmitted * 100.0 / m_nSent)),
    //                  "skipped: {}", m_nSkippedRetx);

    //     if (m_rttEstimator.getMinRtt() == time::nanoseconds::max() ||
    //         m_rttEstimator.getMaxRtt() == time::nanoseconds::min())
    //     {
    //         std::cerr << "stats unavailable\n";
    //         spdlog::info("stats unavailable");
    //     }
    //     else
    //     {
    //         std::cerr << "min/avg/max = " << std::fixed << std::setprecision(3)
    //                   << m_rttEstimator.getMinRtt().count() / 1e6 << "/"
    //                   << m_rttEstimator.getAvgRtt().count() / 1e6 << "/"
    //                   << m_rttEstimator.getMaxRtt().count() / 1e6 << " ms\n";
    //         spdlog::info("min/avg/max = {}/{}/{} ms", m_rttEstimator.getMinRtt().count() / 1e6,
    //                      m_rttEstimator.getAvgRtt().count() / 1e6, m_rttEstimator.getMaxRtt().count() / 1e6);
    //     }
    // }

    // std::ostream &
    // operator<<(std::ostream &os, ChunkState state)
    // {
    //     switch (state)
    //     {
    //     case ChunkState::FirstTimeSent:
    //         os << "FirstTimeSent";
    //         break;
    //     case ChunkState::InRetxQueue:
    //         os << "InRetxQueue";
    //         break;
    //     case ChunkState::Retransmitted:
    //         os << "Retransmitted";
    //         break;
    //     }
    //     return os;
    // }

} // namespace ndn::chunks
