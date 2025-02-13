#ifndef NDN_CONSUMER_WINDOW_H
#define NDN_CONSUMER_WINDOW_H

#include <memory>
#include <functional>
#include <iostream>
#include <string>
#include <map>
#include <ndn-cxx/interest.hpp>
#include <ndn-cxx/lp/nack.hpp>
#include "ndn-consumer.hpp"
class ConsumerWindow : public Consumer
{
public:
    ConsumerWindow();

    virtual void OnData(const ndn::Interest &interest, const ndn::Data &data) override;
    virtual void OnTimeout(const ndn::Interest &interest) override;
    virtual void WillSendOutInterest(uint32_t sequenceNumber) override;

    typedef std::function<void(double)> WindowTraceCallback;

protected:
    virtual void ScheduleNextPacket();

private:
    virtual void SetWindow(uint32_t window);
    uint32_t GetWindow() const;

    virtual void SetPayloadSize(uint32_t payload);
    uint32_t GetPayloadSize() const;

    double GetMaxSize() const;
    void SetMaxSize(double size);

    uint32_t GetSeqMax() const;
    void SetSeqMax(uint32_t seqMax);

protected:
    uint32_t m_payloadSize; // expected payload size
    double m_maxSize;       // max size to request

    uint32_t m_initialWindow;
    bool m_setInitialWindowOnTimeout;

    double m_window;
    uint32_t m_inFlight;
};

#endif // NDN_CONSUMER_WINDOW_H