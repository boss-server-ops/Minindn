#include "ndn-consumer.hpp"
#include "ModelData.hpp"
#include "algorithm/utility/utility.hpp"
#include "algorithm/include/AggregationTree.hpp"

Consumer::Consumer()
    : m_interestName("/example/testApp"),
      m_seq(0),
      m_seqMax(100),
      m_interestLifeTime(std::chrono::milliseconds(2000)),
      m_rand(std::random_device{}()),                     // Use std::random_device to generate random seed
      m_uniformDist(0.0, 1.0),                            // Initialize uniform distribution, range [0.0, 1.0]
      m_rtt(std::make_unique<ndn::util::RttEstimator>()), // Initialize RTT estimator
      suspiciousPacketCount(0),
      totalInterestThroughput(0),
      totalDataThroughput(0),
      numChild(0),
      globalSeq(0),
      globalRound(0),
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
}

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
    globalSeq++;
}

void Consumer::ConstructAggregationTree()
{
    // Call the base class's ConstructAggregationTree method
    App::ConstructAggregationTree();

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
        throw std::runtime_error("Fail to construct aggregation tree!");
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
                broadcastList.push_back(pair.first);
            }
            // 2. Consumer
            // Initialize "globalTreeRound" for all rounds (if there're multiple sub-trees)
            else
            {
                // Specify the number of consumer's child nodes (links)
                if (numChild == 0)
                {
                    numChild = pair.second.size();
                    spdlog::info("The number of child nodes: {}", numChild);
                }

                std::vector<std::string> leavesRound;
                spdlog::info("Round {} has the following leaf nodes: ", i);
                for (const auto &leaves : pair.second)
                {
                    leavesRound.push_back(leaves);
                    spdlog::info("{} ", leaves);
                }
                globalTreeRound.push_back(leavesRound);
            }
        }
        spdlog::info("----"); // Separator between maps
        i++;
    }

    // Initialize variables for RTO computation/congestion control
    for (int i = 0; i < globalTreeRound.size(); i++)
    {
        initRTO[i] = false;
        RTO_Timer[i] = 6 * m_retxTimer;
        m_timeoutThreshold[i] = 6 * m_retxTimer;
        RTT_threshold[i] = 0;
        RTT_count[i] = 0;
    }
}

void Consumer::StartApplication()
{
    // Call the base class's StartApplication method
    App::StartApplication();

    // Call RTORecorder method
    RTORecorder();

    // Construct the aggregation tree
    ConstructAggregationTree();

    // Broadcast the aggregation tree
    TreeBroadcast();

    // Start the interest generator
    InterestGenerator();

    // Schedule the next packet
    ScheduleNextPacket();
}

