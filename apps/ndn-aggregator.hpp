#include <ndn-cxx/face.hpp>
#include <ndn-cxx/util/rtt-estimator.hpp>
#include <ndn-cxx/util/random.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <spdlog/spdlog.h>
#include <set>
#include <map>
#include <vector>
#include <algorithm>
#include <utility>
#include <string>
#include <numeric>
#include <iostream>
#include <sstream>
#include <queue>
#include <utility>
#include <deque>
#include <boost/multi_index_container.hpp>
#include <ndn-cxx/util/scheduler.hpp>
#include <boost/multi_index/tag.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/circular_buffer.hpp>
#include "sliding_window.hpp"
#include "ndn-cxx/lp/nack-header.hpp"
#include "ns3/log.h"
#include "ndn-app.hpp"
#include "ModelData.hpp"

class Aggregator : public App
{
public:
    Aggregator();
    virtual ~Aggregator() = default;

    /**
     * @brief Start the application
     */
    void StartApplication() override;

    /**
     * @brief Stop the application
     */
    void StopApplication() override;

    virtual void OnInterest(const ndn::InterestFilter &filter, const ndn::Interest &interest) override;

    /**
     * @brief Method that will be called every time new Data arrives
     * @param interest The sent Interest packet
     * @param data The received Data packet
     */
    virtual void OnData(const ndn::Interest &interest, const ndn::Data &data);

    /**
     * @brief Method that will be called every time a Nack arrives
     * @param interest The sent Interest packet
     * @param nack The received Nack packet
     */
    virtual void OnNack(const ndn::Interest &interest, const ndn::lp::Nack &nack);

    /**
     * @brief Method that will be called every time an Interest times out
     * @param interest The sent Interest packet
     */
    virtual void OnTimeout(const ndn::Interest &interest);

    /**
     * @brief Schedule the next packet to be sent
     */
    virtual void ScheduleNextPacket(std::string prefix);
    bool InterestSplitting(uint32_t seq);
    void InterestGenerator();

    /**
     * @brief Send a packet
     */
    void SendPacket(std::string prefix);

    /**
     * @brief Send an Interest packet
     * @param newName The name of the Interest to send
     */
    void SendInterest(std::shared_ptr<ndn::Name> newName);

    void SendData(uint32_t seq);
    void SendNack(std::shared_ptr<const ndn::Interest> interest);

    /**
     * @brief Set the window size
     * @param payload The new window size
     */
    void SetWindow(uint32_t payload);

    /**
     * @brief Get the current window size
     * @return The current window size
     */
    uint32_t GetWindow() const;
    double GetDataRate(std::string prefix);

    void BandwidthEstimation(std::string prefix, double qsfUpstream);

    void RateLimitUpdate(std::string prefix);
    void AggTableRecorder(uint32_t seq);
    void InFlightRecorder(std::string prefix);
    /**
     * @brief Set the maximum sequence number
     * @param seqMax The maximum sequence number
     */
    void SetSeqMax(uint32_t seqMax);

    /**
     * @brief Get the maximum sequence number
     * @return The maximum sequence number
     */
    uint32_t GetSeqMax() const;

    /**
     * @brief Increase the window size
     */
    void WindowIncrease(std::string prefix);

    /**
     * @brief Decrease the window size
     * @param type The type of decrease (e.g., congestion)
     */
    void WindowDecrease(std::string prefix, std::string type);

    void CubicIncerase(std::string prefix);
    void CubicDecrease(std::string prefix, std::string type);

    /**
     * @brief Aggregate data
     * @param data The data to aggregate
     * @param seq The sequence number
     */
    void Aggregate(const ModelData &data, const uint32_t &seq);

    /**
     * @brief Get the mean of the aggregated data
     * @param seq The sequence number
     * @return The mean of the aggregated data
     */
    ModelData GetMean(const uint32_t &seq);

    /**
     * @brief Sum the response time
     * @param response_time The response time to sum
     */
    void ResponseTimeSum(int64_t response_time);

    /**
     * @brief Get the average response time
     * @return The average response time
     */
    int64_t GetResponseTimeAverage();

