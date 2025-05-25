#include "chunks-interests-adaptive.hpp"
#include "../pipeline/data-fetcher.hpp"
#include "../pipeline/pipeline-interests-adaptive.hpp"
#include "../pipeline/pipeline-interests-aimd.hpp"
#include "../pipeline/pipeline-interests-cubic.hpp"
#include "../pipeline/pipeline-interests-highspeed.hpp"
#include "../pipeline/pipeline-interests-bic.hpp"
#include "../pipeline/pipeline-interests-hybla.hpp"
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
        if (allChunksReceived())
        {
            cancel();
            // if (!m_options.isQuiet)
            // {
            //     printSummary();
            // }
            return;
        }

        schedulePackets();
    }

    void
    ChunksInterestsAdaptive::doCancel()
    {
        m_chunkInfo.clear();
    }

    void
    ChunksInterestsAdaptive::sendInterest(uint64_t chuNo)
    {
        if (isStopping())
            return;

        if (chuNo >= m_options.TotalChunksNumber)
            return;

        if (m_options.isVerbose)
        {
            std::cerr << "Requesting chunk #" << chuNo << "\n";
            spdlog::debug("Requesting chunk #{}, Name :{}", chuNo, m_prefix.toUri());
        }

        ChunkInfo &chuInfo = m_chunkInfo[chuNo];
        chuInfo.pipeliner = new Pipeliner(security::getAcceptAllValidator());
        Name namewithchuno;

        spdlog::debug("Lambda expression executed for chunk #{}", chuNo);
        // auto &chuInfo = m_chunkInfo[chuNo];
        namewithchuno = Name(m_prefix).append(std::to_string(chuNo));
        spdlog::debug("Name :{}", namewithchuno.toUri());
        auto discover = std::make_unique<DiscoverVersion>(m_face, namewithchuno, m_options);
        std::unique_ptr<PipelineInterestsAdaptive> pipeline;

        if (m_options.pipelineType == "aimd")
        {
            pipeline = std::make_unique<PipelineInterestsAimd>(m_face, m_rttEstimator, m_options);
        }
        else if (m_options.pipelineType == "highspeed")
        {
            pipeline = std::make_unique<PipelineInterestsHscc>(m_face, m_rttEstimator, m_options);
        }
        else if (m_options.pipelineType == "cubic")
        {
            pipeline = std::make_unique<PipelineInterestsCubic>(m_face, m_rttEstimator, m_options);
        }
        else if (m_options.pipelineType == "bic")
        {
            pipeline = std::make_unique<PipelineInterestsBic>(m_face, m_rttEstimator, m_options);
        }
        else if (m_options.pipelineType == "hybla")
        {
            pipeline = std::make_unique<PipelineInterestsHybla>(m_face, m_rttEstimator, m_options);
        }
        else
        {
            spdlog::error("Invalid pipeline type: {}", m_options.pipelineType);
            return;
        }

        pipeline->setChunker(this);
        chuInfo.pipeliner->run(std::move(discover), std::move(pipeline));
        chuInfo.timeSent = time::steady_clock::now();
        m_nSent++;
        m_highInterest = chuNo;
        spdlog::debug("Finished sending interest for chunk #{}", chuNo);

        checkSendNext(chuNo);
    }

    void
    ChunksInterestsAdaptive::checkSendNext(uint64_t chuNo)
    {

        spdlog::debug("Check send next");
        ChunkInfo &chuInfo = m_chunkInfo[chuNo];
        if (m_checkEvent)
        {
            m_checkEvent.cancel();
        }
        if (chuInfo.pipeliner->m_pipeline->m_canschedulenext)
        {
            spdlog::debug("Send next chunk");
            sendInterest(getNextChunkNo());
        }
        else
        {
            spdlog::debug("Schedule next check");
            m_checkEvent = m_scheduler.schedule(time::milliseconds(0), [this, chuNo]
                                                { checkSendNext(chuNo); });
        }
        // recordThroughput();
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
            double throughput = 8 * (*m_received) / (m_options.recordingCycle.count() / 1000.0);
            *m_received = 0;

            // 读取拓扑文件中的参数
            std::ifstream topoFile("../../topologies/Linetest.conf");
            std::string line;
            std::string filename;
            if (topoFile.is_open())
            {
                while (std::getline(topoFile, line))
                {
                    if (line.find("con0:pro0") != std::string::npos)
                    {
                        std::istringstream iss(line);
                        std::string token;
                        std::vector<std::string> params;
                        while (iss >> token)
                        {
                            params.push_back(token);
                        }
                        if (params.size() >= 5)
                        {
                            std::string bw = params[1].substr(params[1].find('=') + 1);
                            std::string delay = params[2].substr(params[2].find('=') + 1);
                            std::string max_queue_size = params[3].substr(params[3].find('=') + 1);
                            std::string loss = params[4].substr(params[4].find('=') + 1);

                            // 读取proconfig.ini文件中的chunk-size
                            std::ifstream proconfigFile("../../chunkworkdir/experiments/conconfig.ini");
                            std::string chunkSize;
                            if (proconfigFile.is_open())
                            {
                                std::string configLine;
                                while (std::getline(proconfigFile, configLine))
                                {
                                    if (configLine.find("chunk-size") != std::string::npos)
                                    {
                                        chunkSize = configLine.substr(configLine.find('=') + 1);
                                        break;
                                    }
                                }
                                proconfigFile.close();
                            }

                            filename = "throughput_bw" + bw + "_delay" + delay + "_queue" + max_queue_size + "_loss" + loss + "_chunksize" + chunkSize + ".txt";
                        }
                        break;
                    }
                }
                topoFile.close();
            }

            std::ofstream logFile;
            if (firstTime)
            {
                logFile.open("./logs/" + filename, std::ios_base::trunc);
                firstTime = false;
            }
            else
            {
                logFile.open("./logs/" + filename, std::ios_base::app);
            }
            logFile << duration<double, milliseconds::period>(m_timeStamp - getStartTime())
                    << ": " << formatThroughput(throughput) << "\n";
            logFile.close();
            m_timeStamp = now;
        }
    }

    void
    ChunksInterestsAdaptive::schedulePackets()
    {
        // BOOST_ASSERT(safe_getInFlight() >= 0);
        // auto availableWindowSize = static_cast<int64_t>(safe_getWindowSize()) - safe_getInFlight();

        // while (availableWindowSize > 0)
        // {
        // availableWindowSize = static_cast<int64_t>(safe_getWindowSize()) - safe_getInFlight();
        // }
        // for (int i = 0; i < m_options.TotalChunksNumber; i++)
        // {

        sendInterest(getNextChunkNo());
        // }
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
    void ChunksInterestsAdaptive::safe_setLastMaxWin(double value)
    {
        m_lastMaxWin = value;
    }
    double ChunksInterestsAdaptive::safe_getLastMaxWin()
    {
        return m_lastMaxWin;
    }

} // namespace ndn::chunks