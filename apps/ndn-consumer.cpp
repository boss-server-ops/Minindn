#include "ndn-consumer.hpp"

Consumer::Consumer()
    : m_scheduler(m_face.getIoContext()),
      m_interestName("/example/testApp"),
      m_nodeprefix("con0"),
      m_seq(0),
      m_seqMax(100),
      m_interestLifeTime(std::chrono::milliseconds(4000)),
      m_rand(std::random_device{}()),                     // Use std::random_device to generate random seed
      m_uniformDist(0.0, 1.0),                            // Initialize uniform distribution, range [0.0, 1.0]
      m_rtt(std::make_unique<ndn::util::RttEstimator>()), // Initialize RTT estimator
      suspiciousPacketCount(0),
      totalInterestThroughput(0),
      totalDataThroughput(0),
      dataOverflow(0),
      nackCount(0),
      throughputStable(false),
      linkCount(0), initSeq(0),
      globalSeq(0),
      broadcastSync(false),
      total_response_time(0),
      round(0),
      totalAggregateTime(0),
      iterationCount(0)
{
    // Initialize spdlog
    m_logger = spdlog::basic_logger_mt("consumer_logger", "logs/consumer.log");
    spdlog::set_default_logger(m_logger);
    spdlog::set_level(spdlog::level::info); // Set log level
    spdlog::flush_on(spdlog::level::info);  // Flush after each log

    spdlog::info("Consumer initialized with interest name: {}", m_interestName);

    // Read configuration from config.ini
    ReadConfig();
    // SetRetxTimer(std::chrono::milliseconds(10));
}

void Consumer::ReadConfig()
{
    boost::property_tree::ptree pt;
    boost::property_tree::ini_parser::read_ini("../experiments/config.ini", pt);

    // General section
    m_topologyType = pt.get<std::string>("General.TopologyType", "BINARY");
    m_constraint = pt.get<int>("General.Constraint", 2);
    m_minWindow = pt.get<int>("General.Window", 1);
    m_initPace = pt.get<int>("General.InitPace", 2);
    std::string ccAlgorithm = pt.get<std::string>("General.CcAlgorithm", "AIMD");
    m_ccAlgorithm = (ccAlgorithm == "AIMD") ? CcAlgorithm::AIMD : CcAlgorithm::CUBIC;
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

    // Consumer section
    m_iteNum = pt.get<int>("Consumer.Iteration", 200);
    m_interestQueue = pt.get<int>("Consumer.ConInterestQueue", 5);
    m_dataQueue = pt.get<int>("Consumer.ConDataQueue", 20);
}

/**
 * Consumer sends relevant aggregation tree to all aggregators
 */
void Consumer::TreeBroadcast()
{

    const auto &broadcastTree = aggregationTree[0];

    for (const auto &[parentNode, childList] : broadcastTree)
    {
        // Don't broadcast to itself
        if (parentNode == m_nodeprefix)
        {
            continue;
        }

        std::string nameWithType;
        std::string nameType = "initialization";
        nameWithType += "/" + parentNode;
        auto result = getLeafNodes(parentNode, broadcastTree);

        // Construct nameWithType variable for tree broadcast
        for (const auto &[childNode, leaves] : result)
        {
            std::string name_indication;
            name_indication += childNode + ".";
            for (const auto &leaf : leaves)
            {
                name_indication += leaf + ".";
            }
            name_indication.resize(name_indication.size() - 1); // Remove the last extra dot
            nameWithType += "/" + name_indication;
        }
        nameWithType += "/" + nameType;

        // Use spdlog instead of cout
        spdlog::info("Node {}'s name is: {}", parentNode, nameWithType);

        std::shared_ptr<ndn::Name> newName = std::make_shared<ndn::Name>(nameWithType);
        newName->appendSequenceNumber(globalSeq);
        SendInterest(newName);
    }
    initSeq++;
}

void Consumer::ConstructAggregationTree()
{
    // Call the base class's ConstructAggregationTree method
    App::ConstructAggregationTree();

    // Choose the corresponding topology type
    if (m_topologyType == "DCN")
    {

        filename = "../topologies/DataCenterTopology.txt";
    }
    else if (m_topologyType == "ISP")
    {

        filename = "../topologies/ISPTopology.txt";
    }
    else if (m_topologyType == "BINARY")
    {

        filename = "../topologies/BinaryTreeTopology.txt";
        spdlog::info("Binary tree topology is chosen.");
    }
    else
    {
        spdlog::error("Topology type error, please check!");
        std::exit(EXIT_FAILURE);
        return;
    }
    // Create AggregationTree object
    AggregationTree tree(filename);
    std::vector<std::string> dataPointNames = Utility::getProducers(filename);
    std::map<std::string, std::vector<std::string>> rawAggregationTree;
    std::vector<std::vector<std::string>> rawSubTree;

    // Construct the aggregation tree
    if (tree.aggregationTreeConstruction(dataPointNames, m_constraint))
    {
        rawAggregationTree = tree.aggregationAllocation;
        rawSubTree = tree.noCHTree;
    }
    else
    {
        spdlog::error("Fail to construct aggregation tree!");
        std::exit(EXIT_FAILURE);
    }

    // Get the number of producers
    producerCount = Utility::countProducers(filename);

    // Create producer list
    for (const auto &item : dataPointNames)
    {
        proList += item + ".";
    }
    proList.resize(proList.size() - 1);

    // Create complete "aggregationTree" from raw ones
    aggregationTree.push_back(rawAggregationTree);
    while (!rawSubTree.empty())
    {
        const auto &item = rawSubTree[0];
        rawAggregationTree[m_nodeprefix] = item;
        aggregationTree.push_back(rawAggregationTree);
        rawSubTree.erase(rawSubTree.begin());
    }

    int i = 0;
    spdlog::info("Iterate all aggregation tree (including main tree and sub-trees).");
    for (const auto &map : aggregationTree)
    {
        for (const auto &pair : map)
        {
            spdlog::info("{}: ", pair.first);
            for (const auto &value : pair.second)
            {
                spdlog::info("{} ", value);
            }

            // 1. Aggregator
            // Initialize "broadcastList" for tree broadcasting synchronization
            if (pair.first != m_nodeprefix)
            {
                broadcastList.insert(pair.first); // Defined using set, no duplicate elements
            }
            // 2. Consumer
            // Initialize "globalTreeRound" for all rounds (if there're multiple sub-trees)
            else
            {
                // Specify the number of consumer's child nodes (links)

                std::vector<std::string> leavesRound;
                spdlog::info("Round {} has the following leaf nodes: ", i);
                for (const auto &leaves : pair.second)
                {
                    leavesRound.push_back(leaves);
                    spdlog::info("{} ", leaves);
                }
                globalTreeRound.push_back(leavesRound); // Initialize "globalTreeRound"
            }
        }
        spdlog::info("----"); // Separator between maps
        i++;
    }

    // Compute link count
    for (const auto &a : globalTreeRound)
    {
        for (const auto &b : a)
        {
            linkCount++;
        }
    }
}

