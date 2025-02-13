#include "ndn-aggregator.hpp"

Aggregator::Aggregator()
    : m_rand(std::random_device{}()), // 使用随机设备初始化随机数生成器
      m_uniformDist(0, std::numeric_limits<uint32_t>::max()),
      m_scheduler(m_face.getIoContext()),
      m_seq(0),
      m_interestLifeTime(4000),
      m_freshness(0),
      m_setInitialWindowOnTimeout(true),
      m_reactToCongestionMarks(true)
{
    // 初始化其他成员变量
    // 初始化 spdlog
    SetWindow(1);
    SetSeqMax(std::numeric_limits<uint32_t>::max());
    SetRetxTimer(std::chrono::milliseconds(2));
    m_ccAlgorithm = CcAlgorithm::AIMD;
    m_logger = spdlog::basic_logger_mt("aggregator_logger", "logs/aggregator.log");

    spdlog::set_default_logger(m_logger);
    spdlog::set_level(spdlog::level::info); // 设置日志级别
    spdlog::flush_on(spdlog::level::info);  // 每条 info 级别及以上的日志后刷新

    spdlog::info("Aggregator initialized");

    // Read configuration from config.ini
    ReadConfig();
}

void Aggregator::ReadConfig()
{
    boost::property_tree::ptree pt;
    boost::property_tree::ini_parser::read_ini("../experiments/config.ini", pt);

    // General section
    m_minWindow = pt.get<int>("General.Window", 1);
    m_initPace = pt.get<int>("General.InitPace", 2);
    std::string ccAlgorithm = pt.get<std::string>("General.CcAlgorithm", "AIMD");
    m_ccAlgorithm = (ccAlgorithm == "AIMD") ? CcAlgorithm::AIMD : CcAlgorithm::CUBIC;
    m_alpha = pt.get<double>("General.Alpha", 0.5);
    m_beta = pt.get<double>("General.Beta", 0.7);
    m_gamma = pt.get<double>("General.Gamma", 0.7);
    m_EWMAFactor = pt.get<double>("General.EWMAFactor", 0.3);
    m_thresholdFactor = pt.get<double>("General.ThresholdFactor", 1.0);
    m_useWIS = pt.get<bool>("General.UseWIS", true);
    m_useCwa = pt.get<bool>("General.UseCwa", true);
    m_useCubicFastConv = pt.get<bool>("General.UseCubicFastConv", false);
    m_smooth_window_size = pt.get<int>("General.RTTWindowSize", 3);
    m_dataSize = pt.get<int>("General.DataSize", 150);

    // QSF section
    m_qsfQueueThreshold = pt.get<int>("QSF.QueueThreshold", 3);
    m_qsfMDFactor = pt.get<double>("QSF.MDFactor", 0.9);
    m_qsfRPFactor = pt.get<double>("QSF.RPFactor", 1.05);
    m_qsfTimeDuration = pt.get<int>("QSF.SlidingWindow", 20);
    m_qsfInitRate = pt.get<double>("QSF.InitRate", 0.0005);

    // Aggregator section
    m_interestQueue = pt.get<int>("Aggregator.AggInterestQueue", 10);
    m_dataQueue = pt.get<int>("Aggregator.AggDataQueue", 20);
}

void Aggregator::setPrefix(const ndn::Name &prefix)
{
    m_prefix = prefix;
}
/**
 * Intermediate function to parse string, used in aggTreeProcessStrings()
 * Changed from set into list
 * @param input
 * @return Parsed string
 */
std::pair<std::string, std::vector<std::string>> Aggregator::aggTreeProcessSingleString(const std::string &input)
{
    std::istringstream iss(input);
    std::string segment;
    std::vector<std::string> segments;

    // Use getline to split the string by '.'
    while (getline(iss, segment, '.'))
    {
        segments.push_back(segment);
    }

    // Check if there are enough segments to form a key and a vector
    if (segments.size() > 1)
    {
        std::string key = segments[0];
        std::vector<std::string> values(segments.begin() + 1, segments.end());
        return {key, values};
    }

    return {}; // Return an empty pair if not enough segments
}

/**
 * When receive "initialization" message (tree construction) from consumer, parse the message to get required info
 * Changed from set into list
 * @param inputs
 * @return A map consist child node info for current aggregator
 */
std::map<std::string, std::vector<std::string>>
Aggregator::aggTreeProcessStrings(const std::vector<std::string> &inputs)
{
    std::map<std::string, std::vector<std::string>> result;

    for (const std::string &input : inputs)
    {
        auto entry = aggTreeProcessSingleString(input);
        if (!entry.first.empty())
        {
            // Append all elements from entry.second to the vector at result[entry.first]
            result[entry.first].insert(
                result[entry.first].end(),
                entry.second.begin(),
                entry.second.end());
        }
    }

    return result;
}

/**
 * Get data queue of certain flow
 */
double
Aggregator::GetDataQueueSize(std::string prefix)
{
    double queueSize = 0.0;
    for (const auto &[seq, aggList] : map_agg_oldSeq_newName)
    {
        // TODO: delete later
        /*         NS_LOG_DEBUG("Seq: " << seq);
                // Iterate through the vector of strings (value)
                NS_LOG_DEBUG("aggList: ");
                for (const auto& str : aggList) {
                    NS_LOG_DEBUG(str); // Print each string in the vector
                } */

        if (std::find(aggList.begin(), aggList.end(), prefix) == aggList.end())
        {
            queueSize += 1.0;
        }
    }

    spdlog::debug("Flow: {} -> Data queue size: {}", prefix, queueSize);
    return queueSize;
}

/**
 * Sum response time
 * @param response_time
 */
void Aggregator::ResponseTimeSum(int64_t response_time)
{
    totalResponseTime += response_time;
    ++round;
}

// /**
//  * Compute average for response time
//  * @return Average response time
//  */
// int64_t
// Aggregator::GetResponseTimeAverage()
// {
//     if (round == 0)
//     {

//         spdlog::debug("Error happened when calculating average response time!");
//         return 0;
//     }

//     return totalResponseTime / round;
// }

/**
 * Sum aggregation time from each iteration
 * @param aggregate_time
 */
void Aggregator::AggregateTimeSum(int64_t aggregate_time)
{
    totalAggregateTime += aggregate_time;
    ++iterationCount;
}

/**
 * Get average aggregation time
 * @return Average aggregation time
 */
int64_t
Aggregator::GetAggregateTimeAverage()
{
    if (iterationCount == 0)
    {
        spdlog::debug("Error happened when calculating aggregate time!");
        return 0;
    }

    return totalAggregateTime / iterationCount;
}

/**
//  * Check timeout every certain interval
//  */
void Aggregator::CheckRetxTimeout()
{
    std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime);

    for (auto it = m_timeoutCheck.begin(); it != m_timeoutCheck.end();)
    {
        std::string name = it->first;
        std::string flow = std::make_shared<ndn::Name>(name)->get(0).toUri();
        if (now - it->second > RTO_threshold[flow])
        {
            it = m_timeoutCheck.erase(it);
            ndn::Interest interest(name);
            m_pendingInterest[name].cancel();
            // OnTimeout(interest);
            m_timeoutEvent = m_scheduler.schedule(ndn::time::milliseconds(0), [this, name]
                                                  { this->OnTimeout(ndn::Interest(name)); });
        }
        // else
        // {
        //     ++it;
        // }
        it++;
    }
    m_retxEvent = m_scheduler.schedule(ndn::time::milliseconds(m_retxTimer.count()), [this]
                                       { this->CheckRetxTimeout(); });
}

/**
 * Based on RTT of the first iteration, compute their RTT average as threshold, use the threshold for congestion control
 * Apply Exponentially Weighted Moving Average (EWMA) for RTT Threshold Computation
 * @param prefix
 * @param responseTime
 * @return congestion signal
 */
