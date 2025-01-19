#include "ndn-consumer-INA.hpp"

ConsumerINA::ConsumerINA()
{

    // m_logger = spdlog::basic_logger_mt("consumer_logger", "logs/consumer.log");
    // spdlog::set_default_logger(m_logger);
    spdlog::info("ConsumerINA initialized");
}

void ConsumerINA::SendInterest(std::shared_ptr<ndn::Name> newName)
{
    // Get the prefix of the interest, which is the flow name
    std::string flow = newName->get(0).toUri();
    // Record inFlight for congestion control
    // m_inFlight[flow]++;
    Consumer::SendInterest(newName);
}
// TODO:delete comment later
//  void ConsumerINA::ScheduleNextPacket(std::string prefix)
//  {
//      // ps:deleted schedule and imported the thread
//      if (interestQueue.find(prefix) == interestQueue.end())
//      {
//          spdlog::debug("Flow {} is not found in the interest queue.", prefix);
//          std::exit(EXIT_FAILURE);
//          return;
//      }
//      //? Check whether interest queue is null, if so, split new interests...
//      // Interest splitting
//      if (interestQueue[prefix].empty())
//      {
//          // Reach the last iteration, stop scheduling new packets for current flow
//          if (globalSeq == m_iteNum)
//          {
//              spdlog::info("All iterations have been finished, no need to schedule new interests.");
//              return;
//          }

//         // Check whether interest queue is full
//         if (!InterestSplitting())
//         {
//             //? Fail to split new interests, schedule this flow later
//             spdlog::debug("Other flows' queue is full, schedule this flow later.");
//         }
//         else
//         {
//             if (m_sendEvent[prefix])
//             {
//                 m_sendEvent[prefix].cancel();
//                 m_sendEvent[prefix].reset();
//                 NS_LOG_DEBUG("Suspicious, remove the previous event.");
//             }
//             m_sendEvent[prefix] = Simulator::ScheduleNow(&Consumer::SendPacket, this, prefix);
//         }
//     }
//     else
//     {
//         if (m_sendEvent[prefix])
//         {
//             m_sendEvent[prefix].cancel();
//             m_sendEvent[prefix].reset();
//             NS_LOG_DEBUG("Suspicious, remove the previous event.");
//         }
//         // TODO: waiting for modification
//         m_sendEvent[prefix] = Simulator::ScheduleNow(&Consumer::SendPacket, this, prefix);
//     }
//     // Schdule next scheduling event
//     double nextTime = 1 / m_rateLimit[prefix]; // Unit: us
//     NS_LOG_INFO("Flow " << prefix << " -> Schedule next sending event after " << nextTime / 1000 << " ms.");
//     // TODO:wating for modification
//     m_scheduleEvent[prefix] = Simulator::Schedule(MicroSeconds(nextTime), &ConsumerINA::ScheduleNextPacket, this, prefix);
// }

void ConsumerINA::StartApplication()
{
    Consumer::StartApplication();
}

// void ConsumerINA::OnData(const ndn::Interest &interest, const ndn::Data &data)
// {
//     Consumer::OnData(interest, data);

//     // std::string dataName = data.getName().toUri();
//     // uint64_t sequenceNum = data.getName().get(-1).toSequenceNumber();
//     // std::string type = data.getName().get(-2).toUri();
//     // std::string name_sec0 = data.getName().get(0).toUri();

//     // // Set highest received Data to sequence number
//     // if (m_highData < sequenceNum)
//     // {
//     //     m_highData = sequenceNum;
//     // }

//     // // Only perform congestion control for those type is "data", disable this function for "initialization" type
//     // if (type == "data")
//     // {
//     //     // Get current packet's round index
//     //     int roundIndex = Consumer::findRoundIndex(name_sec0);
//     //     if (roundIndex == -1)
//     //     {
//     //         spdlog::debug("Error on roundIndex!");
//     //         std::terminate();
//     //     }
//     //     spdlog::debug("This packet comes from round {}", roundIndex);

