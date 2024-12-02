#include <iostream>
#include <ndn-cxx/face.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/encoding/block.hpp>
#include <memory>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

class ProducerTest
{
public:
    ProducerTest()
    {
        // 初始化 spdlog
        auto logger = spdlog::basic_logger_mt("producer_logger", "log/producer.log");
        spdlog::set_default_logger(logger);
        spdlog::set_level(spdlog::level::info); // 设置日志级别
        spdlog::flush_on(spdlog::level::info);  // 每条日志后刷新

        spdlog::info("ProducerTest initialized");
    }

    void run()
    {
        // 注册前缀
        m_face.setInterestFilter("/example/testApp",
                                 std::bind(&ProducerTest::onInterest, this, std::placeholders::_1, std::placeholders::_2),
                                 std::bind(&ProducerTest::onRegisterSuccess, this, std::placeholders::_1),
                                 std::bind(&ProducerTest::onRegisterFailure, this, std::placeholders::_1, std::placeholders::_2));
        spdlog::info("Registering prefix /example/testApp");

        // 处理事件循环
        m_face.processEvents();
    }

private:
    void onInterest(const ndn::InterestFilter &filter, const ndn::Interest &interest)
    {
        spdlog::info("Received Interest: {}", interest.getName().toUri());

        // 创建 Data 包
        auto data = std::make_shared<ndn::Data>(interest.getName());
        data->setFreshnessPeriod(ndn::time::seconds(10));
        std::string content = "HELLO WORLD";
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
};

int main()
{
    ProducerTest producer;
    producer.run();
    return 0;
}