void Consumer::StartApplication()
{
    // Call the base class's StartApplication method
    App::StartApplication();
    startTime = std::chrono::steady_clock::now();

    // Construct the aggregation tree
    // TODO: delete comment later
    ConstructAggregationTree();
    // TreeBroadcast();

    // // Init log file and parameters
    InitializeLogFile();
    InitializeParameter();

    // // Start the interest generator
    InterestGenerator();

    // // Broadcast the aggregation tree
    TreeBroadcast();

    m_face.processEvents();
}

void Consumer::StopApplication() // Called at time specified by Stop
{
    // waiting for implementation
    spdlog::info("Stopping Consumer application");
    // ps: deleted cancel event
    // if (m_sendEvent.joinable())
    // {
    //     m_sendEvent.join();
    // }
    App::StopApplication();
}

std::map<std::string, std::set<std::string>> Consumer::getLeafNodes(const std::string &key,
                                                                    const std::map<std::string, std::vector<std::string>> &treeMap)
{
    return App::getLeafNodes(key, treeMap);
}

int Consumer::findRoundIndex(const std::string &target)
{
    return App::findRoundIndex(globalTreeRound, target);
}

double Consumer::getDataQueueSize(std::string prefix)
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
            queueSize = 1.0;
        }
    }

    spdlog::debug("Flow: {} -> Data queue size: {}", prefix, queueSize);
    return queueSize;
}

void Consumer::Aggregate(const ModelData &data, const uint32_t &seq)
{
    // first initialization
    if (sumParameters.find(seq) == sumParameters.end())
    {
        sumParameters[seq] = std::vector<double>(m_dataSize, 0.0);
    }

    // Aggregate data
    std::transform(sumParameters[seq].begin(), sumParameters[seq].end(), data.parameters.begin(), sumParameters[seq].begin(), std::plus<double>());
}

std::vector<double> Consumer::getMean(const uint32_t &seq)
{
    std::vector<double> result;
    if (sumParameters[seq].empty() || producerCount == 0)
    {
        spdlog::error("Error when calculating average model, please check!");
        return result;
    }

    for (auto value : sumParameters[seq])
    {
        result.push_back(value / static_cast<double>(producerCount));
    }

    return result;
}

void Consumer::ResponseTimeSum(int64_t response_time)
{
    total_response_time += response_time;
    ++round;
}

int64_t Consumer::GetResponseTimeAverage()
{
    if (round == 0)
    {
        spdlog::error("Error happened when calculating response time!");
        return 0;
    }
    return total_response_time / round;
}

void Consumer::AggregateTimeSum(int64_t aggregate_time)
{
    totalAggregateTime += aggregate_time;
    ++iterationCount;
}

int64_t Consumer::GetAggregateTimeAverage()
{
    if (iterationCount == 0)
    {
        spdlog::error("Error happened when calculating aggregate time!");
        throw std::runtime_error("Error happened when calculating aggregate time!");
    }

    return totalAggregateTime / iterationCount / 1000;
}

