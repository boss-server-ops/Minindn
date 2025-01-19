#ifndef SLIDING_WINDOW_HPP
#define SLIDING_WINDOW_HPP

#include <deque>
#include <numeric>
#include <chrono>
#include <spdlog/spdlog.h>

template <typename T>
class SlidingWindow
{
public:
    using Time = std::chrono::milliseconds;
    using Duration = std::chrono::milliseconds;

    struct DataInfo
    {
        Time arrivalTime; // Arrival time of the data
        T value;          // Value associated with the data, expected as qsf
    };

    SlidingWindow() : m_windowDuration(Duration(10)) {}

    SlidingWindow(Duration windowDuration) : m_windowDuration(windowDuration) {}

    void AddPacket(Time newTime, T value)
    {
        m_data.push_back({newTime, value});

        // Remove outdated packets
        while (!m_data.empty() && (newTime - m_data.front().arrivalTime) > m_windowDuration)
        {
            m_data.pop_front();
        }
    }

    size_t GetCurrentWindowSize() const
    {
        return m_data.size();
    }

    double GetAverageQsf() const
    {
        if (m_data.empty())
            return 0.0;

        double sum = std::accumulate(m_data.begin(), m_data.end(), 0.0, [](double acc, const DataInfo &info)
                                     { return acc + info.value; });
        return sum / m_data.size();
    }

    // Unit - pkgs/ms
    double GetDataArrivalRate() const
    {
        if (m_data.size() < 2)
        {
            return 0.0;
        }

        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(m_data.back().arrivalTime - m_data.front().arrivalTime).count();
        if (duration <= 0)
        {
            spdlog::info("Current number of elements within the sliding window: {}", m_data.size());
            spdlog::info("Back element: {} us.", std::chrono::duration_cast<std::chrono::microseconds>(m_data.back().arrivalTime.time_since_epoch()).count());
            spdlog::info("Front element: {} us.", std::chrono::duration_cast<std::chrono::microseconds>(m_data.front().arrivalTime.time_since_epoch()).count());
            spdlog::info("Actual duration: {} ns.", duration);
            return -1.0;
        }
        return static_cast<double>((m_data.size() - 1) / duration * 1e6);
    }

private:
    Duration m_windowDuration;
    std::deque<DataInfo> m_data;
};

#endif // SLIDING_WINDOW_HPP