#ifndef NDN_CONSUMER_HPP
#define NDN_CONSUMER_HPP

#include <ndn-cxx/face.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/encoding/block.hpp>
#include <ndn-cxx/util/random.hpp>
#include <ndn-cxx/util/rtt-estimator.hpp>
#include <ndn-cxx/util/time.hpp>
#include <ndn-cxx/util/scheduler.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <memory>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <set>
#include <map>
#include <vector>
#include <string>
#include <queue>
#include <tuple>
#include <chrono>
#include "ndn-app.hpp"
#include "ModelData.hpp"
#include "sliding_window.hpp"

class Consumer : public App
{
public:
    Consumer();
    virtual ~Consumer() = default;

    void run();

    // /**
    //  * @brief Method that will be called every time new Data arrives
    //  * @param interest The sent Interest packet
    //  * @param data The received Data packet
    //  */
    // virtual void OnData(const ndn::Interest &interest, const ndn::Data &data);

    // /**
    //  * @brief Method that will be called every time a Nack arrives
    //  * @param nack The received Nack packet
    //  */
    // virtual void OnNack(const ndn::lp::Nack &nack);

    // /**
    //  * @brief Method that will be called every time an Interest times out
    //  * @param interest The sent Interest packet
    //  */
    // virtual void OnTimeout(std::string nameString);

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

    void TreeBroadcast();
    void ConstructAggregationTree();

    void SendPacket(std::string prefix);
    void InterestGenerator();

    bool InterestSplitting();
    /**
     * @brief Method to aggregate data
     * @param data The received ModelData
     * @param seq The sequence number
     */
    void Aggregate(const ModelData &data, const uint32_t &seq);

    /**
     * @brief Method to get the mean of the parameters
     * @param seq The sequence number
     * @return A vector of mean values
     */
    std::vector<double> getMean(const uint32_t &seq);

    /**
     * @brief Method to calculate the sum of response times
     * @param response_time The response time to add
     */
    void ResponseTimeSum(int64_t response_time);

    /**
     * @brief Method to get the average response time
     * @return The average response time
     */
    int64_t GetResponseTimeAverage();

    /**
     * @brief Method to calculate the sum of aggregate times
     * @param response_time The aggregate time to add
     */
    void AggregateTimeSum(int64_t response_time);

    /**
     * @brief Method to get the average aggregate time
     * @return The average aggregate time
     */
    int64_t GetAggregateTimeAverage();

    /**
     * @brief Utility function to get the size of the data queue for a given prefix
     * @param prefix The prefix for which to get the data queue size
     * @return The size of the data queue
     */
    double getDataQueueSize(std::string prefix);

    /**
     * @brief Measure the Round-Trip Time (RTT) for a given prefix
     * @param prefix The prefix for which to measure RTT
     * @param resTime The response time
     */
    void RTTMeasure(std::string prefix, int64_t resTime);

    /**
     * @brief Get the data rate for a given prefix
     * @param prefix The prefix for which to get the data rate
     * @return The data rate
     */
    double GetDataRate(std::string prefix);

    /**
     * @brief Estimate the bandwidth for a given prefix based on the QSF (Queue Size Factor) upstream
     * @param prefix The prefix for which to estimate bandwidth
     * @param qsfUpstream The QSF upstream value
     */
    void BandwidthEstimation(std::string prefix, double qsfUpstream);

    /**
     * @brief Update the rate limit for a given prefix
     * @param prefix The prefix for which to update the rate limit
     */
    void RateLimitUpdate(std::string prefix);

    /**
     * @brief Record the QSF (Queue Size Factor) for a given prefix
     * @param prefix The prefix for which to record the QSF
     * @param qsf The QSF value
     */
    void QsfRecorder(std::string prefix, double qsf);

    /**
     * @brief Detect congestion for a given prefix based on the response time
     * @param prefix The prefix for which to detect congestion
     * @param responseTime The response time
     * @return True if congestion is detected, false otherwise
     */
    bool CongestionDetection(std::string prefix, int64_t responseTime);

    /**
     * @brief Method to measure the Round-Trip Time (RTT) for a given prefix based on response time
     * @param resTime The response time
     * @param prefix The prefix for which to measure RTT
     * @return The measured RTT in milliseconds
     */
    std::chrono::milliseconds RTOMeasure(int64_t resTime, std::string prefix);

    /**
     * @brief Method to record RTO results in files for testing purposes
     */
    void RTORecorder(std::string prefix);

    // waiting for comments
    void InFlightRecorder(std::string prefix);

