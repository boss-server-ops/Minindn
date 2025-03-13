#include "split-interests-adaptive.hpp"
#include "../pipeline/data-fetcher.hpp"
#include "../pipeline/discover-version.hpp"
#include "../chunk/consumer.hpp"

#include <boost/lexical_cast.hpp>
#include <ndn-cxx/security/validator-null.hpp>
#include <iomanip>
#include <iostream>
#include <fstream>

namespace ndn::chunks
{

    SplitInterestsAdaptive::SplitInterestsAdaptive(Face &face,
                                                   RttEstimatorWithStats &rttEstimator,
                                                   const Options &opts)
        : SplitInterests(face, opts), m_cwnd(m_options.initCwnd), m_ssthresh(m_options.initSsthresh), m_rttEstimator(rttEstimator), m_scheduler(m_face.getIoContext())
    {
    }

    SplitInterestsAdaptive::~SplitInterestsAdaptive()
    {
        cancel();
    }

    void
    SplitInterestsAdaptive::doRun()
    {
        spdlog::debug("SplitInterestsAdaptive::doRun() called");
        m_aggTree.getTreeTopology(m_options.topoFile, "con0");
        if (allSplitReceived())
        {
            cancel();
            // if (!m_options.isQuiet)
            // {
            //     printSummary();
            // }
            return;
        }
        recordThroughput();
        schedulePackets();
    }

    void
    SplitInterestsAdaptive::doCancel()
    {
        m_recordEvent.cancel();
        m_splitInfo.clear();
    }

    void
    SplitInterestsAdaptive::sendInterest(Name &interestName)
    {
        if (isStopping())
            return;

        // if (chuNo >= m_options.TotalSplitNumber)
        //     return;
        std::string firstComponent = interestName.get(0).toUri();

        if (m_options.isVerbose)
        {
            std::cerr << "Requesting interest with first component: " << firstComponent << "\n";
            spdlog::debug("Requesting interest with first component: {}", firstComponent);
        }

        SplitInfo &splitInfo = m_splitInfo[firstComponent];
        try
        {
            splitInfo.consumer = new Consumer(security::getAcceptAllValidator());

            auto discover = std::make_unique<DiscoverVersion>(m_face, interestName, m_options);
            std::unique_ptr<ChunksInterests> chunks =
                std::make_unique<ChunksInterestsAdaptive>(m_face, m_rttEstimator, m_options);
            chunks->setSplitinterest(this);
            splitInfo.consumer->run(std::move(discover), std::move(chunks));
        }
        catch (const Consumer::ApplicationNackError &e)
        {
            std::cerr << "ERROR: " << e.what() << "\n";
            return;
        }
        catch (const Consumer::DataValidationError &e)
        {
            std::cerr << "ERROR: " << e.what() << "\n";
            return;
        }
        catch (const std::exception &e)
        {
            std::cerr << "ERROR: " << e.what() << "\n";
            return;
        }
        splitInfo.timeSent = time::steady_clock::now();
        m_nSent++;
        spdlog::debug("Finished sending first interest for interest name: {}", interestName.toUri());
    }

    void
    SplitInterestsAdaptive::recordThroughput()
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
            std::ifstream topoFile(m_options.topoFile);
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

                            // 读取proconfig.ini文件中的split-size
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

                            filename = "throughput_bw" + bw + "_delay" + delay + "_queue" + max_queue_size + "_loss" + loss + "_splitsize" + splitSize + ".txt";
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
        if (m_recordEvent)
        {
            m_recordEvent.cancel();
        }
        m_recordEvent = m_scheduler.schedule(time::milliseconds(0), [this]
                                             { recordThroughput(); });
    }

    void
    SplitInterestsAdaptive::sendInitialInterest()
    {

        for (auto interestName : m_aggTree.interestNames)
        {
            interestName.append("init");

            Interest interest(interestName);
            interest.setCanBePrefix(false);
            interest.setMustBeFresh(true);

            m_face.expressInterest(interest, [this](const Interest &, const Data &data)
                                   {
                spdlog::info("Successfully received data: {}", data.getName().toUri());
                initOnData(data); }, [this](const Interest &, const lp::Nack &nack)
                                   { spdlog::warn("Received Nack for interest: {}", nack.getInterest().getName().toUri()); }, [this](const Interest &interest)
                                   { spdlog::error("Interest timed out: {}", interest.getName().toUri()); });

            spdlog::info("Sent interest: {}", interestName.toUri());
        }
    }

    void
    SplitInterestsAdaptive::schedulePackets()
    {
        spdlog::debug("SplitInterestsAdaptive::schedulePackets() called");
        sendInitialInterest();
    }

    void SplitInterestsAdaptive::safe_WindowIncrement(double value)
    {

        m_cwnd += value;
    }
    void SplitInterestsAdaptive::safe_WindowDecrement(double value)
    {

        m_cwnd -= value;
    }
    void SplitInterestsAdaptive::safe_setWindowSize(double value)
    {
        m_cwnd = value;
    }

    void SplitInterestsAdaptive::safe_setSsthresh(double value)
    {
        m_ssthresh = value;
    }

    void SplitInterestsAdaptive::safe_InFlightIncrement()
    {
        m_nInFlight++;
    }
    void SplitInterestsAdaptive::safe_InFlightDecrement()
    {
        m_nInFlight--;
    }

    double SplitInterestsAdaptive::safe_getWindowSize()
    {
        return m_cwnd;
    }

    int64_t SplitInterestsAdaptive::safe_getInFlight()
    {
        return m_nInFlight;
    }

    double SplitInterestsAdaptive::safe_getSsthresh()
    {
        return m_ssthresh;
    }

    void
    SplitInterestsAdaptive::initOnData(const Data &data)
    {
        spdlog::info("Data received: {}", data.getName().toUri());

        // Add the received data name to a set to track received interests
        m_receivedinitialInterests.insert(data.getName());

        // Check if all interests have been received
        if (m_receivedinitialInterests.size() == m_aggTree.interestNames.size())
        {
            spdlog::info("All initial interests have been successfully received.");
            // Schedule sending interests for all interest names
            for (auto &interestName : m_aggTree.interestNames)
            {
                m_scheduler.schedule(time::milliseconds(0), [this, &interestName]
                                     { sendInterest(interestName); });
            }
        }
    }

} // namespace ndn::chunks