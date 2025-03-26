#include "aggregator.hpp"

#include <ndn-cxx/metadata-object.hpp>
#include <ndn-cxx/util/segmenter.hpp>
#include <ndn-cxx/security/validator-null.hpp>
#include <iostream>
#include "../request.hpp"
#include "../pipeline/discover-version.hpp"
#include "../pipeline/statistics-collector.hpp"

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

namespace ndn::chunks
{
    Aggregator::Aggregator(const Name &prefix, Face &face, KeyChain &keyChain,
                           const Options &opts, uint64_t chunkNumber, uint64_t totalChunkNumber)
        : m_face(face), m_keyChain(keyChain), m_options(opts), m_totalChunkNumber(totalChunkNumber), m_scheduler(m_face.getIoContext())
    {
        spdlog::debug("Aggregator::Aggregator()");

        m_prefix = prefix;

        try
        {
            boost::property_tree::ptree tree;
            boost::property_tree::ini_parser::read_ini("../experiments/aggregatorcat.ini", tree);
            m_numFaces = tree.get<size_t>("General.num-faces", 2);
            spdlog::info("Read num-faces = {} from config file", m_numFaces);
        }
        catch (const std::exception &e)
        {
            spdlog::warn("Error reading config file: {}", e.what());
            m_numFaces = 2;
        }
        // register m_prefix without Interest handler
        m_face.registerPrefix(m_prefix, nullptr, [this](const Name &prefix, const auto &reason)
                              {            
            spdlog::error("ERROR: Failed to register prefix '{}'({})", prefix.toUri(), boost::lexical_cast<std::string>(reason));
            m_face.shutdown(); });

        face.setInterestFilter(m_prefix, [this](const auto &, const auto &interest)
                               { processSegmentInterest(interest); });

        if (!m_options.isQuiet)
        {
            std::cerr << "Registered prefix: " << m_prefix << "\n";
            spdlog::info("Registered prefix: {}", m_prefix.toUri());
        }
    }
    Aggregator::~Aggregator()
    {
        m_flowController->clearStreamCache();
        delete m_request;
    }
    void
    Aggregator::run()
    {
        spdlog::debug("Aggregator::run()");
        m_request = new Request("../experiments/aggregatorcat.ini", this);
        m_face.processEvents();
        spdlog::debug("Aggregator::run() end");
    }

    void Aggregator::initializeFromInterest(const Interest &interest)
    {
        // Get structured child node information
        m_childNodeInfos = parseChildNodes(interest);

        // Also keep backward compatibility with the old structure
        for (const auto &childInfo : m_childNodeInfos)
        {
            m_childNodes.push_back(childInfo.name);
        }

        // Log the parsed child nodes if verbose mode is enabled
        if (m_options.isVerbose)
        {
            spdlog::debug("Parsed child nodes:");
            for (const auto &info : m_childNodeInfos)
            {
                spdlog::info("  Child node: {}, Interest to send: {}",
                             info.name,
                             info.interestName.toUri());
            }
        }

        // Extract just the node names for flow controller
        std::vector<std::string> childNodeNames;
        for (const auto &info : m_childNodeInfos)
        {
            childNodeNames.push_back(info.name);
        }

        // Create and initialize flow controller
        m_flowController = FlowController::createFromChildNodeInfos(
            "../experiments/aggregatorcat.ini",
            m_childNodeInfos); // Pass node names instead of ChildNodeInfo objects
    }
    void
    Aggregator::processSegmentInterest(const Interest &interest)
    {
        spdlog::info("Received interest: {}", interest.getName().toUri());
        // Parse child node names from the interest
        static bool isFirst = true;
        if (isFirst)
        {
            isFirst = false;
            initializeFromInterest(interest);
            storeOriginalInterest(interest);
            sendInitialInterest(interest);
            return;
        }
        respondToInterest(interest);
    }

    void Aggregator::storeOriginalInterest(const Interest &interest)
    {
        m_originalInterest = interest;
        m_hasOriginalInterest = true;
        spdlog::debug("Stored original interest: {}", interest.getName().toUri());
    }