void Consumer::OnData(const ndn::Interest &interest, const ndn::Data &data)
{
    spdlog::info("Consumer received data");
    if (!m_active)
        return;

    App::OnData(interest, data);
    std::string type = data.getName().get(-2).toUri();
    std::string name_sec0 = data.getName().get(0).toUri();
    uint32_t seq = data.getName().at(-1).toSequenceNumber();
    std::string dataName = data.getName().toUri();
    int dataSize = data.wireEncode().size();
    // Check whether this's duplicate data packet
    // question: why can't use receive duplicate data
    if (m_agg_finished.find(seq) != m_agg_finished.end())
    {
        spdlog::error("This data packet is duplicate, stop and check!");
        std::exit(EXIT_FAILURE);
        return;
    }

    // TODO: testing, delete later
    // getDataQueueSize(name_sec0);

    // TODO: what's the best strategy under qsf design?
    //? Currently pause the interest sending for 5 * current period
    // Check partial aggregation table
    if (sumParameters.find(seq) == sumParameters.end())
    {
        // New iteration, currently not exist in the partial agg result
        if (partialAggResult.size() >= m_dataQueue)
        {
            // Exceed max data size
            spdlog::info("Exceeding the max data queue, stop interest sending for flow ", name_sec0);
            dataOverflow++;

            // Schdule next event after 5 * current period
            if (m_scheduleEvent[name_sec0])
            {
                m_scheduleEvent[name_sec0].cancel();
                m_scheduleEvent[name_sec0].reset();
            }

            double nextTime = 5 * 1 / m_rateLimit[name_sec0]; // Unit: us
            spdlog::info("Flow {} -> Schedule next sending event after {} ms. from consumer ondata", name_sec0, nextTime / 1000);
            // TODO:check whether int is suitable for the design
            m_scheduleEvent[name_sec0] = m_scheduler.schedule(ndn::time::microseconds(static_cast<int64_t>(nextTime)),
                                                              [this, name_sec0]
                                                              { this->ScheduleNextPacket(name_sec0); });
        }
        partialAggResult[seq] = true;
    }

    // Erase timeout
    if (m_timeoutCheck.find(dataName) != m_timeoutCheck.end())
        m_timeoutCheck.erase(dataName);
    else
    {
        spdlog::error("Suspicious data packet, not exists in timeout list.");
        std::exit(EXIT_FAILURE);
        return;
    }

    if (m_inFlight[name_sec0] > 0)
    {
        m_inFlight[name_sec0]--;
    }
    if (type == "data")
    {
        // Perform data name matching with interest name
        ModelData modelData;
        auto data_agg = map_agg_oldSeq_newName.find(seq);
        if (data_agg != map_agg_oldSeq_newName.end())
        {
            // Aggregation starts
            auto &aggVec = data_agg->second;
            auto aggVecIt = std::find(aggVec.begin(), aggVec.end(), name_sec0);
            std::vector<uint8_t> oldbuffer(data.getContent().value(), data.getContent().value() + data.getContent().value_size());

            if (deserializeModelData(oldbuffer, modelData))
            {
                if (aggVecIt != aggVec.end())
                {
                    Aggregate(modelData, seq);
                    aggVec.erase(aggVecIt);
                }
                else
                {
                    spdlog::error("This data packet is duplicate, error!");
                    std::exit(EXIT_FAILURE);
                    return;
                }
            }
            else
            {
                spdlog::error("Error when deserializing data packet, please check!");
                std::exit(EXIT_FAILURE);
                return;
            }

            // RTT measurement
            if (rttStartTime.find(dataName) != rttStartTime.end())
            {
                auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime);
                responseTime[dataName] = now - rttStartTime[dataName];
                ResponseTimeSum(responseTime[dataName].count());
                rttStartTime.erase(dataName);
                spdlog::info("Consumer's response time of sequence {} is: {} ms.", dataName, responseTime[dataName].count());
            }
            // RTO/RTT measure
            // zyx: data arrives so fastly that responseTime is 0
            RTOMeasure(responseTime[dataName].count(), name_sec0);
            RTTMeasure(name_sec0, responseTime[dataName].count());

            //! Debugging, qsf design
            // Update estimated bandwidth
            BandwidthEstimation(name_sec0, modelData.qsf);

            // Init rate limit update
            if (firstData.at(name_sec0))
            {

                spdlog::debug("Init rate limit update for flow {}", name_sec0);
                m_rateEvent[name_sec0] = m_scheduler.schedule(ndn::time::milliseconds(0), [this, name_sec0]
                                                              { this->RateLimitUpdate(name_sec0); });
                firstData[name_sec0] = false;
            }

            // Get round index
            int roundIndex = findRoundIndex(name_sec0);
            if (roundIndex == -1)
            {
                spdlog::error("Error on roundIndex!");
                std::exit(EXIT_FAILURE);
            }
            spdlog::debug("This packet comes from round {}", roundIndex);

            // qsf recorder
            QsfRecorder(name_sec0, modelData.qsf);
            QueueRecorder(name_sec0, getDataQueueSize(name_sec0));

            // Record RTT
            ResponseTimeRecorder(roundIndex, name_sec0, seq, responseTime[dataName]);
            // Record RTO
            RTORecorder(name_sec0);
            InFlightRecorder(name_sec0);
            // Check whether the aggregation iteration has finished
            if (aggVec.empty())
            {
                spdlog::debug("Aggregation of iteration {} finished!", seq);
                // Measure aggregation time
                if (aggregateStartTime.find(seq) != aggregateStartTime.end())
                {
                    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime);
                    aggregateTime[seq] = now - aggregateStartTime[seq];
                    AggregateTimeSum(aggregateTime[seq].count());
                    spdlog::info("Iteration {}'s aggregation time is:{}  ms.", seq, aggregateTime[seq].count());

                    aggregateStartTime.erase(seq);
                }
                else
                {
                    spdlog::debug("Error when calculating aggregation time, no reference found for seq {}", seq);
                }

                // Record aggregation time
                AggregateTimeRecorder(aggregateTime[seq], seq);

                // Get aggregation result and store them
                aggregationResult[seq] = getMean(seq);

                // Mark the map that current iteration has finished
                m_agg_finished[seq] = true;

                // Clear aggregation time mapping for current iteration
                aggregateTime.erase(seq);

                // Remove seq from aggMap
                map_agg_oldSeq_newName.erase(seq);
                partialAggResult.erase(seq);
            }

            // Stop simulation
            if (iterationCount == m_iteNum)
            {
                // waiting for firguring out what the function of stoptime is
                // auto stoptime = std::chrono::steady_clock::now();
                spdlog::debug("Reach {} iterations, stop!", m_iteNum);
                spdlog::info("Timeout is triggered {} times.", suspiciousPacketCount);
                spdlog::info("The average aggregation time of Consumer in {} iteration is: {} ms", iterationCount, GetAggregateTimeAverage());
                // ThroughputRecorder(totalInterestThroughput, totalDataThroughput, startSimulation, startThroughputMeasurement);
                // Record result into file
                auto now = std::chrono::steady_clock::now();
                int64_t totalTime = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count() - 1000000;
                ResultRecorder(m_iteNum, suspiciousPacketCount, GetAggregateTimeAverage(), totalTime);

                // Stop simulation
                std::exit(EXIT_SUCCESS);
                return;
            }
            // Clear rtt mapping of this packet
            rttStartTime.erase(dataName);
            responseTime.erase(dataName);
        }
        else
        {
            spdlog::error("Suspicious data packet, not exist in aggregation map.");
            std::exit(EXIT_FAILURE);
        }
    }
    else if (type == "initialization")
    {
        spdlog::debug("Initialization data packet received, start aggregation!");
        auto it = std::find(broadcastList.begin(), broadcastList.end(), name_sec0);
        if (it != broadcastList.end())
        {
            broadcastList.erase(it);
            spdlog::debug("Node {} has received aggregationTree map, erase it from broadcastList", name_sec0);
        }
        // qsf: init for each flow
        // m_scheduleEvent[name_sec0] = Simulator::ScheduleNow(&Consumer::ScheduleNextPacket, this, name_sec0);

        // Tree broadcasting synchronization is done
        if (broadcastList.empty())
        {
            broadcastSync = true;
            spdlog::debug("Synchronization of tree broadcasting finished!");

            // Record aggregation tree into file
            AggTreeRecorder();
        }

        //! Schedule all flows together after synchronization
        if (broadcastSync)
        {
            for (const auto &vec_round : globalTreeRound)
            {
                for (const auto &flow : vec_round)
                {
                    spdlog::debug("Flow {} -> Schedule next sending event after initialization", flow);
                    m_scheduleEvent[flow] = m_scheduler.schedule(ndn::time::milliseconds(0), [this, flow]
                                                                 { this->ScheduleNextPacket(flow); });
                }
            }
        }
    }
}