bool Aggregator::CongestionDetection(std::string prefix, int64_t responseTime)
{
    //* Normal usage is "push_back" "pop_front"
    // Update RTT windowed queue and historical estimation
    RTT_windowed_queue[prefix].push_back(responseTime);
    RTT_count[prefix]++;

    if (RTT_windowed_queue[prefix].size() > m_smooth_window_size)
    {
        int64_t transitionValue = RTT_windowed_queue[prefix].front();
        RTT_windowed_queue[prefix].pop_front();

        if (RTT_historical_estimation[prefix] == 0)
        {
            RTT_historical_estimation[prefix] = transitionValue;
        }
        else
        {
            RTT_historical_estimation[prefix] = m_EWMAFactor * transitionValue + (1 - m_EWMAFactor) * RTT_historical_estimation[prefix];
        }
    }
    else
    {
        spdlog::debug("RTT_windowed_queue size: {}", RTT_windowed_queue[prefix].size());
    }

    // Detect congestion
    if (RTT_count[prefix] >= 2 * m_smooth_window_size)
    {
        int64_t pastRTTAverage = 0;
        for (int64_t pastRTT : RTT_windowed_queue[prefix])
        {
            pastRTTAverage += pastRTT;
        }
        pastRTTAverage /= m_smooth_window_size;

        // Enable RTT-estimation for scheduler
        isRTTEstimated = true;

        int64_t rtt_threshold = m_thresholdFactor * RTT_historical_estimation[prefix];
        if (rtt_threshold < pastRTTAverage)
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        spdlog::debug("RTT_count: {}", RTT_count[prefix]);
        return false;
    }
}

/**
 * Measure new RTO
 * @param prefix
 * @param resTime unit - us
 * @return New RTO
 */
void Aggregator::RTOMeasure(std::string prefix, int64_t resTime)
{
    if (roundRTT[prefix] == 0)
    {
        RTTVAR[prefix] = resTime / 2;
        SRTT[prefix] = resTime;
    }
    else
    {
        RTTVAR[prefix] = 0.75 * RTTVAR[prefix] + 0.25 * std::abs(SRTT[prefix] - resTime); // RTTVAR = (1 - b) * RTTVAR + b * |SRTT - RTTsample|, where b = 0.25
        SRTT[prefix] = 0.875 * SRTT[prefix] + 0.125 * resTime;                            // SRTT = (1 - a) * SRTT + a * RTTsample, where a = 0.125
    }
    roundRTT[prefix]++;
    int64_t RTO = SRTT[prefix] + 4 * RTTVAR[prefix]; // RTO = SRTT + K * RTTVAR, where K = 4

    RTO_threshold[prefix] = std::chrono::milliseconds(4 * RTO);

    // NS_LOG_DEBUG("RTO measurement: " << RTO_threshold[name_sec0].GetMilliSeconds() << " ms");
}

/**
 * Triggered when timeout
 * @param nameString
 */
void Aggregator::OnTimeout(const ndn::Interest &interest)
{
    if (m_timeoutCheck.find(interest.getName().toUri()) != m_timeoutCheck.end())
    {
        m_timeoutCheck.erase(interest.getName().toUri());
    }
    else
    {
        spdlog::info("already recieved packet");
        return;
    }
    std::shared_ptr<ndn::Name> name = std::make_shared<ndn::Name>(interest.getName());
    std::string name_sec0 = name->get(0).toUri();
    uint32_t seq = name->get(-1).toSequenceNumber();
    spdlog::debug("Flow {} - name -> {}: timeout.", name_sec0, interest.getName().toUri());

    if (m_inFlight[name_sec0] > 0)
    {
        m_inFlight[name_sec0]--;
    }
    else
    {
        spdlog::error("Error when timeout, please exit and check!");
        std::exit(EXIT_FAILURE);
        return;
    }

    // TODO: Should we implement qsf rate decrease when timeout?

    // qsf timeout handling
    if (interestQueue.find(name_sec0) == interestQueue.end())
    {
        spdlog::error("Error when timeout, please exit and check!");
        std::exit(EXIT_FAILURE);
        return;
    }
    interestQueue[name_sec0].push_front(seq);

    suspiciousPacketCount++;
}

/**
 * Set initial timeout checking interval
 * @param retxTimer
 */
void Aggregator::SetRetxTimer(std::chrono::milliseconds retxTimer)
{
    m_retxTimer = retxTimer;
    if (m_retxEvent)
    {

        m_retxEvent.cancel();
        m_retxEvent.reset();
    }
    m_retxEvent = m_scheduler.schedule(ndn::time::milliseconds(m_retxTimer.count()), [this]
                                       { this->CheckRetxTimeout(); });
}

/**
 * Get timeout checking interval
 * @return Timeout checking interval
 */
std::chrono::milliseconds Aggregator::GetRetxTimer() const
{
    return m_retxTimer;
}

/**
 * Override, start this class
 */
void Aggregator::StartApplication()
{
    // NS_LOG_FUNCTION_NOARGS();
    App::StartApplication();
    startTime = std::chrono::steady_clock::now();
    std::string prefix = m_prefix.toUri();
    m_face.setInterestFilter(prefix,
                             std::bind(&Aggregator::OnInterest, this, std::placeholders::_1, std::placeholders::_2),
                             std::bind(&App::OnRegisterSuccess, this, std::placeholders::_1),
                             std::bind(&App::OnRegisterFailure, this, std::placeholders::_1, std::placeholders::_2));

    m_face.processEvents();
    spdlog::info("Aggregator finished processing events");
}

/**
 * Override, stop this class
 */
void Aggregator::StopApplication()
{
    // Cancel packet generation - can be a way to stop simulation gracefully?
    // Simulator::Cancel(m_sendEvent);
    App::StopApplication();
}

/**
 * Perform aggregation for incoming data packets (sum)
 * @param data
 * @param dataName
 */
void Aggregator::Aggregate(const ModelData &data, const uint32_t &seq)
{
    // first initialization
    if (sumParameters.find(seq) == sumParameters.end())
    {
        sumParameters[seq] = std::vector<double>(m_dataSize, 0.0f);
    }

    // Aggregate data
    std::transform(sumParameters[seq].begin(), sumParameters[seq].end(), data.parameters.begin(), sumParameters[seq].begin(), std::plus<double>());

    // Aggregate congestion signal
    congestionSignalList[seq].insert(congestionSignalList[seq].end(), data.congestedNodes.begin(), data.congestedNodes.end());
}

/**
 * Don't get mean for now, just reformating the data packets, perform aggregation at consumer
 * @param dataName
 * @return Data content
 */
ModelData
Aggregator::GetMean(const uint32_t &seq)
{
    ModelData result;
    if (sumParameters.find(seq) != sumParameters.end())
    {
        result.parameters = sumParameters[seq];

        // Encapsulate qsf as meta data
        double maxQsf = 0;
        for (const auto &[key, value] : aggregationMap)
        {
            spdlog::debug("Flow - {} . Interest queue size: {}", key, interestQueue[key].size());
            maxQsf = std::max(maxQsf, static_cast<double>(interestQueue[key].size()));
        }

        spdlog::info("Max interest queue size: {}", maxQsf);

        maxQsf = std::max(maxQsf, static_cast<double>(sumParameters.size()));

        spdlog::info("Final QSF: {}", maxQsf);
        result.qsf = maxQsf;

        // Add congestionSignal of current node if necessary, currently disable!
        /*         if (congestionSignal[seq]) {
                    congestionSignalList[seq].push_back(m_prefix.toUri());
                    NS_LOG_DEBUG("Congestion detected on current node!");
                }
                result.congestedNodes = congestionSignalList[seq]; */
    }
    else
    {
        spdlog::debug("Error when get aggregation result, please exit and check!");
        std::exit(EXIT_FAILURE);
    }

    return result;
}

