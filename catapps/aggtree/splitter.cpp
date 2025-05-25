#include "splitter.hpp"
#include "split-interests.hpp"

#include <ndn-cxx/util/exception.hpp>
#include <spdlog/spdlog.h>

namespace ndn::chunks
{

    Splitter::Splitter(security::Validator &validator, std::ostream &os)
        : m_validator(validator), m_outputStream(os)
    {
    }

    void
    Splitter::run(std::unique_ptr<DiscoverVersion> discover, std::unique_ptr<SplitInterests> splits)
    {
        m_discover = std::move(discover);
        m_split = std::move(splits);
        m_nextToPrint = 0;
        m_bufferedData.clear();

        m_discover->onDiscoverySuccess.connect([this](const Name &versionedName)
                                               { m_split->run(versionedName,
                                                              FORWARD_TO_MEM_FN(handleData),
                                                              [](const std::string &msg)
                                                              { NDN_THROW(std::runtime_error(msg)); }); });
        m_discover->onDiscoveryFailure.connect([](const std::string &msg)
                                               { NDN_THROW(std::runtime_error(msg)); });
        m_discover->run();
        spdlog::debug("Splitter::run() finished");
    }

    void
    Splitter::handleData(std::map<uint64_t, std::shared_ptr<const Data>> &data)
    {
        return;
    }

} // namespace ndn::chunks
