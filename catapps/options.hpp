
#ifndef IMAgg_OPTIONS_HPP
#define IMAgg_OPTIONS_HPP

#include <ndn-cxx/interest.hpp>
#include <ndn-cxx/util/time.hpp>

#include <limits>

namespace ndn::chunks
{
  // TODO: original input is from cin but my sys needs to be changed into file
  struct Options
  {
    // Common options
    time::milliseconds interestLifetime = DEFAULT_INTEREST_LIFETIME;
    int maxRetriesOnTimeoutOrNack = 15;
    bool disableVersionDiscovery = true;
    bool mustBeFresh = false; ///< false means the data can come from cache
    bool isQuiet = false;
    bool isVerbose = false;

    // Fixed pipeline options
    size_t maxPipelineSize = 1;

    // Adaptive pipeline common options
    double initCwnd = 2.0;                                    ///< initial congestion window size
    double initSsthresh = std::numeric_limits<double>::max(); ///< initial slow start threshold
    time::milliseconds rtoCheckInterval{10};                  ///< interval for checking retransmission timer
    bool ignoreCongMarks = false;                             ///< disable window decrease after receiving congestion mark
    bool disableCwa = false;                                  ///< disable conservative window adaptation

    // AIMD pipeline options
    double aiStep = 1.0;          ///< AIMD additive increase step (in segments)
    double mdCoef = 0.5;          ///< AIMD multiplicative decrease factor
    bool resetCwndToInit = false; ///< reduce cwnd to initCwnd when loss event occurs

    // Cubic pipeline options
    double cubicBeta = 0.7;      ///< cubic multiplicative decrease factor
    bool enableFastConv = false; ///< use cubic fast convergence

    // Chunks pipeline options
    size_t TotalChunksNumber = 2; ///< total number of chunks in the Aggregation process
  };

} // namespace ndn::chunks

#endif // IMAgg_OPTIONS_HPP
