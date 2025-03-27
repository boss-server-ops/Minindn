#ifndef IMAgg_Request_HPP
#define IMAgg_Request_HPP

#include "aggregation/splitter.hpp"
#include "aggregation/split-interests-adaptive.hpp"
#include "pipeline/discover-version.hpp"
#include "pipeline/pipeline-interests-aimd.hpp"
#include "pipeline/pipeline-interests-cubic.hpp"
#include "pipeline/statistics-collector.hpp"
#include "aggregation/aggregator.hpp"

#include <ndn-cxx/security/validator-null.hpp>
#include <ndn-cxx/util/rtt-estimator.hpp>
#include <boost/property_tree/ptree.hpp>
#include <spdlog/spdlog.h>

#include <atomic>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

namespace ndn::chunks
{

    class Request
    {
    public:
        // Constructor with optional config file path
        Request(const std::string &configFile, Aggregator *aggregator);

        // Destructor to clean up resources
        ~Request();

        // Initialize but don't start processing
        bool initialize();

        // Start processing (non-blocking)
        void start();

        // Start and wait until completion (blocking)
        void startBlocking();

        // Stop processing
        void stop();

        // Configuration setters
        void setPrefix(const std::string &newPrefix) { prefix = newPrefix; }
        void setNumFaces(int newNumFaces) { numFaces = newNumFaces; }
        void setLogLevel(const std::string &level);
        void setInterestLifetime(time::milliseconds lifetime) { options.interestLifetime = lifetime; }

        // Status getters
        bool isRunning() const { return !shouldStop; }

    private:
        // Configuration
        Options options;
        std::string configFilePath;
        std::string prefix;
        std::string nameConv;
        std::string pipelineType;
        std::string logLevel;
        std::string cwndPath, rttPath;
        int numFaces{2};
        std::shared_ptr<util::RttEstimator::Options> rttEstOptions;

        // Components
        std::vector<std::unique_ptr<Face>> faces;
        std::unique_ptr<DiscoverVersion> discover;
        std::unique_ptr<PipelineInterests> pipeline;
        std::unique_ptr<StatisticsCollector> statsCollector;
        std::unique_ptr<RttEstimatorWithStats> rttEstimator;
        std::unique_ptr<SplitInterests> split;
        std::unique_ptr<Splitter> splitter;

        // Stats output
        std::ofstream statsFileCwnd;
        std::ofstream statsFileRtt;

        // Threading
        std::vector<std::thread> faceThreads;
        std::atomic<bool> shouldStop{false};

        Aggregator *m_aggregator;

        // Implementation methods
        bool readConfigFile();
        void initializeComponents();
        void cleanupResources();
    };

} // namespace ndn::chunks

#endif // IMAgg_Request_HPP