    void Aggregator::respondToInterest(const Interest &interest)
    {
        const Name &name = interest.getName();
        uint64_t chunkNo = std::stoi(name[-2].toUri());
        if (m_flowController->isChunkProcessed(chunkNo))
        {
            spdlog::debug("Producer::processSegmentInterest()");
            if (m_options.isVerbose)
            {
                std::cerr << "Interest: " << interest << "\n";
                spdlog::info("Interest: {}", interest.getName().toUri());
            }
            const Name &name = interest.getName();
            uint64_t chunkNo = std::stoi(name[-2].toUri());
            spdlog::debug("chunkNo is {}", chunkNo);

            if (m_store[chunkNo].empty())
            {
                // spdlog::debug("temporarily no data");
                // return;
                segmentationChunk(chunkNo, interest);
            }
            BOOST_ASSERT(!m_store[chunkNo].empty());

            std::shared_ptr<Data> data;

            if (name.size() == m_chunkedPrefix.size() + 1 && name[-1].isSegment())
            {
                const auto segmentNo = static_cast<size_t>(interest.getName()[-1].toSegment());
                // specific segment retrieval
                if (segmentNo < m_store[chunkNo].size())
                {
                    data = m_store[chunkNo][segmentNo];
                    m_nSentSegments[chunkNo]++;
                }
            }
            else if (interest.matchesData(*m_store[chunkNo][0]))
            {

                // unspecified version or segment number, return first segment
                data = m_store[chunkNo][0];
                m_nSentSegments[chunkNo] = 1;
            }

            if (data != nullptr)
            {
                if (m_options.isVerbose)
                {
                    std::cerr << "Data: " << *data << "\n";
                    spdlog::info("Data: {}", (*data).getName().toUri());
                }
                m_face.put(*data);

                // check all the segments are sent
                const Name &dataName = data->getName();
                if (dataName.size() > m_chunkedPrefix.size() && dataName[-1].isSegment())
                {
                    uint64_t sentSegments = m_nSentSegments[chunkNo];
                    auto it = m_store.find(chunkNo);
                    if (it != m_store.end())
                    {
                        size_t totalSegments = it->second.size();
                        if (sentSegments == totalSegments)
                        {
                            m_store.erase(chunkNo);
                            spdlog::debug("Cleared chunk {} after sending {} segments ", chunkNo, sentSegments);
                        }
                    }
                }
            }
            else
            {
                if (m_options.isVerbose)
                {
                    std::cerr << "Interest cannot be satisfied, sending Nack\n";
                }
                spdlog::warn("Interest cannot be satisfied, sending Nack");
                m_face.put(lp::Nack(interest));
            }
        }
        else
        {
            spdlog::warn("Chunk {} not processed yet ischunkprocessed is {}", chunkNo, m_flowController->isChunkProcessed(chunkNo));
            if (m_respondEvents[name.toUri()])
            {
                m_respondEvents[name.toUri()].cancel();
            }
            m_respondEvents[name.toUri()] = m_scheduler.schedule(time::milliseconds(0), [this, interest]
                                                                 { respondToInterest(interest); });
        }
    }
    void Aggregator::respondToOriginalInterest()
    {
        if (!m_hasOriginalInterest)
        {
            spdlog::error("Cannot respond to original interest: no interest stored");
            return;
        }

        try
        {
            // Create a consolidated response summarizing child node responses
            std::string responseContent = "Initialization complete. Child node statuses:\n";

            for (const auto &[nodeName, response] : m_initializationResponses)
            {
                responseContent += "- " + nodeName + ": " +
                                   (response.empty() ? "OK" : response) + "\n";
            }

            // Create a response data packet
            auto responseData = make_shared<Data>(m_originalInterest.getName());
            spdlog::debug("Responding to original interest: {}", m_originalInterest.getName().toUri());
            responseData->setFreshnessPeriod(time::seconds(10));
            responseData->setContent(makeStringBlock(tlv::Content, responseContent));

            // Sign the data packet
            m_keyChain.sign(*responseData);

            // Send the data packet back
            m_face.put(*responseData);

            spdlog::info("Sent aggregated initialization response for {} child nodes",
                         m_initializationResponses.size());
        }
        catch (const std::exception &e)
        {
            spdlog::error("Error responding to original interest: {}", e.what());
        }
    }
    void Aggregator::onInitialData(const Interest &interest, const Data &data, const std::string &childName)
    {
        // Log receipt of initialization response
        spdlog::info("Received initialization response from child node: {}", childName);

        try
        {
            // Extract content from the data packet
            const Block &content = data.getContent();
            std::string contentString(reinterpret_cast<const char *>(content.value()), content.value_size());

            spdlog::debug("Initialization content from {}: {}", childName, contentString);

            // Mark this node as initialized and store its response
            m_initializedNodes[childName] = true;
            m_initializationResponses[childName] = contentString;

            // Check if all child nodes have been initialized
            bool allInitialized = true;
            for (const auto &info : m_childNodeInfos)
            {
                if (m_initializedNodes.find(info.name) == m_initializedNodes.end() ||
                    !m_initializedNodes[info.name])
                {
                    allInitialized = false;
                    break;
                }
            }

            if (allInitialized)
            {
                spdlog::info("All child nodes have been successfully initialized, responding to original interest");
                respondToOriginalInterest();

                // Initialize and start (blocking until completion)
                std::thread requestThread([this]()
                                          {
                    if (m_request->initialize())
                    {
                        m_request->start();
                    } });
                requestThread.detach();
            }
            else
            {
                spdlog::debug("Waiting for more child nodes to initialize ({}/{} received)",
                              m_initializedNodes.size(), m_childNodeInfos.size());
            }
        }
        catch (const std::exception &e)
        {
            spdlog::error("Error processing initialization data from {}: {}", childName, e.what());
        }
    }