    /**
     * @brief Sum the aggregation time
     * @param response_time The aggregation time to sum
     */
    void AggregateTimeSum(int64_t response_time);

    /**
     * @brief Get the average aggregation time
     * @return The average aggregation time
     */
    int64_t GetAggregateTimeAverage();

    void RTOMeasure(std::string prefix, int64_t resTime);

    bool CongestionDetection(std::string prefix, int64_t responseTime);
    // QSF
    void RTTMeasure(std::string prefix, int64_t resTime);

    /**
     * @brief Parse the received aggregation tree
     * @param input The input string
     * @return A pair containing the parsed aggregation tree
     */
    std::pair<std::string, std::vector<std::string>> aggTreeProcessSingleString(const std::string &input);

    /**
     * @brief Parse the received aggregation trees
     * @param inputs The input strings
     * @return A map containing the parsed aggregation trees
     */
    std::map<std::string, std::vector<std::string>> aggTreeProcessStrings(const std::vector<std::string> &inputs);

    double GetDataQueueSize(std::string prefix);
    /**
     * @brief Record the window size for testing purposes
     */
    void WindowRecorder(std::string prefix);

    /**
     * @brief Record the RTO for testing purposes
     */
    void RTORecorder(std::string prefix);

    /**
     * @brief Record the response time
     * @param responseTime The response time
     * @param seq The sequence number
     * @param ECN The ECN flag
     * @param threshold_actual The actual threshold
     */
    void ResponseTimeRecorder(std::chrono::milliseconds responseTime, uint32_t seq, std::string prefix);

    /**
     * @brief Record the aggregation time
     * @param aggregateTime The aggregation time
     */
    void AggregateTimeRecorder(std::chrono::milliseconds aggregateTime, uint32_t seq);

    /**
     * @brief Initialize the log file
     */
    void InitializeLogFile();

    void InitializeParameters();
    /**
     * @brief Check if the window can be decreased
     * @param threshold The threshold
     * @return True if the window can be decreased, false otherwise
     */
    bool CanDecreaseWindow(std::string prefix, int64_t threshold);

    /**
     * @brief Record the throughput
     * @param interestThroughput The interest throughput
     * @param dataThroughput The data throughput
     */
    void ThroughputRecorder(int interestThroughput, int dataThroughput, std::chrono::milliseconds start_simulation);

    void ResultRecorder(int64_t aveAggTime);

    void QsfRecorder(std::string prefix, double qsf);

    void QueueRecorder(std::string prefix, double queueSize);

protected:
    /**
     * @brief Check for retransmission timeouts
     */
    void CheckRetxTimeout();

    /**
     * @brief Set the retransmission timer
     * @param retxTimer The retransmission timer
     */
    void SetRetxTimer(std::chrono::milliseconds retxTimer);

    /**
     * @brief Get the retransmission timer
     * @return The retransmission timer
     */
    std::chrono::milliseconds GetRetxTimer() const;

protected:
    std::mt19937 m_rand;                            // random number generator
    std::uniform_real_distribution<> m_uniformDist; // average distribution

    ndn::Name m_prefix;
    ndn::Name m_nexthop;
    ndn::Name m_nexttype;
    std::chrono::milliseconds m_interestLifeTime;

    // Function pointers for callbacks
    typedef std::function<void(std::shared_ptr<App> app, uint32_t seqno, std::chrono::milliseconds delay, int32_t hopCount)> LastRetransmittedInterestDataDelayCallback;
    typedef std::function<void(std::shared_ptr<App> app, uint32_t seqno, std::chrono::milliseconds delay, uint32_t retxCount, int32_t hopCount)> FirstInterestDataDelayCallback;

    // log file
    std::string folderPath = "logs/agg";

