#ifndef NDN_CONSUMER_Pcon_HPP
#define NDN_CONSUMER_Pcon_HPP

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
#include <thread>
#include <fstream>
#include <string>
#include <queue>
#include <tuple>
#include <chrono>
#include "ndn-app.hpp"
#include "ModelData.hpp"
#include "ndn-consumer.hpp"

class ConsumerPcon : public Consumer
{
public:
    ConsumerPcon();

    /**
     * Override from Consumer class
     * Start the application
     */
    virtual void StartApplication() override;

    /**
     * Override from Consumer class
     * Handle received data
     * @param interest The interest that triggered the data
     * @param data The received data
     */
    virtual void OnData(const ndn::Interest &interest, const ndn::Data &data) override;

    /**
     * Override from Consumer class
     * Handle timeout event
     * @param interest The interest that timed out
     */
    virtual void OnTimeout(const ndn::Interest &interest) override;

    virtual void OnNack(const ndn::Interest &interest, const ndn::lp::Nack &nack) override;
    /**
     * Override from Consumer class
     * Send an interest
     * @param newName The name of the interest to send
     */
    virtual void SendInterest(std::shared_ptr<ndn::Name> newName) override;

    /**
     * Override from Consumer class
     * Schedule the next packet to be sent
     */
    virtual void ScheduleNextPacket(std::string prefix) override;

private:
    /**
     * Increase the window size
     */
    void WindowIncrease(std::string prefix);

    /**
     * Decrease the window size
     * @param type The type of decrease (e.g., congestion)
     */
    void WindowDecrease(std::string prefix, std::string type);

    void CubicIncerase(std::string prefix);

    void CubicDecrease(std::string prefix, std::string type);
    /**
     * Set the window size
     * @param window The new window size
     */
    virtual void SetWindow(uint32_t window);

    /**
     * Get the current window size
     * @return The current window size
     */
    uint32_t GetWindow() const;

    /**
     * Record the window size for testing purposes
     */
    void WindowRecorder(std::string prefix);

    /**
     * Record the response time
     * @param flag A flag indicating whether to record the response time
     */
    void ResponseTimeRecorder(std::string prefix, bool flag);

    /**
     * Initialize log files
     */
    void InitializeLogFile();

    void InitializeParameter();

public:
    typedef std::function<void(double)> WindowTraceCallback;

private:
    bool m_setInitialWindowOnTimeout; // Seems not enabled

    ndn::scheduler::EventId windowMonitor;
};

#endif