void Consumer::OnNack(const ndn::Interest &interest, const ndn::lp::Nack &nack)
{
    App::OnNack(interest, nack);
    std::string dataName = nack.getInterest().getName().toUri();
    std::string name_sec0 = nack.getInterest().getName().get(0).toUri();
    uint32_t seq = nack.getInterest().getName().get(-1).toSequenceNumber();
    // if (nack.getReason() == ndn::lp::NackReason::DUPLICATE)
    // {

    //     return;
    // }

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

    // Insert the rejected interest back to the front of the interest queue
    interestQueue[name_sec0].push_front(seq);

    // Decrease sending rate for certain flow
    // WindowDecrease(name_sec0, "nack");

    // Stop tracing rtt and timeout
    rttStartTime.erase(dataName);
    m_timeoutCheck.erase(dataName);
    nackCount++;
}

void Consumer::OnTimeout(const ndn::Interest &interest)
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
    App::OnTimeout(interest);
    auto newInterest = std::make_shared<ndn::Name>(interest.getName());
    SendInterest(newInterest);
    suspiciousPacketCount++;
}

// question:how to usr it
void Consumer::SetRetxTimer(std::chrono::milliseconds retxTimer)
{

    m_retxTimer = retxTimer;

    // Cancel the existing retransmission event if it's running
    if (m_retxEvent)
    {
        m_retxEvent.cancel();
        m_retxEvent.reset();
    }

    // Log the next interval to check timeout
    spdlog::debug("Next interval to check timeout is: {} ms", m_retxTimer.count());

    // Schedule timeout check event
    m_retxEvent = m_scheduler.schedule(ndn::time::milliseconds(m_retxTimer.count()), [this]
                                       { this->CheckRetxTimeout(); });
}

std::chrono::milliseconds Consumer::GetRetxTimer() const
{
    return m_retxTimer;
}

void Consumer::CheckRetxTimeout()
{
    // ps:deleted schedule event and have different ontimeout
    std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime);

    for (auto it = m_timeoutCheck.begin(); it != m_timeoutCheck.end();)
    {
        // Parse the string and extract the first segment, e.g. "agg0", then find out its round
        std::string type = std::make_shared<ndn::Name>(it->first)->get(-2).toUri();

        // For two types of data, check timeout respectively
        if (type == "initialization")
        {
            if (now - it->second > (3 * m_retxTimer))
            {

                std::string name = it->first;
                // it = m_timeoutCheck.erase(it);

                m_pendingInterest[name].cancel();
                // TODO：why should I wait for 1ms
                m_timeoutEvent = m_scheduler.schedule(ndn::time::milliseconds(1), [this, name]
                                                      { this->OnTimeout(ndn::Interest(name)); });
            }
            // else
            // {
            //     ++it;
            // }
            it++;
        }
        else if (type == "data")
        {
            std::string name = it->first;
            std::string prefix = std::make_shared<ndn::Name>(name)->get(0).toUri();

            if (now - it->second > RTO_threshold[prefix])
            {
                std::string name = it->first;
                // it = m_timeoutCheck.erase(it);
                numTimeout[prefix]++;

                m_pendingInterest[name].cancel();
                spdlog::info("Timeout check name: {}", name);
                m_timeoutEvent = m_scheduler.schedule(ndn::time::milliseconds(1), [this, name]
                                                      { this->OnTimeout(ndn::Interest(name)); });
                // OnTimeout(ndn::Interest(name));
            }
            // else
            // {
            //     ++it;
            // }
            it++;
        }
    }

    // Reschedule the next timeout check event
    m_retxEvent = m_scheduler.schedule(ndn::time::milliseconds(m_retxTimer.count()), [this]
                                       { this->CheckRetxTimeout(); });
    // m_retxEvent = m_checkretxScheduler.schedule(ndn::time::milliseconds(0), [this]
    //                                             { this->CheckRetxTimeout(); });
}

