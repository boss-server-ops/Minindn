#include "ndn-consumer-INA.hpp"

ConsumerINA::ConsumerINA()
    : m_initialWindow(1),
      m_window(1.0),
      m_inFlight(0),
      m_setInitialWindowOnTimeout(false),
      m_ssthresh(0),
      m_useCwa(false),
      m_highData(0),
      m_recPoint(0),
      m_alpha(0.5),
      m_beta(0.5),
      m_gamma(0.5),
      m_addRttSuppress(0.0),
      m_reactToCongestionMarks(false)
{
    // Initialize spdlog
    spdlog::info("ConsumerINA initialized");
}

void ConsumerINA::SendInterest(std::shared_ptr<ndn::Name> newName)
{
    m_inFlight++;
    Consumer::SendInterest(newName);
}

void ConsumerINA::ScheduleNextPacket()
{
    // ps:deleted schedule and imported the thread
    if (!broadcastSync && globalSeq != 0)
    {
        spdlog::info("Haven't finished tree broadcasting synchronization, don't send actual data packet for now.");
    }
    else if (m_window == static_cast<uint32_t>(0))
    {
        if (m_sendEvent.joinable())
        {
            m_sendEvent.join();
        }

        spdlog::error("Error! Window becomes 0!!!!!!");
        auto delay = std::chrono::milliseconds(static_cast<int>(std::min<double>(0.5, (m_retxTimer * 6).count()) * 1000));
        m_sendEvent = std::thread([this, delay]()
                                  {
            std::this_thread::sleep_for(delay);
            SendPacket(); });
    }
    else if (m_window - m_inFlight <= 0)
    {
        spdlog::info("Wait until cwnd allows new transmission.");
        spdlog::debug("Window: {}, InFlight: {}", m_window, m_inFlight);
        // do nothing
    }
    else
    {
        if (m_sendEvent.joinable())
        {
            m_sendEvent.join();
        }
        spdlog::debug("Window: {}, InFlight: {}", m_window, m_inFlight);
        m_sendEvent = std::thread([this]()
                                  { SendPacket(); });
    }
}

void ConsumerINA::StartApplication()
{
    // Initialize log files
    InitializeLogFile();

    Consumer::StartApplication();
}

void ConsumerINA::OnData(const ndn::Interest &interest, const ndn::Data &data)
{
    Consumer::OnData(interest, data);

    std::string dataName = data.getName().toUri();
    uint64_t sequenceNum = data.getName().get(-1).toSequenceNumber();
    std::string type = data.getName().get(-2).toUri();
    std::string name_sec0 = data.getName().get(0).toUri();

    // Set highest received Data to sequence number
    if (m_highData < sequenceNum)
    {
        m_highData = sequenceNum;
    }

    // Only perform congestion control for those type is "data", disable this function for "initialization" type
    if (type == "data")
    {
        // Get current packet's round index
        int roundIndex = Consumer::findRoundIndex(name_sec0);
        if (roundIndex == -1)
        {
            spdlog::debug("Error on roundIndex!");
            std::terminate();
        }
        spdlog::debug("This packet comes from round {}", roundIndex);

        // Perform congestion control
        // Priority
        // 1. Whether broadcasting("initialization") has finished
        // 2. PCON's ECN mark - not enable now
        // 3. Based on congestion signal, perform congestion/rate control
        // 4. CWA algorithm control whether window decrease should be performed
        if (!broadcastSync)
        {
            spdlog::info("Currently broadcasting aggregation tree, ignore relevant cwnd/congestion management");
        }
        /*        else if (data.getCongestionMark() > 0) {
                    if (m_reactToCongestionMarks) {
                        spdlog::debug("Received congestion mark: {}", data.getCongestionMark());
                        WindowDecrease("ConsumerCongestion");
                    }
                    else {
                        spdlog::debug("Ignored received congestion mark: {}", data.getCongestionMark());
                    }
                }*/
        else if (ECNLocal)
        {
            // Whether CWA is enabled
            if (m_useCwa && !CanDecreaseWindow(RTT_measurement[roundIndex]))
            {
                isWindowDecreaseSuppressed = true;
                spdlog::info("Window decrease is suppressed.");
            }
            else
            {
                spdlog::info("Congestion signal exists in consumer!");
                WindowDecrease("ConsumerCongestion");
            }
        }
        /*        else if (ECNRemote) {
                    spdlog::info("Congestion signal exists in aggregator!");
                    WindowDecrease("AggregatorCongestion");
                }*/
        else
        {
            spdlog::info("No congestion, increase the cwnd.");
            WindowIncrease();
        }

        // Record the last flag in in RTT's log
        ResponseTimeRecorder(isWindowDecreaseSuppressed);
    }

    if (m_inFlight > static_cast<uint32_t>(0))
    {
        m_inFlight--;
    }

    spdlog::debug("Window: {}, InFlight: {}", m_window, m_inFlight);

    // Record cwnd
    WindowRecorder();

    ScheduleNextPacket();
}

