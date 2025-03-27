#include "aggregation/aggregator.hpp"

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

#include <iostream>
#include <spdlog/sinks/basic_file_sink.h>
#include <ndn-cxx/security/validator-null.hpp>

namespace ndn::chunks
{
    namespace po = boost::program_options;
    namespace pt = boost::property_tree;

    static void
    usage(std::ostream &os, std::string_view programName, const po::options_description &desc)
    {
        os << "Usage: " << programName << " [options]\n"
           << "\n"
           << "Run an aggregator node to handle and forward child node requests.\n"
           << "Configuration is read from 'aggregatorput.ini' file.\n"
           << "\n"
           << "PutOptions:\n"
           << "  --help, -h                   Print this help message and exit\n"
           << "  --version, -V                Print program version and exit\n"
           << "  --prefix, -p <prefix>        NDN name prefix for the aggregator\n"
           << "\n"
           << "Configuration file parameters (aggregatorput.ini):\n"
           << "  [General]\n"
           << "    freshness                  FreshnessPeriod of the published Data packets, in milliseconds\n"
           << "    num-faces                  Number of faces to use for forwarding (default: 2)\n"
           << "    signing-info               Signing information\n"
           << "    quiet                      Turn off all non-error output (true/false)\n"
           << "    verbose                    Turn on verbose output (per Interest information) (true/false)\n"
           << "  [Logging]\n"
           << "    log-file                   Path to the log file\n"
           << "    log-level                  Logging level (e.g., info, debug, error)\n"
           << "\n"
           << desc;
    }

    static bool
    readConfigFile(const std::string &filename, Aggregator::PutOptions &opts, std::string &signingStr, std::string &logFile, std::string &logLevel)
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
            opts.freshnessPeriod = time::milliseconds(tree.get<time::milliseconds::rep>("General.freshness", opts.freshnessPeriod.count()));
            signingStr = tree.get<std::string>("General.signing-info", "");
            opts.isQuiet = tree.get<bool>("General.quiet", opts.isQuiet);
            opts.isVerbose = tree.get<bool>("General.verbose", opts.isVerbose);
            logFile = tree.get<std::string>("Logging.log-file", "logs/aggregator.log");
            logLevel = tree.get<std::string>("Logging.log-level", "info");
        }
        catch (const pt::ptree_error &e)
        {
            std::cerr << "ERROR: Missing or invalid configuration parameter: " << e.what() << "\n";
            return false;
        }

        return true;
    }

    static int
    main(int argc, char *argv[])
    {
        // Initialize logging
        auto m_logger = spdlog::basic_logger_mt("aggregator_logger", "logs/aggregator.log");
        spdlog::set_default_logger(m_logger);
        spdlog::set_level(spdlog::level::debug);
        spdlog::flush_on(spdlog::level::debug);
        spdlog::debug("Started aggregator");

        const std::string programName(argv[0]);

        Aggregator::PutOptions opts;
        std::string prefix, signingStr, logFile, logLevel;

        po::options_description visibleDesc("PutOptions");
        visibleDesc.add_options()("help,h", "print this help message and exit")("prefix,p", po::value<std::string>(&prefix)->required(), "NDN name prefix for the aggregator");

        po::variables_map vm;
        try
        {
            po::store(po::parse_command_line(argc, argv, visibleDesc), vm);

            if (vm.count("help") > 0)
            {
                usage(std::cout, programName, visibleDesc);
                return 0;
            }

            po::notify(vm);
        }
        catch (const po::error &e)
        {
            std::cerr << "ERROR: " << e.what() << "\n";
            return 2;
        }

        // Read configuration from file
        if (!readConfigFile("../experiments/aggregatorput.ini", opts, signingStr, logFile, logLevel))
        {
            return 2;
        }
        spdlog::debug("Finished reading configuration file");

        // Validate options
        if (opts.freshnessPeriod < 0_ms)
        {
            std::cerr << "ERROR: freshness cannot be negative\n";
            return 2;
        }

        try
        {
            opts.signingInfo = security::SigningInfo(signingStr);
        }
        catch (const std::invalid_argument &e)
        {
            std::cerr << "ERROR: " << e.what() << "\n";
            return 2;
        }

        if (opts.isQuiet && opts.isVerbose)
        {
            std::cerr << "ERROR: cannot be quiet and verbose at the same time\n";
            return 2;
        }

        // Update logging settings
        spdlog::set_level(spdlog::level::from_str(logLevel));
        spdlog::flush_on(spdlog::level::from_str(logLevel));

        try
        {
            // Create Face and KeyChain
            Face face;
            KeyChain keyChain;

            // Create and run the Aggregator
            spdlog::info("Creating aggregator with prefix: {}", prefix);

            // Assuming Aggregator requires these parameters: prefix, face, keychain, options,
            // chunk number and total chunk number
            uint64_t currentChunkNumber = 0;
            uint64_t totalChunkNumber = 100; // This might need to be configured differently

            Aggregator aggregator(Name(prefix), face, keyChain, opts, currentChunkNumber, totalChunkNumber);

            // Run the aggregator (this will block until completion or error)
            spdlog::info("Starting aggregator event processing");
            aggregator.run();
        }
        catch (const std::exception &e)
        {
            std::cerr << "ERROR: " << e.what() << "\n";
            spdlog::error("Fatal error: {}", e.what());
            return 1;
        }

        spdlog::info("Aggregator completed successfully");
        return 0;
    }
}

int main(int argc, char *argv[])
{
    std::remove("logs/aggregator.log");
    return ndn::chunks::main(argc, argv);
}