//     //     // Perform congestion control
//     //     // Priority
//     //     // 1. Whether broadcasting("initialization") has finished
//     //     // 2. PCON's ECN mark - not enable now
//     //     // 3. Based on congestion signal, perform congestion/rate control
//     //     // 4. CWA algorithm control whether window decrease should be performed
//     //     if (!broadcastSync)
//     //     {
//     //         spdlog::info("Currently broadcasting aggregation tree, ignore relevant cwnd/congestion management");
//     //     }
//     //     /*        else if (data.getCongestionMark() > 0) {
//     //                 if (m_reactToCongestionMarks) {
//     //                     spdlog::debug("Received congestion mark: {}", data.getCongestionMark());
//     //                     WindowDecrease("ConsumerCongestion");
//     //                 }
//     //                 else {
//     //                     spdlog::debug("Ignored received congestion mark: {}", data.getCongestionMark());
//     //                 }
//     //             }*/
//     //     else if (ECNLocal)
//     //     {
//     //         // Whether CWA is enabled
//     //         if (m_useCwa && !CanDecreaseWindow(RTT_measurement[roundIndex]))
//     //         {
//     //             isWindowDecreaseSuppressed = true;
//     //             spdlog::info("Window decrease is suppressed.");
//     //         }
//     //         else
//     //         {
//     //             spdlog::info("Congestion signal exists in consumer!");
//     //             WindowDecrease("ConsumerCongestion");
//     //         }
//     //     }
//     //     /*        else if (ECNRemote) {
//     //                 spdlog::info("Congestion signal exists in aggregator!");
//     //                 WindowDecrease("AggregatorCongestion");
//     //             }*/
//     //     else
//     //     {
//     //         spdlog::info("No congestion, increase the cwnd.");
//     //         WindowIncrease();
//     //     }

//     //     // Record the last flag in in RTT's log
//     //     ResponseTimeRecorder(isWindowDecreaseSuppressed);
//     // }

//     // if (m_inFlight > static_cast<uint32_t>(0))
//     // {
//     //     m_inFlight--;
//     // }

//     // spdlog::debug("Window: {}, InFlight: {}", m_window, m_inFlight);

//     // // Record cwnd
//     // WindowRecorder();

//     // ScheduleNextPacket();
// }

// void ConsumerINA::OnTimeout(const ndn::Interest &interest)
// {
//     // WindowDecrease("timeout");

//     // if (m_inFlight > static_cast<uint32_t>(0))
//     // {
//     //     m_inFlight--;
//     // }

//     // spdlog::debug("Window: {}, InFlight: {}", m_window, m_inFlight);

//     Consumer::OnTimeout(interest);
// }

// void ConsumerINA::SetWindow(uint32_t window)
// {
//     m_initialWindow = window;
//     // m_window = m_initialWindow;
// }

// uint32_t ConsumerINA::GetWindow() const
// {
//     return m_initialWindow;
// }

// void ConsumerINA::WindowIncrease(std::string prefix)
// {
//     if (m_ccAlgorithm == CcAlgorithm::AIMD)
//     {
//         // If cwnd is larger than 8, check whether current bottleneck is because of downstream slow interest, if so, stop increasing cwnd
//         /*         if (m_window[prefix] > 8.0 && m_window[prefix] - m_inFlight[prefix] > 30.0){
//                     NS_LOG_DEBUG("Current bottleneck is downstream slow interest, stop increasing cwnd.");
//                 } else  */
//         if (m_useWIS)
//         {
//             if (m_window[prefix] < m_ssthresh[prefix])
//             {
//                 m_window[prefix] += 1.0;
//             }
//             else
//             {
//                 m_window[prefix] += (1.0 / m_window[prefix]);
//             }
//             spdlog::debug("Window size of flow '{}' is increased to {}", prefix, m_window[prefix]);
//         }
//         else
//         {
//             m_window[prefix] += 1.0;
//             spdlog::debug("Window size of flow '{}' is increased to {}", prefix, m_window[prefix]);
//         }
//     }
//     else if (m_ccAlgorithm == CcAlgorithm::CUBIC)
//     {
//         CubicIncerase(prefix);
//     }
//     else
//     {
//         NS_LOG_DEBUG("CC alogrithm can't be recognized, please check!");
//         std::exit(EXIT_FAILURE);
//     }
// }

// void ConsumerINA::WindowDecrease(std::string prefix, std::string type)
// {

//     // Track last window decrease time
//     auto now = std::chrono::steady_clock::now();
//     lastWindowDecreaseTime[prefix] = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime);

//     // AIMD for timeout

//     if (m_ccAlgorithm == CcAlgorithm::AIMD)
//     {
//         if (type == "timeout")
//         {
//             m_ssthresh[prefix] = m_window[prefix] * m_alpha;
//             m_window[prefix] = m_ssthresh[prefix];
//         }
//         else if (type == "nack")
//         {
//             m_ssthresh[prefix] = m_window[prefix] * m_alpha;
//             m_window[prefix] = m_ssthresh[prefix];
//         }
//         else if (type == "ConsumerCongestion")
//         {
//             m_ssthresh[prefix] = m_window[prefix] * m_beta;
//             m_window[prefix] = m_ssthresh[prefix];
//         }
//         else if (type == "RemoteCongestion")
//         {
//             m_ssthresh[prefix] = m_window[prefix] * m_gamma;
//             m_window[prefix] = m_ssthresh[prefix];
//         }
//         else
//         {
//             spdlog::info("Unknown congestion type, please check!");
//             std::exit(EXIT_FAILURE);
//         }
//     }
//     else if (m_ccAlgorithm == CcAlgorithm::CUBIC)
//     {
//         if (type == "timeout")
//         {
//             m_ssthresh[prefix] = m_window[prefix] * m_alpha;
//             m_window[prefix] = m_ssthresh[prefix];
//         }
//         else if (type == "nack")
//         {
//             m_ssthresh[prefix] = m_window[prefix] * m_alpha;
//             m_window[prefix] = m_ssthresh[prefix];
//         }
//         else if (type == "ConsumerCongestion")
//         {
//             CubicDecrease(prefix, type);
//         }
//         else if (type == "RemoteCongestion")
//         {
//             // Do nothing, currently disabled
//         }
//         else
//         {
//             spdlog::info("Unknown congestion type, please check!");
//             std::exit(EXIT_FAILURE);
//         }
//     }
//     else
//     {
//         spdlog::debug("CC alogrithm can't be recognized, please check!");
//         std::exit(EXIT_FAILURE);
//     }