/**
 * Invoked when Nack
 * @param nack
 */
void Aggregator::OnNack(const ndn::Interest &interest, const ndn::lp::Nack &nack)
{
    App::OnNack(interest, nack);
    spdlog::info("NACK received for: {}, reason: {}", nack.getInterest().getName().toUri(), static_cast<int>(nack.getReason()));
    std::string dataName = nack.getInterest().getName().toUri();
    std::string name_sec0 = nack.getInterest().getName().get(0).toUri();
    uint32_t seq = nack.getInterest().getName().get(-1).toSequenceNumber();

    if (m_inFlight[name_sec0] > 0)
    {
        m_inFlight[name_sec0]--;
    }
    else
    {
        spdlog::error("InFlight number error, please exit and check!");
        std::exit(EXIT_FAILURE);
        return;
    }

    //! To be updated, when nack is received, what's the best strategy to control sending rate?
    // Insert the rejected interest back to the front of the interest queue
    interestQueue[name_sec0].push_front(seq);

    // Decrease sending rate for certain flow
    // WindowDecrease(name_sec0, "nack");

    // Stop tracing rtt and timeout
    rttStartTime.erase(dataName);
    m_timeoutCheck.erase(dataName);
    nackCount++;
}

/**
 * Set cwnd
 * @param window
 */
void Aggregator::SetWindow(uint32_t window)
{
    m_initialWindow = window;
}

// /**
//  * Get cwnd
//  * @return cwnd
//  */
// uint32_t
// Aggregator::GetWindow() const
// {
//     return m_initialWindow;
// }

/**
 * Set max sequence number, not used now
 * @param seqMax
 */
void Aggregator::SetSeqMax(uint32_t seqMax)
{
    // Be careful, ignore maxSize here
    m_seqMax = seqMax;
}

/**
 * Get max sequence number, not used now
 * @return
 */
uint32_t
Aggregator::GetSeqMax() const
{
    return m_seqMax;
}

// /**
//  * Increase cwnd
//  * @param prefix Flow name
//  */
void Aggregator::WindowIncrease(std::string prefix)
{
    if (m_ccAlgorithm == CcAlgorithm::AIMD)
    {
        // If cwnd is larger than 8, check whether current bottleneck is because of downstream slow interest, if so, stop increasing cwnd
        /*         if (m_window[prefix] > 8.0 && m_window[prefix] - m_inFlight[prefix] > 30.0){
                    NS_LOG_DEBUG("Current bottleneck is downstream slow interest, stop increasing cwnd.");
                } else  */
        if (m_useWIS)
        {
            if (m_window[prefix] < m_ssthresh[prefix])
            {
                m_window[prefix] += 1.0;
            }
            else
            {
                m_window[prefix] += (1.0 / m_window[prefix]);
            }
            spdlog::debug("Window size of flow '{}' is increased to {}", prefix, m_window[prefix]);
        }
        else
        {
            m_window[prefix] += 1.0;
            spdlog::debug("Window size of flow '{}' is increased to {}", prefix, m_window[prefix]);
        }
    }
    else if (m_ccAlgorithm == CcAlgorithm::CUBIC)
    {
        CubicIncerase(prefix);
    }
    else
    {
        spdlog::error("CC alogrithm can't be recognized, please check!");
        std::exit(EXIT_FAILURE);
    }
}

/**
 * Decrease cwnd
 * @param prefix Flow name
 * @param type Congestion type
 */
void Aggregator::WindowDecrease(std::string prefix, std::string type)
{
    // Track last window decrease time
    std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime);
    lastWindowDecreaseTime[prefix] = now;

    // AIMD for timeout
    if (m_ccAlgorithm == CcAlgorithm::AIMD)
    {
        if (type == "timeout")
        {
            m_ssthresh[prefix] = m_window[prefix] * m_alpha;
            m_window[prefix] = m_ssthresh[prefix];
        }
        else if (type == "nack")
        {
            m_ssthresh[prefix] = m_window[prefix] * m_alpha;
            m_window[prefix] = m_ssthresh[prefix];
        }
        else if (type == "LocalCongestion")
        {
            m_ssthresh[prefix] = m_window[prefix] * m_beta;
            m_window[prefix] = m_ssthresh[prefix];
        }
        else if (type == "RemoteCongestion")
        {
            m_ssthresh[prefix] = m_window[prefix] * m_gamma;
            m_window[prefix] = m_ssthresh[prefix];
        }
    }
    else if (m_ccAlgorithm == CcAlgorithm::CUBIC)
    {
        if (type == "timeout")
        {
            m_ssthresh[prefix] = m_window[prefix] * m_alpha;
            m_window[prefix] = m_ssthresh[prefix];
        }
        else if (type == "nack")
        {
            m_ssthresh[prefix] = m_window[prefix] * m_alpha;
            m_window[prefix] = m_ssthresh[prefix];
        }
        else if (type == "LocalCongestion")
        {
            CubicDecrease(prefix, type);
        }
        else if (type == "RemoteCongestion")
        {
            // Do nothing, currently disabled
        }
    }
    else
    {
        spdlog::error("CC alogrithm can't be recognized, please check!");
        std::exit(EXIT_FAILURE);
    }

    // Window size can't be reduced below 1
    if (m_window[prefix] < m_minWindow)
    {
        m_window[prefix] = m_minWindow;
    }
    spdlog::debug("Window size of flow '{}' is decreased to {}. Reason: {}", prefix, m_window[prefix], type);
}

/**
 * Cubic increase
 * @param prefix Flow name
 */
void Aggregator::CubicIncerase(std::string prefix)
{
    // 1. Time since last congestion event in Seconds, round the value to 3 decimal places
    std::chrono::microseconds now = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - startTime);
    const double t = std::round(1000 * (now.count() - std::chrono::duration_cast<std::chrono::microseconds>(lastWindowDecreaseTime[prefix]).count()) / 1e9) / 1000;
    spdlog::debug("Time since last congestion event: {}", t);

    // 2. Time it takes to increase the window to cubic_wmax
    // K = cubic_root(W_max*(1-beta_cubic)/C) (Eq. 2)
    const double k = std::cbrt(m_cubicWmax[prefix] * (1 - m_cubicBeta) / m_cubic_c);
    spdlog::debug("K value: {}", k);

    // 3. Target: W_cubic(t) = C*(t-K)^3 + W_max (Eq. 1)
    const double w_cubic = m_cubic_c * std::pow(t - k, 3) + m_cubicWmax[prefix];
    spdlog::debug("Cubic increase target: {}", w_cubic);

    // 4. Estimate of Reno Increase (Currently Disabled)
    //  const double rtt = m_rtt->GetCurrentEstimate().GetSeconds();
    //  const double w_est = m_cubic_wmax*m_beta + (3*(1-m_beta)/(1+m_beta)) * (t/rtt);

    //* TCP-friendly region, need to be disabled for ICN, "w_est" is not needed
    // constexpr double w_est = 0.0;

    //! Original cubic increase
    /*     if (m_cubicWmax[prefix] <= 0) {
            NS_LOG_DEBUG("Error! Wmax is less than 0, check cubic increase!");
            Simulator::Stop();
        }

        double cubic_increment = std::max(w_cubic, 0.0) - m_window[prefix];
        // Cubic increment must be positive:
        // Note: This change is not part of the RFC, but I added it to improve performance.
        if (cubic_increment < 0) {
            cubic_increment = 0.0;
        }

        NS_LOG_DEBUG("Cubic increment: " << cubic_increment);
        m_window[prefix] += cubic_increment / m_window[prefix]; */

    //! Customized cubic increase
    if (m_window[prefix] < m_ssthresh[prefix])
    {
        m_window[prefix] += 1.0;
    }
    else
    {
        if (m_cubicWmax[prefix] <= 0)
        {
            spdlog::error("Error! Wmax is less than 0, check cubic increase!");
            std::exit(EXIT_FAILURE);
        }

        double cubic_increment = std::max(w_cubic, 0.0) - m_window[prefix];
        // Cubic increment must be positive:
        // Note: This change is not part of the RFC, but I added it to improve performance.
        if (cubic_increment < 0)
        {
            cubic_increment = 0.0;
        }

        spdlog::debug("Cubic increment: {}", cubic_increment);
        m_window[prefix] += cubic_increment / m_window[prefix];
    }

    spdlog::debug("Window size of flow '{}' is increased to {}", prefix, m_window[prefix]);
}