void Consumer::RTOMeasure(int64_t resTime, std::string prefix)
{
    if (!initRTO[prefix])
    {
        RTTVAR[prefix] = resTime / 2;
        SRTT[prefix] = resTime;
        spdlog::debug("Initialize RTO for flow:{}", prefix);
        initRTO[prefix] = true;
    }
    else
    {
        RTTVAR[prefix] = 0.75 * RTTVAR[prefix] + 0.25 * std::abs(SRTT[prefix] - resTime); // RTTVAR = (1 - b) * RTTVAR + b * |SRTT - RTTsample|, where b = 0.25
        SRTT[prefix] = 0.875 * SRTT[prefix] + 0.125 * resTime;                            // SRTT = (1 - a) * SRTT + a * RTTsample, where a = 0.125
    }
    int64_t RTO = SRTT[prefix] + 4 * RTTVAR[prefix]; // RTO = SRTT + K * RTTVAR, where K = 4
    // TODO: check if milliseconds is the right unit or microseconds
    RTO_threshold[prefix] = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::microseconds(4 * RTO));
}

void Consumer::InterestGenerator()
{
    // Generate name from section 1 to 3 (except for seq)
    spdlog::debug("consumer start generate interest");
    std::vector<std::string> objectProducer;
    std::string token;
    std::istringstream tokenStream(proList);
    char delimiter = '.';
    while (std::getline(tokenStream, token, delimiter))
    {
        objectProducer.push_back(token);
    }

    for (const auto &aggTree : aggregationTree)
    {
        auto initialAllocation = getLeafNodes(m_nodeprefix, aggTree); // example - {agg0: [pro0, pro1]}
        // spdlog::debug("Initial allocation: {}", initialAllocation);
        for (const auto &[child, leaves] : initialAllocation)
        {
            std::string name_sec0_2;
            std::string name_sec1;

            for (const auto &leaf : leaves)
            {
                name_sec1 += leaf + ".";
            }
            name_sec1.resize(name_sec1.size() - 1);
            spdlog::debug("Name section 1: {}", name_sec1);
            name_sec0_2 = "/" + child + "/" + name_sec1 + "/data";
            spdlog::debug("Name section 0-2: {}", name_sec0_2);
            NameSec0_2[child] = name_sec0_2;
            vec_iteration.push_back(child); // Will be added to aggregation map later
        }
    }
}

bool Consumer::InterestSplitting()
{
    bool canSplit = true;
    for (const auto &[prefix, queue] : interestQueue)
    {
        if (queue.size() >= m_interestQueue)
        {
            canSplit = false;
            break;
        }
    }
    if (canSplit)
    {
        // Update seq
        globalSeq++;
        for (auto &[prefix, queue] : interestQueue)
        {
            queue.push_back(globalSeq);
        }
    }
    else
    {
        spdlog::info("Interest queue is full.");
        return false;
    }

    return true;
}

void Consumer::SendPacket(std::string prefix)
{
    spdlog::info("Consumer starts sending packet");
    // Error handling for queue
    if (interestQueue[prefix].empty())
    {
        spdlog::info("No more Interests to send - prefix {}", prefix);
        std::exit(EXIT_FAILURE);
        return; // Early return if the queue is empty to avoid popping from an empty deque
    }
    uint32_t seq = interestQueue[prefix].front();
    interestQueue[prefix].pop_front();
    SeqMap[prefix] = seq;
    std::shared_ptr<ndn::Name> newName = std::make_shared<ndn::Name>(NameSec0_2[prefix]);
    newName->appendSequenceNumber(seq);
    spdlog::info("Sending packet - {}", newName->toUri());
    SendInterest(newName);
    // This is different from the code in ndnSim because we need to record the aggregation map before sending the interest

    // Check whether it's the start of a new iteration
    if (aggregateStartTime.find(seq) == aggregateStartTime.end())
    {
        std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime);
        aggregateStartTime[seq] = now;
        map_agg_oldSeq_newName[seq] = vec_iteration;
        spdlog::info("map aggreation old seq new name: {} {}", seq, prefix);
    }
}

