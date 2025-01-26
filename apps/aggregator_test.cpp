#include <iostream>
#include <ndn-cxx/face.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

class AggregatorTest
{
public:
    AggregatorTest(const std::string &prefix)
        : m_face(),
          m_prefix(prefix),
          m_keyChain()
    {
        // 初始化 spdlog
        auto logger = spdlog::basic_logger_mt("aggregator_logger", "logs/aggregator.log");
        spdlog::set_default_logger(logger);
        spdlog::set_level(spdlog::level::info); // 设置日志级别
        spdlog::flush_on(spdlog::level::info);  // 每条日志后刷新

        spdlog::info("AggregatorTest initialized");
    }

    void run()
    {
        // 注册前缀
        m_face.setInterestFilter(m_prefix,
                                 std::bind(&AggregatorTest::onInterest, this, std::placeholders::_1, std::placeholders::_2),
                                 std::bind(&AggregatorTest::onRegisterSuccess, this, std::placeholders::_1),
                                 std::bind(&AggregatorTest::onRegisterFailure, this, std::placeholders::_1, std::placeholders::_2));
        spdlog::info("Registering prefix {}", m_prefix.toUri());

        m_face.processEvents();
    }

private:
    void onInterest(const ndn::InterestFilter &filter, const ndn::Interest &interest)
    {
        spdlog::info("Received Interest from Consumer: {}", interest.getName().toUri());

        ndn::Name newName = interest.getName().getSubName(1);

        // 创建新的兴趣包
        ndn::Interest newInterest(newName);
        newInterest.setCanBePrefix(false);
        newInterest.setMustBeFresh(true);
        newInterest.setInterestLifetime(ndn::time::seconds(2));

        spdlog::info("Sending Interest to Producer: {}", newInterest.getName().toUri());

        // 发送兴趣包给 Producer
        m_face.expressInterest(newInterest,
                               std::bind(&AggregatorTest::onData, this, _1, _2),
                               std::bind(&AggregatorTest::onNack, this, _1, _2),
                               std::bind(&AggregatorTest::onTimeout, this, _1));
    }

    void onData(const ndn::Interest &interest, const ndn::Data &data)
    {
        spdlog::info("Received Data from Producer: {}", data.getName().toUri());
        ndn::Name newName;
        newName.append(ndn::Name("/")).append(ndn::Name(m_prefix)).append(data.getName());
        // 将数据包发送回 Consumer
        auto newData = std::make_shared<ndn::Data>(newName);
        newData->setFreshnessPeriod(ndn::time::seconds(10));
        newData->setContent(data.getContent());
        m_keyChain.sign(*newData);
        m_face.put(*newData);
        spdlog::info("Sent Data: {}", newData->getName().toUri());
    }

    void onNack(const ndn::Interest &interest, const ndn::lp::Nack &nack)
    {
        spdlog::warn("Received Nack from Producer: {} for Interest: {}", static_cast<int>(nack.getReason()), interest.getName().toUri());
    }

    void onTimeout(const ndn::Interest &interest)
    {
        spdlog::warn("Timeout for Interest: {}", interest.getName().toUri());
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
    AggregatorTest aggregator(argv[1]);
    aggregator.run();
    return 0;
}