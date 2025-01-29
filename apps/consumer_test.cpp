// #include <iostream>
// #include <ndn-cxx/face.hpp>
// #include <ndn-cxx/security/key-chain.hpp>
// #include <spdlog/spdlog.h>
// #include <spdlog/sinks/basic_file_sink.h>
// #include <chrono>
// #include <fstream>

// class ConsumerTest
// {
// public:
//     ConsumerTest(const std::string &prefix) : m_prefix(prefix), m_firstRequest(true)
//     {
//         // 初始化 spdlog
//         auto logger = spdlog::basic_logger_mt("consumer_logger", "logs/consumer.log");
//         spdlog::set_default_logger(logger);
//         spdlog::set_level(spdlog::level::info); // 设置日志级别
//         spdlog::flush_on(spdlog::level::info);  // 每条日志后刷新

//         spdlog::info("ConsumerTest initialized");
//     }

//     void run()
//     {
//         for (int i = 0; i < 20; ++i)
//         {
//             // 创建一个 Interest 数据包,名称格式 /pro0/data/seq/<i>
//             ndn::Name interestName = m_prefix;
//             interestName.append("seq").append(std::to_string(i));
//             ndn::Interest interest(interestName);
//             interest.setCanBePrefix(false);
//             interest.setInterestLifetime(ndn::time::seconds(30));

//             spdlog::info("Sending Interest: {}", interest.getName().toUri());

//             // 记录开始时间
//             m_startTime = std::chrono::high_resolution_clock::now();

//             // 发送 Interest 数据包
//             m_face.expressInterest(interest,
//                                    bind(&ConsumerTest::onData, this, _1, _2),
//                                    bind(&ConsumerTest::onNack, this, _1, _2),
//                                    bind(&ConsumerTest::onTimeout, this, _1));
//             // 处理事件循环
//             m_face.processEvents();
//         }
//     }

// private:
//     void onData(const ndn::Interest &interest, const ndn::Data &data)
//     {
//         // 记录结束时间
//         auto endTime = std::chrono::high_resolution_clock::now();
//         std::chrono::duration<double> rtt = endTime - m_startTime;

//         spdlog::info("Received Data: {}", data.getName().toUri());
//         spdlog::info("RTT: {} seconds", rtt.count());

//         // 保存 RTT 到文件
//         std::ofstream rtt_file;
//         if (m_firstRequest)
//         {
//             rtt_file.open("../results/rtt_128MB.txt", std::ios::out); // 清空文件内容
//             m_firstRequest = false;
//         }
//         else
//         {
//             rtt_file.open("../results/rtt_128MB.txt", std::ios::app); // 追加内容
//         }

//         if (rtt_file.is_open())
//         {
//             rtt_file << "RTT: " << rtt.count() << " seconds\n";
//             rtt_file.close();
//         }
//         else
//         {
//             spdlog::error("Failed to open RTT file!");
//         }
//     }

//     void onNack(const ndn::Interest &interest, const ndn::lp::Nack &nack)
//     {
//         spdlog::error("Received Nack for Interest: {} ({})", interest.getName().toUri(), static_cast<int>(nack.getReason()));
//     }

//     void onTimeout(const ndn::Interest &interest)
//     {
//         spdlog::error("Interest timeout for: {}", interest.getName().toUri());
//     }

// private:
//     ndn::Face m_face;
//     ndn::KeyChain m_keyChain;
//     ndn::Name m_prefix;
//     std::chrono::high_resolution_clock::time_point m_startTime;
//     bool m_firstRequest;
// };

// int main(int argc, char **argv)
// {
//     if (argc != 2)
//     {
//         std::cerr << "Usage: " << argv[0] << " <prefix>" << std::endl;
//         return 1;
//     }
//     ConsumerTest consumer(argv[1]);
//     consumer.run();
//     return 0;
// }

#include <iostream>
#include <ndn-cxx/face.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/security/validator-null.hpp>
#include <ndn-cxx/util/segment-fetcher.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <chrono>
#include <fstream>

class ConsumerTest
{
public:
    ConsumerTest(const std::string &prefix)
        : m_prefix(prefix), m_firstRequest(true)
    {
        // 初始化 spdlog
        auto logger = spdlog::basic_logger_mt("consumer_logger", "logs/consumer.log");
        spdlog::set_default_logger(logger);
        spdlog::set_level(spdlog::level::info); // 设置日志级别
        spdlog::flush_on(spdlog::level::info);  // 每条日志后刷新

        spdlog::info("ConsumerTest initialized");
    }

    void run()
    {
        for (int i = 0; i < 20; ++i)
        {
            ndn::Name interestName = m_prefix;
            interestName.append("version").append(std::to_string(i));

            spdlog::info("Starting SegmentFetcher for: {}", interestName.toUri());

            // 记录开始时间
            m_startTime = std::chrono::high_resolution_clock::now();

            // 设置 SegmentFetcher 选项
            ndn::SegmentFetcher::Options options;
            options.interestLifetime = ndn::time::milliseconds(150000); // 15 秒
            options.maxTimeout = ndn::time::milliseconds(120000);       // 120 秒

            // 使用 SegmentFetcher 获取数据
            auto fetcher = ndn::SegmentFetcher::start(m_face, ndn::Interest(interestName), m_validator, options);

            fetcher->onComplete.connect([this](const ndn::ConstBufferPtr &content)
                                        { this->onSegmentedData(content); });

            fetcher->onError.connect([](uint32_t errorCode, const std::string &errorMsg)
                                     { spdlog::error("Error occurred: {} - {}", errorCode, errorMsg); });

            // 处理事件循环（保持异步）
            m_face.processEvents();
        }
    }

private:
    void onSegmentedData(const ndn::ConstBufferPtr &buffer)
    {
        // 记录结束时间
        auto endTime = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> rtt = endTime - m_startTime;

        spdlog::info("Received segmented data ({} bytes)", buffer->size());
        spdlog::info("RTT: {} seconds", rtt.count());

        // 保存 RTT 到文件
        std::ofstream rtt_file;
        if (m_firstRequest)
        {
            rtt_file.open("../results/rtt_128MB.txt", std::ios::out); // 清空文件
            m_firstRequest = false;
        }
        else
        {
            rtt_file.open("../results/rtt_128MB.txt", std::ios::app); // 追加内容
        }

        if (rtt_file.is_open())
        {
            rtt_file << "RTT: " << rtt.count() << " seconds, Data size: " << buffer->size() << " bytes\n";
            rtt_file.close();
        }
        else
        {
            spdlog::error("Failed to open RTT file!");
        }
    }

private:
    ndn::Face m_face;
    ndn::KeyChain m_keyChain;
    ndn::Name m_prefix;
    ndn::security::ValidatorNull m_validator; // 使用 ValidatorNull 进行简单验证
    std::chrono::high_resolution_clock::time_point m_startTime;
    bool m_firstRequest;
};

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <prefix>" << std::endl;
        return 1;
    }
    ConsumerTest consumer(argv[1]);
    consumer.run();
    return 0;
}