    /**
     * @brief Method to record response time for a given prefix and sequence number
     * @param roundIndex The round index
     * @param prefix The prefix for which to record response time
     * @param seq The sequence number
     * @param responseTime The response time in milliseconds
     */
    void ResponseTimeRecorder(int roundIndex, std::string prefix, uint32_t seq, std::chrono::milliseconds responseTime);

    /**
     * @brief Method to record aggregate time for a given sequence number
     * @param aggregateTime The aggregate time in milliseconds
     * @param seq The sequence number
     */
    void AggregateTimeRecorder(std::chrono::milliseconds aggregateTime, uint32_t seq);

    /**
     * @brief Method to initialize the log file
     */
    void InitializeLogFile();

    /**
     * @brief Method to initialize parameters
     */
    void InitializeParameter();

    /**
     * @brief Method to check if the window size can be decreased based on a threshold
     * @param prefix The prefix for which to check the window size
     * @param threshold The threshold value
     * @return True if the window size can be decreased, false otherwise
     */
    bool CanDecreaseWindow(std::string prefix, int64_t threshold);

    /**
     * @brief Method to record throughput information
     * @param interestThroughput The throughput of interests
     * @param dataThroughput The throughput of data
     * @param start_simulation The start time of the simulation
     * @param start_throughput The start time of the throughput measurement
     */
    void ThroughputRecorder(int interestThroughput, int dataThroughput, std::chrono::milliseconds start_simulation, std::chrono::milliseconds start_throughput);

    /**
     * @brief Method to record aggregate tree information
     */
    void AggTreeRecorder();

    /**
     * @brief Method to record the results of the simulation
     * @param iteNum The iteration number
     * @param timeoutNum The number of timeouts
     * @param aveAggTime The average aggregation time
     * @param totalTime The total time of the simulation
     */
    void ResultRecorder(uint32_t iteNum, int timeoutNum, int64_t aveAggTime, int64_t totalTime);

    /**
     * @brief Method to record the queue size for a given prefix
     * @param prefix The prefix for which to record the queue size
     * @param queueSize The size of the queue
     */
    void QueueRecorder(std::string prefix, double queueSize);

    /**
     * @brief Override the function in App class to return leaf nodes
     * @param key The key to search for
     * @param treeMap The tree map
     * @return A map of leaf nodes
     */
    std::map<std::string, std::set<std::string>>
    getLeafNodes(const std::string &key, const std::map<std::string, std::vector<std::string>> &treeMap);

    /**
     * @brief Method to find the round index
     * @param target The target string
     * @return The round index
     */
    int findRoundIndex(const std::string &target);

public:
    typedef void (*LastRetransmittedInterestDataDelayCallback)(std::shared_ptr<App> app, uint32_t seqno, std::chrono::milliseconds delay, int32_t hopCount);
    typedef void (*FirstInterestDataDelayCallback)(std::shared_ptr<App> app, uint32_t seqno, std::chrono::milliseconds delay, uint32_t retxCount, int32_t hopCount);

protected:
    // from App
    virtual void StartApplication() override;

    virtual void StopApplication() override;

    virtual void ScheduleNextPacket(std::string prefix) = 0;

    virtual void SendInterest(std::shared_ptr<ndn::Name> newName);

    /**
     * @brief Method to check for retransmission timeout
     */
    void CheckRetxTimeout();

    /**
     * @brief Method to set the retransmission timer
     * @param retxTimer The retransmission timer value
     */
    void SetRetxTimer(std::chrono::milliseconds retxTimer);

    /**
     * @brief Method to get the retransmission timer
     * @return The retransmission timer value
     */
    std::chrono::milliseconds GetRetxTimer() const;

protected:
    // Topology file name
    std::string filename;

    // Log path
    std::string folderPath = "logs/con";
    std::map<std::string, std::string> RTO_recorder;
    std::map<std::string, std::string> responseTime_recorder; // Format: 'Time', 'seq', 'RTT', ‘ECN’, 'isWindowDecreaseSuppressed'
    std::map<std::string, std::string> windowRecorder;        // Format: 'Time', 'cwnd', 'cwnd threshold' (when cwnd is larger than the threshold, cwnd increase will be suppressed)
    std::map<std::string, std::string> inFlight_recorder;     // Format: 'Time', 'inFlight'
    std::map<std::string, std::string> qsf_recorder;          //? qsf
    std::map<std::string, std::string> qsNew_recorder;        //! Updated queue size CC
    std::string aggregateTime_recorder;                       // Format: 'Time', 'aggTime'
    int suspiciousPacketCount;                                // When timeout is triggered, add one
    int dataOverflow;                                         // Record the number of data overflow
    int nackCount;                                            // Record the number of NACK
    int suspiciousPacketCount;                                // When timeout is triggered, add one