    void Aggregator::onInitialNack(const Interest &interest, const lp::Nack &nack, const std::string &childName)
    {
        spdlog::warn("Received Nack from child node {}: {}", childName, boost::lexical_cast<std::string>(nack.getReason()));
    }

    void Aggregator::onInitialTimeout(const Interest &interest, const std::string &childName)
    {
        spdlog::warn("Initialization interest to {} timed out", childName);
    }

    void Aggregator::sendInitialInterest(const Interest &interest)
    {
        if (m_childNodeInfos.empty())
        {
            spdlog::error("No child nodes to send initialization interests to");
            return;
        }

        spdlog::info("Sending initialization interests to {} child nodes", m_childNodeInfos.size());

        // Send initialization interest to each child node
        for (const auto &childInfo : m_childNodeInfos)
        {
            try
            {
                // Use the pre-constructed interest name directly
                // This already includes the child name, substructure, and "init" component
                // as constructed in parseChildNodes()

                // Create and express the interest
                Interest initialInterest(childInfo.interestName);
                initialInterest.setCanBePrefix(false);
                initialInterest.setMustBeFresh(true);
                initialInterest.setInterestLifetime(time::seconds(4));

                m_face.expressInterest(
                    initialInterest,
                    bind(&Aggregator::onInitialData, this, _1, _2, childInfo.name),
                    bind(&Aggregator::onInitialNack, this, _1, _2, childInfo.name),
                    bind(&Aggregator::onInitialTimeout, this, _1, childInfo.name));

                spdlog::info("Sent initialization interest to child node {}: {}",
                             childInfo.name, childInfo.interestName.toUri());
            }
            catch (const std::exception &e)
            {
                spdlog::error("Failed to send initialization interest to {}: {}", childInfo.name, e.what());
            }
        }
    }

