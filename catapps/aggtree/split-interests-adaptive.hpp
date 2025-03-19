#ifndef IMAgg_SPLITS_INTERESTS_ADAPTIVE_HPP
#define IMAgg_SPLITS_INTERESTS_ADAPTIVE_HPP

#include "split-interests.hpp"
#include "../pipeline/pipeline-interests-adaptive.hpp"
#include "aggtree.hpp"

#include <ndn-cxx/util/rtt-estimator.hpp>
#include <ndn-cxx/util/scheduler.hpp>
#include <ndn-cxx/util/signal.hpp>

#include <queue>
#include <unordered_map>
#include <vector>

namespace ndn::chunks
{
    class Pipeliner;
    class PipelineInterestsAimd;
    class DiscoverVersion;
    class Consumer;

    using util::RttEstimatorWithStats;

    /**
     * @brief Wraps up information that's necessary for split transmission
     */
    struct SplitInfo
    {
        Consumer *consumer;
        time::steady_clock::time_point timeSent;
        // time::nanoseconds rto;
    };

    /**
     * @brief Service for retrieving Data via an Interest splits
     *
     * Retrieves all splited Data under the specified prefix by maintaining a dynamic
     * congestion window combined with a Conservative Loss Adaptation algorithm. For details,
     * please refer to the description in section "Interest splits types in ndncatsplits" of
     * tools/splits/README.md
     *
     * Provides retrieved Data on arrival with no ordering guarantees. Data is delivered to the
     * SplitInterests' user via callback immediately upon arrival.
     */
    class SplitInterestsAdaptive : public SplitInterests
    {
    public:
        /**
         * @brief Constructor.
         *
         * Configures the pipelining service without specifying the retrieval namespace. After this
         * configuration the method run must be called to start the Split.
         */
        SplitInterestsAdaptive(std::vector<std::reference_wrapper<Face>> faces,
                               RttEstimatorWithStats &rttEstimator,
                               const Options &opts);

        ~SplitInterestsAdaptive() override;

        /**
         * @brief Signals when the congestion window changes.
         *
         * The callback function should be: `void(nanoseconds age, double cwnd)`, where `age` is the
         * time since the splits started and `cwnd` is the new congestion window size (in splits).
         */
        signal::Signal<SplitInterestsAdaptive, time::nanoseconds, double> afterCwndChange;

        struct RttSample
        {
            uint64_t chuNum;          ///< split number on which this sample was taken
            time::nanoseconds rtt;    ///< measured RTT
            time::nanoseconds sRtt;   ///< smoothed RTT
            time::nanoseconds rttVar; ///< RTT variation
            time::nanoseconds rto;    ///< retransmission timeout
        };

        /**
         * @brief Signals when a new RTT sample has been taken.
         */
        signal::Signal<SplitInterestsAdaptive, RttSample> afterRttMeasurement;

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

        void
        schedulePackets();

    protected:
        DECLARE_SIGNAL_EMIT(afterCwndChange)

        void
        printOptions() const;

    private:
        /**
         * @brief Fetch all the splits between 0 and lastSplit of the specified prefix.
         *
         * Starts the splits with an adaptive window algorithm to control the window size.
         * The splits will fetch every split until the last split is successfully received
         * or an error occurs.
         */
        void
        doRun() final;

        /**
         * @brief Stop all fetch operations.
         */
        void
        doCancel() final;

        /**
         * @param interestName the name of the Interest to be sent
         * @param faceIndex index of the Face to use for sending the Interest
         */
        void
        sendInterest(Name &interestName, size_t faceIndex);

        /**
         * TODO: add comment
         */
        void sendInitialInterest();

        /**
         * @brief Distribute Interest sending across available Faces
         * @param interestName the name of the Interest to be sent
         * @return the index of the Face used to send the Interest
         */
        size_t
        getNextFaceIndex();

        void
        handleData(const Interest &interest, const Data &data);

        /**
         * @brief handle the initial Data packet
         *
         * @param data the received Data packet
         */
        void initOnData(const Data &data);

        void
        handleNack(const Interest &interest, const lp::Nack &nack);

        void
        handleLifetimeExpiration(const Interest &interest);

        void
        recordTimeout(uint64_t chuNo);

        void
        handleFail(uint64_t chuNo, const std::string &reason);

        void
        recordThroughput();

        PUBLIC_WITH_TESTS_ELSE_PROTECTED : static constexpr double MIN_SSTHRESH = 2.0;

        double m_cwnd;     ///< current congestion window size (in splits)
        double m_ssthresh; ///< current slow start threshold
        RttEstimatorWithStats &m_rttEstimator;

        PUBLIC_WITH_TESTS_ELSE_PRIVATE : std::vector<std::unique_ptr<Scheduler>> m_schedulers; ///< one scheduler per Face
        scheduler::ScopedEventId m_recordEvent;

        uint64_t m_highData = 0;     ///< the highest split number of the Data packet the consumer has received so far
        uint64_t m_highInterest = 0; ///< the highest split number of the Interests the consumer has sent so far
        uint64_t m_recPoint = 0;     ///< the value of m_highInterest when a packet loss event occurred,
        ///< it remains fixed until the next packet loss event happens

        int64_t m_nInFlight = 0;      ///< total # of segments in flight beacause splits master the cc
        int64_t m_nLossDecr = 0;      ///< # of window decreases caused by packet loss
        int64_t m_nMarkDecr = 0;      ///< # of window decreases caused by congestion marks
        int64_t m_nTimeouts = 0;      ///< # of timed out splits
        int64_t m_nSkippedRetx = 0;   ///< # of splits queued for retransmission but received before the
                                      ///< retransmission occurred
        int64_t m_nRetransmitted = 0; ///< # of retransmitted splits
        int64_t m_nCongMarks = 0;     ///< # of data packets with congestion mark
        int64_t m_nSent = 0;          ///< # of interest packets sent out (including retransmissions)
        size_t m_nextFaceIndex = 0;   ///< index of the next Face to use for sending Interest

        std::unordered_map<std::string, SplitInfo> m_splitInfo; ///< keeps all the internal information
                                                                ///< on sent but not acked splits
        std::unordered_map<uint64_t, int> m_retxCount;          ///< maps split number to its retransmission count;
                                                                ///< if the count reaches to the maximum number of
                                                                ///< timeout/nack retries, the splits will be aborted
        std::queue<uint64_t> m_retxQueue;                       ///< total # of chuments in retransmission queue

        bool m_hasFailure = false;
        uint64_t m_failedChuNo = 0;
        std::string m_failureReason;

        DiscoverVersion *m_discover;
    };

} // namespace ndn::chunks

#endif // IMAgg_SPLITS_INTERESTS_ADAPTIVE_HPP