#include "split-interests-adaptive.hpp"
#include "../pipeline/data-fetcher.hpp"
#include "../pipeline/discover-version.hpp"
#include "../chunk/consumer.hpp"
#include "aggregator.hpp"

#include <boost/lexical_cast.hpp>
#include <ndn-cxx/security/validator-null.hpp>
#include <iomanip>
#include <iostream>
#include <fstream>

namespace ndn::chunks
{

    SplitInterestsAdaptive::SplitInterestsAdaptive(std::vector<std::reference_wrapper<Face>> faces,
                                                   RttEstimatorWithStats &rttEstimator,
                                                   const Options &opts, Aggregator *aggregator)
        : SplitInterests(std::move(faces), opts, aggregator),
          m_cwnd(m_options.initCwnd),
          m_ssthresh(m_options.initSsthresh),
          m_rttEstimator(rttEstimator)
    {
        // Create a scheduler for each Face
        for (size_t i = 0; i < getFaceCount(); ++i)
        {
            m_schedulers.push_back(std::make_unique<Scheduler>(getFace(i).getIoContext()));
        }
    }

    SplitInterestsAdaptive::~SplitInterestsAdaptive()
    {
        cancel();
    }

    void
    SplitInterestsAdaptive::doRun()
    {
        spdlog::debug("SplitInterestsAdaptive::doRun() called");
        if (allSplitReceived())
        {
            cancel();
            return;
        }
        if (m_recordEvent)
        {
            m_recordEvent.cancel();
        }
        m_recordEvent = m_schedulers[0]->schedule(time::milliseconds(0), [this]
                                                  { recordThroughput(); });
        schedulePackets();
    }

    void
    SplitInterestsAdaptive::doCancel()
    {
        m_recordEvent.cancel();
        m_splitInfo.clear();
    }

    void
    SplitInterestsAdaptive::sendInterest(Name &interestName, size_t faceIndex)
    {
        if (isStopping())
            return;

        std::string firstComponent = interestName.get(0).toUri();

        if (m_options.isVerbose)
        {
            std::cerr << "Requesting interest with first component: " << firstComponent << " on Face #" << faceIndex << "\n";
            spdlog::debug("Requesting interest with first component: {} on Face #{}", firstComponent, faceIndex);
        }

        SplitInfo &splitInfo = m_splitInfo[firstComponent];
        try
        {
            splitInfo.consumer = new Consumer(security::getAcceptAllValidator());

            auto discover = std::make_unique<DiscoverVersion>(getFace(faceIndex), interestName, m_options);
            std::unique_ptr<ChunksInterests> chunks =
                std::make_unique<ChunksInterestsAdaptive>(getFace(faceIndex), m_rttEstimator, m_options);
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

    size_t
    SplitInterestsAdaptive::getNextFaceIndex()
    {
        // Simple round-robin strategy
        size_t index = m_nextFaceIndex;
        m_nextFaceIndex = (m_nextFaceIndex + 1) % getFaceCount();
        return index;
    }

    void
    SplitInterestsAdaptive::recordThroughput()
    {
        std::lock_guard<std::mutex> lock(m_receivedMutex);
        static bool firstTime = true;
        time::steady_clock::time_point now = time::steady_clock::now();
        using namespace ndn::time;
        duration<double, milliseconds::period> timeElapsed = now - m_timeStamp;
        if (timeElapsed > m_options.recordingCycle)
        {
            double throughput = 8 * (*m_received) / (m_options.recordingCycle.count() / 1000.0);
            *m_received = 0;

            // Read parameters from topology file
            std::ifstream topoFile(m_options.topoFile);
            std::string line;
            std::string filename;
            if (topoFile.is_open())
            {
                while (std::getline(topoFile, line))
                {
                    if (line.find("agg0:pro0") != std::string::npos)
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
        // Use the first Face's scheduler to handle throughput recording
        m_recordEvent = m_schedulers[0]->schedule(time::milliseconds(0), [this]
                                                  { recordThroughput(); });
    }

    void
    SplitInterestsAdaptive::schedulePackets()
    {
        spdlog::debug("SplitInterestsAdaptive::schedulePackets() called");

        if (m_aggregator)
        {
            m_schedulers[0]->schedule(time::milliseconds(0), [this]()
                                      { sendInterestsFromAggregator(); });
        }
    }

    void SplitInterestsAdaptive::sendInterestsFromAggregator()
    {
        if (!m_aggregator)
        {
            spdlog::error("Cannot send interests: no Aggregator set");
            return;
        }

        try
        {
            auto childInterests = m_aggregator->getChildInterestNames();

            if (childInterests.empty())
            {
                spdlog::warn("No child interests available from Aggregator");
                return;
            }

            spdlog::info("Sending {} interests from Aggregator configuration", childInterests.size());

            for (const auto &[interestName, faceIndex] : childInterests)
            {
                sendInterestToChild(interestName, faceIndex);
            }
        }
        catch (const std::exception &e)
        {
            spdlog::error("Error sending interests from Aggregator: {}", e.what());
        }
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

    // Method for public API to send an interest to a specific child
    void SplitInterestsAdaptive::sendInterestToChild(const Name &interestName, size_t faceIndex)
    {
        if (isStopping())
            return;

        if (faceIndex >= getFaceCount())
        {
            spdlog::error("Invalid face index: {}, max allowed: {}", faceIndex, getFaceCount() - 1);
            return;
        }

        m_schedulers[faceIndex]->schedule(time::milliseconds(0),
                                          [this, interestName, faceIndex]()
                                          {
                                              Name nameCopy = interestName;
                                              sendInterest(nameCopy, faceIndex);
                                          });
    }

} // namespace ndn::chunks