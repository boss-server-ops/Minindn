#ifndef NDN_PRODUCER_H
#define NDN_PRODUCER_H

#include <ndn-cxx/face.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/encoding/block.hpp>
#include <ndn-cxx/encoding/tlv.hpp>
#include <spdlog/spdlog.h>
#include <random>
#include <set>
#include <map>
#include <memory>
#include <vector>
#include <string>
#include <iostream>
#include "ndn-app.hpp"

class Producer : public App
{
public:
    /**
     * @brief Constructor
     * @param prefix The prefix for which the producer has the data
     */
    Producer(const std::string &prefix);

    /**
     * @brief Constructor
     */
    Producer();

    /**
     * @brief Method that will be called every time new Interest arrives
     * @param interest The received Interest packet
     */
    void OnInterest(const ndn::InterestFilter &filter, const ndn::Interest &interest) override;

    /**
     * @brief Called at time specified by Start
     */
    void StartApplication() override;

    /**
     * @brief Called at time specified by Stop
     */
    void StopApplication() override;

private:
    ndn::Name m_prefix;
    ndn::Name m_postfix;
    uint32_t m_virtualPayloadSize;
    ndn::time::milliseconds m_freshness; // valid time

    // uint32_t m_signature;
    // ndn::Name m_keyLocator;
    ndn::KeyChain m_keyChain;
    uint32_t m_prefixnum; // customized

    int m_dataSize;
};

#endif // NDN_PRODUCER_H