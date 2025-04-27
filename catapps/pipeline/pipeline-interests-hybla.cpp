/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "pipeline-interests-hybla.hpp"
#include "../chunk/chunks-interests-adaptive.hpp"
#include <cmath>
#include <chrono>
#include <limits>

namespace ndn::chunks
{

    PipelineInterestsHybla::PipelineInterestsHybla(Face &face, RttEstimatorWithStats &rttEstimator, const Options &opts)
        : PipelineInterestsAdaptive(face, rttEstimator, opts)
    {
        if (m_options.isVerbose)
        {
            std::cerr << "\tHybla parameters:\n"
                      << "\tMIN_RTT=" << MIN_RTT << "\n"
                      << "\tRHO_MAX=" << RHO_MAX << "\n"
                      << "\tMAX_CWND=" << MAX_CWND << "\n";
        }
    }

    void
    PipelineInterestsHybla::increaseWindow()
    {
        using namespace std::chrono;

        // 获取当前 RTT（单位：纳秒）
        const auto currentRtt = m_rttEstimator.getSmoothedRtt();

        // 转换为毫秒并应用最小值保护
        auto currentRttMs = duration_cast<time::milliseconds>(currentRtt);
        if (currentRttMs < MIN_RTT)
        {
            currentRttMs = MIN_RTT;
        }

        // 初始化基准 RTT（确保不小于 MIN_RTT）
        if (!m_baseRttInitialized)
        {
            m_baseRtt = std::max(currentRttMs, MIN_RTT);
            m_baseRttInitialized = true;
        }

        // 更新基准 RTT（取历史最小值，不低于 MIN_RTT）
        if (currentRttMs < m_baseRtt)
        {
            m_baseRtt = currentRttMs;
        }

        // 计算 RTT 归一化因子 rho = (baseRtt / currentRtt)^2
        const double baseRttSec = m_baseRtt.count() / 1000.0;
        const double currentRttSec = currentRttMs.count() / 1000.0;

        // 避免除零错误（currentRttSec 已受 MIN_RTT 保护）
        double rho = std::pow(baseRttSec / currentRttSec, 2);
        rho = std::min(rho, RHO_MAX); // 限制 rho 最大值

        // 窗口增长逻辑
        double newCwnd = m_chunker->safe_getWindowSize();
        if (newCwnd < m_chunker->safe_getSsthresh())
        {
            // 慢启动阶段：指数增长 × rho
            newCwnd += rho;
        }
        else
        {
            // 拥塞避免阶段：线性增长 × rho
            newCwnd += rho / newCwnd;
        }

        // 应用窗口最大值限制
        newCwnd = std::min(newCwnd, MAX_CWND);
        m_chunker->safe_setWindowSize(newCwnd);

        emitSignal(afterCwndChange, time::steady_clock::now() - getStartTime(), newCwnd);
    }

    void
    PipelineInterestsHybla::decreaseWindow()
    {
        using namespace std::chrono;

        // 乘法减少（MD），窗口设为当前值的一半，不低于 initCwnd
        const double newWin = m_chunker->safe_getWindowSize() * 0.5;
        const double ssthresh = std::max(m_options.initCwnd, newWin);
        m_chunker->safe_setSsthresh(ssthresh);
        m_chunker->safe_setWindowSize(ssthresh);

        // 重置基准 RTT（避免历史最小值过时）
        const auto currentRtt = m_rttEstimator.getSmoothedRtt();
        auto currentRttMs = duration_cast<time::milliseconds>(currentRtt);
        if (currentRttMs < MIN_RTT)
        {
            currentRttMs = MIN_RTT;
        }
        m_baseRtt = currentRttMs;

        emitSignal(afterCwndChange, time::steady_clock::now() - getStartTime(), ssthresh);
    }

} // namespace ndn::chunks