    std::vector<ChildNodeInfo> Aggregator::parseChildNodes(const Interest &interest)
    {
        std::vector<ChildNodeInfo> childInfos;

        const Name &interestName = interest.getName();
        if (interestName.size() < 2)
        {
            // Log warning if interest name has fewer than 2 components
            spdlog::warn("Interest name has fewer than 2 components: {}", interestName.toUri());
            return childInfos;
        }

        // Get the second component which contains child node information
        const name::Component &component = interestName[1];

        // Convert component to string
        std::string componentStr = component.toUri(name::UriFormat::CANONICAL);

        // Decode URL-encoded special characters
        boost::algorithm::replace_all(componentStr, "%28", "(");
        boost::algorithm::replace_all(componentStr, "%29", ")");
        boost::algorithm::replace_all(componentStr, "%2B", "+");

        // Remove component type identifier if present
        size_t startPos = componentStr.find_first_of('=');
        if (startPos != std::string::npos)
        {
            componentStr = componentStr.substr(startPos + 1);
        }

        spdlog::info("Parsing component: {}", componentStr);

        // Collect any trailing components (checking for initialization component)
        std::vector<ndn::name::Component> trailingComponents;
        std::string specialCommand; // For storing special commands like "init"

        // Check if there's a special command like "init" at the end
        if (interestName.size() > 2)
        {
            const auto &lastComponent = interestName[-1];
            std::string lastComponentStr = lastComponent.toUri(name::UriFormat::CANONICAL);

            if (lastComponentStr == "8=init" || lastComponentStr == "init")
            {
                specialCommand = "init";
                // Only collect components before the special command
                for (size_t i = 2; i < interestName.size() - 1; ++i)
                {
                    trailingComponents.push_back(interestName[i]);
                }
            }
            else
            {
                // No special command, collect all trailing components
                for (size_t i = 2; i < interestName.size(); ++i)
                {
                    trailingComponents.push_back(interestName[i]);
                }
            }
        }

        // Log what we found
        if (!specialCommand.empty())
        {
            spdlog::info("Detected special command: {}", specialCommand);
        }
        if (!trailingComponents.empty())
        {
            spdlog::info("Collected {} trailing components", trailingComponents.size());
        }

        // Parse direct child nodes
        size_t pos = 0;
        while (pos < componentStr.length())
        {
            // Find the start of a child node
            size_t nodeStart = pos;
            size_t nodeEnd = componentStr.find_first_of("(+", pos);

            if (nodeEnd == std::string::npos)
            {
                // No brackets or plus sign, the entire remaining part is a node name
                std::string nodeName = componentStr.substr(nodeStart);

                ChildNodeInfo info;
                info.name = nodeName;

                // Create basic interest name for leaf node and add any trailing components
                info.interestName = Name("/" + nodeName);

                // Add trailing components
                for (const auto &comp : trailingComponents)
                {
                    info.interestName.append(comp);
                }

                // If there's a special command, add it too
                if (!specialCommand.empty())
                {
                    info.interestName.append(specialCommand);
                }

                childInfos.push_back(info);
                break;
            }

            // Extract the node name
            std::string nodeName = componentStr.substr(nodeStart, nodeEnd - nodeStart);
            ChildNodeInfo info;
            info.name = nodeName;

            // If followed by opening bracket, extract the substructure
            if (componentStr[nodeEnd] == '(')
            {
                int bracketCount = 1;
                size_t substructureStart = nodeEnd + 1;
                size_t substructureEnd = substructureStart;

                // Find the matching closing bracket
                while (bracketCount > 0 && substructureEnd < componentStr.length())
                {
                    if (componentStr[substructureEnd] == '(')
                        bracketCount++;
                    else if (componentStr[substructureEnd] == ')')
                        bracketCount--;
                    substructureEnd++;
                }

                if (bracketCount == 0)
                {
                    // Successfully found matching brackets
                    substructureEnd--; // Adjust to point to the closing bracket
                    info.substructure = componentStr.substr(substructureStart, substructureEnd - substructureStart);

                    // Build the child node's interest name with substructure
                    info.interestName = Name("/" + nodeName);
                    info.interestName.append(info.substructure);

                    // Add trailing components
                    for (const auto &comp : trailingComponents)
                    {
                        info.interestName.append(comp);
                    }

                    // If there's a special command, add it too
                    if (!specialCommand.empty())
                    {
                        info.interestName.append(specialCommand);
                    }

                    // Update position to after the substructure
                    pos = substructureEnd + 1;

                    // Skip the plus sign if present
                    if (pos < componentStr.length() && componentStr[pos] == '+')
                    {
                        pos++;
                    }
                }
                else
                {
                    spdlog::error("Unbalanced brackets in component: {}", componentStr);
                    break;
                }
            }
            else if (componentStr[nodeEnd] == '+')
            {
                // Node name followed by plus sign, no substructure
                info.interestName = Name("/" + nodeName);

                // Add trailing components
                for (const auto &comp : trailingComponents)
                {
                    info.interestName.append(comp);
                }

                // If there's a special command, add it too
                if (!specialCommand.empty())
                {
                    info.interestName.append(specialCommand);
                }

                pos = nodeEnd + 1; // Skip the plus sign
            }

            childInfos.push_back(info);
        }

        // Log parsing results
        spdlog::info("Parsed {} direct child nodes from interest {}", childInfos.size(), interestName.toUri());
        for (const auto &info : childInfos)
        {
            spdlog::info("Child: {}, Substructure: {}, Interest: {}",
                         info.name,
                         info.substructure.empty() ? "<none>" : info.substructure,
                         info.interestName.toUri());
        }

        return childInfos;
    }

