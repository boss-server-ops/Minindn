#ifndef NDN_CONSUMER_H
#define NDN_CONSUMER_H

#include <memory>
#include <functional>
#include <iostream>
#include <string>
#include <map>
#include <set>
#include <chrono>
#include <random>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/tag.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/member.hpp>
#include <ndn-cxx/interest.hpp>
#include <ndn-cxx/lp/nack.hpp>
#include <ndn-cxx/util/rtt-estimator.hpp>
#include <ndn-cxx/face.hpp>
#include <ndn-cxx/util/scheduler.hpp>

class Consumer
{
public:
    Consumer();
    virtual ~Consumer() {}

    virtual void OnData(const ndn::Interest &interest, const ndn::Data &data);
    virtual void OnNack(const ndn::Interest &interest, const ndn::lp::Nack &nack);
    virtual void OnTimeout(const ndn::Interest &interest);
    void SendPacket();
    virtual void WillSendOutInterest(uint32_t sequenceNumber);

    typedef std::function<void(uint32_t seqno, std::chrono::milliseconds delay, int32_t hopCount)> LastRetransmittedInterestDataDelayCallback;
    typedef std::function<void(uint32_t seqno, std::chrono::milliseconds delay, uint32_t retxCount, int32_t hopCount)> FirstInterestDataDelayCallback;

protected:
    virtual void StartApplication();
    virtual void StopApplication();
    virtual void ScheduleNextPacket() = 0;
    void CheckRetxTimeout();
    void SetRetxTimer(std::chrono::milliseconds retxTimer);
    std::chrono::milliseconds GetRetxTimer() const;

protected:
    std::mt19937 m_rand; ///< @brief nonce generator

    uint32_t m_seq;                        ///< @brief currently requested sequence number
    uint32_t m_seqMax;                     ///< @brief maximum number of sequence number
    std::chrono::milliseconds m_retxTimer; ///< @brief Currently estimated retransmission timer
    std::unique_ptr<ndn::util::RttEstimator> m_rtt;
    std::chrono::milliseconds m_offTime;          ///< \brief Time interval between packets
    std::string m_interestName;                   ///< \brief NDN Name of the Interest (use Name)
    std::chrono::milliseconds m_interestLifeTime; ///< \brief LifeTime for interest packet
    ndn::scheduler::EventId m_retxEvent;          ///< \brief Event for retransmission timer
    ndn::scheduler::EventId m_sendEvent;          ///< \brief Event for sending interest

    struct RetxSeqsContainer : public std::set<uint32_t>
    {
    };

    RetxSeqsContainer m_retxSeqs; ///< \brief ordered set of sequence numbers to be retransmitted

    struct SeqTimeout
    {
        SeqTimeout(uint32_t _seq, std::chrono::milliseconds _time)
            : seq(_seq), time(_time) {}

        uint32_t seq;
        std::chrono::milliseconds time;
    };

    class i_seq
    {
    };
    class i_timestamp
    {
    };

    struct SeqTimeoutsContainer
        : public boost::multi_index::multi_index_container<
              SeqTimeout,
              boost::multi_index::indexed_by<
                  boost::multi_index::ordered_unique<
                      boost::multi_index::tag<i_seq>,
                      boost::multi_index::member<SeqTimeout, uint32_t, &SeqTimeout::seq>>,
                  boost::multi_index::ordered_non_unique<
                      boost::multi_index::tag<i_timestamp>,
                      boost::multi_index::member<SeqTimeout, std::chrono::milliseconds, &SeqTimeout::time>>>>
    {
    };

    SeqTimeoutsContainer m_seqTimeouts; ///< \brief multi-index for the set of SeqTimeout structs
    SeqTimeoutsContainer m_seqLastDelay;
    SeqTimeoutsContainer m_seqFullDelay;
    std::map<uint32_t, uint32_t> m_seqRetxCounts;

    LastRetransmittedInterestDataDelayCallback m_lastRetransmittedInterestDataDelay;
    FirstInterestDataDelayCallback m_firstInterestDataDelay;

    std::chrono::steady_clock::time_point startTime; // TODO:need to initialize
    ndn::Face m_face;
    ndn::Scheduler m_scheduler;
};

#endif // NDN_CONSUMER_H