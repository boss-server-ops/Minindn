#include "discover-version.hpp"
#include "data-fetcher.hpp"

#include <ndn-cxx/metadata-object.hpp>

#include <iostream>

namespace ndn::chunks
{

  DiscoverVersion::DiscoverVersion(Face &face, const Name &prefix, const Options &options)
      : m_face(face), m_prefix(prefix), m_options(options)
  {
  }

  void
  DiscoverVersion::run()
  {
    if (m_options.disableVersionDiscovery || (!m_prefix.empty() && m_prefix[-1].isVersion()))
    {
      onDiscoverySuccess(m_prefix);
      return;
    }

    Interest interest = MetadataObject::makeDiscoveryInterest(m_prefix)
                            .setInterestLifetime(m_options.interestLifetime);

    m_fetcher = DataFetcher::fetch(m_face, interest, m_options.maxRetriesOnTimeoutOrNack, m_options.maxRetriesOnTimeoutOrNack, FORWARD_TO_MEM_FN(handleData), [this](const auto &, const auto &reason)
                                   { onDiscoveryFailure(reason); }, [this](const auto &, const auto &reason)
                                   { onDiscoveryFailure(reason); }, m_options.isVerbose);
  }

  void
  DiscoverVersion::setPrefix(Name &prefix)
  {
    m_prefix = prefix;
  }

  void
  DiscoverVersion::handleData(const Interest &interest, const Data &data)
  {
    if (m_options.isVerbose)
    {
      std::cerr << "Data: " << data << "\n";
      spdlog::info("Data: {}", data.getName().toUri());
    }

    // make a metadata object from received metadata packet
    MetadataObject mobject;
    try
    {
      mobject = MetadataObject(data);
    }
    catch (const tlv::Error &e)
    {
      onDiscoveryFailure("Invalid metadata packet: "s + e.what());
      return;
    }

    if (mobject.getVersionedName().empty() || !mobject.getVersionedName()[-1].isVersion())
    {
      onDiscoveryFailure(mobject.getVersionedName().toUri() + " is not a valid versioned name");
      return;
    }

    if (m_options.isVerbose)
    {
      std::cerr << "Discovered Data version: " << mobject.getVersionedName()[-1] << "\n";
      spdlog::info("Discovered Data version: {}", (mobject.getVersionedName()[-1]).toUri());
    }

    onDiscoverySuccess(mobject.getVersionedName());
  }

} // namespace ndn::chunks