/**
 * Cubic decrease
 * @param prefix Flow name
 * @param type Congestion type
 */
void Aggregator::CubicDecrease(std::string prefix, std::string type)
{
    //! Traditional cubic window decrease
    m_cubicWmax[prefix] = m_window[prefix];
    m_ssthresh[prefix] = m_window[prefix] * m_cubicBeta;
    m_ssthresh[prefix] = std::max<double>(m_ssthresh[prefix], m_minWindow);
    m_window[prefix] = m_window[prefix] * m_cubicBeta;
}

/**
 * Process incoming interest packets
 * @param interest
 */
void Aggregator::OnInterest(const ndn::InterestFilter &filter, const ndn::Interest &interest)
{

    spdlog::debug("The incoming interest packet size is: {}", interest.wireEncode().size());
    App::OnInterest(filter, interest);

    std::string interestType = interest.getName().get(-2).toUri();

    if (interestType == "data")
    {
        spdlog::debug("aggregator received the interest packet that is of type data");
        std::string originalName = interest.getName().toUri();
        uint32_t seq = interest.getName().get(-1).toSequenceNumber();
        bool isQueueFull = false;
        bool isDownstreamRetx = false;

        //? Check whether interest queue is full
        for (const auto &[key, value] : aggregationMap)
        {
            if (interestQueue[key].size() >= m_interestQueue)
            {
                isQueueFull = true;
                spdlog::info("Interest queue of flow {} is full, drop it - {}", key, interest.getName().toUri());
                interestOverflow++;

                // Interest queue overflow, send NACK back to downstream for notification
                SendNack(std::make_shared<ndn::Interest>(interest));
                return;
            }
        }

        //? Check whether the interest is retransmission from downstream
        if (m_agg_newDataName.find(seq) != m_agg_newDataName.end() || map_agg_oldSeq_newName.find(seq) != map_agg_oldSeq_newName.end())
        {
            isDownstreamRetx = true;
            spdlog::info("This is a retransmission interest from downstream, drop it - {}", interest.getName().toUri());
            downstreamRetxCount++;
            return;
        }

        // If queue isn't full, perform interest splitting
        if (!isQueueFull && !isDownstreamRetx)
        {
            // Store original name into aggMap
            m_agg_newDataName[seq] = interest.getName().toUri();

            spdlog::debug("New downstream interest's seq: {}", seq);

            // Split interest
            InterestSplitting(seq);

            //! Debugging, check whether this works in qsf design
            if (firstInterest)
            {
                for (const auto &[key, value] : aggregationMap)
                {

                    m_scheduleEvent[key] = m_scheduler.schedule(ndn::time::milliseconds(0), [this, key]
                                                                { this->ScheduleNextPacket(key); });
                }
                firstInterest = false;
            }
        }
        else
        {
            spdlog::debug("Error! Interest queue is full or downstream retransmission, please check!");
            std::exit(EXIT_FAILURE);
            return;
        }
    }
    //? Initialization should work with qsf, no need to be modified
    else if (interestType == "initialization")
    {
        spdlog::info("Initialization interest received: {}", interest.getName().toUri());
        // Synchronize signal
        treeSync = true;

        std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime);

        // Record current time as simulation start time on aggregator
        startSimulation = now;

        // Read aggregation tree from init message
        std::vector<std::string> inputs;
        if (interest.getName().size() > 3)
        {
            for (size_t i = 1; i < interest.getName().size() - 2; ++i)
            {
                spdlog::info("Aggregation tree received: {}", interest.getName().get(i).toUri());
                inputs.push_back(interest.getName().get(i).toUri());
            }
        }
        aggregationMap = aggTreeProcessStrings(inputs);

        // for (const auto &[key, value] : aggregationMap)
        // {
        //     spdlog::info("Key: {}", key);
        //     spdlog::info("Values: ");
        //     for (const auto &str : value)
        //     {
        //         spdlog::info("  {}", str);
        //     }
        // }

        // Define for new congestion control
        numChild = static_cast<int>(aggregationMap.size());

        // testing, delete later!!!!
        if (aggregationMap.empty())
        {
            spdlog::info("Error, aggregationMap is empty!");
            std::exit(EXIT_FAILURE);
            return;
        } /* else {
            // Print the result mapping
            NS_LOG_DEBUG("Aggregation tree received: ");
            for (const auto& [node, leaves] : aggregationMap) {
                NS_LOG_DEBUG(node << " contains leaf nodes: ");
                for (const auto& leaf : leaves) {
                    NS_LOG_DEBUG(leaf << " ");
                }
            }
        } */

        //! After receiving aggregation tree, start basic initialization
        // Initialize logging session
        InitializeLogFile();

        // Initialize parameteres
        InitializeParameters();

        // Perform interest name splitting
        InterestGenerator();

        //! nack works
        /*         auto nack = std::make_shared<ndn::lp::Nack>(*interest);
                nack->setReason(ndn::lp::NackReason::QUEUE_OVERFLOW);
                //nack->wireEncode();
                m_transmittedNacks(nack, this, m_face);
                m_appLink->onReceiveNack(*nack); */

        // Generate a new data packet to respond to tree broadcasting
        auto data = std::make_shared<ndn::Data>(interest.getName());

        // 设set data packet content
        data->setFreshnessPeriod(ndn::time::milliseconds(m_freshness.count()));
        // data->setFreshnessPeriod(ndn::time::seconds(10));
        // use KeyChain to sign Data packet
        m_keyChain.sign(*data);
        // send Data packet
        m_face.put(*data);
        spdlog::info("Initialization data packet sent: {}", data.get()->getName().toUri());
    }
}

/**
 * Process incoming data packets
 * @param prefix
 */
void Aggregator::ScheduleNextPacket(std::string prefix)
{
    if (interestQueue.find(prefix) == interestQueue.end())
    {
        spdlog::error("Flow {} is not found in the interest queue.", prefix);
        std::exit(EXIT_FAILURE);
        return;
    }

    if (!interestQueue.at(prefix).empty())
    {
        if (m_sendEvent[prefix])
        {

            m_sendEvent[prefix].cancel();
            m_sendEvent[prefix].reset();
            spdlog::debug("Suspicious, remove the previous event.");
        }

        m_sendEvent[prefix] = m_scheduler.schedule(ndn::time::milliseconds(0), [this, prefix]
                                                   { this->SendPacket(prefix); });
        double nextTime = 1 / m_rateLimit[prefix]; // Unit: us

        spdlog::debug("Flow {} -> Schedule next sending event after {} ms.", prefix, nextTime / 1000);
        m_scheduleEvent[prefix] = m_scheduler.schedule(ndn::time::microseconds(static_cast<int64_t>(nextTime)), [this, prefix]
                                                       { this->ScheduleNextPacket(prefix); });
    }
    else
    {
        //! What's the best strategy when interest queue is empty?
        // Schedule again after 1/5 rate limit
        double nextTime = 1 / m_rateLimit[prefix] / 5; // Unit: us

        spdlog::debug("Flow {} -> Interest queue is empty. Schedule next sending event after {} ms.", prefix, nextTime / 1000);
        m_scheduleEvent[prefix] = m_scheduler.schedule(ndn::time::microseconds(static_cast<int64_t>(nextTime)), [this, prefix]
                                                       { this->ScheduleNextPacket(prefix); });
    }
}

