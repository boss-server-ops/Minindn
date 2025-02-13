#include "ndn-consumer.hpp"
#include <spdlog/spdlog.h>
#include <limits>
#include <memory>

Consumer::Consumer()
    : m_seq(0),
      m_seqMax(0),
      m_retxTimer(std::chrono::milliseconds(50)),
      m_offTime(std::chrono::milliseconds(1000)),
      m_interestLifeTime(std::chrono::milliseconds(2000)),
      m_scheduler(m_face.getIoContext())
{
    std::random_device rd;
    m_rand.seed(rd());
}

void Consumer::SetRetxTimer(std::chrono::milliseconds retxTimer)
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

std::chrono::milliseconds Consumer::GetRetxTimer() const
{
    return m_retxTimer;
}

void Consumer::CheckRetxTimeout()
{
    std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime);
    auto rto = std::chrono::milliseconds(1000); // Example RTO value

    while (!m_seqTimeouts.empty())
    {
        auto entry = m_seqTimeouts.get<i_timestamp>().begin();
        if (entry->time + rto <= now)
        {
            uint32_t seqNo = entry->seq;
            m_seqTimeouts.get<i_timestamp>().erase(entry);
            OnTimeout(seqNo);
        }
        else
        {
            break;
        }
    }

    // Implement your timer scheduling logic here
}

void Consumer::StartApplication()
{
    spdlog::info("Starting application");
    // Implement your application start logic here
    ScheduleNextPacket();
}

void Consumer::StopApplication()
{
    spdlog::info("Stopping application");
    // Implement your application stop logic here
}

void Consumer::SendPacket()
{
    if (m_seqMax != std::numeric_limits<uint32_t>::max() && m_seq >= m_seqMax)
    {
        return;
    }

    uint32_t seq = std::numeric_limits<uint32_t>::max();

    if (!m_retxSeqs.empty())
    {
        seq = *m_retxSeqs.begin();
        m_retxSeqs.erase(m_retxSeqs.begin());
    }
    else
    {
        seq = m_seq++;
    }

    auto nameWithSequence = std::make_shared<std::string>(m_interestName + "/" + std::to_string(seq));
    auto interest = std::make_shared<std::string>("Interest: " + *nameWithSequence);

    spdlog::info("> Interest for {}", seq);

    WillSendOutInterest(seq);

    // Simulate sending interest
    spdlog::info("Sending interest: {}", *interest);

    ScheduleNextPacket();
}

void Consumer::OnData(std::shared_ptr<const std::string> data)
{
    spdlog::info("Data received: {}", *data);

    uint32_t seq = std::stoi(data->substr(data->find_last_of('/') + 1));
    spdlog::info("< DATA for {}", seq);

    int hopCount = 0; // Example hop count value

    auto entry = m_seqLastDelay.find(seq);
    if (entry != m_seqLastDelay.end())
    {
        m_lastRetransmittedInterestDataDelay(seq, std::chrono::milliseconds(100), hopCount); // Example delay value
    }

    entry = m_seqFullDelay.find(seq);
    if (entry != m_seqFullDelay.end())
    {
        m_firstInterestDataDelay(seq, std::chrono::milliseconds(100), m_seqRetxCounts[seq], hopCount); // Example delay value
    }

    m_seqRetxCounts.erase(seq);
    m_seqFullDelay.erase(seq);
    m_seqLastDelay.erase(seq);
    m_seqTimeouts.erase(seq);
    m_retxSeqs.erase(seq);

    // Simulate RTT acknowledgment
    spdlog::info("Acknowledged sequence: {}", seq);
}

void Consumer::OnNack(std::shared_ptr<const std::string> nack)
{
    spdlog::info("NACK received for: {}", *nack);
}

void Consumer::OnTimeout(uint32_t sequenceNumber)
{
    spdlog::info("Timeout for sequence number: {}", sequenceNumber);

    // Simulate RTT increase
    spdlog::info("Increased RTT multiplier for sequence: {}", sequenceNumber);

    m_retxSeqs.insert(sequenceNumber);
    ScheduleNextPacket();
}

void Consumer::WillSendOutInterest(uint32_t sequenceNumber)
{
    spdlog::debug("Trying to add {} with current time. Already {} items", sequenceNumber, m_seqTimeouts.size());

    auto now = std::chrono::steady_clock::now();
    m_seqTimeouts.insert(SeqTimeout(sequenceNumber, now));
    m_seqFullDelay.insert(SeqTimeout(sequenceNumber, now));

    m_seqLastDelay.erase(sequenceNumber);
    m_seqLastDelay.insert(SeqTimeout(sequenceNumber, now));

    m_seqRetxCounts[sequenceNumber]++;
}

void Consumer::ScheduleNextPacket()
{
    // Implement your packet scheduling logic here
    spdlog::info("Scheduling next packet");
}