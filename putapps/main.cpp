#include "producer.hpp"

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

#include <iostream>
#include <spdlog/sinks/basic_file_sink.h>
#include "InputGenerator.hpp"

namespace ndn::chunks
{
    namespace po = boost::program_options;
    namespace pt = boost::property_tree;

    static void
    usage(std::ostream &os, std::string_view programName, const po::options_description &desc)
    {
        os << "Usage: " << programName << " [options]\n"
           << "\n"
           << "Publish data under the specified prefix.\n"
           << "Note: this tool expects data from the standard input.\n"
           << "Configuration is read from 'config.ini' file.\n"
           << "\n"
           << "Options:\n"
           << "  --help, -h                   Print this help message and exit\n"
           << "  --version, -V                Print program version and exit\n"
           << "\n"
           << "Configuration file parameters (config.ini):\n"
           << "  [General]\n"
           << "    name                       NDN name for the served content\n"
           << "    freshness                  FreshnessPeriod of the published Data packets, in milliseconds\n"
           << "    size                       Maximum chunk size, in bytes\n"
           << "    naming-convention          Encoding convention to use for name components, either 'marker' or 'typed'\n"
           << "    signing-info               Signing information\n"
           << "    print-data-version         Print Data version to the standard output (true/false)\n"
           << "    quiet                      Turn off all non-error output (true/false)\n"
           << "    verbose                    Turn on verbose output (per Interest information) (true/false)\n"
           << "  [Logging]\n"
           << "    log-file                   Path to the log file\n"
           << "    log-level                  Logging level (e.g., info, debug, error)\n"
           << "\n"
           << desc;
    }

    static bool
    readConfigFile(const std::string &filename, Producer::Options &opts, std::string &prefix, std::string &nameConv, std::string &signingStr, std::string &logFile, std::string &logLevel)
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
            opts.freshnessPeriod = time::milliseconds(tree.get<time::milliseconds::rep>("General.freshness", opts.freshnessPeriod.count()));
            opts.maxSegmentSize = tree.get<size_t>("General.size", opts.maxSegmentSize);
            nameConv = tree.get<std::string>("General.naming-convention", "");
            signingStr = tree.get<std::string>("General.signing-info", "");
            opts.wantShowVersion = tree.get<bool>("General.print-data-version", opts.wantShowVersion);
            opts.isQuiet = tree.get<bool>("General.quiet", opts.isQuiet);
            opts.isVerbose = tree.get<bool>("General.verbose", opts.isVerbose);
            logFile = tree.get<std::string>("Logging.log-file", "logs/producer.log");
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
        auto m_logger = spdlog::basic_logger_mt("producer_logger", "logs/producer.log");
        spdlog::set_default_logger(m_logger);
        spdlog::set_level(spdlog::level::debug);
        spdlog::flush_on(spdlog::level::debug);
        spdlog::debug("Started producer");
        const std::string programName(argv[0]);

        Producer::Options opts;
        std::string prefix, nameConv, signingStr, logFile, logLevel;

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

        if (!readConfigFile("../experiments/proconfig.ini", opts, prefix, nameConv, signingStr, logFile, logLevel))
        {
            return 2;
        }
        spdlog::debug("Finished reading configuration file");

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

        if (opts.freshnessPeriod < 0_ms)
        {
            std::cerr << "ERROR: --freshness cannot be negative\n";
            return 2;
        }

        if (opts.maxSegmentSize < 1 || opts.maxSegmentSize > MAX_NDN_PACKET_SIZE)
        {
            std::cerr << "ERROR: --size must be between 1 and " << MAX_NDN_PACKET_SIZE << "\n";
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

        // auto m_logger = spdlog::basic_logger_mt("producer_logger", logFile);
        // spdlog::set_default_logger(m_logger);
        // spdlog::set_level(spdlog::level::from_str(logLevel));
        // spdlog::flush_on(spdlog::level::from_str(logLevel));

        try
        {
            spdlog::debug("Generating input");
            InputGenerator input("../experiments/proconfig.ini", "../experiments/hello.txt");
            size_t chunknumber = input.readFile();
            spdlog::debug("chunk number: {}", chunknumber);
            Face face;
            KeyChain keyChain;
            std::unique_ptr<std::istream> chunkzero = input.getChunk(0);
            Producer producer(prefix, face, keyChain, *chunkzero, opts, 0);
            // for (size_t i = 1; i < chunknumber; i++)
            // {
            //     spdlog::debug("starting getchunk()");
            //     std::unique_ptr<std::istream> chunk = input.getChunk(i);
            //     spdlog::debug("Chunk number: {}", i);
            //     producer.segmentChunk(i, *chunk);
            // }
            producer.run();
        }
        catch (const std::exception &e)
        {
            std::cerr << "ERROR: " << e.what() << "\n";
            return 1;
        }

        return 0;
    }

}

int main(int argc, char *argv[])
{
    return ndn::chunks::main(argc, argv);
}