    // Update when WindowDecrease() is called every time, used for CWA algorithm
    std::map<std::string, std::chrono::milliseconds> lastWindowDecreaseTime;
    bool isWindowDecreaseSuppressed;

    // Throughput measurement
    int totalInterestThroughput;
    int totalDataThroughput;

    std::chrono::milliseconds startSimulation;
    std::chrono::milliseconds stopSimulation;
    std::chrono::milliseconds startThroughputMeasurement;
    bool throughputStable; // Start record the throughput after first reach congestion, consider the point as the start of stable throughput

    // General window design
    uint32_t m_initialWindow;
    uint32_t m_minWindow;
    std::map<std::string, double> m_window;
    std::map<std::string, uint32_t> m_inFlight;

    // AIMD design
    std::map<std::string, double> m_ssthresh;
    bool m_useCwa;
    uint32_t m_highData;
    double m_recPoint;
    double m_alpha; // Timeout decrease factor
    double m_beta;  // Local congestion decrease factor
    double m_gamma; // Remote congestion decrease factor
    double m_addRttSuppress;
    bool m_reactToCongestionMarks;

    // CUBIC
    static constexpr double m_cubic_c = 0.4;
    static constexpr double m_cubicBeta = 0.7;
    bool m_useCubicFastConv;
    std::map<std::string, double> m_cubicWmax;
    std::map<std::string, double> m_cubicLastWmax;

    // TODO: debugging
    // Interest sending rate pacing
    std::map<std::string, ndn::scheduler::EventId> m_scheduleEvent;
    std::map<std::string, ndn::scheduler::EventId> m_sendEvent;
    bool isRTTEstimated;
    int m_initPace;

    //! QSF
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

    // Global flow map
    std::vector<std::vector<std::string>> globalTreeRound; // First dimension: round. Second dimension: next-tier leaves, initialized in ConstructAggregationTree()
    int linkCount;

    //! Start here, implement per-flow design, change round into per-flow
    std::map<std::string, int> RTT_count; // How many RTT packets this node has received, used to estimate how many iterations have passed

    //? The following is new design for windowed average RTT
    std::map<std::string, std::deque<int64_t>> RTT_windowed_queue; // Store windowed average RTT for each flow
    std::map<std::string, int64_t> RTT_historical_estimation;      // Historical RTT estimation
    int m_smooth_window_size;                                      // Window size for RTT windowed average

    //* initSeq is only used once, when broadcasting the aggregation tree
    uint32_t initSeq;

    // Track seq of each flow when sending interests
    //? Track the global iteration when interest splitting

    uint32_t globalSeq;

    //? Track the latest iteration has been sent. Function - determine whether it's the correct time to start aggregation time
    std::map<std::string, uint32_t> SeqMap; // Key: flow, Value: seq
    //? Keep a interest queue for each flow
    std::map<std::string, std::deque<uint32_t>> interestQueue; // Key: flow, Value: queue

    // Get producer list, which are used to generate new interests
    std::string proList;

    // Constructed aggregation Tree
    std::vector<std::map<std::string, std::vector<std::string>>> aggregationTree; // Entire tree (including main tree and sub-trees)
                                                                                  // Broadcast aggregation tree
    bool broadcastSync;
    std::set<std::string> broadcastList; // Elements within the set need to be broadcasted, all elements are unique

    std::map<uint32_t, std::vector<std::string>> map_agg_oldSeq_newName; // Manage names for entire iteration
    std::map<uint32_t, bool> m_agg_finished;                             // Manage whether aggregation is finished for each iteration

    // Used inside InterestGenerator
    std::map<std::string, std::string> NameSec0_2; // Key: flow, Value: first three segments of interest name, e.g. "/agg0/pro0.pro1/data"
    std::vector<std::string> vec_iteration;        // Store upstream nodes' name

    // Timeout check/ RTO measurement
    std::map<std::string, std::chrono::milliseconds> m_timeoutCheck;
    std::map<std::string, std::chrono::milliseconds> RTO_threshold;
    std::map<std::string, int64_t> SRTT;
    std::map<std::string, int64_t> RTTVAR;
    std::map<std::string, bool> initRTO;
    // TODO: Count the number of timeout, testing purpose only
    std::map<std::string, int> numTimeout;

