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
class ProducerTest
{
public:
    ProducerTest(const std::string &prefix) : m_prefix(prefix)
    {
        // 初始化 spdlog
        auto logger = spdlog::basic_logger_mt("producer_logger", "logs/producer.log");
        spdlog::set_default_logger(logger);
        spdlog::set_level(spdlog::level::info); // 设置日志级别
        spdlog::flush_on(spdlog::level::info);  // 每条日志后刷新

        spdlog::info("ProducerTest initialized");
    }

    void run()
    {
        // register prefix
        m_face.setInterestFilter(m_prefix,
                                 std::bind(&ProducerTest::onInterest, this, std::placeholders::_1, std::placeholders::_2),
                                 std::bind(&ProducerTest::onRegisterSuccess, this, std::placeholders::_1),
                                 std::bind(&ProducerTest::onRegisterFailure, this, std::placeholders::_1, std::placeholders::_2));
        spdlog::info("Registering prefix {}", m_prefix.toUri());

        m_face.processEvents();
    }

private:
    void onInterest(const ndn::InterestFilter &filter, const ndn::Interest &interest)
    {
        spdlog::info("Received Interest: {}", interest.getName().toUri());

        // 创建 Data 包
        auto data = std::make_shared<ndn::Data>(interest.getName());
        data->setFreshnessPeriod(ndn::time::seconds(10));

        // 设置自定义大小的内容
        size_t contentSize = 1024;             // 例如，设置内容大小为 1024 字节
        std::string content(contentSize, 'A'); // 使用字符 'A' 填充内容
        data->setFreshnessPeriod(ndn::time::seconds(10));
        data->setContent(ndn::make_span(reinterpret_cast<const uint8_t *>(content.data()), content.size()));

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

private:
    ndn::Face m_face;
    ndn::KeyChain m_keyChain;
    ndn::Name m_prefix;
};

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << "the seq of the producer" << std::endl;
        return 1;
    }
    ProducerTest producer(argv[1]);
    producer.run();
    return 0;
}
