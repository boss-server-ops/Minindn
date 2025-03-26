#ifndef IMAgg_CHUNKS_INTERESTS_ADAPTIVE_HPP
#define IMAgg_CHUNKS_INTERESTS_ADAPTIVE_HPP

#include "chunks-interests.hpp"
#include "../pipeline/pipeline-interests-adaptive.hpp"

#include <ndn-cxx/util/rtt-estimator.hpp>
#include <ndn-cxx/util/scheduler.hpp>
#include <ndn-cxx/util/signal.hpp>

#include <queue>
#include <unordered_map>
#include <atomic>

namespace ndn::chunks
{
    class Pipeliner;
    class PipelineInterestsAimd;
    class DiscoverVersion;

    using util::RttEstimatorWithStats;

    // std::ostream &
    // operator<<(std::ostream &os, ChunkState state);

    /**
     * @brief Wraps up information that's necessary for chunk transmission
     */
    struct ChunkInfo
    {
        // ScopedPendingInterestHandle interestHdl;
        Pipeliner *pipeliner;
        time::steady_clock::time_point timeSent;
        // time::nanoseconds rto;
    };

    /**
     * @brief Service for retrieving Data via an Interest chunks
     *
     * Retrieves all chunked Data under the specified prefix by maintaining a dynamic
     * congestion window combined with a Conservative Loss Adaptation algorithm. For details,
     * please refer to the description in section "Interest chunks types in ndncatchunks" of
     * tools/chunks/README.md
     *
     * Provides retrieved Data on arrival with no ordering guarantees. Data is delivered to the
     * ChunksInterests' user via callback immediately upon arrival.
     */
    class ChunksInterestsAdaptive : public ChunksInterests
    {
    public:
        /**
         * @brief Constructor.
         *
         * Configures the pipelining service without specifying the retrieval namespace. After this
         * configuration the method run must be called to start the Chunks.
         */
        ChunksInterestsAdaptive(Face &face, RttEstimatorWithStats &rttEstimator, const Options &opts);

        ~ChunksInterestsAdaptive() override;

        /**
         * @brief Signals when the congestion window changes.
         *
         * The callback function should be: `void(nanoseconds age, double cwnd)`, where `age` is the
         * time since the chunks started and `cwnd` is the new congestion window size (in chunks).
         */
        signal::Signal<ChunksInterestsAdaptive, time::nanoseconds, double> afterCwndChange;

        struct RttSample
        {
            uint64_t chuNum;          ///< chunk number on which this sample was taken
            time::nanoseconds rtt;    ///< measured RTT
            time::nanoseconds sRtt;   ///< smoothed RTT
            time::nanoseconds rttVar; ///< RTT variation
            time::nanoseconds rto;    ///< retransmission timeout
        };

        /**
         * @brief Signals when a new RTT sample has been taken.
         */
        signal::Signal<ChunksInterestsAdaptive, RttSample> afterRttMeasurement;

        /**
         * @brief A safe way to increase the congestion window.
         * @param value the value to increase the congestion window by
         */
        void safe_WindowIncrement(double value);
        /**
         * @brief A safe way to decrease the congestion window.
         * @param value the value to decrease the congestion window by
         */
        void safe_WindowDecrement(double value);
        /**
         * @brief A safe way to set the congestion window size.
         * @param value the new congestion window size
         */
        void safe_setWindowSize(double value);
        // /*
        //  * @brief A safe way to increase the slow start threshold.
        //  */
        // void safe_SsthreshIncrement(double value);
        // /*
        //  * @brief A safe way to decrease the slow start threshold.
        //  */
        // void safe_SsthreshDecrement(double value);
        /**
         * @brief A safe way to set the slow start threshold.
         * @param value the new slow start threshold
         */
        void safe_setSsthresh(double value);
        /**
         * @brief A safe way to increase the number of in-flight packets.
         */
        void safe_InFlightIncrement();
        /**
         * @brief A safe way to decrease the number of in-flight packets.
         */
        void safe_InFlightDecrement();

        /**
         * @brief A safe way to get the congestion window size.
         */
        double safe_getWindowSize();
        /**
         * @brief A safe way to get the slow start threshold.
         */
        double safe_getSsthresh();
        /**
         * @brief A safe way to get the number of in-flight packets.
         */
        int64_t safe_getInFlight();
        void safe_setWmax(double value);
        double safe_getWmax();
        void safe_setLastWmax(double value);
        double safe_getLastWmax();
        void safe_setLastDecrease(time::steady_clock::time_point value);
        time::steady_clock::time_point safe_getLastDecrease();
        void
        schedulePackets();

