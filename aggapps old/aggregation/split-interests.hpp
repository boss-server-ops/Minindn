#ifndef IMAgg_SPLITS_INTERESTS_HPP
#define IMAgg_SPLITS_INTERESTS_HPP

#include "../../core/common.hpp"
#include "../pipeline/options.hpp"
#include "../controller/controller.hpp"
#include <ndn-cxx/face.hpp>
#include <unordered_set>
#include <ndn-cxx/name.hpp>

#include <functional>
#include <mutex>
#include <vector>

namespace ndn::chunks
{
    class Aggregator;

    /**
     * @brief Service for retrieving Data via an Interest pipeline
     *
     * Retrieves all segments of Data under a given prefix by maintaining a (variable or fixed-size)
     * window of N Interests in flight. A user-specified callback function is used to notify
     * the arrival of each segment of Data.
     *
     * No guarantees are made as to the order in which segments are fetched or callbacks are invoked,
     * i.e. out-of-order delivery is possible.
     */
    class SplitInterests : noncopyable
    {
    public:
        /**
         * @brief Constructor.
         *
         * Configures the pipelining service without specifying the retrieval namespace.
         * After construction, the method run() must be called in order to start the pipeline.
         */
        SplitInterests(std::vector<std::reference_wrapper<Face>> faces, const Options &opts,
                       Aggregator *aggregator);

        virtual ~SplitInterests();

        using DataCallback = std::function<void(std::map<uint64_t, std::shared_ptr<const Data>> &data)>;
        using FailureCallback = std::function<void(const std::string &reason)>;

        /**
         * @brief start fetching all the segments of the specified prefix
         *
         * @param versionedName the name of the segmented Data ending with a version number
         * @param onData callback for every segment correctly received, must not be empty
         * @param onFailure callback if an error occurs, may be empty
         */
        void
        run(const Name &versionedName, DataCallback onData, FailureCallback onFailure);

        /**
         * @brief stop all fetch operations
         */
        void
        cancel();

        /**
         * @return number of received splits
         */
        int64_t
        getReceivedSplit();

        /**
         * @brief Get the size of received segments within a recording cycle.
         */
        size_t *getReceived();

        /**
         * @brief other classes can call this method to increment the number of received splits
         */
        void
        receivedSplitincrement();

        /**
         * @brief print statistics about this fetching session
         *
         * Subclasses can override this method to print additional stats or change the summary format
         */
        virtual void
        printSummary() const;

        /**
         * @brief subclasses must call this method to notify successful retrieval of a split
         */
        void
        onData(std::map<uint64_t, std::shared_ptr<const Data>> &data);

        void
        ReceivedFlowIncrement()
        {
            m_nReceivedFlow++;
        }

        /**
         * @brief Set the Aggregator to use for interest coordination
         * @param aggregator pointer to the Aggregator instance
         */
        void setAggregator(Aggregator *aggregator) { m_aggregator = aggregator; }

        /**
         * @brief Get the current Aggregator
         * @return pointer to the current Aggregator, or nullptr if none set
         */
        Aggregator *getAggregator() const { return m_aggregator; }

        std::shared_ptr<FlowController> m_flowController;
        Aggregator *m_aggregator{nullptr}; ///< pointer to Aggregator for coordination (may be null)

    protected:
        time::steady_clock::time_point
        getStartTime() const
        {
            return m_startTime;
        }

        void
        setStartTime(time::steady_clock::time_point time)
        {
            m_startTime = time;
        }

        bool
        isStopping() const
        {
            return m_isStopping;
        }

        /**
         * @brief check if the transfer is complete
         * @return true if all segments have been received, false otherwise
         */
        [[nodiscard]] bool
        allSplitReceived() const;

        /**
         * @return next segment number to retrieve
         * @post m_nextSegmentNo == return-value + 1
         */
        uint64_t
        getNextSplitNo();

        /**
         * @brief subclasses can call this method to signal an unrecoverable failure
         */
        void
        onFailure(const std::string &reason);

        void
        printOptions() const;

        /**
         * @param throughput The throughput in bits/s
         */
        static std::string
        formatThroughput(double throughput);

        /**
         * @brief Get a Face by index
         * @param index The index of the Face to retrieve
         * @return Reference to the Face
         */
        Face &
        getFace(size_t index)
        {
            return m_faces[index];
        }

        /**
         * @brief Get the number of Faces
         * @return The number of Faces available
         */
        size_t
        getFaceCount() const
        {
            return m_faces.size();
        }

    private:
        /**
         * @brief perform subclass-specific operations to fetch all the segments
         *
         * When overriding this function, at a minimum, the subclass should implement the retrieving
         * of all the segments. Subclass must guarantee that `onData` is called once for every
         * segment that is fetched successfully.
         *
         * @note m_lastSegmentNo contains a valid value only if m_hasFinalBlockId is true.
         */
        virtual void
        doRun() = 0;

        virtual void
        doCancel() = 0;

    protected:
        const Options &m_options;
        std::vector<std::reference_wrapper<Face>> m_faces;
        Name m_prefix;

        PUBLIC_WITH_TESTS_ELSE_PROTECTED : bool m_hasFinalSplitId = false; ///< true if the last split number is known
        uint64_t m_lastSplitNo = 0;                                        ///< valid only if m_hasFinalBlockId == true
        int64_t m_nReceived = 0;                                           ///< number of splits received
        size_t m_receivedSize = 0;                                         ///< size of received data in bytes
        time::steady_clock::time_point m_timeStamp;                        ///< used to record the throughput
        size_t *m_received = nullptr;
        mutable std::mutex m_receivedMutex;
        std::unordered_set<Name> m_receivedinitialInterests;

    private:
        DataCallback m_onData;
        FailureCallback m_onFailure;
        uint64_t m_nextSplitNo = 0;
        time::steady_clock::time_point m_startTime;
        bool m_isStopping = false;
        int64_t m_nReceivedFlow = 0;
        std::ofstream m_outputFile;
    };

} // namespace ndn::chunks

#endif // IMAgg_PIPELINE_INTERESTS_HPP