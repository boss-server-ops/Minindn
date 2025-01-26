#include <iostream>
#include <ndn-cxx/face.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

class ConsumerTest
{
public:
    ConsumerTest(const std::string &prefix) : m_prefix(prefix)

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
        // 创建一个 Interest 数据包,名称格式 /pro0/data
        ndn::Interest interest(m_prefix);
        interest.setCanBePrefix(false);
        interest.setInterestLifetime(ndn::time::seconds(2));

        spdlog::info("Sending Interest: {}", interest.getName().toUri());

        // 发送 Interest 数据包
        m_face.expressInterest(interest,
                               bind(&ConsumerTest::onData, this, _1, _2),
                               bind(&ConsumerTest::onNack, this, _1, _2),
                               bind(&ConsumerTest::onTimeout, this, _1));
        // 处理事件循环
        m_face.processEvents();
    }

private:
    void onData(const ndn::Interest &interest, const ndn::Data &data)
    {
        spdlog::info("Received Data: {}", data.getName().toUri());
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
    ndn::Name m_prefix;
};

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << "the seq of the producer" << std::endl;
        return 1;
    }
    ConsumerTest consumer(argv[1]);
    consumer.run();
    return 0;
}
