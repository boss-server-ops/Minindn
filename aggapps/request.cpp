#include "request.hpp"
#include <boost/property_tree/ini_parser.hpp>
#include <spdlog/sinks/basic_file_sink.h>

namespace ndn::chunks
{

    namespace pt = boost::property_tree;

    Request::Request(const std::string &configFile, Aggregator *aggregator = nullptr)
        : configFilePath(configFile), rttEstOptions(std::make_shared<util::RttEstimator::Options>()), m_aggregator(aggregator)
    {
        rttEstOptions->k = 8; // Increased from the ndn-cxx default of 4

        // Setup logger
        try
        {
            auto logger = spdlog::basic_logger_mt("request_logger", "logs/aggregator.log");
            spdlog::set_default_logger(logger);
            spdlog::set_level(spdlog::level::debug);
            spdlog::flush_on(spdlog::level::debug);
        }
        catch (const spdlog::spdlog_ex &e)
        {
            std::cerr << "Logger initialization failed: " << e.what() << std::endl;
        }
    }

    Request::~Request()
    {
        stop();
        cleanupResources();
        m_aggregator = nullptr;
    }

    bool Request::initialize()
    {
        // Read configuration
        if (!readConfigFile())
        {
            return false;
        }

        if (prefix.empty())
        {
            spdlog::error("Missing required parameter 'name' in configuration file");
            return false;
        }

        // Configure naming convention
        if (nameConv == "marker" || nameConv == "m" || nameConv == "1")
        {
            name::setConventionEncoding(name::Convention::MARKER);
        }
        else if (nameConv == "typed" || nameConv == "t" || nameConv == "2")
        {
            name::setConventionEncoding(name::Convention::TYPED);
        }
        else if (!nameConv.empty())
        {
            spdlog::error("'{}' is not a valid naming convention", nameConv);
            return false;
        }

        // Validate parameters
        if (options.interestLifetime < 0_ms)
        {
            spdlog::error("Interest lifetime cannot be negative");
            return false;
        }

        if (options.maxRetriesOnTimeoutOrNack < -1 || options.maxRetriesOnTimeoutOrNack > 1024)
        {
            spdlog::error("Retries must be between -1 and 1024");
            return false;
        }

        if (options.isQuiet && options.isVerbose)
        {
            spdlog::error("Cannot be quiet and verbose at the same time");
            return false;
        }

        if (rttEstOptions->k < 0)
        {
            spdlog::error("RTO-K cannot be negative");
            return false;
        }

        if (rttEstOptions->minRto < 0_ms)
        {
            spdlog::error("Min RTO cannot be negative");
            return false;
        }

        if (rttEstOptions->maxRto < rttEstOptions->minRto)
        {
            spdlog::error("Max RTO cannot be smaller than Min RTO");
            return false;
        }

        // Initialize components
        try
        {
            initializeComponents();
        }
        catch (const std::exception &e)
        {
            spdlog::error("Failed to initialize components: {}", e.what());
            return false;
        }

        return true;
    }

    void Request::setLogLevel(const std::string &level)
    {
        try
        {
            spdlog::level::level_enum logLevel = spdlog::level::from_str(level);
            spdlog::set_level(logLevel);
            spdlog::flush_on(logLevel);
        }
        catch (const std::exception &e)
        {
            spdlog::warn("Invalid log level '{}': {}", level, e.what());
        }
    }

