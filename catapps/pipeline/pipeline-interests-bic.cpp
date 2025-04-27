/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "pipeline-interests-bic.hpp"
#include "../chunk/chunks-interests-adaptive.hpp"
#include <cmath>

namespace ndn::chunks
{

    PipelineInterestsBic::PipelineInterestsBic(Face &face, RttEstimatorWithStats &rttEstimator, const Options &opts)
        : PipelineInterestsAdaptive(face, rttEstimator, opts), m_beta(0.8) // 默认 beta=0.8
          ,
          m_maxIncrement(16) // 最大步长限制
          ,
          m_lowWin(0), m_highWin(0), m_targetWin(0), m_inFastGrowth(false)
    {
        if (m_options.isVerbose)
        {
            std::cerr << "\tBIC beta = " << m_beta << "\n";
        }
    }

    void
    PipelineInterestsBic::increaseWindow()
    {
        // 慢启动阶段
        if (m_chunker->safe_getWindowSize() < m_chunker->safe_getSsthresh())
        {
            m_chunker->safe_WindowIncrement(1.0);
            m_inFastGrowth = false;
        }
        // 二分搜索阶段
        else
        {
            // 初始化搜索区间
            if (m_targetWin <= 0)
            {
                m_lowWin = m_chunker->safe_getWindowSize();
                m_highWin = m_chunker->safe_getLastMaxWin();
                m_targetWin = (m_lowWin + m_highWin) / 2;
                m_inFastGrowth = true;
            }

            double increment = 0;

            if (m_inFastGrowth)
            {
                // 快速增长阶段 (指数增长)
                increment = std::min(m_targetWin - m_chunker->safe_getWindowSize(), m_maxIncrement);
                if (m_chunker->safe_getWindowSize() + increment >= m_targetWin)
                {
                    m_inFastGrowth = false;
                }
            }
            else
            {
                // 线性增长阶段
                increment = std::min(m_maxIncrement, (m_highWin - m_lowWin) / (2 * m_chunker->safe_getWindowSize()));
            }

            m_chunker->safe_WindowIncrement(increment / m_chunker->safe_getWindowSize());
        }

        emitSignal(afterCwndChange, time::steady_clock::now() - getStartTime(), m_chunker->safe_getWindowSize());
    }

    void
    PipelineInterestsBic::decreaseWindow()
    {
        // 保存历史最大窗口
        m_chunker->safe_setLastMaxWin(m_chunker->safe_getWindowSize());

        // 窗口乘法减少
        double newWin = m_chunker->safe_getWindowSize() * m_beta;
        m_chunker->safe_setWindowSize(std::max(2.0, newWin));

        // 重置搜索参数
        m_targetWin = 0;
        m_lowWin = m_chunker->safe_getWindowSize();
        m_highWin = m_chunker->safe_getLastMaxWin();

        // 更新 ssthresh
        m_chunker->safe_setSsthresh(std::max(m_options.initCwnd, m_chunker->safe_getWindowSize()));

        emitSignal(afterCwndChange, time::steady_clock::now() - getStartTime(), m_chunker->safe_getWindowSize());
    }

} // namespace ndn::chunks