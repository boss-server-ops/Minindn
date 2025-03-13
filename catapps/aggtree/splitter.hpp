#ifndef IMAgg_CONSUMER_HPP
#define IMAgg_CONSUMER_HPP

#include "../pipeline/discover-version.hpp"
#include "split-interests.hpp"
#include "split-interests-adaptive.hpp"

#include <ndn-cxx/security/validation-error.hpp>
#include <ndn-cxx/security/validator.hpp>

#include <boost/lexical_cast.hpp>
#include <iostream>
#include <map>

namespace ndn::chunks
{

    /**
     * @brief Segmented version Splitter.
     *
     * Discover the latest version of the data published under a specified prefix, and retrieve all the
     * segments associated to that version. The segments are fetched in order and written to a
     * user-specified stream in the same order.
     */
    class Splitter : noncopyable
    {
    public:
        class ApplicationNackError : public std::runtime_error
        {
        public:
            explicit ApplicationNackError(const Data &data)
                : std::runtime_error("Application generated Nack: " + boost::lexical_cast<std::string>(data))
            {
            }
        };

        class DataValidationError : public std::runtime_error
        {
        public:
            explicit DataValidationError(const security::ValidationError &error)
                : std::runtime_error(boost::lexical_cast<std::string>(error))
            {
            }
        };

        /**
         * @brief Create the Splitter
         */
        explicit Splitter(security::Validator &validator, std::ostream &os = std::cout);

        /**
         * @brief Run the Splitter
         */
        void
        run(std::unique_ptr<DiscoverVersion> discover, std::unique_ptr<SplitInterests> splits);

    private:
        void
        handleData();

        PUBLIC_WITH_TESTS_ELSE_PRIVATE : void
                                         writeInOrderData();

    private:
        security::Validator &m_validator;
        std::ostream &m_outputStream;
        std::unique_ptr<DiscoverVersion> m_discover;
        std::unique_ptr<SplitInterests> m_split;
        uint64_t m_nextToPrint = 0;

        PUBLIC_WITH_TESTS_ELSE_PRIVATE : std::map<uint64_t, std::shared_ptr<const Data>> m_bufferedData;
    };

} // namespace ndn::chunks

#endif // IMAgg_CONSUMER_HPP