void Consumer::SendInterest(std::shared_ptr<ndn::Name> newName)
{
    spdlog::debug("Consumer starts sending interest");
    if (!m_active)
        return;

    std::string nameWithSeq = newName->toUri();
    std::string name_sec0 = newName->get(0).toUri();
    // Trace timeout
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime);
    m_timeoutCheck[nameWithSeq] = now;

    // Start response time
    rttStartTime[nameWithSeq] = now;

    std::shared_ptr<ndn::Interest> interest = std::make_shared<ndn::Interest>();

    // Check if nonce already exists for this interest
    if (m_nonceMap.find(nameWithSeq) == m_nonceMap.end())
    {
        // Generate a new nonce and store it
        uint32_t nonce = static_cast<uint32_t>(m_uniformDist(m_rand));
        m_nonceMap[nameWithSeq] = nonce;
        spdlog::info("New nonce generated for interest: {}", nameWithSeq);
    }
    interest->setNonce(m_nonceMap[nameWithSeq]);
    // interest->setNonce(m_uniformDist(m_rand));
    // TODO: check if the nonce is the reason?
    interest->setName(*newName);
    interest->setCanBePrefix(false);
    // interest->setInterestLifetime(ndn::time::milliseconds(m_interestLifeTime.count()));
    interest->setInterestLifetime(ndn::time::seconds(2));

    spdlog::info("Sending interest >>>> {}", nameWithSeq);
    // TODO: there are some problems with the following code because actully the ontimeout doesn't suit the situation
    m_pendingInterest[nameWithSeq] = m_face.expressInterest(*interest,
                                                            std::bind(&Consumer::OnData, this, _1, _2),
                                                            std::bind(&Consumer::OnNack, this, _1, _2),
                                                            std::bind(&Consumer::OnTimeout, this, _1));
    // m_face.processEvents();

    // Record interest throughput
    // Actual interests sending and retransmission are recorded as well
    m_inFlight[name_sec0]++;
    spdlog::debug("consumer finished sending interest");
}

bool Consumer::CongestionDetection(std::string prefix, int64_t responseTime)
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
        spdlog::debug("m_smooth_window_size: {}", m_smooth_window_size);
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

void Consumer::RTORecorder(std::string prefix)
{
    // Open the file using fstream in append mode
    std::ofstream file(RTO_recorder[prefix], std::ios::app);

    if (!file.is_open())
    {
        spdlog::error("Failed to open the file: {}", RTO_recorder[prefix]);
        return;
    }

    // Write the response_time to the file, followed by a newline
    auto now = std::chrono::steady_clock::now();
    file << std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count() << " " << RTO_threshold[prefix].count() << std::endl;

    // Close the file
    file.close();
}

void Consumer::ResponseTimeRecorder(int roundIndex, std::string prefix, uint32_t seq, std::chrono::milliseconds responseTime)
{

    std::ofstream file(responseTime_recorder[prefix], std::ios::app);

    if (!file.is_open())
    {
        spdlog::error("Failed to open the file: {}", responseTime_recorder[prefix]);
        std::exit(EXIT_FAILURE);
        return;
    }

    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime);

    file << now.count() << " " << seq << " " << responseTime.count() << std::endl;
    file.close();
}

void Consumer::AggregateTimeRecorder(std::chrono::milliseconds aggregateTime, uint32_t seq)
{
    // Open the file using fstream in append mode
    std::ofstream file(aggregateTime_recorder, std::ios::app);

    if (!file.is_open())
    {
        spdlog::error("Failed to open the file: {}", aggregateTime_recorder);
        return;
    }

    // Write aggregation time to file, followed by a new line
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime);
    file << now.count() << " " << seq << " " << aggregateTime.count() << std::endl;

    file.close();
}

void Consumer::InitializeLogFile()
{
    // waiting for modify path
    //  Check whether object path exists, create it if not
    CheckDirectoryExist(folderPath);
    for (int roundIndex = 0; roundIndex < globalTreeRound.size(); roundIndex++)
    {
        for (int i = 0; i < globalTreeRound[roundIndex].size(); i++)
        {
            // RTT/RTO recorder
            // responseTime_recorder[globalTreeRound[roundIndex][i]] = folderPath + "/consumer_RTT_round" + std::to_string(roundIndex) + "_" + globalTreeRound[roundIndex][i] + ".txt";
            // RTO_recorder[globalTreeRound[roundIndex][i]] = folderPath + "/consumer_RTO_round" + std::to_string(roundIndex) + "_" + globalTreeRound[roundIndex][i] + ".txt";
            responseTime_recorder[globalTreeRound[roundIndex][i]] = folderPath + "/consumer_RTT_" + globalTreeRound[roundIndex][i] + ".txt";
            spdlog::debug("responseTime_recorder[{}]: {}", globalTreeRound[roundIndex][i], responseTime_recorder[globalTreeRound[roundIndex][i]]);
            RTO_recorder[globalTreeRound[roundIndex][i]] = folderPath + "/consumer_RTO_" + globalTreeRound[roundIndex][i] + ".txt";
            OpenFile(responseTime_recorder[globalTreeRound[roundIndex][i]]);
            OpenFile(RTO_recorder[globalTreeRound[roundIndex][i]]);
        }
    }
    for (const auto &round : globalTreeRound)
    {
        for (const auto &prefix : round)
        {
            qsNew_recorder[prefix] = folderPath + "/consumer_queue_" + prefix + ".txt";
            qsf_recorder[prefix] = folderPath + "/consumer_qsf_" + prefix + ".txt";
            inFlight_recorder[prefix] = folderPath + "/consumer_inFlight_" + prefix + ".txt";
            OpenFile(qsNew_recorder[prefix]);
            OpenFile(qsf_recorder[prefix]);
            OpenFile(inFlight_recorder[prefix]);
        }
    }
    // Aggregation time, AggTree, throughput
    aggregateTime_recorder = folderPath + "/consumer_aggregationTime.txt";
    // Open the file and clear all contents for all log files
    OpenFile(aggregateTime_recorder);
    OpenFile(throughput_recorder);
    OpenFile(aggTree_recorder);
    // Result log
    OpenFile(result_recorder);
}
/**
 * Initialize all parameters for consumer class
 */
