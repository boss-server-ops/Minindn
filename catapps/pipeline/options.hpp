
#ifndef IMAgg_OPTIONS_HPP
#define IMAgg_OPTIONS_HPP

#include <ndn-cxx/interest.hpp>
#include <ndn-cxx/util/time.hpp>

#include <limits>
#include <string>

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
    std::string pipelineType = "aimd";

    // Fixed pipeline options
    size_t maxPipelineSize = 1;

    // Adaptive pipeline common options
    double initCwnd = 2.0;                                    ///< initial congestion window size
    double initSsthresh = std::numeric_limits<double>::max(); ///< initial slow start threshold
    time::milliseconds rtoCheckInterval{1};                   ///< interval for checking retransmission timer
    bool ignoreCongMarks = false;                             ///< disable window decrease after receiving congestion mark
    bool disableCwa = false;                                  ///< disable conservative window adaptation

    // AIMD pipeline options
    double aiStep = 1.0;          ///< AIMD additive increase step (in segments)
    double mdCoef = 0.5;          ///< AIMD multiplicative decrease factor
    bool resetCwndToInit = false; ///< reduce cwnd to initCwnd when loss event occurs

    // Cubic pipeline options
    double cubicBeta = 0.7;      ///< cubic multiplicative decrease factor
    bool enableFastConv = false; ///< use cubic fast convergence

    // highspeed pipeline options
    double hsccGrowthFactor = 0.01;
    double hsccReductionFactor = 0.2;
    double bandwidthExp = 0.8;
    double bdpScale = 1.1;

    // Chunks pipeline options
    size_t TotalChunksNumber = 5; ///< total number of chunks in the Aggregation process

    std::string outputFile = "../experiments/output.txt"; ///< output file name
    // Recording cycle
    time::milliseconds recordingCycle = time::milliseconds(1000);
    std::string topoFile = "../../topologies/Customtest.conf";
    std::string primarytopoFile = "../../topologies/dcn_primary_a2_p2_b30_l0.0.conf";
  };

} // namespace ndn::chunks

#endif // IMAgg_OPTIONS_HPP
