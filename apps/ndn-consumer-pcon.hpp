#ifndef NDN_CONSUMER_PCON_H
#define NDN_CONSUMER_PCON_H

#include <memory>
#include <chrono>
#include <iostream>
#include <functional>
#include <map>
#include <string>
#include <ndn-cxx/interest.hpp>
#include <ndn-cxx/lp/nack.hpp>
#include "ndn-consumer-window.hpp"

enum CcAlgorithm
{
    AIMD,
    BIC,
    CUBIC
};

class ConsumerPcon : public ConsumerWindow
{
public:
    ConsumerPcon();

    virtual void OnData(const ndn::Interest &interest, const ndn::Data &data) override;
    virtual void OnTimeout(const ndn::Interest &interest) override;

private:
    void WindowIncrease();
    void WindowDecrease();
    void CubicIncrease();
    void CubicDecrease();
    void BicIncrease();
    void BicDecrease();

private:
    CcAlgorithm m_ccAlgorithm;
    double m_beta;
    double m_addRttSuppress;
    bool m_reactToCongestionMarks;
    bool m_useCwa;

    double m_ssthresh;
    uint32_t m_highData;
    double m_recPoint;

    // TCP CUBIC Parameters //
    static constexpr double CUBIC_C = 0.4;
    bool m_useCubicFastConv;
    double m_cubicBeta;

    double m_cubicWmax;
    double m_cubicLastWmax;
    std::chrono::steady_clock::time_point m_cubicLastDecrease;

    // TCP BIC Parameters //
    static constexpr uint32_t BIC_LOW_WINDOW = 14;
    static constexpr uint32_t BIC_MAX_INCREMENT = 16;

    double m_bicMinWin;
    double m_bicMaxWin;
    double m_bicTargetWin;
    double m_bicSsCwnd;
    double m_bicSsTarget;
    bool m_isBicSs;
};

#endif // NDN_CONSUMER_PCON_H