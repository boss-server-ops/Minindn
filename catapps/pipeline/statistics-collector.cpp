#include "statistics-collector.hpp"

namespace ndn::chunks
{

  StatisticsCollector::StatisticsCollector(PipelineInterestsAdaptive &pipeline,
                                           std::ostream &osCwnd, std::ostream &osRtt)
      : m_osCwnd(osCwnd), m_osRtt(osRtt)
  {
    m_osCwnd << "time\tcwndsize\n";
    m_osRtt << "segment\trtt\trttvar\tsrtt\trto\n";

    pipeline.afterCwndChange.connect([this](time::nanoseconds timeElapsed, double cwnd)
                                     { m_osCwnd << timeElapsed.count() / 1e9 << '\t' << cwnd << '\n'; });

    pipeline.afterRttMeasurement.connect([this](const auto &sample)
                                         { m_osRtt << sample.segNum << '\t'
                                                   << sample.rtt.count() / 1e6 << '\t'
                                                   << sample.rttVar.count() / 1e6 << '\t'
                                                   << sample.sRtt.count() / 1e6 << '\t'
                                                   << sample.rto.count() / 1e6 << '\n'; });
  }

} // namespace ndn::chunks
