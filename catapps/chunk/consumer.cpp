#include "consumer.hpp"

#include <ndn-cxx/util/exception.hpp>
#include <spdlog/spdlog.h>

namespace ndn::chunks
{

    Consumer::Consumer(security::Validator &validator, std::ostream &os)
        : m_validator(validator), m_outputStream(os)
    {
    }

    void
    Consumer::run(std::unique_ptr<DiscoverVersion> discover, std::unique_ptr<ChunksInterests> chunks)
    {
        m_discover = std::move(discover);
        m_chunks = std::move(chunks);
        m_nextToPrint = 0;
        m_bufferedData.clear();

        m_discover->onDiscoverySuccess.connect([this](const Name &versionedName)
                                               { m_chunks->run(versionedName,
                                                               FORWARD_TO_MEM_FN(handleData),
                                                               [](const std::string &msg)
                                                               { NDN_THROW(std::runtime_error(msg)); }); });
        m_discover->onDiscoveryFailure.connect([](const std::string &msg)
                                               { NDN_THROW(std::runtime_error(msg)); });
        m_discover->run();
        spdlog::debug("Consumer::run() finished");
    }

    void
    Consumer::handleData(std::map<uint64_t, std::shared_ptr<const Data>> &data)
    {

        return;
    }

    // void
    // Consumer::writeInOrderData()
    // {
    //     for (auto it = m_bufferedData.begin();
    //          it != m_bufferedData.end() && it->first == m_nextToPrint;
    //          it = m_bufferedData.erase(it), ++m_nextToPrint)
    //     {
    //         const Block &content = it->second->getContent();
    //         m_outputStream.write(reinterpret_cast<const char *>(content.value()), content.value_size());
    //     }
    // }

} // namespace ndn::chunks