void Consumer::StopApplication() // Called at time specified by Stop
{
    spdlog::info("Stopping Consumer application");
    // ps: deleted cancel event
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

void Consumer::aggregate(const ModelData &data, const uint32_t &seq)
{
    // first initialization
    if (sumParameters.find(seq) == sumParameters.end())
    {
        sumParameters[seq] = std::vector<float>(300, 0.0f);
    }

    // Aggregate data
    std::transform(sumParameters[seq].begin(), sumParameters[seq].end(), data.parameters.begin(),
                   sumParameters[seq].begin(), std::plus<float>());
}

std::vector<float> Consumer::getMean(const uint32_t &seq)
{
    std::vector<float> result;
    if (sumParameters[seq].empty() || producerCount == 0)
    {
        spdlog::error("Error when calculating average model, please check!");
        return result;
    }

    for (auto value : sumParameters[seq])
    {
        result.push_back(value / static_cast<float>(producerCount));
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

    return totalAggregateTime / iterationCount;
}

// void Consumer::OnNack(const ndn::lp::Nack &nack)
// {
//     App::OnNack(nack);

//     spdlog::info("NACK received for: {}, reason: {}", nack.getInterest().getName().toUri(), nack.getReason());
// }

// void Consumer::OnTimeout(std::string nameString)
// {
//     std::shared_ptr<ndn::Name> name = std::make_shared<ndn::Name>(nameString);
//     SendInterest(name);

//     // Add one to "suspiciousPacketCount"
//     suspiciousPacketCount++;
// }

void Consumer::OnData(const ndn::Interest &interest, const ndn::Data &data)
{

    if (!m_active)
        return;

    App::OnData(interest, data);
    std::string type = data.getName().get(-2).toUri();
    uint32_t seq = data.getName().at(-1).toSequenceNumber();
    std::string dataName = data.getName().toUri();
    int dataSize = data.wireEncode().size();
    ECNRemote = false;
    ECNLocal = false;
    isWindowDecreaseSuppressed = false;
    spdlog::info("Received content object: {}", dataName);

    // Record data throughput
    totalDataThroughput += dataSize;
    spdlog::debug("The incoming data packet size is: {}", dataSize);

    // Erase timeout
    if (m_timeoutCheck.find(dataName) != m_timeoutCheck.end())
        m_timeoutCheck.erase(dataName);
    else
        spdlog::debug("Suspicious data packet, not exists in timeout list.");

    if (type == "data")
    {
        std::string name_sec0 = data.getName().get(0).toUri();

        // Perform data name matching with interest name
        ModelData modelData;
        auto data_map = map_agg_oldSeq_newName.find(seq);
        auto data_agg = m_agg_newDataName.find(seq);
        if (data_map != map_agg_oldSeq_newName.end() && data_agg != m_agg_newDataName.end())
        {

            // Response time computation (RTT)
            if (startTime.find(dataName) != startTime.end())
            {
                auto now = std::chrono::steady_clock::now();
                responseTime[dataName] = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch() - startTime[dataName]);
                ResponseTimeSum(responseTime[dataName].count());
                startTime.erase(dataName);
                spdlog::info("Consumer's response time of sequence {} is: {} ms", dataName, responseTime[dataName].count());
            }

            // Search round index
            int roundIndex = findRoundIndex(name_sec0);
            if (roundIndex == -1)
            {
                spdlog::debug("Error on roundIndex!");
                throw std::runtime_error("Error on roundIndex!");
            }
            spdlog::debug("This packet comes from round {}", roundIndex);

            // RTT_threshold measurement initialization is done after 3 iterations, before that, don't perform cwnd control
            if (RTT_count[roundIndex] >= globalTreeRound[roundIndex].size() * 3 && responseTime[dataName].count() > RTT_threshold[roundIndex])
            {
                ECNLocal = true;
            }

            // Setup RTT_threshold based on RTT of the first 5 iterations, then update RTT_threshold after each new iteration based on EWMA
            RTTThresholdMeasure(responseTime[dataName].count(), roundIndex);

            // Record response time
            ResponseTimeRecorder(responseTime[dataName], seq, ECNLocal, RTT_measurement[roundIndex], RTT_threshold[roundIndex]);

            // Reset RetxTimer and timeout interval
            RTO_Timer[roundIndex] = RTOMeasurement(responseTime[dataName].count(), roundIndex);
            m_timeoutThreshold[roundIndex] = RTO_Timer[roundIndex];
            spdlog::debug("responseTime for name : {} is: {} ms", dataName, responseTime[dataName].count());
            spdlog::debug("Current RTO measurement: {} ms", RTO_Timer[roundIndex].count());

            // This data exist in the map, perform aggregation
            auto &aggVec = data_agg->second;
            auto aggVecIt = std::find(aggVec.begin(), aggVec.end(), name_sec0);

            std::vector<uint8_t> oldbuffer(data.getContent().value(), data.getContent().value() + data.getContent().value_size());

            if (deserializeModelData(oldbuffer, modelData) && aggVecIt != aggVec.end())
            {
                aggregate(modelData, seq); // Aggregate data payload
                ECNRemote = !modelData.congestedNodes.empty();
                aggVec.erase(aggVecIt);
            }
            else
            {
                spdlog::info("Data name doesn't exist in map_agg_oldSeq_newName, meaning this data packet is duplicate, do nothing!");
                return;
            }

            // Judge whether the aggregation iteration has finished
            if (aggVec.empty())
            {
                spdlog::debug("Aggregation of iteration {} finished!", seq);

                // Get aggregation result and store them
                aggregationResult[seq] = getMean(seq);

                // Update new elements for interestQueue if necessary
                if (interestQueue.size() < m_interestQueue && globalSeq <= m_iteNum)
                {
                    InterestGenerator();
                }

                // Calculate aggregation time
                if (aggregateStartTime.find(seq) != aggregateStartTime.end())
                {
                    auto now = std::chrono::steady_clock::now();
                    aggregateTime[seq] = std::chrono::duration_cast<std::chrono::milliseconds>(now - std::chrono::steady_clock::time_point(aggregateStartTime[seq]));
                    AggregateTimeSum(aggregateTime[seq].count());
                    spdlog::debug("Iteration {} aggregation time is: {} ms", seq, aggregateTime[seq].count());
                    aggregateStartTime.erase(seq);
                }
                else
                {
                    spdlog::debug("Error when calculating aggregation time, no reference found for seq {}", seq);
                }

                // Record aggregation time
                AggregateTimeRecorder(aggregateTime[seq]);
            }

            // Stop simulation
            if (iterationCount == m_iteNum)
            {
                spdlog::debug("Reach {} iterations, stop!", m_iteNum);
                spdlog::info("Timeout is triggered {} times.", suspiciousPacketCount);
                spdlog::info("Total interest throughput is: {} bytes.", totalInterestThroughput);
                spdlog::info("Total data throughput is: {} bytes.", totalDataThroughput);
                spdlog::info("The average aggregation time of Consumer in {} iteration is: {} ms", iterationCount, GetAggregateTimeAverage());

                // Record throughput into file
                ThroughputRecorder(totalInterestThroughput, totalDataThroughput);

                // Stop simulation
                throw std::runtime_error("Simulation stopped after reaching iteration limit.");
            }
        }
        else
        {
            spdlog::debug("Suspicious data packet, not exist in data map.");
            throw std::runtime_error("Suspicious data packet, not exist in data map.");
        }
    }
    else if (type == "initialization")
    {
        std::string destNode = data.getName().get(0).toUri();

        // Update synchronization info
        auto it = std::find(broadcastList.begin(), broadcastList.end(), destNode);
        if (it != broadcastList.end())
        {
            broadcastList.erase(it);
            spdlog::debug("Node {} has received aggregationTree map, erase it from broadcastList", destNode);
        }

        // Tree broadcasting synchronization is done
        if (broadcastList.empty())
        {
            broadcastSync = true;
            spdlog::debug("Synchronization of tree broadcasting finished!");
        }
    }
}

void Consumer::OnNack(const ndn::Interest &interest, const ndn::lp::Nack &nack)
{
    App::OnNack(interest, nack);
}

void Consumer::OnTimeout(const ndn::Interest &interest)
{
    App::OnTimeout(interest);
    auto newInterest = std::make_shared<ndn::Name>(interest.getName());
    SendInterest(newInterest);
    suspiciousPacketCount++;
}

void Consumer::SetRetxTimer(std::chrono::milliseconds retxTimer)
{
    // ps:deleted schedule event
}

std::chrono::milliseconds Consumer::GetRetxTimer() const
{
    return m_retxTimer;
}

void Consumer::CheckRetxTimeout()
{
    // ps:deleted schedule event and have different ontimeout
}

std::chrono::milliseconds Consumer::RTOMeasurement(int64_t resTime, int roundIndex)
{
    if (!initRTO[roundIndex])
    {
        RTTVAR[roundIndex] = resTime / 2;
        SRTT[roundIndex] = resTime;
        spdlog::debug("Initialize RTO for round: {}", roundIndex);
        initRTO[roundIndex] = true;
    }
    else
    {
        RTTVAR[roundIndex] =
            0.75 * RTTVAR[roundIndex] + 0.25 * std::abs(
                                                   SRTT[roundIndex] - resTime); // RTTVAR = (1 - b) * RTTVAR + b * |SRTT - RTTsample|, where b = 0.25
        SRTT[roundIndex] = 0.875 * SRTT[roundIndex] + 0.125 * resTime;          // SRTT = (1 - a) * SRTT + a * RTTsample, where a = 0.125
    }
    int64_t RTO = SRTT[roundIndex] + 4 * RTTVAR[roundIndex]; // RTO = SRTT + K * RTTVAR, where K = 4

    return std::chrono::milliseconds(2 * RTO);
}

void Consumer::SendPacket()
{
    if (!interestQueue.empty())
    {
        auto interestTuple = interestQueue.front();
        interestQueue.pop();
        uint32_t iteration = std::get<0>(interestTuple);
        bool isNewIteration = std::get<1>(interestTuple);
        std::shared_ptr<ndn::Name> name = std::get<2>(interestTuple);

        // New iteration, start compute aggregation time
        if (isNewIteration)
        {
            auto now = std::chrono::steady_clock::now();
            aggregateStartTime[iteration] = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
        }
        SendInterest(name);
        ScheduleNextPacket();
    }
    else
    {
        spdlog::info("All available interests have been sent, no further operation required.");
    }
}

void Consumer::InterestGenerator()
{
    // Generate name from section 1 to 3 (except for seq)
    if (map_round_nameSec1_3.empty())
    {
        std::vector<std::string> objectProducer;
        std::string token;
        std::istringstream tokenStream(proList);
        char delimiter = '.';
        while (std::getline(tokenStream, token, delimiter))
        {
            objectProducer.push_back(token);
        }

        int i = 0;
        for (const auto &aggTree : aggregationTree)
        {
            auto initialAllocation = getLeafNodes(m_nodeprefix, aggTree);
            std::vector<std::string> roundChild;
            std::vector<std::string> vec_sec1_3;

            for (const auto &[child, leaves] : initialAllocation)
            {
                std::string name_sec1_3;
                std::string name_sec1;
                roundChild.push_back(child);

                for (const auto &leaf : leaves)
                {
                    name_sec1 += leaf + ".";
                }
                name_sec1.resize(name_sec1.size() - 1);
                name_sec1_3 = "/" + child + "/" + name_sec1 + "/data";
                vec_sec1_3.push_back(name_sec1_3);

                vec_iteration.push_back(child); // Iteration's vector
            }
            vec_round.push_back(roundChild);      // Round's vector
            map_round_nameSec1_3[i] = vec_sec1_3; // Name (section1 - section3), to be pushed into queue later
            i++;
        }
    }

    // Generate entire interest name for all iterations
    while (interestQueue.size() <= m_interestQueue)
    {
        // Generate new interest for upcoming iteration if necessary
        if (globalSeq <= m_iteNum)
        {
            map_agg_oldSeq_newName[globalSeq] = vec_round;
            m_agg_newDataName[globalSeq] = vec_iteration;

            bool isNewIteration = true;

            for (const auto &map : map_round_nameSec1_3)
            {
                for (const auto &name1_3 : map.second)
                {
                    std::shared_ptr<ndn::Name> name = std::make_shared<ndn::Name>(name1_3);
                    name->appendSequenceNumber(globalSeq);
                    interestQueue.push(std::make_tuple(globalSeq, isNewIteration, name));

                    isNewIteration = false;
                }
            }

            globalSeq++;
        }
        else
        {
            spdlog::info("All iterations' interests have been generated, no need for further operation.");
            break;
        }
    }
}

void Consumer::SendInterest(std::shared_ptr<ndn::Name> newName)
{
    if (!m_active)
        return;

    std::string nameWithSeq = newName->toUri();

    // Trace timeout
    auto now = std::chrono::steady_clock::now();
    m_timeoutCheck[nameWithSeq] = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());

    // Start response time
    if (startTime.find(nameWithSeq) == startTime.end())
        startTime[nameWithSeq] = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());

    std::shared_ptr<ndn::Interest> interest = std::make_shared<ndn::Interest>();
    interest->setNonce(m_uniformDist(m_rand));
    interest->setName(*newName);
    interest->setCanBePrefix(false);
    interest->setInterestLifetime(ndn::time::milliseconds(m_interestLifeTime.count()));
    spdlog::info("Sending interest >>>> {}", nameWithSeq);
    m_face->expressInterest(*interest,
                            std::bind(&Consumer::OnData, this, _1, _2),
                            std::bind(&Consumer::OnNack, this, _1, _2),
                            std::bind(&Consumer::OnTimeout, this, _1));

    // Record interest throughput
    // Actual interests sending and retransmission are recorded as well
    int interestSize = interest->wireEncode().size();
    totalInterestThroughput += interestSize;
    spdlog::debug("Interest size: {}", interestSize);
}

