#ifndef NDN_CHUNKS_PIPELINE_INTERESTS_HIGHSPEED_HPP
#define NDN_CHUNKS_PIPELINE_INTERESTS_HIGHSPEED_HPP

#include "pipeline-interests-adaptive.hpp"
#include "options.hpp"
#include <ndn-cxx/util/random.hpp>
#include <memory>

namespace ndn::chunks
{

    class BdpEstimator
    {
    public:
        explicit BdpEstimator(RttEstimatorWithStats &rttEst);
        void update(double rtt, double currentWindow);
        double estimate() const;

    private:
        RttEstimatorWithStats &m_rttEstimator;
        double m_bandwidth;
    };

    class PipelineInterestsHscc : public PipelineInterestsAdaptive
    {
    public:
        PipelineInterestsHscc(Face &face, RttEstimatorWithStats &rttEstimator, const Options &opts);

    private:
        // 新增私有方法
        void increaseWindow() override;
        void decreaseWindow() override;
        void afterRttMeasurement(double rtt); // 新增RTT回调

        // 新增成员变量
        Options m_options;
        double m_rttGradient;                         // RTT变化梯度 [新增]
        double m_lastRtt;                             // 上一次RTT记录 [新增]
        bool m_conservativeMode;                      // 保守模式标志 [新增]
        std::unique_ptr<BdpEstimator> m_bdpEstimator; // BDP估算器 [新增]

        // 日志标签
        static const std::string LOG_TAG;
    };

} // namespace ndn::chunks

#endif // NDN_CHUNKS_PIPELINE_INTERESTS_HIGHSPEED_HPP