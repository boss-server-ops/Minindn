#include "pipeline-interests-highspeed.hpp"
#include "../chunk/chunks-interests-adaptive.hpp"
#include <spdlog/spdlog.h>
#include <cmath>
#include <ndn-cxx/util/random.hpp>
#include <algorithm> // for std::clamp

namespace ndn::chunks
{

    PipelineInterestsHscc::PipelineInterestsHscc(Face &face, RttEstimatorWithStats &rttEstimator,
                                                 const Options &opts)
        : PipelineInterestsAdaptive(face, rttEstimator, opts), m_options(opts), m_rttGradient(0.0), m_lastRtt(0.0), m_conservativeMode(false)
    {
        // 初始化带宽延迟积估算器
        m_bdpEstimator = make_unique<BdpEstimator>(rttEstimator);

        if (m_options.isVerbose)
        {
            spdlog::info("HSCC增强参数: 基础增长={}, 动态带宽指数={} BDP缩放系数={:.2f}",
                         m_options.hsccGrowthFactor, m_options.bandwidthExp, m_options.bdpScale);
        }
    }

    void
    PipelineInterestsHscc::increaseWindow()
    {
        const double currentWindow = m_chunker->safe_getWindowSize();
        const double baseIncrement = m_options.hsccGrowthFactor * std::pow(currentWindow, m_options.bandwidthExp);

        // 动态调整逻辑
        double dynamicFactor = 1.0;
        const double estimatedBdp = m_bdpEstimator->estimate() * m_options.bdpScale;

        // 进入保守模式的逻辑
        if (currentWindow > estimatedBdp * 0.7)
        {
            dynamicFactor *= 0.3; // 超过BDP的70%时降低增速
            if (!m_conservativeMode)
            {
                spdlog::debug("进入保守模式 (当前窗口={} 估算BDP={})", currentWindow, estimatedBdp);
                m_conservativeMode = true;
            }
        }

        // RTT梯度抑制
        if (m_rttGradient > 0.05)
        { // RTT上升超过5%
            dynamicFactor *= std::max(0.5, 1 - m_rttGradient);
            spdlog::debug("RTT梯度抑制 factor={:.2f}", dynamicFactor);
        }

        const double actualIncrement = baseIncrement * dynamicFactor;

        m_chunker->safe_WindowIncrement(actualIncrement);

        spdlog::debug("HSCC增强: 窗口[{}] 增量={:.2f} BDP={:.2f} RTT梯度={:.2f}",
                      currentWindow + actualIncrement, actualIncrement, estimatedBdp, m_rttGradient);

        emitSignal(afterCwndChange, time::steady_clock::now() - getStartTime(), m_chunker->safe_getWindowSize());
    }

    void
    PipelineInterestsHscc::decreaseWindow()
    {
        const double currentWindow = m_chunker->safe_getWindowSize();
        double reductionFactor = m_options.hsccReductionFactor;

        // 基于RTT梯度的强化降窗
        if (m_rttGradient > 0.1)
        { // RTT快速上升时加强响应
            reductionFactor = std::min(0.5, reductionFactor + 0.15 * m_rttGradient);
            spdlog::debug("强化降窗 factor={:.2f}", reductionFactor);
        }

        const double reduction = reductionFactor * std::pow(currentWindow, m_options.bandwidthExp - 1);
        double newSsthresh = std::max(MIN_SSTHRESH, currentWindow * (1 - reduction));

        // BDP下限保护
        const double minBdp = m_bdpEstimator->estimate() * 0.3;
        newSsthresh = std::max(newSsthresh, minBdp);

        m_chunker->safe_setSsthresh(newSsthresh);
        m_chunker->safe_setWindowSize(m_options.resetCwndToInit ? m_options.initCwnd : newSsthresh);

        // 退出保守模式
        if (m_conservativeMode)
        {
            m_conservativeMode = false;
            spdlog::debug("退出保守模式");
        }

        spdlog::debug("HSCC增强: 窗口降至[{}] 降幅={:.2f}%",
                      m_chunker->safe_getWindowSize(), reductionFactor * 100);

        emitSignal(afterCwndChange, time::steady_clock::now() - getStartTime(), m_chunker->safe_getWindowSize());
    }

    void
    PipelineInterestsHscc::afterRttMeasurement(double rtt)
    {
        // 更新RTT梯度计算
        if (m_lastRtt > 0)
        {
            m_rttGradient = std::clamp(m_rttGradient, -0.5, 1.0); // 限制梯度范围
        }
        m_lastRtt = rtt;

        // 更新BDP估算
        m_bdpEstimator->update(rtt, m_chunker->safe_getWindowSize());

        // 如果持续高梯度触发提前降窗
        if (m_rttGradient > 0.15)
        {
            spdlog::debug("主动降窗触发 RTT梯度={:.2f}", m_rttGradient);
            decreaseWindow();
        }
    }

    BdpEstimator::BdpEstimator(RttEstimatorWithStats &rttEst)
        : m_rttEstimator(rttEst), m_bandwidth(1.0)
    {
    }

    void BdpEstimator::update(double rtt, double currentWindow)
    {
        const double instantBw = currentWindow / rtt;
        m_bandwidth = 0.8 * m_bandwidth + 0.2 * instantBw;
    }

    double BdpEstimator::estimate() const
    {
        return m_bandwidth * m_rttEstimator.getSmoothedRtt().count();
    }

} // namespace ndn::chunks