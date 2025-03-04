#include "chunks-interests-adaptive.hpp"
#include "data-fetcher.hpp"
#include "pipeline-interests-adaptive.hpp"
#include "pipeline-interests-aimd.hpp"
#include "pipeliner.hpp"
#include "discover-version.hpp"

#include <boost/lexical_cast.hpp>
#include <ndn-cxx/security/validator-null.hpp>
#include <iomanip>
#include <iostream>
#include <thread>

namespace ndn::chunks
{

    ChunksInterestsAdaptive::ChunksInterestsAdaptive(Face &face,
                                                     RttEstimatorWithStats &rttEstimator,
                                                     const Options &opts)
        : ChunksInterests(face, opts), m_cwnd(m_options.initCwnd), m_ssthresh(m_options.initSsthresh), m_rttEstimator(rttEstimator), m_scheduler(m_face.getIoContext())
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
            // if (!m_options.isQuiet)
            // {
            //     printSummary();
            // }
            return;
        }

        schedulePackets();
    }

    void
    ChunksInterestsAdaptive::doCancel()
    {
        m_chunkInfo.clear();
    }

    void
    ChunksInterestsAdaptive::sendInterest(uint64_t chuNo)
    {
        if (isStopping())
            return;

        if (chuNo >= m_options.TotalChunksNumber)
            return;

        if (m_options.isVerbose)
        {
            std::cerr << "Requesting chunk #" << chuNo << "\n";
            spdlog::debug("Requesting chunk #{}, Name :{}", chuNo, m_prefix.toUri());
        }

        ChunkInfo &chuInfo = m_chunkInfo[chuNo];
        chuInfo.pipeliner = new Pipeliner(security::getAcceptAllValidator());
        Name namewithchuno;
        m_scheduler.schedule(time::milliseconds(0), [this, chuNo, namewithchuno]() mutable
                             {
        spdlog::debug("Lambda expression executed for chunk #{}", chuNo);
        auto& chuInfo = m_chunkInfo[chuNo];
        namewithchuno = Name(m_prefix).append(std::to_string(chuNo));
        spdlog::debug("Name :{}", namewithchuno.toUri());
        auto discover = std::make_unique<DiscoverVersion>(m_face, namewithchuno, m_options);
        auto pipeline = std::make_unique<PipelineInterestsAimd>(m_face, m_rttEstimator, m_options);
        pipeline->setChunker(this);
        chuInfo.pipeliner->run(std::move(discover), std::move(pipeline)); });

        chuInfo.timeSent = time::steady_clock::now();
        m_nSent++;
        m_highInterest = chuNo;
    }

    void
    ChunksInterestsAdaptive::schedulePackets()
    {
        // BOOST_ASSERT(safe_getInFlight() >= 0);
        // auto availableWindowSize = static_cast<int64_t>(safe_getWindowSize()) - safe_getInFlight();

        // while (availableWindowSize > 0)
        // {
        // availableWindowSize = static_cast<int64_t>(safe_getWindowSize()) - safe_getInFlight();
        // }
        for (int i = 0; i < m_options.TotalChunksNumber; i++)
        {
            sendInterest(getNextChunkNo());
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
    void ChunksInterestsAdaptive::safe_setWindowSize(double value)
    {
        m_cwnd.store(value, std::memory_order_relaxed);
    }

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

} // namespace ndn::chunks