//     // Window size can't be reduced below 1
//     if (m_window[prefix] < m_minWindow)
//     {
//         m_window[prefix] = m_minWindow;
//     }

//     spdlog::debug("Flow: {}. Window size decreased to {}. Reason: {}", prefix, m_window[prefix], type);
// }

// /**
//    + * Cubic increase
//    + * @param prefix Flow name
//    + */
// void ConsumerINA::CubicIncerase(std::string prefix)
// {
//     // 1. Time since last congestion event in Seconds

//     // TODO: Check if t is correct
//     auto now = std::chrono::steady_clock::now();
//     const double t = (std::chrono::duration_cast<std::chrono::microseconds>(now - startTime).count() - std::chrono::duration_cast<std::chrono::microseconds>(lastWindowDecreaseTime[prefix]).count()) / 1e6;
//     NS_LOG_DEBUG("Time since last congestion event: " << t);
//     // 2. Time it takes to increase the window to cubic_wmax
//     // K = cubic_root(W_max*(1-beta_cubic)/C) (Eq. 2)
//     const double k = std::cbrt(m_cubicWmax[prefix] * (1 - m_cubicBeta) / m_cubic_c);
//     NS_LOG_DEBUG("K value: " << k);
//     // 3. Target: W_cubic(t) = C*(t-K)^3 + W_max (Eq. 1)
//     const double w_cubic = m_cubic_c * std::pow(t - k, 3) + m_cubicWmax[prefix];
//     NS_LOG_DEBUG("Cubic increase target: " << w_cubic);
//     // 4. Estimate of Reno Increase (Currently Disabled)
//     //  const double rtt = m_rtt->GetCurrentEstimate().GetSeconds();
//     //  const double w_est = m_cubic_wmax*m_beta + (3*(1-m_beta)/(1+m_beta)) * (t/rtt);
//     //* TCP-friendly region, need to be disabled for ICN, "w_est" is not needed
//     // constexpr double w_est = 0.0;
//     //! Original cubic increase
//     /*     if (m_cubicWmax[prefix] <= 0) {
//     +        NS_LOG_DEBUG("Error! Wmax is less than 0, check cubic increase!");
//     +        Simulator::Stop();
//     +    }
//     +
//     +    double cubic_increment = std::max(w_cubic, 0.0) - m_window[prefix];
//     +    // Cubic increment must be positive:
//     +    // Note: This change is not part of the RFC, but I added it to improve performance.
//     +    if (cubic_increment < 0) {
//     +        cubic_increment = 0.0;
//     +    NS_LOG_DEBUG("Cubic increment: " << cubic_increment);
//     +    m_window[prefix] += cubic_increment / m_window[prefix]; */
//     //! Customized cubic increase
//     if (m_window[prefix] < m_ssthresh[prefix])
//     {
//         m_window[prefix] += 1.0;
//     }
//     else
//     {
//         if (m_cubicWmax[prefix] <= 0)
//         {
//             NS_LOG_DEBUG("Error! Wmax is less than 0, check cubic increase!");
//             std::exit(EXIT_FAILURE);
//         }
//         double cubic_increment = std::max(w_cubic, 0.0) - m_window[prefix];
//         // Cubic increment must be positive:
//         // Note: This change is not part of the RFC, but I added it to improve performance.
//         if (cubic_increment < 0)
//         {
//             cubic_increment = 0.0;
//         }
//         NS_LOG_DEBUG("Cubic increment: " << cubic_increment);
//         m_window[prefix] += cubic_increment / m_window[prefix];
//     }
//     spdlog::debug("Window size of flow '{}' is increased to {}", prefix, m_window[prefix]);
// }

