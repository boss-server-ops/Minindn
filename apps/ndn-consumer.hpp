#ifndef NDN_CONSUMER_HPP
#define NDN_CONSUMER_HPP

#include <ndn-cxx/face.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/encoding/block.hpp>
#include <ndn-cxx/util/random.hpp>
#include <ndn-cxx/util/rtt-estimator.hpp>
#include <ndn-cxx/util/time.hpp>
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

    void SendPacket();
    void InterestGenerator();

    /**
     * @brief Method to aggregate data
     * @param data The received ModelData
     * @param seq The sequence number
     */
    void aggregate(const ModelData &data, const uint32_t &seq);

    /**
     * @brief Method to get the mean of the parameters
     * @param seq The sequence number
     * @return A vector of mean values
     */
    std::vector<float> getMean(const uint32_t &seq);

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
     * @brief Method to measure the RTT threshold for congestion control
     * @param responseTime The response time
     * @param roundIndex The round index
     */
    void RTTThresholdMeasure(int64_t responseTime, int roundIndex);

    /**
     * @brief Method to measure RTT for each round based on response time
     * @param resTime The response time
     * @param roundIndex The round index
     * @return The measured RTT
     */
    std::chrono::milliseconds RTOMeasurement(int64_t resTime, int roundIndex);

    /**
     * @brief Method to record RTO results in files for testing purposes
     */
    void RTORecorder();

    /**
     * @brief Method to record RTT of each packet and other relevant info
     * @param responseTime The response time
     * @param seq The sequence number
     * @param ECN The ECN flag
     * @param threshold_measure The measured threshold
     * @param threshold_actual The actual threshold
     */
    void ResponseTimeRecorder(std::chrono::milliseconds responseTime, uint32_t seq, bool ECN, int64_t threshold_measure, int64_t threshold_actual);

    /**
     * @brief Method to record aggregate times
     * @param aggregateTime The aggregate time
     */
    void AggregateTimeRecorder(std::chrono::milliseconds aggregateTime);

    /**
     * @brief Method to record throughput
     * @param interestThroughput The interest throughput
     * @param dataThroughput The data throughput
     */
    void ThroughputRecorder(int interestThroughput, int dataThroughput);

    /**
     * @brief Method to initialize the log file
     */
    void InitializeLogFile();

    /**
     * @brief Method to check if the window can be decreased
     * @param threshold The threshold value
     * @return True if the window can be decreased, false otherwise
     */
    bool CanDecreaseWindow(int64_t threshold);

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

    virtual void ScheduleNextPacket() = 0;

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
    std::string filename = "src/ndnSIM/examples/topologies/DataCenterTopology.txt";

    // Testing log file
    // ToDo: Update logging for multiple rounds
    std::string RTO_recorder = folderPath + "/consumer_RTO.txt";                       //'Time', 'RTO'
    std::string responseTime_recorder = folderPath + "/consumer_RTT.txt";              //'Time', 'seq', ‘ECN’， ‘RTT threshold’，'RTT', 'isWindowDecreaseSuppressed'
    std::string aggregateTime_recorder = folderPath + "/consumer_aggregationTime.txt"; //'Time', 'aggTime'
    // std::string throughput_recorder = folderPath + "/throughput.txt";
    int suspiciousPacketCount; // When timeout is triggered, add one

    // Update when WindowDecrease() is called every time, used for CWA algorithm
    std::chrono::milliseconds lastWindowDecreaseTime;
    bool isWindowDecreaseSuppressed;

    // Local throughput measurement
    int totalInterestThroughput;
    int totalDataThroughput;
    int numChild; // The number of child nodes of consumer, used to specify the actual number of links for bandwidth utilization

    // Congestion/rate control
    std::vector<std::vector<std::string>> globalTreeRound;
    std::map<int, std::vector<int64_t>> RTT_threshold_vec; // Each mapping represents one round (if there're more than one round)
    std::map<int, int64_t> RTT_threshold;                  // Actual threshold used to detect congestion
    std::map<int, int64_t> RTT_measurement;                // The estimated RTT value using EWMA
    std::map<int, int> RTT_count;                          // How many RTT packets this node has received, used to estimate how many iterations have passed

    // Global sequence number
    uint32_t globalSeq;
    int globalRound;

    // Interest queue
    std::queue<std::tuple<uint32_t, bool, std::shared_ptr<ndn::Name>>> interestQueue; // tuple: iteration, round, name

    // Get producer list
    std::string proList;

    // Constructed aggregation Tree
    std::vector<std::map<std::string, std::vector<std::string>>> aggregationTree; // Entire tree (including main tree and sub-trees)
                                                                                  // Broadcast aggregation tree
    bool broadcastSync;
    std::vector<std::string> broadcastList; // Elements within this vector need to be broadcasted

    // Aggregation synchronization
    std::map<uint32_t, std::vector<std::vector<std::string>>> map_agg_oldSeq_newName; // Manage names for round
    std::map<uint32_t, std::vector<std::string>> m_agg_newDataName;                   // Manage names for entire iteration

    // Used inside InterestGenerator function
    std::map<int, std::vector<std::string>> map_round_nameSec1_3;
    std::vector<std::string> vec_iteration;
    std::vector<std::vector<std::string>> vec_round;

    // Timeout check/ RTO measurement
    std::map<std::string, std::chrono::milliseconds> m_timeoutCheck;
    std::map<int, std::chrono::milliseconds> m_timeoutThreshold;
    std::map<int, std::chrono::milliseconds> RTO_Timer;
    std::map<int, int64_t> SRTT;
    std::map<int, int64_t> RTTVAR;
    std::map<int, bool> initRTO;
    std::map<int, int> numTimeout; // Count the number of timeout, for testing purpose

    // Designed for actual aggregation operations
    std::map<uint32_t, std::vector<float>> sumParameters;
    std::map<uint32_t, std::vector<float>> aggregationResult;
    int producerCount;

    // Congestion signal
    bool ECNLocal;
    bool ECNRemote;

    // defined for response time
    std::map<std::string, std::chrono::milliseconds> startTime;
    std::map<std::string, std::chrono::milliseconds> responseTime;
    int64_t total_response_time;
    int round;

    // defined for aggregation time
    std::map<uint32_t, std::chrono::milliseconds> aggregateStartTime;
    std::map<uint32_t, std::chrono::milliseconds> aggregateTime;
    int64_t totalAggregateTime;
    int iterationCount;

    // New defined attribute variables
    std::string m_interestName; // Consumer's interest prefix
    std::string m_nodeprefix;   // Consumer's node prefix
    uint32_t m_iteNum;          // The number of iterations
    int m_interestQueue;        // Queue size
    int m_constraint;           // Constraint of each sub-tree
    double m_EWMAFactor;        // Factor used in EWMA, recommended value is between 0.1 and 0.3
    double m_thresholdFactor;   // Factor to compute "RTT_threshold", i.e. "RTT_threshold = Threshold_factor * RTT_measurement"

    // use m_rand and m_uniformDist to replace the m_rand in ndnSIM
    std::mt19937 m_rand;                            // random number generator
    std::uniform_real_distribution<> m_uniformDist; // average distribution
    uint32_t m_seq;                                 ///< @brief currently requested sequence number
    uint32_t m_seqMax;                              ///< @brief maximum number of sequence number
    std::function<void()> m_sendEvent;              ///< @brief EventId of pending "send packet" event
    std::chrono::milliseconds m_retxTimer;          ///< @brief Currently estimated retransmission timer
    std::function<void()> m_retxEvent;              ///< @brief Event to check whether or not retransmission should be performed

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
};

#endif // NDN_CONSUMER_HPP