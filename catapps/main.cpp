#include "aggtree/splitter.hpp"
#include "aggtree/split-interests-adaptive.hpp"
#include "pipeline/discover-version.hpp"
#include "pipeline/pipeline-interests-aimd.hpp"
#include "pipeline/statistics-collector.hpp"
#include "../core/version.hpp"

#include <ndn-cxx/security/validator-null.hpp>
#include <ndn-cxx/util/rtt-estimator.hpp>

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <spdlog/sinks/basic_file_sink.h>

#include <fstream>
#include <iostream>
#include <spdlog/spdlog.h>
#include <vector>
#include <thread>

namespace ndn::chunks
{
    namespace po = boost::program_options;
    namespace pt = boost::property_tree;

    static void
    usage(std::ostream &os, std::string_view programName, const po::options_description &desc)
    {
        os << "Usage: " << programName << " [options]\n"
           << "\n"
           << "Retrieve data from the specified prefix.\n"
           << "Configuration is read from 'config.ini' file.\n"
           << "\n"
           << "Options:\n"
           << "  --help, -h                   Print this help message and exit\n"
           << "  --version, -V                Print program version and exit\n"
           << "\n"
           << "Configuration file parameters (config.ini):\n"
           << "  [General]\n"
           << "    name                       NDN name of the requested content\n"
           << "    lifetime                   Lifetime of expressed Interests, in milliseconds\n"
           << "    retries                    Maximum number of retries in case of Nack or timeout (-1 = no limit)\n"
           << "    pipeline-type              Type of Interest pipeline to use; valid values are: 'aimd'\n"
           << "    naming-convention          Encoding convention to use for name components, either 'marker' or 'typed'\n"
           << "    quiet                      Suppress all diagnostic output, except fatal errors (true/false)\n"
           << "    verbose                    Turn on verbose output (per segment information) (true/false)\n"
           << "    num-faces                  Number of faces to use for parallel data retrieval\n"
           << "  [AdaptivePipeline]\n"
           << "    ignore-marks               Do not reduce the window after receiving a congestion mark (true/false)\n"
           << "    disable-cwa                Disable Conservative Window Adaptation (true/false)\n"
           << "    init-cwnd                  Initial congestion window in segments\n"
           << "    init-ssthresh              Initial slow start threshold in segments\n"
           << "    rto-alpha                  Alpha value for RTO calculation\n"
           << "    rto-beta                   Beta value for RTO calculation\n"
           << "    rto-k                      K value for RTO calculation\n"
           << "    min-rto                    Minimum RTO value, in milliseconds\n"
           << "    max-rto                    Maximum RTO value, in milliseconds\n"
           << "    log-cwnd                   Log file for congestion window stats\n"
           << "    log-rtt                    Log file for round-trip time stats\n"
           << "  [AIMDPipeline]\n"
           << "    aimd-step                  Additive increase step\n"
           << "    aimd-beta                  Multiplicative decrease factor\n"
           << "    reset-cwnd-to-init         Reset the window to the initial value after a congestion event (true/false)\n"
           << "\n"
           << desc;
    }