    std::map<std::string, std::string> RTO_recorder;
    std::map<std::string, std::string> window_recorder;
    std::map<std::string, std::string> responseTime_recorder;
    std::map<std::string, std::string> inFlight_recorder;
    std::map<std::string, std::string> qsf_recorder;   //? qsf
    std::map<std::string, std::string> qsNew_recorder; //! Updated queue size CC
    std::string aggTable_recorder;
    std::string aggregateTime_recorder;
    int suspiciousPacketCount; // Record the number of timeout
    int downstreamRetxCount;   // Record the number of retransmission interests from downstream
    int interestOverflow;      // Record the number of interest overflow within interest queue
    int dataOverflow;          // Record the number of data overflow within data queue
    int nackCount;             // Record the number of NACK

    // Local throughput measurement
    int totalInterestThroughput;
    int totalDataThroughput;

    std::chrono::milliseconds startSimulation;
    std::chrono::milliseconds stopSimulation;

    // Tree broadcast synchronization
    bool treeSync;

    int numChild;                         // Start congestion control after 3 iterations
    std::map<std::string, int> RTT_count; // How many RTT packets this node has received, used to estimate how many iterations have passed

    //? The following is new design for windowed average RTT
    std::map<std::string, std::deque<int64_t>> RTT_windowed_queue; // Store windowed average RTT for each flow
    std::map<std::string, int64_t> RTT_historical_estimation;      // Historical RTT estimation
    int m_smooth_window_size;                                      // Window size for RTT windowed average

    // Congestion signal
    bool ECNLocal;
    bool ECNRemote;

    //// Basic cwnd management
    uint32_t m_initialWindow;
    std::map<std::string, double> m_window;
    uint32_t m_minWindow;
    std::map<std::string, uint32_t> m_inFlight;
    std::map<std::string, double> m_ssthresh;
    bool m_setInitialWindowOnTimeout;

    // Window decrease suppression
    std::map<std::string, std::chrono::milliseconds> lastWindowDecreaseTime;
    bool isWindowDecreaseSuppressed;

    // CUBIC
    static constexpr double m_cubic_c = 0.4;
    static constexpr double m_cubicBeta = 0.7;
    bool m_useCubicFastConv;
    std::map<std::string, double> m_cubicWmax;
    std::map<std::string, double> m_cubicLastWmax;

    // AIMD
    std::map<std::string, uint32_t> lastCongestionSeq;
    std::map<std::string, int> successiveCongestion;
    bool m_useCwa;
    double m_alpha;                // Timeout decrease factor
    double m_beta;                 // Local congestion decrease factor
    double m_gamma;                // Remote congestion decrease factor
    double m_EWMAFactor;           // Factor used in EWMA, recommended value is between 0.1 and 0.3
    double m_thresholdFactor;      // Factor to compute "RTT_threshold", i.e. "RTT_threshold = Threshold_factor * RTT_measurement"
    bool m_useWIS;                 // Whether to suppress the window increasing rate after congestion
    bool m_reactToCongestionMarks; // PCON's implementation, disable it for now
    int m_interestQueue;           // Max interest queue size
    int m_dataQueue;               // Max data queue size
    int m_dataSize;                // Max data size
    uint32_t m_iteNum;

    // TODO from yitong:debugging this section now
    // Interest sending rate pacing
    std::map<std::string, ndn::scheduler::EventId> m_scheduleEvent;
    std::map<std::string, ndn::scheduler::EventId> m_sendEvent;
    bool firstInterest;
    bool isRTTEstimated;
    int m_initPace;

    //? QSF
    int m_qsfQueueThreshold;
    double m_qsfMDFactor;
    double m_qsfRPFactor;
    int m_qsfTimeDuration;
    double m_qsfInitRate; // Unit: pgks/ms
    std::map<std::string, bool> firstData;
    std::map<std::string, SlidingWindow<double>> m_qsfSlidingWindows;
    std::map<std::string, ndn::scheduler::EventId> m_rateEvent;
    std::map<std::string, double> m_rateLimit;         // Unit: pkgs/ms
    std::map<std::string, double> m_estimatedBW;       // Unit: pkgs/ms
    std::map<std::string, int64_t> RTT_estimation_qsf; // Unit: ms

    // Interest queue (for each flow), keep tracking about each flow
    std::map<std::string, std::deque<uint32_t>> interestQueue;

