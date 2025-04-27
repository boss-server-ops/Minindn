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

    SplitInterestsAdaptive::SplitInterestsAdaptive(std::vector<std::reference_wrapper<Face>> faces,
                                                   RttEstimatorWithStats &rttEstimator,
                                                   const Options &opts)
        : SplitInterests(std::move(faces), opts),
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
                                   "_queue" + max_queue_size + "_loss" + loss + "type" + m_options.pipelineType + ".txt";
                        break; // Stop after processing the first valid line
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
        if (!isStopping() && !allSplitReceived())
        {
            if (m_recordEvent)
            {
                m_recordEvent.cancel();
            }
            m_recordEvent = m_schedulers[0]->schedule(time::milliseconds(0), [this]
                                                      { recordThroughput(); });
        }
        else
        {
            spdlog::info("Recording throughput stopped as all splits are received or pipeline is stopping");
        }
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

            // Use the first Face to send initial interest packets
            getFace(0).expressInterest(interest, [this](const Interest &, const Data &data)
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

        // Add received data name to the set to track received interests
        m_receivedinitialInterests.insert(data.getName());

        // Check if all interests have been received
        if (m_receivedinitialInterests.size() == m_aggTree.interestNames.size())
        {
            spdlog::info("All initial interests have been successfully received.");

            // Distribute interests across available Faces
            for (size_t i = 0; i < m_aggTree.interestNames.size(); ++i)
            {
                std::cerr << "aggtree interest name size" << m_aggTree.interestNames.size() << std::endl;
                size_t faceIndex = i % getFaceCount(); // Simple distribution across Faces
                Name &interestName = m_aggTree.interestNames[i];

                m_schedulers[faceIndex]->schedule(time::milliseconds(0),
                                                  [this, &interestName, faceIndex]
                                                  { sendInterest(interestName, faceIndex); });
            }
        }
    }

} // namespace ndn::chunks