/**
 * Generate interests for all iterations and push them into queue for further interest sending scheduling
 * First invoke when start simulation, every time when one iteration finished aggregation, update interestQueue again
 * The number of interests of each round are checked, if any of them is not enough, generate interests into the queue
 */
void Aggregator::InterestGenerator()
{
    // Divide interests and push them into queue
    for (const auto &[key, values] : aggregationMap)
    {
        std::string name_sec1;
        std::string name_sec0_2;

        for (const auto &value : values)
        {
            name_sec1 += value + ".";
        }
        name_sec1.resize(name_sec1.size() - 1);
        name_sec0_2 = "/" + key + "/" + name_sec1 + "/data";
        NameSec0_2[key] = name_sec0_2;
        vec_iteration.push_back(key); // Will be added to aggregation map later
    }
}

/**
 * Divide interest into several new interests, performed whe receiving aggregation tree
 */
void Aggregator::InterestSplitting(uint32_t seq)
{
    // Divide interests and push them into queue
    for (const auto &[key, value] : aggregationMap)
    {
        interestQueue[key].push_back(seq);
    }
}

/**
 * Check whether interest buffer is empty, if not, send new interests
 */
void Aggregator::SendPacket(std::string prefix)
{
    if (!interestQueue.at(prefix).empty())
    {
        uint32_t iteration = interestQueue.at(prefix).front();
        interestQueue.at(prefix).pop_front();
        spdlog::debug("delete interest from queue: {}", iteration);
        std::shared_ptr<ndn::Name> name = std::make_shared<ndn::Name>(NameSec0_2[prefix]);
        name->appendSequenceNumber(iteration);

        SendInterest(name);

        // Check whether it's new iteration
        if (aggregateStartTime.find(iteration) == aggregateStartTime.end())
        {
            std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime);
            aggregateStartTime[iteration] = now;

            // TODO: delete later
            /*             std::vector<std::string> vec_flow; // Store all flow segments within aggMap
                        for (const auto& [key, value] : NameSec0_2) {
                            std::string nameNoSeq = value;
                            shared_ptr<Name> nameAgg = make_shared<Name>(nameNoSeq);
                            nameAgg->appendSequenceNumber(iteration);
                            std::string nameWithSeq = nameAgg->toUri();
                            vec_flow.push_back(nameWithSeq);
                        } */

            map_agg_oldSeq_newName[iteration] = vec_iteration;
        }

        // Stop interest scheduling after reaching the last iteration
        if (iteration == m_iteNum)
        {
            spdlog::info("All iterations have been finished, no need to schedule new interests.");
            if (m_scheduleEvent[prefix])
            {
                m_scheduleEvent[prefix].cancel();
                m_scheduleEvent[prefix].reset();
            }
        }
    }
    else
    {
        spdlog::debug("Flow - {}: interest queue is empty, this should never happen!", prefix);
        std::exit(EXIT_FAILURE);
        return;
    }
}

/**
 * Format the interest packet and send out interests
 * @param newName
 */
void Aggregator::SendInterest(std::shared_ptr<ndn::Name> newName)
{
    if (!m_active)
        return;

    std::string nameWithSeq = newName->toUri();
    std::string name_sec0 = newName->get(0).toUri();

    std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime);

    // Trace timeout
    m_timeoutCheck[nameWithSeq] = now;

    // Start response time
    rttStartTime[nameWithSeq] = now;
    uint32_t nonce = static_cast<uint32_t>(m_uniformDist(m_rand));
    spdlog::info("Sending new interest: {}", nameWithSeq);
    std::shared_ptr<ndn::Interest> newInterest = std::make_shared<ndn::Interest>();
    newInterest->setNonce(nonce);
    newInterest->setCanBePrefix(false);
    newInterest->setName(*newName);
    std::chrono::milliseconds interestLifeTime(m_interestLifeTime);
    newInterest->setInterestLifetime(ndn::time::milliseconds(interestLifeTime.count()));
    m_pendingInterest[nameWithSeq] = m_face.expressInterest(*newInterest,
                                                            std::bind(&Aggregator::OnData, this, _1, _2),
                                                            std::bind(&Aggregator::OnNack, this, _1, _2),
                                                            std::bind(&Aggregator::OnTimeout, this, _1));

    // Designed for congestion control recording
    m_inFlight[name_sec0]++;

    // Record interest throughput
    // Actual interests sending and retransmission are recorded as well
    int interestSize = newInterest->wireEncode().size();
    totalInterestThroughput += interestSize;
    spdlog::debug("Interest size: {}", interestSize);
}

/**
 * Send data packets
 * @param data
 */
void Aggregator::SendData(uint32_t seq)
{
    // Get aggregation result for current iteration
    std::vector<uint8_t> newbuffer;
    serializeModelData(GetMean(seq), newbuffer);

    // create data packet
    auto data = std::make_shared<ndn::Data>();

    std::string name_string = m_agg_newDataName[seq];
    spdlog::info("New aggregated data's name: {}", name_string);
    std::shared_ptr<ndn::Name> newName = std::make_shared<ndn::Name>(name_string);
    data->setName(*newName);
    data->setContent(std::make_shared<::ndn::Buffer>(newbuffer.begin(), newbuffer.end()));
    data->setFreshnessPeriod(ndn::time::milliseconds(m_freshness.count()));
    // use KeyChain to sign Data packet
    m_keyChain.sign(*data);
    // send Data packet
    m_face.put(*data);

    // Clear aggregation mapping and aggregation result for current iteration
    aggregateTime.erase(seq);
    map_agg_oldSeq_newName.erase(seq);
    m_agg_newDataName.erase(seq);
    sumParameters.erase(seq);
    partialAggResult.erase(seq);
}

/**
 * Send Nack back in the format of data packet
 * Currently used when interest queue is overflowed
 * @param interestName
 */
void Aggregator::SendNack(std::shared_ptr<const ndn::Interest> interest)
{
    auto nack = std::make_shared<ndn::lp::Nack>(*interest);
    nack->setReason(ndn::lp::NackReason::QUEUE_OVERFLOW);
    // nack->wireEncode();
    m_face.put(*nack);
    // TODO: check whether it needs the on receive nack
    //  m_transmittedNacks(nack, this, m_face);
    //  m_appLink->onReceiveNack(*nack);
}

/**
 * Process incoming data packets
 * @param data
 */