void Consumer::InitializeParameter()
{
    int i = 0;
    // Each round
    for (const auto &round : globalTreeRound)
    {
        // Individual flow
        for (const auto &prefix : round)
        {
            //* Initialize RTO and RTT parameters
            initRTO[prefix] = false;
            RTO_threshold[prefix] = 5 * m_retxTimer;
            // RTT_threshold[prefix] = 0;
            RTT_count[prefix] = 0;
            RTT_historical_estimation[prefix] = 0;
            //* Initialize sequence map, interest queue
            SeqMap[prefix] = 0;
            interestQueue[prefix] = std::deque<uint32_t>();
            m_inFlight[prefix] = 0;
            //! Debugging - Initialize qsf info
            m_qsfSlidingWindows[prefix] = SlidingWindow<double>(std::chrono::milliseconds(m_qsfTimeDuration));
            m_estimatedBW[prefix] = m_qsfInitRate;
            m_rateLimit[prefix] = m_qsfInitRate;
            firstData[prefix] = true;
            // m_rateEvent[prefix] = Simulator::ScheduleNow(&Consumer::RateLimitUpdate, this, prefix);
            RTT_estimation_qsf[prefix] = 0; // Init rtt estimation as 0
            spdlog::info("Init rate limit - {} pkgs/ms.", m_rateLimit[prefix] * 1000);
        }
        i++;
    }
    // Init params for interest sending rate pacing
    isRTTEstimated = false;
}

bool Consumer::CanDecreaseWindow(std::string prefix, int64_t threshold)
{
    auto now = std::chrono::steady_clock::now();
    auto lastDecrease_ms = lastWindowDecreaseTime[prefix].count();

    if (std::chrono::duration_cast<std::chrono::microseconds>(now - startTime).count() - lastDecrease_ms >= threshold)
    {
        // spdlog::debug("Window decrease is allowed.");
        return true;
    }
    else
    {
        // spdlog::debug("Window decrease is suppressed.");
        return false;
    }
}

/**
 * Record in-flight packets when receiving a new packet
 */
void Consumer::InFlightRecorder(std::string prefix)
{
    // Open file; on first call, truncate it to delete old content
    std::ofstream file(inFlight_recorder[prefix], std::ios::app);
    if (!file.is_open())
    {
        std::cerr << "Failed to open the file: " << inFlight_recorder[prefix] << std::endl;
        return;
    }
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime);
    file << now.count() << " " << m_inFlight[prefix] << std::endl;
    file.close();
}

void Consumer::ThroughputRecorder(int interestThroughput, int dataThroughput, std::chrono::milliseconds start_simulation, std::chrono::milliseconds start_throughput)
{
    // Open the file using fstream in append mode
    std::ofstream file(throughput_recorder, std::ios::app);

    if (!file.is_open())
    {
        spdlog::error("Failed to open the file: {}", throughput_recorder);
        return;
    }

    // Write throughput to file, followed by a new line
    auto now = std::chrono::steady_clock::now();
    file << interestThroughput << " " << dataThroughput << " " << linkCount << " " << start_throughput.count() << " " << std::chrono::duration_cast<std::chrono::microseconds>(now - startTime).count() << std::endl;
    file.close();
}

/**
 * Record the constructed aggregation tree into file
 * It's done when receiving "Initialization" data
 */
void Consumer::AggTreeRecorder()
{

    // Open the file using fstream in append mode
    std::ofstream file(aggTree_recorder, std::ios::app);

    if (!file.is_open())
    {
        spdlog::error("Failed to open the file: {}", aggTree_recorder);
        return;
    }

    int i = 0;
    spdlog::debug("Start writing aggregation tree into the log file.");

    for (const auto &map : aggregationTree)
    {
        for (const auto &pair : map)
        {
            // Aggregator
            if (pair.first != m_nodeprefix)
            {
                file << pair.first << ": ";
                for (const auto &value : pair.second)
                {
                    file << value << " ";
                }
            }
            // Consumer
            else
            {
                file << pair.first << " -> round " << i << ": ";
                for (const auto &value : pair.second)
                {
                    file << value << " ";
                }
            }
            file << std::endl;
        }
        i++;
    }
    file.close();
    spdlog::debug("Finished writing aggregation tree into the log file.");
}

/**
 * Record final simulation results
 */
void Consumer::ResultRecorder(uint32_t iteNum, int timeoutNum, int64_t aveAggTime, int64_t totalTime)
{
    // Open the file using fstream in append mode
    std::ofstream file(result_recorder, std::ios::app);

    if (!file.is_open())
    {
        spdlog::error("Failed to open the file: {}", result_recorder);
        return;
    }

    file << "Consumer's result" << std::endl;
    file << "Total iterations: " << iteNum << std::endl;
    file << "Timeout is triggered for " << timeoutNum << " times" << std::endl;
    file << "Data queue overflow is triggered for " << dataOverflow << " times" << std::endl;
    file << "Nack(upstream interest queue overflow) is triggered for " << nackCount << " times" << std::endl;
    file << "Average aggregation time: " << aveAggTime << " ms." << std::endl;
    file << "Total aggregation time: " << totalTime << " ms." << std::endl;
    file << "-----------------------------------" << std::endl;
}

/**
 * Record qsf congestion info
 * Unit - transferred into ms or pkgs/ms
 */