    std::vector<std::pair<Name, size_t>> Aggregator::getChildInterestNames() const
    {
        std::vector<std::pair<Name, size_t>> result;

        if (m_childNodeInfos.empty())
        {
            spdlog::warn("No child interest names available: child node information not initialized");
            return result;
        }

        size_t faceCount = m_numFaces > 0 ? m_numFaces : 1;

        // remove init component
        size_t faceIndex = 0;
        for (const auto &childInfo : m_childNodeInfos)
        {
            Name interestName = childInfo.interestName;

            if (interestName.size() > 0 &&
                (interestName[-1].toUri(name::UriFormat::CANONICAL) == "8=init" ||
                 interestName[-1].toUri(name::UriFormat::CANONICAL) == "init"))
            {
                interestName = interestName.getPrefix(-1);
                spdlog::debug("Removed 'init' component from interest name: {}", interestName.toUri());
            }

            size_t assignedFace = faceIndex % faceCount;

            result.emplace_back(interestName, assignedFace);
            faceIndex++;
            spdlog::debug("Added child interest: {} with face index {}", interestName.toUri(), assignedFace);
        }

        spdlog::info("Returning {} child interest names with {} available faces", result.size(), faceCount);
        return result;
    }

    std::shared_ptr<FlowController> Aggregator::getFlowController() const
    {
        return m_flowController;
    }

    void
    Aggregator::segmentationChunk(uint64_t chunkNumber, const Interest &interest)
    {
        std::istream *is = m_flowController->getProcessedChunkAsStream(chunkNumber);
        if (is != nullptr && m_options.isVerbose)
        {

            std::streampos origPos = is->tellg();

            std::string content;
            char buffer[4096];
            while (is->read(buffer, sizeof(buffer)) || is->gcount())
            {
                content.append(buffer, is->gcount());
            }

            std::cerr << "\n===== Chunk " << chunkNumber << " Content ====="
                      << "\nSize: " << content.size() << " bytes\n"
                      << "First 512 bytes (HEX):\n";

            const size_t printLimit = 512;
            for (size_t i = 0; i < std::min(content.size(), printLimit); ++i)
            {
                printf("%02X ", static_cast<unsigned char>(content[i]));
                if ((i + 1) % 16 == 0)
                    std::cerr << '\n';
            }

            std::cerr << "\n\nASCII预览:\n";
            for (size_t i = 0; i < std::min(content.size(), printLimit); ++i)
            {
                if (isprint(content[i]))
                {
                    std::cerr << content[i];
                }
                else
                {
                    std::cerr << '.';
                }
            }
            std::cerr << "\n==============================\n";

            is->clear();
            is->seekg(origPos);
        }
        else
        {
            std::cerr << "ERROR: Null stream for chunk " << chunkNumber << "\n";
        }
        m_chunkedPrefix = interest.getName().getPrefix(-1);
        if (!m_options.isQuiet)
        {
            std::cerr << "Loading input ...\n";
            spdlog::info("Loading input ...");
        }
        Segmenter segmenter(m_keyChain, m_options.signingInfo);
        // All the data packets are segmented and stored in m_store
        m_store[chunkNumber] = segmenter.segment(*is, m_chunkedPrefix, m_options.maxSegmentSize, m_options.freshnessPeriod);
        if (!m_options.isQuiet)
        {
            std::cerr << "Published " << m_store[chunkNumber].size() << " Data packet" << (m_store[chunkNumber].size() > 1 ? "s" : "")
                      << " with prefix " << m_chunkedPrefix << "\n";
            spdlog::info("Published {} Data packet(s) with prefix {}", m_store[chunkNumber].size(), m_chunkedPrefix.toUri());
        }
    }
} // namespace ndn::chunks