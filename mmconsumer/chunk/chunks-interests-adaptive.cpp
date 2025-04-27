#include "chunks-interests-adaptive.hpp"
#include "../pipeline/data-fetcher.hpp"
#include "../pipeline/pipeline-interests-adaptive.hpp"
#include "../pipeline/pipeline-interests-aimd.hpp"
#include "../pipeline/pipeline-interests-cubic.hpp"
#include "../pipeline/pipeliner.hpp"
#include "../pipeline/discover-version.hpp"

#include <boost/lexical_cast.hpp>
#include <ndn-cxx/security/validator-null.hpp>
#include <iomanip>
#include <iostream>
#include <thread>
#include <fstream>

namespace ndn::chunks
{

    ChunksInterestsAdaptive::ChunksInterestsAdaptive(Face &face,
                                                     RttEstimatorWithStats &rttEstimator,
                                                     const Options &opts)
        : ChunksInterests(face, opts), m_cwnd(m_options.initCwnd), m_ssthresh(m_options.initSsthresh), m_rttEstimator(rttEstimator), m_scheduler(m_face.getIoContext()), m_lastDecrease(time::steady_clock::now())
    {
    }

    ChunksInterestsAdaptive::~ChunksInterestsAdaptive()
    {
        cancel();
    }

    void
    ChunksInterestsAdaptive::doRun()
    {
        if (allDataReceived())
        {
            cancel();
            return;
        }

        schedulePackets();
    }

    void
    ChunksInterestsAdaptive::doCancel()
    {
        m_typeInfo.clear();
    }

    void
    ChunksInterestsAdaptive::sendInterest(DATA_TYPE datatype)
    {
        if (isStopping())
            return;

        if (datatype > m_options.TotalChunksNumber)
            return;
        // if (allDataReceived())
        // {
        //     cancel();
        //     return;
        // }
        if (m_options.isVerbose)
        {
            std::cerr << "Requesting DATATYPE " << datatype << "\n";
            spdlog::debug("Requesting DATATYPE {}, Name :{}", datatype, m_prefix.toUri());
        }

        TypeInfo &typeInfo = m_typeInfo[datatype];
        typeInfo.pipeliner = new Pipeliner(security::getAcceptAllValidator());
        Name namewithdatatype;

        spdlog::debug("Lambda expression executed for datatype {}", datatype);
        // auto &typeInfo = m_typeInfo[chuNo];
        switch (datatype)
        {
        case DATA_TYPE_METADATA:
            namewithdatatype = Name(m_prefix).append("MetaData");
            break;
        case DATA_TYPE_TOPLAYER:
            namewithdatatype = Name(m_prefix).append("TimeWindow_20240314T120000").append("GoF_0001").append("TopLayer");
            break;
        case DATA_TYPE_LASTLAYER_30:
            namewithdatatype = Name(m_prefix).append("TimeWindow_20240314T120000").append("GoF_0001").append("LastLayer").append("30");
            break;
        case DATA_TYPE_LASTLAYER_50:
            namewithdatatype = Name(m_prefix).append("TimeWindow_20240314T120000").append("GoF_0001").append("LastLayer").append("enhanced30-50");
            break;
        case DATA_TYPE_LASTLAYER_75:
            namewithdatatype = Name(m_prefix).append("TimeWindow_20240314T120000").append("GoF_0001").append("LastLayer").append("enhanced50-75");
            break;
        case DATA_TYPE_LASTLAYER_100:
            namewithdatatype = Name(m_prefix).append("TimeWindow_20240314T120000").append("GoF_0001").append("LastLayer").append("enhanced75-100");
            break;
        default:
            spdlog::error("Invalid datatype");
            return;
        }
        spdlog::debug("Name :{}", namewithdatatype.toUri());
        auto discover = std::make_unique<DiscoverVersion>(m_face, namewithdatatype, m_options);
        std::unique_ptr<PipelineInterestsAdaptive> pipeline;

        if (m_options.pipelineType == "aimd")
        {
            pipeline = std::make_unique<PipelineInterestsAimd>(m_face, m_rttEstimator, m_options);
        }
        else
        {
            pipeline = std::make_unique<PipelineInterestsCubic>(m_face, m_rttEstimator, m_options);
        }
        pipeline->setChunker(this);
        typeInfo.pipeliner->run(std::move(discover), std::move(pipeline));
        typeInfo.timeSent = time::steady_clock::now();
        checkSendNext(datatype);
    }

    void
    ChunksInterestsAdaptive::checkSendNext(DATA_TYPE datatype)
    {
        recordThroughput();
        spdlog::debug("Check send next for datatype {}", datatype);
        BOOST_ASSERT(m_typeInfo.find(datatype) != m_typeInfo.end());
        TypeInfo &typeInfo = m_typeInfo[datatype];
        if (m_checkEvent)
        {
            m_checkEvent.cancel();
        }
        if (typeInfo.pipeliner->m_pipeline->m_canschedulenext)
        {
            spdlog::debug("Send next interest of datatype");
            datatype = static_cast<DATA_TYPE>(static_cast<std::underlying_type<DATA_TYPE>::type>(datatype) + 1);
            spdlog::debug("Next datatype {}", datatype);
            sendInterest(datatype);
        }
        else
        {
            spdlog::debug("Schedule next check");
            m_checkEvent = m_scheduler.schedule(time::milliseconds(0), [this, datatype]
                                                { checkSendNext(datatype); });
        }
        }