    // Interest splitting - divided interests
    std::map<std::string, std::string> NameSec0_2;
    std::map<std::string, uint32_t> SeqMap;
    std::vector<std::string> vec_iteration; // Store upstream nodes' name

    // Timeout check and RTT measurement
    std::map<std::string, std::chrono::milliseconds> m_timeoutCheck;
    std::map<std::string, int64_t> SRTT;
    std::map<std::string, int64_t> RTTVAR;
    std::map<std::string, int> roundRTT;
    std::map<std::string, std::chrono::milliseconds> RTO_threshold;
    std::map<std::string, int> numTimeout;

    // Aggregation list
    std::map<uint32_t, std::vector<std::string>> map_agg_oldSeq_newName; // name segments
    std::map<uint32_t, std::string> m_agg_newDataName;                   // whole name

    // Aggregation variables storage
    std::map<uint32_t, bool> partialAggResult;
    std::map<uint32_t, std::vector<double>> sumParameters;             // result after performing aggregation(arithmetic add them together)
    std::map<uint32_t, std::vector<std::string>> congestionSignalList; // result after aggregating congestion signal
    std::map<uint32_t, bool> congestionSignal;                         // congestion signal for current node

    // Response/Aggregation time measurement
    std::map<std::string, std::chrono::milliseconds> rttStartTime;
    std::map<std::string, std::chrono::milliseconds> responseTime;
    int64_t totalResponseTime;
    int round;

    std::map<uint32_t, std::chrono::milliseconds> aggregateStartTime;
    std::map<uint32_t, std::chrono::milliseconds> aggregateTime;
    int64_t totalAggregateTime;
    int iterationCount;

    // Receive aggregation tree from consumer
    std::map<std::string, std::vector<std::string>> aggregationMap;

    uint32_t m_seq;                                 ///< @brief currently requested sequence number
    uint32_t m_seqMax;                              ///< @brief maximum number of sequence number
    std::chrono::milliseconds m_retxTimer;          ///< @brief Currently estimated retransmission timer
    ndn::scheduler::EventId m_retxEvent;            ///< @brief Event to check whether or not retransmission should be performed
    std::shared_ptr<ndn::util::RttEstimator> m_rtt; ///< @brief RTT estimator
    std::chrono::milliseconds m_offTime;            ///< \brief Time interval between packets
    ndn::Name m_interestName;                       ///< \brief NDN Name of the Interest (use Name)
    std::chrono::milliseconds m_freshness;
    uint32_t m_signature;
    ndn::Name m_keyLocator;

    struct RetxSeqsContainer : public std::set<uint32_t>
    {
    };

    RetxSeqsContainer m_retxSeqs; ///< \brief ordered set of sequence numbers to be retransmitted

    struct SeqTimeout
    {
        SeqTimeout(uint32_t _seq, std::chrono::steady_clock::time_point _time)
            : seq(_seq), time(_time) {}

        uint32_t seq;
        std::chrono::steady_clock::time_point time;
    };

    class i_seq
    {
    };

    class i_timestamp
    {
    };

    struct SeqTimeoutsContainer
        : public boost::multi_index::multi_index_container<
              SeqTimeout, boost::multi_index::indexed_by<
                              boost::multi_index::ordered_unique<
                                  boost::multi_index::tag<i_seq>,
                                  boost::multi_index::member<SeqTimeout, uint32_t, &SeqTimeout::seq>>,
                              boost::multi_index::ordered_non_unique<
                                  boost::multi_index::tag<i_timestamp>,
                                  boost::multi_index::member<SeqTimeout, std::chrono::steady_clock::time_point, &SeqTimeout::time>>>>
    {
    };

    SeqTimeoutsContainer m_seqTimeouts; ///< \brief multi-index for the set of SeqTimeout structs
    SeqTimeoutsContainer m_seqLastDelay;
    SeqTimeoutsContainer m_seqFullDelay;
    std::map<uint32_t, uint32_t> m_seqRetxCounts;
    ndn::KeyChain m_keyChain;

    std::chrono::steady_clock::time_point startTime;
};