    protected:
        DECLARE_SIGNAL_EMIT(afterCwndChange)

        void
        printOptions() const;

        // private:
        //     /**
        //      * @brief Increase congestion window.
        //      */
        //     virtual void
        //     increaseWindow() = 0;

        //     /**
        //      * @brief Decrease congestion window.
        //      */
        //     virtual void
        //     decreaseWindow() = 0;

    private:
        /**
         * @brief Fetch all the chunks between 0 and lastChunk of the specified prefix.
         *
         * Starts the chunks with an adaptive window algorithm to control the window size.
         * The chunks will fetch every chunk until the last chunk is successfully received
         * or an error occurs.
         */
        void
        doRun() final;

        /**
         * @brief Stop all fetch operations.
         */
        void
        doCancel() final;

        // /**
        //  * @brief Check RTO for all sent-but-not-acked chunks.
        //  */
        // void
        // checkRto();

        /**
         * @param chuNo the chunk # of the to-be-sent Interest
         */
        void
        sendInterest(uint64_t chuNo);

        void
        handleData(const Interest &interest, const Data &data);

        void
        handleNack(const Interest &interest, const lp::Nack &nack);

        void
        handleLifetimeExpiration(const Interest &interest);

        void
        recordTimeout(uint64_t chuNo);
        /**
         * @brief Enqueue the chunk for retransmission
         *
         * @param chuNo the chunk number to be retransmitted
         */
        void
        enqueueForRetransmission(uint64_t chuNo);

        void
        handleFail(uint64_t chuNo, const std::string &reason);

        void
        cancelInFlightChunksGreaterThan(uint64_t chuNo);

        /**
         * @brief Check if the next chunk can be sent.
         */
        void
        checkSendNext(uint64_t chuNo);

        void
        recordThroughput();

        // PUBLIC_WITH_TESTS_ELSE_PRIVATE : void
        //                                  printSummary() const final;

        PUBLIC_WITH_TESTS_ELSE_PROTECTED : static constexpr double MIN_SSTHRESH = 2.0;

        double m_cwnd;     ///< current congestion window size (in chunks)
        double m_ssthresh; ///< current slow start threshold
        RttEstimatorWithStats &m_rttEstimator;

        PUBLIC_WITH_TESTS_ELSE_PRIVATE : Scheduler m_scheduler;
        scheduler::ScopedEventId m_checkEvent;
        // scheduler::ScopedEventId m_checkRtoEvent;

        uint64_t m_highData = 0;     ///< the highest chunk number of the Data packet the consumer has received so far
        uint64_t m_highInterest = 0; ///< the highest chunk number of the Interests the consumer has sent so far
        uint64_t m_recPoint = 0;     ///< the value of m_highInterest when a packet loss event occurred,
        ///< it remains fixed until the next packet loss event happens

        int64_t m_nInFlight = 0;      ///< total # of segments in flight beacause chunks master the cc
        int64_t m_nLossDecr = 0;      ///< # of window decreases caused by packet loss
        int64_t m_nMarkDecr = 0;      ///< # of window decreases caused by congestion marks
        int64_t m_nTimeouts = 0;      ///< # of timed out chunks
        int64_t m_nSkippedRetx = 0;   ///< # of chunks queued for retransmission but received before the
                                      ///< retransmission occurred
        int64_t m_nRetransmitted = 0; ///< # of retransmitted chunks
        int64_t m_nCongMarks = 0;     ///< # of data packets with congestion mark
        int64_t m_nSent = 0;          ///< # of interest packets sent out (including retransmissions)

        std::unordered_map<uint64_t, ChunkInfo> m_chunkInfo; ///< keeps all the internal information
                                                             ///< on sent but not acked chunks
        std::unordered_map<uint64_t, int> m_retxCount;       ///< maps chunk number to its retransmission count;
                                                             ///< if the count reaches to the maximum number of
                                                             ///< timeout/nack retries, the chunks will be aborted
        std::queue<uint64_t> m_retxQueue;                    ///< total # of chuments in retransmission queue

        bool m_hasFailure = false;
        uint64_t m_failedChuNo = 0;
        std::string m_failureReason;

        DiscoverVersion *m_discover;

        // These are for cubic
        double m_wmax = 0.0;                           ///< window size before last window decrease
        double m_lastWmax = 0.0;                       ///< last wmax
        time::steady_clock::time_point m_lastDecrease; ///< time of last window decrease
    };

} // namespace ndn::chunks

#endif // IMAgg_CHUNKS_INTERESTS_ADAPTIVE_HPP
