#ifndef IMAgg_STATISTICS_COLLECTOR_HPP
#define IMAgg_STATISTICS_COLLECTOR_HPP

#include "pipeline-interests-adaptive.hpp"

namespace ndn::chunks
{

  /**
   * @brief Statistics collector for Adaptive pipelines
   */
  class StatisticsCollector : noncopyable
  {
  public:
    StatisticsCollector(PipelineInterestsAdaptive &pipeline,
                        std::ostream &osCwnd, std::ostream &osRtt);

  private:
    std::ostream &m_osCwnd;
    std::ostream &m_osRtt;
  };

} // namespace ndn::chunks

#endif // IMAgg_STATISTICS_COLLECTOR_HPP