void Aggregator::OnData(const ndn::Interest &interest, const ndn::Data &data)
{

    if (!m_active)
        return;

    App::OnData(interest, data);
    spdlog::info("Received content object: {}", data.getName().toUri());
    int dataSize = data.wireEncode().size();

    std::string dataName = data.getName().toUri();
    std::string name_sec0 = data.getName().get(0).toUri();
    uint32_t seq = data.getName().at(-1).toSequenceNumber();
    std::string type = data.getName().get(-2).toUri();

    //! For testing purpose
    auto start = std::chrono::high_resolution_clock::now();
    auto stop = std::chrono::high_resolution_clock::now();

    // Record data throughput
    totalDataThroughput += dataSize;
    spdlog::debug("The incoming data packet size is: {}", dataSize);

    // Stop checking timeout associated with this name
    if (m_timeoutCheck.find(dataName) != m_timeoutCheck.end())
        m_timeoutCheck.erase(dataName);
    else
    {
        spdlog::debug("Suspicious data packet, not exists in timeout list.");
        std::exit(EXIT_FAILURE);
    }

    // TODO from yitong : testing, delete later
    GetDataQueueSize(name_sec0);

    // TODO from yitong : what's the best strategy to react to data queue overflow?
    // Check whether data queue exceeds the limit
    if (sumParameters.find(seq) == sumParameters.end())
    {
        // New iteration, currently not exist in the partial agg result
        if (partialAggResult.size() >= m_dataQueue)
        {
            // Exceed max data size
            spdlog::info("Exceeding the max data queue, stop interest sending for flow {}", name_sec0);
            dataOverflow++;

            // Schdule next event after 5 * current period
            if (m_scheduleEvent[name_sec0])
            {

                m_scheduleEvent[name_sec0].cancel();
                m_scheduleEvent[name_sec0].reset();
            }

            double nextTime = 5 * 1 / m_rateLimit[name_sec0]; // Unit: us
            spdlog::info("Flow {} -> Schedule next sending event after {} ms.", name_sec0, nextTime / 1000);

            m_scheduleEvent[name_sec0] = m_scheduler.schedule(ndn::time::microseconds(static_cast<int64_t>(nextTime)), [this, name_sec0]
                                                              { this->ScheduleNextPacket(name_sec0); });
        }
        partialAggResult[seq] = true;
    }

    if (m_inFlight[name_sec0] > 0)
    {
        m_inFlight[name_sec0]--;
    }
    else
    {
        spdlog::debug("Error! In-flight packet is less than 0, please check!");
        std::exit(EXIT_FAILURE);
        return;
    }

    //! For testing purpose
    stop = std::chrono::high_resolution_clock::now();
    spdlog::debug("1: {} us", std::chrono::duration_cast<std::chrono::microseconds>(stop - start).count());
    // Check data type
    if (type == "data")
    {
        // Perform data name matching with interest name
        ModelData upstreamModelData;

        auto data_agg = map_agg_oldSeq_newName.find(seq);
        auto data_map = m_agg_newDataName.find(seq);
        if (data_map != m_agg_newDataName.end() && data_agg != map_agg_oldSeq_newName.end())
        {
            // Aggregation starts
            auto &vec = data_agg->second;
            auto vecIt = std::find(vec.begin(), vec.end(), name_sec0);
            std::vector<uint8_t> oldbuffer(data.getContent().value(), data.getContent().value() + data.getContent().value_size());
            if (deserializeModelData(oldbuffer, upstreamModelData))
            {
                if (vecIt != vec.end())
                {
                    Aggregate(upstreamModelData, seq);
                    vec.erase(vecIt);

                    //! For testing purpose
                    stop = std::chrono::high_resolution_clock::now();
                    spdlog::debug("2: {} us", std::chrono::duration_cast<std::chrono::microseconds>(stop - start).count());
                }
                else
                {
                    spdlog::info("Data name doesn't exist in aggMap, meaning this data packet is duplicate from upstream!");
                    std::exit(EXIT_FAILURE);
                    return;
                }
            }
            else
            {
                spdlog::info("Error when deserializing data packet, please check!");
                std::exit(EXIT_FAILURE);
                return;
            }

            // RTT measurement
            if (rttStartTime.find(dataName) != rttStartTime.end())
            {

                std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime);
                responseTime[dataName] = now - rttStartTime[dataName];
                ResponseTimeSum(std::chrono::duration_cast<std::chrono::microseconds>(responseTime[dataName]).count());
                spdlog::info("ResponseTime for data packet : {}=> is: {} us", dataName, std::chrono::duration_cast<std::chrono::microseconds>(responseTime[dataName]).count());
            }

            // RTO/RTT measure
            RTOMeasure(name_sec0, std::chrono::duration_cast<std::chrono::microseconds>(responseTime[dataName]).count());
            RTTMeasure(name_sec0, std::chrono::duration_cast<std::chrono::microseconds>(responseTime[dataName]).count());

            //! Debugging, qsf design
            // Update estimated bandwidth
            BandwidthEstimation(name_sec0, upstreamModelData.qsf);

            // Init rate limit update
            if (firstData.at(name_sec0))
            {
                spdlog::info("Init rate limit update for flow {}", name_sec0);
                // question: is it too late to start?
                m_rateEvent[name_sec0] = m_scheduler.schedule(ndn::time::microseconds(0), [this, name_sec0]
                                                              { this->RateLimitUpdate(name_sec0); });
                firstData[name_sec0] = false;
            }

            //! For testing purpose
            stop = std::chrono::high_resolution_clock::now();
            spdlog::debug("3: {} us", std::chrono::duration_cast<std::chrono::microseconds>(stop - start).count());
            // Record qsf info
            QsfRecorder(name_sec0, upstreamModelData.qsf);
            QueueRecorder(name_sec0, GetDataQueueSize(name_sec0));

            AggTableRecorder(seq);

            // Record RTT
            ResponseTimeRecorder(responseTime[dataName], seq, name_sec0);

            // Record RTO
            RTORecorder(name_sec0);

            InFlightRecorder(name_sec0);

            // Check whether the aggregation of current iteration is done
            if (vec.empty())
            {
                spdlog::info("Aggregation of iteration {} finished.", seq);
                // Measure aggregation time
                if (aggregateStartTime.find(seq) != aggregateStartTime.end())
                {

                    std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime);
                    aggregateTime[seq] = now - aggregateStartTime[seq];
                    AggregateTimeSum(std::chrono::duration_cast<std::chrono::microseconds>(aggregateTime[seq]).count());
                    aggregateStartTime.erase(seq);

                    spdlog::info("Aggregator's aggregate time of sequence {} is: {} ms", seq, aggregateTime[seq].count());
                }
                else
                {
                    spdlog::debug("Error when calculating aggregation time, no reference found for seq {}", seq);
                }

                // Record aggregation time
                AggregateTimeRecorder(aggregateTime[seq], seq);

                // Send data
                spdlog::debug("Send data packet after 2 ms.");
                m_scheduler.schedule(ndn::time::milliseconds(2), [this, seq]
                                     { this->SendData(seq); });

                // All iterations finished, record the entire throughput
                if (iterationCount == m_iteNum)
                {

                    std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime);
                    stopSimulation = now;

                    // Record throughput and results
                    ThroughputRecorder(totalInterestThroughput, totalDataThroughput, startSimulation);
                    ResultRecorder(GetAggregateTimeAverage());
                }
            }
            else
            {
                spdlog::debug("Wait for others to aggregate.");
            }

            // Clear rtt mapping of this packet
            rttStartTime.erase(dataName);
            responseTime.erase(dataName);
        }
        else
        {
            spdlog::debug("Error, data name can't be recognized!");
            std::exit(EXIT_FAILURE);
            return;
        }
    }
}

// /**
//  * Record window when receiving a new packet
//  */
void Aggregator::WindowRecorder(std::string prefix)
{
    // Open file; on first call, truncate it to delete old content
    std::ofstream file(window_recorder[prefix], std::ios::app);

    if (file.is_open())
    {

        std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime);
        file << now.count() << " " << m_window[prefix] << " " << m_ssthresh[prefix] << " " << interestQueue[prefix].size() << std::endl; // Write text followed by a newline
        file.close();                                                                                                                    // Close the file after writing
    }
    else
    {
        spdlog::error("Unable to open file: {}", window_recorder[prefix]);
    }
}

/**
 * Record in-flight packets when receiving a new packet
 */
void Aggregator::InFlightRecorder(std::string prefix)
{
    // Open file; on first call, truncate it to delete old content
    std::ofstream file(inFlight_recorder[prefix], std::ios::app);

    if (file.is_open())
    {

        std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime);
        file << now.count() << " " << m_inFlight[prefix] << std::endl; // Write text followed by a newline
        file.close();                                                  // Close the file after writing
    }
    else
    {
        std::cerr << "Unable to open file: " << inFlight_recorder[prefix] << std::endl;
    }
}

