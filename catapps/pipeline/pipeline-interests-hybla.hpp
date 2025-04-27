/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef NDN_TOOLS_CHUNKS_CATCHUNKS_PIPELINE_INTERESTS_HYBLA_HPP
#define NDN_TOOLS_CHUNKS_CATCHUNKS_PIPELINE_INTERESTS_HYBLA_HPP

#include "pipeline-interests-adaptive.hpp"

namespace ndn::chunks
{

    class PipelineInterestsHybla final : public PipelineInterestsAdaptive
    {
    public:
        PipelineInterestsHybla(Face &face, RttEstimatorWithStats &rttEstimator, const Options &opts);

    private:
        void
        increaseWindow() final;

        void
        decreaseWindow() final;

    private:
        static constexpr double RHO_MAX = 100.0;        // rho 增长上限
        static constexpr double MAX_CWND = 1e6;         // 最大窗口限制
        static constexpr time::milliseconds MIN_RTT{1}; // 最小 RTT 1ms

        time::milliseconds m_baseRtt{MIN_RTT}; // 基准 RTT（单位：毫秒）
        bool m_baseRttInitialized = false;
    };

} // namespace ndn::chunks

#endif // NDN_TOOLS_CHUNKS_CATCHUNKS_PIPELINE_INTERESTS_HYBLA_HPP