    void Request::start()
    {
        if (faces.empty() || !splitter)
        {
            spdlog::error("Cannot start: components not initialized");
            return;
        }

        shouldStop = false;

        // Start threads for additional faces
        for (size_t i = 1; i < faces.size(); i++)
        {
            faceThreads.emplace_back([this, i]()
                                     {
            try {
                spdlog::info("Starting event processing for Face #{}", i);
                while (!shouldStop) {
                    faces[i]->processEvents();
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            }
            catch (const std::exception& e) {
                spdlog::error("Face #{} processing error: {}", i, e.what());
            } });
        }

        // Start main face processing in a separate thread (non-blocking)
        faceThreads.emplace_back([this]()
                                 {
        try {
            spdlog::info("Starting event processing for main Face #0");
            while (!shouldStop) {
                faces[0]->processEvents();
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        catch (const std::exception& e) {
            spdlog::error("Face #0 processing error: {}", e.what());
            shouldStop = true;
        } });
    }

    void Request::startBlocking()
    {
        if (faces.empty() || !splitter)
        {
            spdlog::error("Cannot start: components not initialized");
            return;
        }

        shouldStop = false;

        // Start threads for additional faces
        for (size_t i = 1; i < faces.size(); i++)
        {
            faceThreads.emplace_back([this, i]()
                                     {
            try {
                spdlog::info("Starting event processing for Face #{}", i);
                while (!shouldStop) {
                    faces[i]->processEvents();
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            }
            catch (const std::exception& e) {
                spdlog::error("Face #{} processing error: {}", i, e.what());
            } });
        }

        // Process events on the main face (blocking until completion or error)
        try
        {
            spdlog::info("Starting event processing for main Face #0");
            faces[0]->processEvents();
        }
        catch (const Splitter::ApplicationNackError &e)
        {
            spdlog::error("Application error: {}", e.what());
        }
        catch (const Splitter::DataValidationError &e)
        {
            spdlog::error("Data validation error: {}", e.what());
        }
        catch (const std::exception &e)
        {
            spdlog::error("Face #0 processing error: {}", e.what());
        }

        // Signal other threads to stop
        stop();
    }

    void Request::stop()
    {
        shouldStop = true;

        // Wait for all threads to finish
        for (auto &thread : faceThreads)
        {
            if (thread.joinable())
            {
                thread.join();
            }
        }

        // Clear the threads vector
        faceThreads.clear();

        spdlog::info("All processing threads stopped");
    }

    bool Request::readConfigFile()
    {
        pt::ptree tree;
        try
        {
            pt::read_ini(configFilePath, tree);
        }
        catch (const pt::ini_parser_error &e)
        {
            spdlog::error("Cannot open or parse config file: {}", e.what());
            return false;
        }

        try
        {
            prefix = tree.get<std::string>("General.name");
            options.interestLifetime = time::milliseconds(
                tree.get<time::milliseconds::rep>("General.lifetime", options.interestLifetime.count()));
            options.maxRetriesOnTimeoutOrNack = tree.get<int>("General.retries", options.maxRetriesOnTimeoutOrNack);
            pipelineType = tree.get<std::string>("General.pipeline-type", "aimd");
            options.pipelineType = pipelineType;
            nameConv = tree.get<std::string>("General.naming-convention", "");
            options.isQuiet = tree.get<bool>("General.quiet", options.isQuiet);
            options.isVerbose = tree.get<bool>("General.verbose", options.isVerbose);
            options.TotalChunksNumber = tree.get<size_t>("General.totalchunksnumber", options.TotalChunksNumber);

            // Read face count
            numFaces = tree.get<int>("General.num-faces", numFaces);
            if (numFaces < 1)
            {
                spdlog::error("num-faces must be at least 1");
                return false;
            }

            options.ignoreCongMarks = tree.get<bool>("AdaptivePipeline.ignore-marks", options.ignoreCongMarks);
            options.disableCwa = tree.get<bool>("AdaptivePipeline.disable-cwa", options.disableCwa);
            options.initCwnd = tree.get<double>("AdaptivePipeline.init-cwnd", options.initCwnd);
            options.initSsthresh = tree.get<double>("AdaptivePipeline.init-ssthresh", options.initSsthresh);
            rttEstOptions->alpha = tree.get<double>("AdaptivePipeline.rto-alpha", rttEstOptions->alpha);
            rttEstOptions->beta = tree.get<double>("AdaptivePipeline.rto-beta", rttEstOptions->beta);
            rttEstOptions->k = tree.get<int>("AdaptivePipeline.rto-k", rttEstOptions->k);
            rttEstOptions->minRto = time::milliseconds(
                tree.get<time::milliseconds::rep>("AdaptivePipeline.min-rto",
                                                  time::duration_cast<time::milliseconds>(rttEstOptions->minRto).count()));
            rttEstOptions->maxRto = time::milliseconds(
                tree.get<time::milliseconds::rep>("AdaptivePipeline.max-rto",
                                                  time::duration_cast<time::milliseconds>(rttEstOptions->maxRto).count()));
            cwndPath = tree.get<std::string>("AdaptivePipeline.log-cwnd", "");
            rttPath = tree.get<std::string>("AdaptivePipeline.log-rtt", "");

            options.aiStep = tree.get<double>("AIMDPipeline.aimd-step", options.aiStep);
            options.mdCoef = tree.get<double>("AIMDPipeline.aimd-beta", options.mdCoef);
            options.resetCwndToInit = tree.get<bool>("AIMDPipeline.reset-cwnd-to-init", options.resetCwndToInit);

            options.cubicBeta = tree.get<double>("CubicPipeline.cubic-beta", options.cubicBeta);
            options.enableFastConv = tree.get<bool>("CubicPipeline.enable-fast-conv", options.enableFastConv);

            options.recordingCycle = time::milliseconds(
                tree.get<time::milliseconds::rep>("General.recordingcycle", options.recordingCycle.count()));
            options.topoFile = tree.get<std::string>("General.topofilepath", options.topoFile);
            logLevel = tree.get<std::string>("General.log-level", "debug");

            setLogLevel(logLevel);
        }
        catch (const pt::ptree_error &e)
        {
            spdlog::error("Missing or invalid configuration parameter: {}", e.what());
            return false;
        }

        spdlog::debug("Finished reading configuration file");
        return true;
    }

    void Request::initializeComponents()
    {
        // Create faces
        faces.clear();
        for (int i = 0; i < numFaces; i++)
        {
            faces.push_back(std::make_unique<Face>());
            spdlog::info("Created Face #{}", i);
        }

        // Initialize RTT estimator
        if (options.isVerbose)
        {
            using namespace ndn::time;
            spdlog::info("RTT estimator parameters:");
            spdlog::info("\tAlpha = {}", rttEstOptions->alpha);
            spdlog::info("\tBeta = {}", rttEstOptions->beta);
            spdlog::info("\tK = {}", rttEstOptions->k);
            spdlog::info("\tInitial RTO = {} ms", duration_cast<milliseconds>(rttEstOptions->initialRto).count());
            spdlog::info("\tMin RTO = {} ms", duration_cast<milliseconds>(rttEstOptions->minRto).count());
            spdlog::info("\tMax RTO = {} ms", duration_cast<milliseconds>(rttEstOptions->maxRto).count());
            spdlog::info("\tBackoff multiplier = {}", rttEstOptions->rtoBackoffMultiplier);
        }

        rttEstimator = std::make_unique<RttEstimatorWithStats>(std::make_shared<util::RttEstimator::Options>(*rttEstOptions));

        // Create discover version component
        discover = std::make_unique<DiscoverVersion>(*faces[0], Name(prefix), options);

        std::unique_ptr<PipelineInterestsAdaptive> adaptivePipeline;
        // Create pipeline
        if (pipelineType == "aimd")
            adaptivePipeline = std::make_unique<PipelineInterestsAimd>(*faces[0], *rttEstimator, options);
        else if (pipelineType == "cubic")
            adaptivePipeline = std::make_unique<PipelineInterestsCubic>(*faces[0], *rttEstimator, options);

        // Create face references vector
        std::vector<std::reference_wrapper<Face>> faceRefs;
        for (size_t i = 0; i < faces.size(); i++)
        {
            faceRefs.emplace_back(std::ref(*faces[i]));
        }

        // Create split-interests component
        split = std::make_unique<SplitInterestsAdaptive>(faceRefs, *rttEstimator, options, m_aggregator);
        spdlog::debug("Finished creating split with {} faces", numFaces);

        // Create statistics collector if needed
        if (!cwndPath.empty() || !rttPath.empty())
        {
            if (!cwndPath.empty())
            {
                statsFileCwnd.open(cwndPath);
                if (statsFileCwnd.fail())
                {
                    throw std::runtime_error("Failed to open '" + cwndPath + "'");
                }
            }

            if (!rttPath.empty())
            {
                statsFileRtt.open(rttPath);
                if (statsFileRtt.fail())
                {
                    throw std::runtime_error("Failed to open '" + rttPath + "'");
                }
            }

            statsCollector = std::make_unique<StatisticsCollector>(*adaptivePipeline, statsFileCwnd, statsFileRtt);
        }
        pipeline = std::move(adaptivePipeline);
        // Create and initialize splitter
        splitter = std::make_unique<Splitter>(security::getAcceptAllValidator());
        splitter->run(std::move(discover), std::move(split));

        spdlog::debug("All components initialized successfully");
    }

    void Request::cleanupResources()
    {
        // Close statistics files
        if (statsFileCwnd.is_open())
        {
            statsFileCwnd.close();
        }

        if (statsFileRtt.is_open())
        {
            statsFileRtt.close();
        }

        // Clear components
        splitter.reset();
        statsCollector.reset();
        pipeline.reset();
        rttEstimator.reset();
        split.reset();

        // Clear faces
        faces.clear();

        spdlog::debug("Resources cleaned up");
    }

} // namespace ndn::chunks

// // Example usage
// #include "request.hpp"

// int main(int argc, char *argv[])
// {
//     // Optional: delete previous log file
//     std::remove("logs/consumer.log");

//     // Create Request instance
//     ndn::chunks::Request request("../experiments/conconfig.ini");

//     // Initialize and start (blocking until completion)
//     if (request.initialize())
//     {
//         request.startBlocking();
//     }

//     return 0;
// }