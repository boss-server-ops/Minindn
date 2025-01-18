#include <iostream>
#include <ndn-cxx/face.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/util/scheduler.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <fstream>
const int total_iterations = 3;
const int interval = 0;
class SConsumer
{
public:
    SConsumer()
        : m_scheduler(m_face.getIoContext())
    {
        // 初始化 spdlog
        auto logger = spdlog::basic_logger_mt("consumer_logger", "logs/sconsumer.log");
        spdlog::set_default_logger(logger);
        spdlog::set_level(spdlog::level::debug); // 设置日志级别
        spdlog::flush_on(spdlog::level::debug);  // 每条日志后刷新

        spdlog::info("SConsumer initialized");
    }

    void run(int n)
    {
        for (int j = 1; j < n + 1; j++)
        {

            for (int i = 0; i < total_iterations; i++)
            {
                // 创建一个 Interest 数据包
                std::string prefix = "/producer_" + std::to_string(j) + "/iteration_" + std::to_string(i);
                ndn::Interest interest(prefix);
                interest.setCanBePrefix(false);
                interest.setInterestLifetime(ndn::time::seconds(2));

                spdlog::info("Sending Interest: {}", interest.getName().toUri());

                // 发送 Interest 数据包
                startTime[interest.getName().toUri()] = std::chrono::steady_clock::now();
                m_face.expressInterest(interest,
                                       bind(&SConsumer::onData, this, _1, _2),
                                       bind(&SConsumer::onNack, this, _1, _2),
                                       bind(&SConsumer::onTimeout, this, _1));
            }
            // 处理事件循环
        }
        // m_scheduler.schedule(ndn::time::milliseconds(m_retxTimer.count()), [this]
        //                      { CheckRetxTimeout(); });

        m_face.processEvents();
        // std::this_thread::sleep_for(std::chrono::seconds(5));
        // StopRetx();
        // std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    // void SetRetxTimer(std::chrono::milliseconds retxTimer)
    // {
    //     // ps:deleted schedule event
    //     m_retxTimer = retxTimer;

    //     // Cancel the existing retransmission event if it's running
    //     if (m_retxEvent.joinable())
    //     {
    //         m_stopRetx = true;
    //         m_retxEvent.join();
    //         m_stopRetx = false;
    //     }

    //     // Log the next interval to check timeout
    //     spdlog::debug("Next interval to check timeout is: {} ms", m_retxTimer.count());

    //     // Schedule timeout check event
    //     m_stopRetx = false;
    //     m_retxEvent = std::thread([this]()
    //                               {
    //         while (!m_stopRetx)
    //         {
    //             std::this_thread::sleep_for(m_retxTimer);
    //             CheckRetxTimeout();
    //         } });
    // }

    // void CheckRetxTimeout()
    // {
    //     spdlog::info("enter into CheckRetxTimeout");
    //     // ps:deleted schedule event and have different ontimeout
    //     auto now = std::chrono::steady_clock::now();

    //     auto now_c = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    //     std::stringstream ss;
    //     ss << std::put_time(std::localtime(&now_c), "%Y-%m-%d %X");

    //     spdlog::info("Check timeout after: {} ms at {}", m_retxTimer.count(), ss.str());
    // }

    // void StopRetx()
    // {
    //     m_stopRetx = true;
    //     if (m_retxEvent.joinable())
    //     {
    //         m_retxEvent.join();
    //     }
    // }

    void SetRetxTimer(std::chrono::milliseconds retxTimer)
    {
        m_retxTimer = retxTimer;
    }

    void CheckRetxTimeout()
    {
        spdlog::info("enter into CheckRetxTimeout");
        // ps:deleted schedule event and have different ontimeout
        auto now = std::chrono::steady_clock::now();

        auto now_c = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::stringstream ss;
        ss << std::put_time(std::localtime(&now_c), "%Y-%m-%d %X");

        spdlog::info("Check timeout after: {} ms at {}", m_retxTimer.count(), ss.str());

        // Schedule the next timeout check
        m_timeoutEvent = m_scheduler.schedule(ndn::time::milliseconds(m_retxTimer.count()), [this]
                                              { CheckRetxTimeout(); });
    }

    void StopRetx()
    {
        if (m_timeoutEvent)
        {
            m_timeoutEvent.cancel();
            m_timeoutEvent.reset();
        }
    }

private:
    void
    onData(const ndn::Interest &interest, const ndn::Data &data)
    {
        spdlog::info("Received Data: {}", data.getName().toUri());

        const ndn::Block &content = data.getContent();

        // 确保数据大小正确
        if (content.value_size() != sizeof(double))
        {
            spdlog::error("Invalid data size. Expected: {}, Got: {}", sizeof(double), content.value_size());
            return;
        }

        // 解析为 double
        double elapsedTime = *reinterpret_cast<const double *>(content.value());

        spdlog::info("Received elapsed time: {} seconds", elapsedTime);

        // 查找并计算响应时间
        auto it = startTime.find(interest.getName().toUri());
        if (it != startTime.end())
        {
            auto now = std::chrono::steady_clock::now();
            auto responseTime = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second).count();
            spdlog::info("Response time for Interest {}: {} ms", interest.getName().toUri(), responseTime);

            // 移除记录的发送时间
            startTime.erase(it);
        }
        else
        {
            spdlog::warn("No start time found for Interest {}", interest.getName().toUri());
        }
    }

    void onNack(const ndn::Interest &interest, const ndn::lp::Nack &nack)
    {
        spdlog::error("Received Nack for Interest: {} ({})", interest.getName().toUri(), static_cast<int>(nack.getReason()));
    }

    void onTimeout(const ndn::Interest &interest)
    {
        spdlog::error("Interest timeout for: {}", interest.getName().toUri());
    }

private:
    ndn::Face m_face;
    ndn::KeyChain m_keyChain;
    std::chrono::milliseconds m_retxTimer;
    std::thread m_retxEvent;
    std::atomic<bool> m_stopRetx{false};
    ndn::Scheduler m_scheduler;
    ndn::scheduler::EventId m_timeoutEvent;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> startTime;
};

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << "the seq of the producer" << std::endl;
        return 1;
    }

    // // 创建并打开文件
    // std::ofstream outFile("../topologies/hello_world.txt");
    // if (!outFile)
    // {
    //     std::cerr << "Failed to create the file." << std::endl;
    //     return 1;
    // }

    // // 向文件中写入 "hello world"
    // outFile << "hello world" << std::endl;

    // 关闭文件
    // outFile.close();
    int n = std::stoi(argv[1]);
    SConsumer consumer;
    consumer.SetRetxTimer(std::chrono::milliseconds(50));
    consumer.run(n);
    return 0;
}