    // Designed for actual aggregation operations
    std::map<uint32_t, bool> partialAggResult;
    std::map<uint32_t, std::vector<double>> sumParameters;
    std::map<uint32_t, std::vector<double>> aggregationResult;
    int producerCount;

    // Congestion signal
    bool ECNLocal;
    bool ECNRemote;

    // defined for response time
    std::map<std::string, std::chrono::milliseconds> rttStartTime;
    std::map<std::string, std::chrono::milliseconds> responseTime;
    int64_t total_response_time;
    int round;

    // defined for aggregation time
    std::map<uint32_t, std::chrono::milliseconds> aggregateStartTime;
    std::map<uint32_t, std::chrono::milliseconds> aggregateTime;
    int64_t totalAggregateTime;
    int iterationCount;

    // New defined attribute variables
    std::string m_topologyType;
    std::string m_interestName; // Consumer's interest prefix
    std::string m_nodeprefix;   // Consumer's node prefix
    uint32_t m_iteNum;          // The number of iterations
    int m_interestQueue;        // Interest queue size
    int m_dataQueue;            // Data queue size
    int m_dataSize;             // Data size
    int m_constraint;           // Constraint of each sub-tree
    double m_EWMAFactor;        // Factor used in EWMA, recommended value is between 0.1 and 0.3
    double m_thresholdFactor;   // Factor to compute "RTT_threshold", i.e. "RTT_threshold = Threshold_factor * RTT_measurement"
    bool m_useWIS;
    // use m_rand and m_uniformDist to replace the m_rand in ndnSIM
    std::mt19937 m_rand;                            // random number generator
    std::uniform_real_distribution<> m_uniformDist; // average distribution
    uint32_t m_seq;                                 ///< @brief currently requested sequence number
    uint32_t m_seqMax;                              ///< @brief maximum number of sequence number
    std::chrono::milliseconds m_retxTimer;          ///< @brief Currently estimated retransmission timer
    ndn::scheduler::EventId m_retxEvent;            ///< @brief Event to check whether or not retransmission should be performed

    // change the type of the m_rtt of the ndnSIM
    std::unique_ptr<ndn::util::RttEstimator> m_rtt; ///< @brief RTT estimator

    std::chrono::milliseconds m_offTime;          ///< \brief Time interval between packets
    std::chrono::milliseconds m_interestLifeTime; ///< \brief LifeTime for interest packet

    /// @cond include_hidden
    /**
     * \struct This struct contains sequence numbers of packets to be retransmitted
     */
    struct RetxSeqsContainer : public std::set<uint32_t>
    {
    };

    RetxSeqsContainer m_retxSeqs; ///< \brief ordered set of sequence numbers to be retransmitted

    /**
     * \struct This struct contains a pair of packet sequence number and its timeout
     */
    struct SeqTimeout
    {
        SeqTimeout(uint32_t _seq, std::chrono::milliseconds _time)
            : seq(_seq), time(_time)
        {
        }

        uint32_t seq;
        std::chrono::milliseconds time;
    };
    /// @endcond

    /// @cond include_hidden
    class i_seq
    {
    };
    class i_timestamp
    {
    };
    /// @endcond

    /// @cond include_hidden
    /**
     * \struct This struct contains a multi-index for the set of SeqTimeout structs
     */
    struct SeqTimeoutsContainer
        : public boost::multi_index::
              multi_index_container<SeqTimeout,
                                    boost::multi_index::
                                        indexed_by<boost::multi_index::
                                                       ordered_unique<boost::multi_index::tag<i_seq>,
                                                                      boost::multi_index::
                                                                          member<SeqTimeout, uint32_t,
                                                                                 &SeqTimeout::seq>>,
                                                   boost::multi_index::
                                                       ordered_non_unique<boost::multi_index::
                                                                              tag<i_timestamp>,
                                                                          boost::multi_index::
                                                                              member<SeqTimeout, std::chrono::milliseconds,
                                                                                     &SeqTimeout::time>>>>
    {
    };

    SeqTimeoutsContainer m_seqTimeouts; ///< \brief multi-index for the set of SeqTimeout structs

    SeqTimeoutsContainer m_seqLastDelay;
    SeqTimeoutsContainer m_seqFullDelay;
    std::map<uint32_t, uint32_t> m_seqRetxCounts;

    std::chrono::steady_clock::time_point startTime;
};

#endif // NDN_CONSUMER_HPP