#include "pipeliner.hpp"
#include "../chunk/chunks-interests-adaptive.hpp"
#include <spdlog/spdlog.h>
#include <ndn-cxx/util/exception.hpp>

namespace ndn::chunks
{

  Pipeliner::Pipeliner(security::Validator &validator, std::ostream &os)
      : m_validator(validator), m_outputStream(os)
  {
  }

  void
  Pipeliner::run(std::unique_ptr<DiscoverVersion> discover, std::unique_ptr<PipelineInterests> pipeline)
  {
    m_discover = std::move(discover);
    m_pipeline = std::move(pipeline);
    m_nextToPrint = 0;
    m_bufferedData.clear();

    m_discover->onDiscoverySuccess.connect([this](const Name &versionedName)
                                           { m_pipeline->run(versionedName,
                                                             FORWARD_TO_MEM_FN(handleData),
                                                             [](const std::string &msg)
                                                             { NDN_THROW(std::runtime_error(msg)); }); });
    m_discover->onDiscoveryFailure.connect([](const std::string &msg)
                                           { NDN_THROW(std::runtime_error(msg)); });
    m_discover->run();
  }

  void
  Pipeliner::handleData(const Data &data)
  {
    auto dataPtr = data.shared_from_this();

    m_validator.validate(data, [this, dataPtr](const Data &data)
                         {
                           if (data.getContentType() == ndn::tlv::ContentType_Nack)
                           {
                             NDN_THROW(ApplicationNackError(data));
                           }

                           // 'data' passed to callback comes from DataValidationState and was not created with make_shared
                           m_bufferedData[getSegmentFromPacket(data)] = dataPtr;
                           // writeInOrderData();
                         },
                         [](const Data &, const security::ValidationError &error)
                         { NDN_THROW(DataValidationError(error)); });
    if (m_pipeline->allSegmentsReceived())
    {
      m_pipeline->getChunker()->onData(m_bufferedData);
      m_bufferedData.clear();
    }
  }

  // void
  // Pipeliner::writeInOrderData()
  // {
  //   for (auto it = m_bufferedData.begin();
  //        it != m_bufferedData.end() && it->first == m_nextToPrint;
  //        it = m_bufferedData.erase(it), ++m_nextToPrint)
  //   {
  //     const Block &content = it->second->getContent();
  //     m_outputStream.write(reinterpret_cast<const char *>(content.value()), content.value_size());
  //   }
  //   m_pipeline->getChunker()->schedulePackets();
  //   std::cerr << "Finished segments data" << std::endl;
  //   spdlog::info("Finished segments data");
  // }

} // namespace ndn::chunks