/**
 * Record the response time for each returned packet, store them in a file
 * @param responseTime
 */
void Aggregator::ResponseTimeRecorder(std::chrono::milliseconds responseTime, uint32_t seq, std::string prefix)
{
    // Open the file using fstream in append mode
    std::ofstream file(responseTime_recorder[prefix], std::ios::app);

    if (!file.is_open())
    {
        std::cerr << "Failed to open the file: " << responseTime_recorder[prefix] << std::endl;
        return;
    }

    // Write the response_time to the file, followed by a newline

    std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime);
    file << now.count() << " " << seq << " " << responseTime.count() / 1000 << std::endl;

    // Close the file
    file.close();
}

/**
 * Record RTO when receiving data packet
 * @param prefix
 */
void Aggregator::RTORecorder(std::string prefix)
{
    // Open the file using fstream in append mode
    std::ofstream file(RTO_recorder[prefix], std::ios::app);

    if (!file.is_open())
    {
        std::cerr << "Failed to open the file: " << RTO_recorder[prefix] << std::endl;
        return;
    }

    // Write the response_time to the file, followed by a newline

    std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime);
    file << now.count() << " " << RTO_threshold[prefix].count() / 1000 << std::endl;

    // Close the file
    file.close();
}

/**
 * Record the aggregate time when each iteration finished
 * @param aggregateTime
 */
void Aggregator::AggregateTimeRecorder(std::chrono::milliseconds aggregateTime, uint32_t seq)
{
    // Open the file using fstream in append mode
    std::ofstream file(aggregateTime_recorder, std::ios::app);

    if (!file.is_open())
    {
        std::cerr << "Failed to open the file: " << aggregateTime_recorder << std::endl;
        return;
    }

    // Write aggregation time to file, followed by a new line

    std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime);
    file << now.count() << " " << seq << " " << aggregateTime.count() / 1000 << std::endl;

    file.close();
}

/**
 * Initialize all new log files, called in the beginning of simulation
 */
void Aggregator::InitializeLogFile()
{
    // Check whether the object path exists, if not, create it first
    CheckDirectoryExist(folderPath);

    // Open the file and clear all contents for all log files
    // Initialize file name for different upstream node, meaning that RTT/cwnd is measured per flow
    for (const auto &[child, leaves] : aggregationMap)
    {
        RTO_recorder[child] = folderPath + m_prefix.toUri() + "_RTO_" + child + ".txt";
        responseTime_recorder[child] = folderPath + m_prefix.toUri() + "_RTT_" + child + ".txt";
        // window_recorder[child] = folderPath + m_prefix.toUri() + "_window_" + child + ".txt";
        inFlight_recorder[child] = folderPath + m_prefix.toUri() + "_inFlight_" + child + ".txt";
        qsf_recorder[child] = folderPath + m_prefix.toUri() + "_qsf_" + child + ".txt";
        qsNew_recorder[child] = folderPath + m_prefix.toUri() + "_queue_" + child + ".txt";
        OpenFile(RTO_recorder[child]);
        OpenFile(responseTime_recorder[child]);
        // OpenFile(window_recorder[child]);
        OpenFile(inFlight_recorder[child]);
        OpenFile(qsf_recorder[child]);
        OpenFile(qsNew_recorder[child]);
    }

    // Initialize log file for aggregate time
    aggregateTime_recorder = folderPath + m_prefix.toUri() + "_aggregationTime.txt";
    OpenFile(aggregateTime_recorder);
    aggTable_recorder = folderPath + m_prefix.toUri() + "_aggTable_.txt";
    OpenFile(aggTable_recorder);
}

/**
 * Initialize all parameters
 * Initialize timeout checking mechanism, remove part of SetRetcTimer()'s function to here
 */
void Aggregator::InitializeParameters()
{
    // Initialize window
    for (const auto &[key, value] : aggregationMap)
    {
        m_window[key] = m_initialWindow;
        m_inFlight[key] = 0;
        m_ssthresh[key] = std::numeric_limits<double>::max();
        // successiveCongestion[key] = 0;

        // Initialize RTO measurement parameters
        SRTT[key] = 0;
        RTTVAR[key] = 0;
        roundRTT[key] = 0;

        // Initialize CUBIC factor
        m_cubicLastWmax[key] = m_initialWindow;
        m_cubicWmax[key] = m_initialWindow;

        std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime);
        lastWindowDecreaseTime[key] = now;
        RTT_historical_estimation[key] = 0; //! Re-initialize this RTT-estimation, what's the correct way?
        RTT_count[key] = 0;

        // Initialize timeout checking
        RTO_threshold[key] = 5 * m_retxTimer;

        // Initialize seq
        SeqMap[key] = 0;

        //! Debugging - Initialize qsf info
        m_qsfSlidingWindows[key] = SlidingWindow<double>(std::chrono::milliseconds(m_qsfTimeDuration));
        m_estimatedBW[key] = m_qsfInitRate;
        m_rateLimit[key] = m_qsfInitRate;
        firstData[key] = true;
        // m_rateEvent[key] = Simulator::ScheduleNow(&Aggregator::RateLimitUpdate, this, key);
        // Difference : add a new schedule here
        // m_rateEvent[key] = m_scheduler.schedule(ndn::time::milliseconds(0), [this, key]
        //                                         { this->RateLimitUpdate(key); });
        RTT_estimation_qsf[key] = 0; // Init rtt estimation as 0
    }

    // Init params for interest sending rate pacing
    firstInterest = true;
    isRTTEstimated = false;
}

// /**
//  * Check whether the cwnd has been decreased within the last RTT duration
//  * @param threshold
//  */
bool Aggregator::CanDecreaseWindow(std::string prefix, int64_t threshold)
{

    std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime);
    if (now.count() - lastWindowDecreaseTime[prefix].count() > threshold)
    {
        return true;
    }
    else
    {
        return false;
    }
}

/**
 * Record the final throughput into file at the end of simulation
 * @param interestThroughput
 * @param dataThroughput
 */
void Aggregator::ThroughputRecorder(int interestThroughput, int dataThroughput, std::chrono::milliseconds start_simulation)
{
    // Open the file using fstream in append mode
    std::ofstream file(throughput_recorder, std::ios::app);

    if (!file.is_open())
    {
        std::cerr << "Failed to open the file: " << throughput_recorder << std::endl;
        return;
    }

    // Write aggregation time to file, followed by a new line

    std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime);
    file << interestThroughput << " " << dataThroughput << " " << numChild << " " << start_simulation.count() << " " << now.count() << std::endl;

    file.close();
}

void Aggregator::AggTableRecorder(uint32_t seq)
{
    // Open the file using fstream in append mode
    std::ofstream file(aggTable_recorder, std::ios::app);

    if (!file.is_open())
    {
        std::cerr << "Failed to open the file: " << aggTable_recorder << std::endl;
        return;
    }

    // Write aggregation time to file, followed by a new line

    std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime);
    file << now.count() << " " << seq << " " << partialAggResult.size() << " " << sumParameters.size() << std::endl;

    file.close();
}

/**
 * Record the final results
 */
void Aggregator::ResultRecorder(int64_t aveAggTime)
{
    // Open the file using fstream in append mode
    std::ofstream file(result_recorder, std::ios::app);

    if (!file.is_open())
    {
        std::cerr << "Failed to open the file: " << result_recorder << std::endl;
        return;
    }

    file << m_prefix << "'s result" << std::endl;
    file << "Total iterations: " << m_iteNum << std::endl;
    file << "Timeout is triggered for " << suspiciousPacketCount << " times" << std::endl;
    file << "The number of downstream duplicate interest retransmission is " << downstreamRetxCount << " times" << std::endl;
    file << "Interest queue overflow is triggered for " << interestOverflow << " times" << std::endl;
    file << "Data queue overflow is triggered for " << dataOverflow << " times" << std::endl;
    file << "Nack(upstream interest queue overflow) is triggered for " << nackCount << " times" << std::endl;
    file << "Average aggregation time: " << aveAggTime / 1000 << " ms" << std::endl;
    file << "-----------------------------------" << std::endl;
}

