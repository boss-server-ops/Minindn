/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef NDN_TOOLS_CHUNKS_CATCHUNKS_PIPELINE_INTERESTS_BIC_HPP
#define NDN_TOOLS_CHUNKS_CATCHUNKS_PIPELINE_INTERESTS_BIC_HPP

#include "pipeline-interests-adaptive.hpp"

namespace ndn::chunks
{

    class PipelineInterestsBic final : public PipelineInterestsAdaptive
    {
    public:
        PipelineInterestsBic(Face &face, RttEstimatorWithStats &rttEstimator, const Options &opts);

    private:
        void
        increaseWindow() final;

        void
        decreaseWindow() final;

    private:
        double m_beta;         // BIC 减少因子 (默认 0.8)
        double m_maxIncrement; // 最大增长步长
        double m_lowWin;       // 当前搜索区间下限
        double m_highWin;      // 当前搜索区间上限
        double m_targetWin;    // 当前目标窗口
        bool m_inFastGrowth;   // 是否处于快速增长阶段
    };

} // namespace ndn::chunks

#endif // NDN_TOOLS_CHUNKS_CATCHUNKS_PIPELINE_INTERESTS_BIC_HPP