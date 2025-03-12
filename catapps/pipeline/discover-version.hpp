#ifndef IMAgg_DISCOVER_VERSION_HPP
#define IMAgg_DISCOVER_VERSION_HPP

#include "options.hpp"

#include <ndn-cxx/face.hpp>
#include <ndn-cxx/util/signal.hpp>

namespace ndn::chunks
{

    class DataFetcher;

    /**
     * @brief Service for discovering the latest Data version.
     *
     * DiscoverVersion's user is notified once after identifying the latest retrievable version or
     * on failure to find any Data version.
     */
    class DiscoverVersion
    {
    public:
        DiscoverVersion(Face &face, const Name &prefix, const Options &options);
        DiscoverVersion(const DiscoverVersion &other);
        /**
         * @brief Signal emitted when the versioned name of Data is found.
         */
        signal::Signal<DiscoverVersion, Name> onDiscoverySuccess;

        /**
         * @brief Signal emitted when a failure occurs.
         */
        signal::Signal<DiscoverVersion, std::string> onDiscoveryFailure;

        void
        run();

        void
        setPrefix(Name &prefix);

    private:
        void
        handleData(const Interest &interest, const Data &data);

    private:
        Face &m_face;
        Name m_prefix;
        const Options &m_options;
        std::shared_ptr<DataFetcher> m_fetcher;
    };

} // namespace ndn::chunks

#endif //  IMAgg_DISCOVER_VERSION_HPP