void ConsumerINA::OnTimeout(const ndn::Interest &interest)
{
    WindowDecrease("timeout");

    if (m_inFlight > static_cast<uint32_t>(0))
    {
        m_inFlight--;
    }

    spdlog::debug("Window: {}, InFlight: {}", m_window, m_inFlight);

    Consumer::OnTimeout(interest);
}

void ConsumerINA::SetWindow(uint32_t window)
{
    m_initialWindow = window;
    m_window = m_initialWindow;
}

uint32_t ConsumerINA::GetWindow() const
{
    return m_initialWindow;
}

void ConsumerINA::WindowIncrease()
{
    if (m_window < m_ssthresh)
    {
        m_window += 1.0;
    }
    else
    {
        m_window += (1.0 / m_window);
    }
    spdlog::debug("Window size increased to {}", m_window);
}

void ConsumerINA::WindowDecrease(const std::string &type)
{
    // AIMD for timeout
    if (type == "timeout")
    {
        m_ssthresh = m_window * m_alpha;
        m_window = m_ssthresh;
    }
    else if (type == "ConsumerCongestion")
    {
        m_ssthresh = m_window * m_beta;
        m_window = m_ssthresh;

        // Perform CWA when handling consumer congestion
        lastWindowDecreaseTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch());
    }
    else if (type == "AggregatorCongestion")
    {
        m_ssthresh = m_window * m_gamma;
        m_window = m_ssthresh;
    }

    // Window size can't be reduced below initial size
    if (m_window < m_initialWindow)
    {
        m_window = m_initialWindow;
    }

    spdlog::debug("Encounter {}. Window size decreased to {}", type, m_window);
}

void ConsumerINA::WindowRecorder()
{
    // Open file; on first call, truncate it to delete old content
    std::ofstream file(windowTimeRecorder, std::ios::app);

    if (!file.is_open())
    {
        spdlog::error("Failed to open the file: {}", windowTimeRecorder);
        return;
    }

    // Get current time in milliseconds
    auto now = std::chrono::steady_clock::now();
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    file << now_ms << " " << m_window << " " << m_ssthresh << std::endl;

    file.close();
}

void ConsumerINA::WindowRecorder()
{
    // Open file; on first call, truncate it to delete old content
    std::ofstream file(windowTimeRecorder, std::ios::app);

    if (!file.is_open())
    {
        spdlog::error("Failed to open the file: {}", windowTimeRecorder);
        return;
    }

    // Get current time in milliseconds
    auto now = std::chrono::steady_clock::now();
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    file << now_ms << " " << m_window << " " << m_ssthresh << std::endl;

    file.close();
}
void ConsumerINA::InitializeLogFile()
{
    // Check whether object path exists, create it if not
    CheckDirectoryExist(folderPath);

    // Open the file and clear all contents for log file
    OpenFile(windowTimeRecorder);

    Consumer::InitializeLogFile();
}