void Consumer::RTTThresholdMeasure(int64_t responseTime, int index)
{
    int aggregationSize = globalTreeRound[index].size();
    if (RTT_count[index] == 0)
    {
        RTT_measurement[index] = responseTime;
    }
    else
    {
        RTT_measurement[index] =
            m_EWMAFactor * responseTime + (1 - m_EWMAFactor) * RTT_measurement[index];
        RTT_threshold[index] = m_thresholdFactor * RTT_measurement[index];
    }

    if (RTT_count[index] >= aggregationSize * 3) // Whether it's larger than 3 iterations
    {
        spdlog::info("Apply RTT_threshold, current value is: {}", RTT_threshold[index]);
    }
    RTT_count[index]++;
}

void Consumer::RTORecorder()
{
    // Open the file using fstream in append mode
    std::ofstream file(RTO_recorder, std::ios::app);

    if (!file.is_open())
    {
        spdlog::error("Failed to open the file: {}", RTO_recorder);
        return;
    }

    // Write the response_time to the file, followed by a newline
    auto now = std::chrono::steady_clock::now();
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    file << now_ms;
    for (const auto &timer : RTO_Timer)
    {
        file << " " << timer.second.count();
    }
    file << std::endl;

    // Close the file
    file.close();

    // Schedule the next call to RTORecorder
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    RTORecorder();
}