    // it isn't used
    void
    ChunksInterestsAdaptive::recordThroughput()
    {

        static bool firstTime = true;
        time::steady_clock::time_point now = time::steady_clock::now();
        using namespace ndn::time;
        duration<double, milliseconds::period> timeElapsed = now - m_timeStamp;
        if (timeElapsed > m_options.recordingCycle)
        {
            spdlog::debug("record throughput");
            double throughput = 8 * (*m_received) / (m_options.recordingCycle.count() / 1000.0);
            *m_received = 0;

            // Read parameters from topology file
            std::ifstream topoFile(m_options.topoFile);
            spdlog::debug("Opening topology file: {}", m_options.topoFile);
            std::string line;
            std::string filename;
            if (topoFile.is_open())
            {
                bool foundLinks = false;
                while (std::getline(topoFile, line))
                {
                    // Skip comments and empty lines
                    if (line.empty() || line[0] == '#')
                        continue;

                    // Check if we are in the [links] section
                    if (line == "[links]")
                    {
                        foundLinks = true;
                        continue;
                    }

                    // Parse the first valid line under [links]
                    if (foundLinks)
                    {
                        std::istringstream iss(line);
                        std::string connection, bw, delay, max_queue_size, loss;

                        // Parse the line into tokens
                        iss >> connection >> bw >> delay >> max_queue_size >> loss;

                        // Extract parameters
                        bw = bw.substr(bw.find('=') + 1);
                        delay = delay.substr(delay.find('=') + 1);
                        max_queue_size = max_queue_size.substr(max_queue_size.find('=') + 1);
                        loss = loss.substr(loss.find('=') + 1);

                        // Read split-size from proconfig.ini file
                        std::ifstream proconfigFile("../../chunkworkdir/experiments/conconfig.ini");
                        std::string splitSize;
                        if (proconfigFile.is_open())
                        {
                            std::string configLine;
                            while (std::getline(proconfigFile, configLine))
                            {
                                if (configLine.find("split-size") != std::string::npos)
                                {
                                    splitSize = configLine.substr(configLine.find('=') + 1);
                                    break;
                                }
                            }
                            proconfigFile.close();
                        }

                        // Generate the filename dynamically based on the parameters
                        filename = "throughput_bw" + bw + "_delay" + delay +
                                   "_queue" + max_queue_size + "_loss" + loss + "_splitsize" + splitSize + ".txt";
                        spdlog::debug("Generated filename: {}", filename);
                        break; // Stop after processing the first valid line
                    }
                }
                topoFile.close();
            }
            else
            {
                spdlog::error("Unable to open topology file: {}", m_options.topoFile);
            }

            std::ofstream logFile;
            if (firstTime)
            {
                logFile.open("./logs/" + filename, std::ios_base::trunc);
                firstTime = false;
            }
            else
            {
                spdlog::debug("Opening log file: ./logs/{}", filename);
                logFile.open("./logs/" + filename, std::ios_base::app);
            }
            logFile << duration<double, milliseconds::period>(m_timeStamp - getStartTime())
                    << ": " << formatThroughput(throughput) << "\n";
            logFile.close();
            m_timeStamp = now;
        }
        if (!isStopping() && !allDataReceived())
        {
            if (m_recordEvent)
            {
                m_recordEvent.cancel();
            }
            m_recordEvent = m_scheduler.schedule(time::milliseconds(0), [this]
                                                 { recordThroughput(); });
        }
        else
        {
            spdlog::info("Recording throughput stopped as all splits are received or pipeline is stopping");
        }
    }

    void
    ChunksInterestsAdaptive::schedulePackets()
    {
        DATA_TYPE datatype = DATA_TYPE_METADATA;
        sendInterest(datatype);
    }

    void ChunksInterestsAdaptive::safe_WindowIncrement(double value)
    {

        m_cwnd += value;
    }
    void ChunksInterestsAdaptive::safe_WindowDecrement(double value)
    {

        m_cwnd -= value;
    }
    void ChunksInterestsAdaptive::safe_setWindowSize(double value)
    {
        m_cwnd = value;
    }

    void ChunksInterestsAdaptive::safe_setSsthresh(double value)
    {
        m_ssthresh = value;
    }

    void ChunksInterestsAdaptive::safe_InFlightIncrement()
    {
        m_nInFlight++;
    }
    void ChunksInterestsAdaptive::safe_InFlightDecrement()
    {
        m_nInFlight--;
    }

    double ChunksInterestsAdaptive::safe_getWindowSize()
    {
        return m_cwnd;
    }

    int64_t ChunksInterestsAdaptive::safe_getInFlight()
    {
        return m_nInFlight;
    }

    double ChunksInterestsAdaptive::safe_getSsthresh()
    {
        return m_ssthresh;
    }

    void ChunksInterestsAdaptive::safe_setWmax(double value)
    {
        m_wmax = value;
    }
    double ChunksInterestsAdaptive::safe_getWmax()
    {
        return m_wmax;
    }
    void ChunksInterestsAdaptive::safe_setLastWmax(double value)
    {
        m_lastWmax = value;
    }
    double ChunksInterestsAdaptive::safe_getLastWmax()
    {
        return m_lastWmax;
    }
    void ChunksInterestsAdaptive::safe_setLastDecrease(time::steady_clock::time_point value)
    {
        m_lastDecrease = value;
    }
    time::steady_clock::time_point ChunksInterestsAdaptive::safe_getLastDecrease()
    {
        return m_lastDecrease;
    }

} // namespace ndn::chunks