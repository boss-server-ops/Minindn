#include <iostream>
#include <ndn-cxx/face.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/encoding/block.hpp>
#include <memory>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <cstdlib> // for system()
#include <thread>  // for sleep
#include <chrono>  // for sleep

const int total_iterations = 3;
const int interval = 0;
class MProducer
{
public:
    MProducer()
    {
        // 初始化 spdlog
        auto logger = spdlog::basic_logger_mt("mproducer_logger", "logs/mproducer.log");
        spdlog::set_default_logger(logger);
        spdlog::set_level(spdlog::level::info); // 设置日志级别
        spdlog::flush_on(spdlog::level::info);  // 每条日志后刷新

        spdlog::info("MProducer initialized");
    }

    void run(int n)
    {
        for (int i = 0; i < total_iterations; ++i)
        {
            std::string prefix = "/producer_" + std::to_string(n) + "/iteration_" + std::to_string(i);
            m_face.setInterestFilter(prefix,
                                     std::bind(&MProducer::onInterest, this, std::placeholders::_1, std::placeholders::_2),
                                     std::bind(&MProducer::onRegisterSuccess, this, std::placeholders::_1),
                                     std::bind(&MProducer::onRegisterFailure, this, std::placeholders::_1, std::placeholders::_2));
            spdlog::info("Registering prefix {}", prefix);
            std::this_thread::sleep_for(std::chrono::milliseconds(interval));
        }
        m_face.processEvents();
    }

private:
    void onInterest(const ndn::InterestFilter &filter, const ndn::Interest &interest)
    {
        spdlog::info("Received Interest: {}", interest.getName().toUri());

        double elapsed_time = performComputation(10000000);

        const uint8_t *elapsedTimeBytes = reinterpret_cast<const uint8_t *>(&elapsed_time);

        // 创建 Data 包
        auto data = std::make_shared<ndn::Data>(interest.getName());
        data->setFreshnessPeriod(ndn::time::seconds(10));
        data->setContent(ndn::make_span(elapsedTimeBytes, sizeof(double)));

        // 使用 KeyChain 对 Data 包进行签名
        m_keyChain.sign(*data);

        // 发送 Data 包
        m_face.put(*data);
    }

    void onRegisterSuccess(const ndn::Name &prefix)
    {
        spdlog::info("Successfully registered prefix: {}", prefix.toUri());
    }

    void onRegisterFailure(const ndn::Name &prefix, const std::string &reason)
    {
        spdlog::error("Failed to register prefix: {} ({})", prefix.toUri(), reason);
    }

    double performComputation(long long iterations)
    {

        spdlog::info("计算开始");
        auto start_time = std::chrono::steady_clock::now();
        long long result = 0;
        for (long long i = 0; i < iterations; ++i)
        {
            result += (i * i) % 12345; // 模拟复杂计算
        }

        auto now = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed = now - start_time;
        spdlog::info("计算完成，用时 {} 秒", elapsed.count());
        return elapsed.count();
    }

private:
    ndn::Face m_face;
    ndn::KeyChain m_keyChain;
};

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << "the seq of the producer" << std::endl;
        return 1;
    }

    int n = std::stoi(argv[1]);
    MProducer mproducer;
    mproducer.run(n);
    return 0;
}