void Consumer::QsfRecorder(std::string prefix, double qsf)
{
    // Open the file using fstream in append mode
    std::ofstream file(qsf_recorder[prefix], std::ios::app);

    if (!file.is_open())
    {
        spdlog::error("Failed to open the file: {}", qsf_recorder[prefix]);
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

    // time - rate limit - BW - throughput(data arrival rate) - qsf - local queue size - rtt estimation
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime);
    file << now.count() << " "
         << m_rateLimit[prefix] * 1000 << " "
         << m_estimatedBW[prefix] * 1000 << " "
         << GetDataRate(prefix) * 1000 << " "
         << actualQsf << " "
         << static_cast<double>(std::max(interestQueue[prefix].size(), partialAggResult.size())) << " "
         << RTT_estimation_qsf[prefix] / 1000 << " "
         << std::endl;

    file.close();
}

/**
 * Record queue size based CC info
 */
void Consumer::QueueRecorder(std::string prefix, double queueSize)
{
    // Open the file using fstream in append mode
    std::ofstream file(qsNew_recorder[prefix], std::ios::app);

    if (!file.is_open())
    {
        spdlog::error("Failed to open the file: {}", qsNew_recorder[prefix]);
        return;
    }

    // Write the response_time to the file, followed by a newline
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime);
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

// zyx : avoid RTT_estimation_qsf[prefix] == 0
void Consumer::RTTMeasure(std::string prefix, int64_t resTime)
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
    // TODO : need to consider more instead of assigning it 1
    if (RTT_estimation_qsf[prefix] == 0)
    {
        RTT_estimation_qsf[prefix] = 1;
    }
}

/**
 * Get the data rate and return with correct value from the sliding window
 */
double
Consumer::GetDataRate(std::string prefix)
{
    double rawDataRate = m_qsfSlidingWindows[prefix].GetDataArrivalRate();

    // "0": sliding window size is less than one, keep init rate as data arrival rate; "-1" indicates error
    if (rawDataRate == -1)
    {
        spdlog::error("Returned data arrival rate is -1, please check!");
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
void Consumer::BandwidthEstimation(std::string prefix, double qsfUpstream)
{
    auto arrivalTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime);
    spdlog::info("Flow: {} - Arrival time: {}", prefix, arrivalTime.count());
    if (qsfUpstream == -1)
    {
        // Upstream aggregator which connects to producers directly
        double localQueue = static_cast<double>(std::max(interestQueue[prefix].size(), partialAggResult.size()));
        m_qsfSlidingWindows[prefix].AddPacket(arrivalTime, localQueue);
    }
    else
    {
        // Other aggregators
        m_qsfSlidingWindows[prefix].AddPacket(arrivalTime, qsfUpstream);
    }

    double aveQSF = m_qsfSlidingWindows[prefix].GetAverageQsf();
    double dataArrivalRate = GetDataRate(prefix);

    // Correction for qsf and data arrival rate
    if (aveQSF == -1)
    {
        spdlog::info("Returned QSF is -1, please check!");
        std::exit(EXIT_FAILURE);
        return;
    }

    // Update bandwidth estimation
    if (aveQSF > m_qsfQueueThreshold)
    {
        spdlog::info("QSF is larger than threshold, update bandwidth estimation: {} pkgs/ms", dataArrivalRate * 1000);
        m_estimatedBW[prefix] = dataArrivalRate;
    }

    if (dataArrivalRate > m_estimatedBW[prefix])
    {
        m_estimatedBW[prefix] = dataArrivalRate;
    }

    spdlog::info("Flow: {} - Average QSF: {}, Arrival Rate: {} pkgs/ms, Bandwidth estimation: {} pkgs/ms",
                 prefix, aveQSF, dataArrivalRate * 1000, m_estimatedBW[prefix] * 1000);
}

/**
 * Update each flow's rate limit.
 */
void Consumer::RateLimitUpdate(std::string prefix)
{
    double qsf = m_qsfSlidingWindows[prefix].GetAverageQsf();
    spdlog::debug("Flow {} - qsf: {}", prefix, qsf);

    // Congestion control
    if (qsf > 2 * m_qsfQueueThreshold)
    {
        m_rateLimit[prefix] = m_estimatedBW[prefix] * m_qsfMDFactor;
        spdlog::debug("Congestion detected. Update rate limit: {} pkgs/ms", m_rateLimit[prefix] * 1000);
    }
    else
    {
        m_rateLimit[prefix] = m_estimatedBW[prefix];
        spdlog::debug("No congestion. Update rate limit by estimated BW: {} pkgs/ms", m_rateLimit[prefix] * 1000);
    }

    // Rate probing
    if (qsf < m_qsfQueueThreshold)
    {
        m_rateLimit[prefix] = m_rateLimit[prefix] * m_qsfRPFactor;
        spdlog::debug("Start rate probing. Updated rate limit: {} pkgs/ms", m_rateLimit[prefix] * 1000);
    }

    // Error handling
    if (RTT_estimation_qsf[prefix] == 0)
    {
        spdlog::error("RTT estimation is 0, please check!");
        std::exit(EXIT_FAILURE);
        return;
    }

    spdlog::debug("Flow {} - Schedule next rate limit update after {} ms", prefix, RTT_estimation_qsf[prefix] / 1000);

    m_rateEvent[prefix] = m_scheduler.schedule(ndn::time::microseconds(RTT_estimation_qsf[prefix]), [this, prefix]
                                               { this->RateLimitUpdate(prefix); });
}