/**
 * Record qsf congestion info
 * Note that all rate and time is parsed from "us" into "ms"
 */
void Aggregator::QsfRecorder(std::string prefix, double qsf)
{
    // Open the file using fstream in append mode
    std::ofstream file(qsf_recorder[prefix], std::ios::app);

    if (!file.is_open())
    {
        std::cerr << "Failed to open the file: " << qsf_recorder[prefix] << std::endl;
        return;
    }

    double actualQsf;
    if (qsf == -1)
    {
        actualQsf = static_cast<double>(std::max(interestQueue[prefix].size(), partialAggResult.size()));
    }
    else
    {
        actualQsf = qsf;
    }

    // Unit: ms or pkgs/ms
    // time - rate limit - BW - throughput(data arrival rate) - qsf - local queue size(larger one between interest/data queue) - interest queue size - data queue size - rtt estimation

    std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime);
    file << now.count() << " "
         << m_rateLimit[prefix] * 1000 << " "
         << m_estimatedBW[prefix] * 1000 << " "
         << GetDataRate(prefix) * 1000 << " "
         << actualQsf << " "
         << static_cast<double>(std::max(interestQueue[prefix].size(), partialAggResult.size())) << " "
         << interestQueue[prefix].size() << " "
         << partialAggResult.size() << " "
         << static_cast<int64_t>(RTT_estimation_qsf[prefix] / 1000) << " "
         << std::endl;

    file.close();
}

/**
 * Record queue size based CC info
 */
void Aggregator::QueueRecorder(std::string prefix, double queueSize)
{
    // Open the file using fstream in append mode
    std::ofstream file(qsNew_recorder[prefix], std::ios::app);

    if (!file.is_open())
    {
        std::cerr << "Failed to open the file: " << qsNew_recorder[prefix] << std::endl;
        return;
    }

    // Write the response_time to the file, followed by a newline

    std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime);
    file << now.count() << " "
         << m_rateLimit[prefix] * 1000 << " "
         << m_estimatedBW[prefix] * 1000 << " "
         << GetDataRate(prefix) * 1000 << " "
         << queueSize << " "
         << m_inFlight[prefix] << " "
         << RTT_estimation_qsf[prefix] / 1000 << " "
         << std::endl;

    // Close the file
    file.close();
}

/**
 * Based on returned data, update rtt estimation
 * @param prefix
 * @param resTime unit - us
 */
void Aggregator::RTTMeasure(std::string prefix, int64_t resTime)
{
    // Update RTT estimation
    if (RTT_estimation_qsf[prefix] == 0)
    {
        RTT_estimation_qsf[prefix] = resTime;
    }
    else
    {
        RTT_estimation_qsf[prefix] = m_EWMAFactor * RTT_estimation_qsf[prefix] + (1 - m_EWMAFactor) * resTime;
    }
}

/**
 * Get the data rate and return with correct value from the sliding window
 */
double
Aggregator::GetDataRate(std::string prefix)
{
    double rawDataRate = m_qsfSlidingWindows[prefix].GetDataArrivalRate();

    // "0": sliding window size is less than one, keep init rate as data arrival rate; "-1" indicates error
    if (rawDataRate == -1)
    {
        spdlog::info("Returned data arrival rate is -1, please check!");
        std::exit(EXIT_FAILURE);
        return 0;
    }
    else if (rawDataRate == 0)
    {
        spdlog::info("Sliding window is not enough, data arrival rate is corrected as init rate: {} pkgs/ms", m_qsfInitRate * 1000);
        return m_qsfInitRate;
    }
    else
    {
        return rawDataRate;
    }
}

/**
 * Bandwidth estimation
 * @param prefix flow
 * @param dataArrivalRate data arrival rate
 */
void Aggregator::BandwidthEstimation(std::string prefix, double qsfUpstream)
{

    std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime);
    std::chrono::milliseconds arrivalTime = now;

    if (qsfUpstream == -1)
    {
        // Aggregator which connects to producers directly
        double localQueue = static_cast<double>(std::max(interestQueue[prefix].size(), sumParameters.size()));
        spdlog::info("Use local queue size as qsf: {}", localQueue);
        m_qsfSlidingWindows[prefix].AddPacket(arrivalTime, localQueue);
    }
    else
    {
        // Other aggregators
        spdlog::info("Upstream qsf: {}", qsfUpstream);
        m_qsfSlidingWindows[prefix].AddPacket(arrivalTime, qsfUpstream);
    }

    double aveQSF = m_qsfSlidingWindows[prefix].GetAverageQsf();
    double dataArrivalRate = GetDataRate(prefix);

    // Error handling
    if (aveQSF == -1)
    {
        spdlog::error("Returned QSF is -1, please check!");
        std::exit(EXIT_FAILURE);
        return;
    }

    // Update bandwidth estimation
    if (aveQSF > m_qsfQueueThreshold)
    {
        m_estimatedBW[prefix] = dataArrivalRate;
    }
    if (dataArrivalRate > m_estimatedBW[prefix])
    {
        m_estimatedBW[prefix] = dataArrivalRate;
    }

    spdlog::info("Flow: {} - Average QSF: {}, Arrival Rate: {} pkgs/ms, Bandwidth estimation: {} pkgs/ms", prefix, aveQSF, dataArrivalRate * 1000, m_estimatedBW[prefix] * 1000);
}

/**
 * Update each flow's rate limit.
 */
void Aggregator::RateLimitUpdate(std::string prefix)
{
    double qsf = m_qsfSlidingWindows[prefix].GetAverageQsf();
    spdlog::info("Flow {} - qsf: {}", prefix, qsf);

    // Congestion control
    if (qsf > 2 * m_qsfQueueThreshold)
    {
        m_rateLimit[prefix] = m_estimatedBW[prefix] * m_qsfMDFactor;
        spdlog::info("Congestion detected. Update rate limit: {} pkgs/ms", m_rateLimit[prefix] * 1000);
    }
    else
    {
        m_rateLimit[prefix] = m_estimatedBW[prefix];
        spdlog::info("No congestion. Update rate limit by estimated BW: {} pkgs/ms", m_rateLimit[prefix] * 1000);
    }

    // Rate probing
    if (qsf < m_qsfQueueThreshold)
    {
        m_rateLimit[prefix] = m_rateLimit[prefix] * m_qsfRPFactor;
        spdlog::info("Start rate probing. Updated rate limit: {} pkgs/ms", m_rateLimit[prefix] * 1000);
    }

    // Error handling
    if (RTT_estimation_qsf[prefix] == 0)
    {
        spdlog::info("RTT estimation is 0, please check!");
        std::exit(EXIT_FAILURE);
        return;
    }

    spdlog::info("Flow {} - Schedule next rate limit update after {} ms", prefix, RTT_estimation_qsf[prefix] / 1000);
    // waiting for modification
    m_rateEvent[prefix] = m_scheduler.schedule(ndn::time::microseconds(RTT_estimation_qsf[prefix]), [this, prefix]
                                               { this->RateLimitUpdate(prefix); });
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <node-name>" << std::endl;
        return 1;
    }

    std::string nodeName = argv[1];
    ndn::Name prefix(nodeName);

    Aggregator agg;
    agg.setPrefix(prefix);
    agg.StartApplication();

    return 0;
}