    static bool
    readConfigFile(const std::string &filename, Options &opts, std::string &prefix,
                   std::string &nameConv, std::string &pipelineType, std::string &cwndPath,
                   std::string &rttPath, std::shared_ptr<util::RttEstimator::Options> &rttEstOptions,
                   std::string &logLevel, int &numFaces)
    {
        pt::ptree tree;
        try
        {
            pt::read_ini(filename, tree);
        }
        catch (const pt::ini_parser_error &e)
        {
            std::cerr << "ERROR: Cannot open or parse config file: " << e.what() << "\n";
            return false;
        }

        try
        {
            prefix = tree.get<std::string>("General.name");
            opts.interestLifetime = time::milliseconds(tree.get<time::milliseconds::rep>("General.lifetime", opts.interestLifetime.count()));
            opts.maxRetriesOnTimeoutOrNack = tree.get<int>("General.retries", opts.maxRetriesOnTimeoutOrNack);
            pipelineType = tree.get<std::string>("General.pipeline-type", pipelineType);
            nameConv = tree.get<std::string>("General.naming-convention", "");
            opts.isQuiet = tree.get<bool>("General.quiet", opts.isQuiet);
            opts.isVerbose = tree.get<bool>("General.verbose", opts.isVerbose);
            opts.TotalChunksNumber = tree.get<size_t>("General.totalchunksnumber", opts.TotalChunksNumber);

            // 读取face数量
            numFaces = tree.get<int>("General.num-faces", 2); // 默认两个face
            if (numFaces < 1)
            {
                std::cerr << "ERROR: num-faces must be at least 1\n";
                return false;
            }

            opts.ignoreCongMarks = tree.get<bool>("AdaptivePipeline.ignore-marks", opts.ignoreCongMarks);
            opts.disableCwa = tree.get<bool>("AdaptivePipeline.disable-cwa", opts.disableCwa);
            opts.initCwnd = tree.get<double>("AdaptivePipeline.init-cwnd", opts.initCwnd);
            opts.initSsthresh = tree.get<double>("AdaptivePipeline.init-ssthresh", opts.initSsthresh);
            rttEstOptions->alpha = tree.get<double>("AdaptivePipeline.rto-alpha", rttEstOptions->alpha);
            rttEstOptions->beta = tree.get<double>("AdaptivePipeline.rto-beta", rttEstOptions->beta);
            rttEstOptions->k = tree.get<int>("AdaptivePipeline.rto-k", rttEstOptions->k);
            rttEstOptions->minRto = time::milliseconds(tree.get<time::milliseconds::rep>("AdaptivePipeline.min-rto", time::duration_cast<time::milliseconds>(rttEstOptions->minRto).count()));
            rttEstOptions->maxRto = time::milliseconds(tree.get<time::milliseconds::rep>("AdaptivePipeline.max-rto", time::duration_cast<time::milliseconds>(rttEstOptions->maxRto).count()));
            cwndPath = tree.get<std::string>("AdaptivePipeline.log-cwnd", "");
            rttPath = tree.get<std::string>("AdaptivePipeline.log-rtt", "");

            opts.aiStep = tree.get<double>("AIMDPipeline.aimd-step", opts.aiStep);
            opts.mdCoef = tree.get<double>("AIMDPipeline.aimd-beta", opts.mdCoef);
            opts.resetCwndToInit = tree.get<bool>("AIMDPipeline.reset-cwnd-to-init", opts.resetCwndToInit);

            opts.recordingCycle = time::milliseconds(tree.get<time::milliseconds::rep>("General.recordingcycle", opts.recordingCycle.count()));
            opts.topoFile = tree.get<std::string>("General.topofilepath", opts.topoFile);
            logLevel = tree.get<std::string>("General.log-level", "debug");
        }
        catch (const pt::ptree_error &e)
        {
            std::cerr << "ERROR: Missing or invalid configuration parameter: " << e.what() << "\n";
            return false;
        }
        spdlog::debug("Finished reading configuration file");
        return true;
    }

    static int
    main(int argc, char *argv[])
    {
        auto m_logger = spdlog::basic_logger_mt("splitter_logger", "logs/consumer.log");
        spdlog::set_default_logger(m_logger);
        spdlog::set_level(spdlog::level::debug);
        spdlog::flush_on(spdlog::level::debug);
        const std::string programName(argv[0]);

        Options options;
        std::string prefix, nameConv, pipelineType("aimd"), logLevel, logFile;
        std::string cwndPath, rttPath;
        int numFaces = 2; // 默认使用2个face
        auto rttEstOptions = std::make_shared<util::RttEstimator::Options>();
        rttEstOptions->k = 8; // increased from the ndn-cxx default of 4

        po::options_description visibleDesc("Options");
        visibleDesc.add_options()("help,h", "print this help message and exit");

        po::variables_map vm;
        try
        {
            po::store(po::parse_command_line(argc, argv, visibleDesc), vm);
            po::notify(vm);
        }
        catch (const po::error &e)
        {
            std::cerr << "ERROR: " << e.what() << "\n";
            return 2;
        }

        if (vm.count("help") > 0)
        {
            usage(std::cout, programName, visibleDesc);
            return 0;
        }

        if (!readConfigFile("../experiments/conconfig.ini", options, prefix, nameConv, pipelineType,
                            cwndPath, rttPath, rttEstOptions, logLevel, numFaces))
        {
            return 2;
        }

        if (prefix.empty())
        {
            std::cerr << "ERROR: Missing required parameter 'name' in configuration file\n";
            usage(std::cerr, programName, visibleDesc);
            return 2;
        }

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
            std::cerr << "ERROR: '" << nameConv << "' is not a valid naming convention\n";
            return 2;
        }

        if (options.interestLifetime < 0_ms)
        {
            std::cerr << "ERROR: --lifetime cannot be negative\n";
            return 2;
        }

        if (options.maxRetriesOnTimeoutOrNack < -1 || options.maxRetriesOnTimeoutOrNack > 1024)
        {
            std::cerr << "ERROR: --retries must be between -1 and 1024\n";
            return 2;
        }

        if (options.isQuiet && options.isVerbose)
        {
            std::cerr << "ERROR: cannot be quiet and verbose at the same time\n";
            return 2;
        }

        if (rttEstOptions->k < 0)
        {
            std::cerr << "ERROR: --rto-k cannot be negative\n";
            return 2;
        }

        if (rttEstOptions->minRto < 0_ms)
        {
            std::cerr << "ERROR: --min-rto cannot be negative\n";
            return 2;
        }

        if (rttEstOptions->maxRto < rttEstOptions->minRto)
        {
            std::cerr << "ERROR: --max-rto cannot be smaller than --min-rto\n";
            return 2;
        }