void Consumer::ResponseTimeRecorder(std::chrono::milliseconds responseTime, uint32_t seq, bool ECN, int64_t threshold_measure, int64_t threshold_actual)
{

    std::ofstream file(responseTime_recorder, std::ios::app);

    if (!file.is_open())
    {
        spdlog::error("Failed to open the file: {}", responseTime_recorder);
        return;
    }

    auto now = std::chrono::steady_clock::now();
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    file << now_ms << " " << seq << " " << ECN << " " << threshold_actual << " " << responseTime.count() << std::endl;
    file.close();
}

void Consumer::AggregateTimeRecorder(std::chrono::milliseconds aggregateTime)
{
    // Open the file using fstream in append mode
    std::ofstream file(aggregateTime_recorder, std::ios::app);

    if (!file.is_open())
    {
        spdlog::error("Failed to open the file: {}", aggregateTime_recorder);
        return;
    }

    // Write aggregation time to file, followed by a new line
    auto now = std::chrono::steady_clock::now();
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    file << now_ms << " " << aggregateTime.count() << std::endl;

    file.close();
}

void Consumer::InitializeLogFile()
{
    // Open the file and clear all contents for all log files
    OpenFile(RTO_recorder);
    OpenFile(responseTime_recorder);
    OpenFile(aggregateTime_recorder);
    OpenFile(throughput_recorder);
}

bool Consumer::CanDecreaseWindow(int64_t threshold)
{
    auto now = std::chrono::steady_clock::now();
    auto lastDecrease_ms = lastWindowDecreaseTime.count();
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    if (now_ms - lastDecrease_ms >= threshold)
    {
        spdlog::debug("Window decrease is allowed.");
        return true;
    }
    else
    {
        spdlog::debug("Window decrease is suppressed.");
        return false;
    }
}

void Consumer::ThroughputRecorder(int interestThroughput, int dataThroughput)
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
    auto now_s = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    file << interestThroughput << " " << dataThroughput << " " << now_s << std::endl;

    file.close();
}