// void ConsumerINA::CubicDecrease(std::string prefix, std::string type)
// {
//     //! Traditional cubic window decrease
//     m_cubicWmax[prefix] = m_window[prefix];
//     m_ssthresh[prefix] = m_window[prefix] * m_cubicBeta;
//     m_ssthresh[prefix] = std::max<double>(m_ssthresh[prefix], m_minWindow);
//     m_window[prefix] = m_window[prefix] * m_cubicBeta;

//     //! Cubic with fast convergence
//     /*     const double FAST_CONV_DIFF = 1.0; // In percent
//         // Current w_max < last_wmax
//         if (m_useCubicFastConv && m_window < m_cubicLastWmax * (1 - FAST_CONV_DIFF / 100)) {
//             m_cubicLastWmax = m_window;
//             m_cubicWmax = m_window * (1.0 + m_cubicBeta) / 2.0;
//         }
//         else {
//             // Save old cwnd as w_max:
//             m_cubicLastWmax = m_window;
//             m_cubicWmax = m_window;
//          }

//         m_ssthresh = m_window * m_cubicBeta;
//         m_ssthresh = std::max<double>(m_ssthresh, m_initialWindow);
//         m_window = m_ssthresh;

//         m_cubicLastDecrease = time::steady_clock::now(); */
// }

// void ConsumerINA::WindowRecorder(std::string prefix)
// {
//     // Open file; on first call, truncate it to delete old content
//     std::ofstream file(windowRecorder[prefix], std::ios::app);

//     if (!file.is_open())
//     {
//         std::cerr << "Failed to open the file: " << windowRecorder[prefix] << std::endl;
//         return;
//     }

//     // Get current time in milliseconds
//     auto now = std::chrono::steady_clock::now();
//     auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();

//     file << now_ms << " " << m_window[prefix] << " " << m_ssthresh[prefix] << " " << interestQueue[prefix].size() << std::endl;

//     file.close();
// }

// void ConsumerINA::ResponseTimeRecorder(std::string prefix, bool flag)
// {
//     // Open the file using fstream in append mode
//     std::ofstream file(responseTime_recorder[prefix], std::ios::app);

//     if (!file.is_open())
//     {

//         spdlog::error("Failed to open the file: {}", responseTime_recorder[prefix]);
//         return;
//     }

//     // Write the response_time to the file, followed by a newline
//     file << " " << flag << std::endl;

//     // Close the file
//     file.close();
// }
void ConsumerINA::InitializeLogFile()
{

    Consumer::InitializeLogFile();
}

/**
 * Initialize parameters
 */
void ConsumerINA::InitializeParameter()
{
    Consumer::InitializeParameter();

    // Initialize cwnd
    for (const auto &round : globalTreeRound)
    {
        for (const auto &flow : round)
        {
            m_window[flow] = m_initialWindow;
            m_inFlight[flow] = 0;
            m_ssthresh[flow] = std::numeric_limits<double>::max();

            // Initialize CUBIC factor
            m_cubicLastWmax[flow] = m_initialWindow;
            m_cubicWmax[flow] = m_initialWindow;
            auto now = std::chrono::steady_clock::now();
            lastWindowDecreaseTime[flow] = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime);
        }
    }
}

void ConsumerINA::OnData(const ndn::Interest &interest, const ndn::Data &data)
{
    spdlog::info("ConsumerINA received data");
    // 占位实现
}

void ConsumerINA::OnNack(const ndn::Interest &interest, const ndn::lp::Nack &nack)
{
    // 占位实现
    spdlog::info("ConsumerINA received nack");
}

void ConsumerINA::OnTimeout(const ndn::Interest &interest)
{
    // 占位实现
    spdlog::info("ConsumerINA received timeout");
}

void ConsumerINA::SetWindow(uint32_t window)
{
    // 占位实现
}

uint32_t ConsumerINA::GetWindow() const
{
    // 占位实现
    return 0;
}

void ConsumerINA::WindowIncrease(std::string prefix)
{
    // 占位实现
}

void ConsumerINA::WindowDecrease(std::string prefix, std::string type)
{
    // 占位实现
}

void ConsumerINA::CubicIncerase(std::string prefix)
{
    // 占位实现
}

void ConsumerINA::CubicDecrease(std::string prefix, std::string type)
{
    // 占位实现
}

void ConsumerINA::WindowRecorder(std::string prefix)
{
    // 占位实现
}

void ConsumerINA::ResponseTimeRecorder(std::string prefix, bool flag)
{
    // 占位实现
}

void ConsumerINA::ScheduleNextPacket(std::string prefix)
{
    // 占位实现
}
// void ConsumerINA::SendInterest(std::shared_ptr<ndn::Name> newName)
// {
//     // 占位实现
//     spdlog::info("consumerINA virtual sendInterest", newName->toUri());
// }
int main()
{
    ConsumerINA consumerINA;
    consumerINA.StartApplication();
    return 0;
}