        std::atomic<bool> shouldStop{false};
        try
        {
            // 创建多个face
            std::vector<std::unique_ptr<Face>> faces;
            for (int i = 0; i < numFaces; i++)
            {
                faces.push_back(std::make_unique<Face>());
                spdlog::info("Created Face #{}", i);
            }

            auto discover = std::make_unique<DiscoverVersion>(*faces[0], Name(prefix), options);
            std::unique_ptr<PipelineInterestsAimd> pipeline;
            std::unique_ptr<StatisticsCollector> statsCollector;
            std::unique_ptr<RttEstimatorWithStats> rttEstimator;
            std::ofstream statsFileCwnd;
            std::ofstream statsFileRtt;

            if (options.isVerbose)
            {
                using namespace ndn::time;
                std::cerr << "RTT estimator parameters:\n"
                          << "\tAlpha = " << rttEstOptions->alpha << "\n"
                          << "\tBeta = " << rttEstOptions->beta << "\n"
                          << "\tK = " << rttEstOptions->k << "\n"
                          << "\tInitial RTO = " << duration_cast<milliseconds>(rttEstOptions->initialRto) << "\n"
                          << "\tMin RTO = " << duration_cast<milliseconds>(rttEstOptions->minRto) << "\n"
                          << "\tMax RTO = " << duration_cast<milliseconds>(rttEstOptions->maxRto) << "\n"
                          << "\tBackoff multiplier = " << rttEstOptions->rtoBackoffMultiplier << "\n";
            }
            rttEstimator = std::make_unique<RttEstimatorWithStats>(std::move(rttEstOptions));

            pipeline = std::make_unique<PipelineInterestsAimd>(*faces[0], *rttEstimator, options);

            // 创建Face引用的vector
            std::vector<std::reference_wrapper<Face>> faceRefs;
            for (size_t i = 0; i < faces.size(); i++)
            {
                faceRefs.emplace_back(std::ref(*faces[i]));
            }

            std::unique_ptr<SplitInterests> split = std::make_unique<SplitInterestsAdaptive>(
                faceRefs, *rttEstimator, options);

            spdlog::debug("finished creating split with {} faces", numFaces);

            if (!cwndPath.empty() || !rttPath.empty())
            {
                if (!cwndPath.empty())
                {
                    statsFileCwnd.open(cwndPath);
                    if (statsFileCwnd.fail())
                    {
                        std::cerr << "ERROR: failed to open '" << cwndPath << "'\n";
                        return 4;
                    }
                }
                if (!rttPath.empty())
                {
                    statsFileRtt.open(rttPath);
                    if (statsFileRtt.fail())
                    {
                        std::cerr << "ERROR: failed to open '" << rttPath << "'\n";
                        return 4;
                    }
                }
                statsCollector = std::make_unique<StatisticsCollector>(*pipeline, statsFileCwnd, statsFileRtt);
            }
            spdlog::debug("Starting splitter");
            Splitter splitter(security::getAcceptAllValidator());
            BOOST_ASSERT(discover != nullptr);
            BOOST_ASSERT(pipeline != nullptr);
            spdlog::set_level(spdlog::level::from_str(logLevel));
            spdlog::flush_on(spdlog::level::from_str(logLevel));
            splitter.run(std::move(discover), std::move(split));
            spdlog::info("starting processing events");

            // 为除了主Face之外的所有Face创建处理线程
            std::vector<std::thread> faceThreads;
            for (size_t i = 1; i < faces.size(); i++)
            {
                faceThreads.emplace_back([i, &faces, &shouldStop]()
                                         {
                    try {
                        spdlog::info("Starting event processing for Face #{}", i);
                        while (!shouldStop) {
                            faces[i]->processEvents();
                            std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        }
                    }
                    catch (const std::exception& e) {
                        spdlog::error("Face #{} 处理事件出错: {}", i, e.what());
                    } });
            }

            spdlog::info("starting processing events for main Face #0");

            try
            {
                // 主线程处理face的事件
                faces[0]->processEvents();
            }
            catch (const std::exception &e)
            {
                spdlog::error("Face #0 处理事件出错: {}", e.what());
            }

            // 主face处理结束，通知其他线程也结束
            shouldStop = true;

            // 等待所有线程结束
            for (auto &thread : faceThreads)
            {
                if (thread.joinable())
                {
                    thread.join();
                }
            }

            spdlog::info("Finished processing events on all faces");
        }
        catch (const Splitter::ApplicationNackError &e)
        {
            std::cerr << "ERROR: " << e.what() << "\n";
            return 3;
        }
        catch (const Splitter::DataValidationError &e)
        {
            std::cerr << "ERROR: " << e.what() << "\n";
            return 5;
        }
        catch (const std::exception &e)
        {
            std::cerr << "ERROR: " << e.what() << "\n";
            return 1;
        }

        return 0;
    }

} // namespace ndn::chunks

int main(int argc, char *argv[])
{
    std::remove("logs/consumer.log");
    return ndn::chunks::main(argc, argv);
}