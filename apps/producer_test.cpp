// #include <iostream>
// #include <ndn-cxx/face.hpp>
// #include <ndn-cxx/security/key-chain.hpp>
// #include <ndn-cxx/encoding/block.hpp>
// #include <memory>
// #include <spdlog/spdlog.h>
// #include <spdlog/sinks/basic_file_sink.h>
// #include <boost/property_tree/ptree.hpp>
// #include <boost/property_tree/ini_parser.hpp>
// #include <cstdlib> // for system()
// #include <thread>  // for sleep
// #include <chrono>  // for sleep

// class ProducerTest
// {
// public:
//     ProducerTest(const std::string &prefix) : m_prefix(prefix)
//     {
//         // 初始化 spdlog
//         auto logger = spdlog::basic_logger_mt("producer_logger", "logs/producer.log");
//         spdlog::set_default_logger(logger);
//         spdlog::set_level(spdlog::level::info); // 设置日志级别
//         spdlog::flush_on(spdlog::level::info);  // 每条日志后刷新

//         spdlog::info("ProducerTest initialized");

//         // 读取数据大小配置
//         boost::property_tree::ptree pt;
//         try
//         {
//             boost::property_tree::ini_parser::read_ini("../experiments/experiment.ini", pt);
//             m_contentSize = pt.get<size_t>("Datasize.datasize");
//             spdlog::info("Content size set to {} bytes", m_contentSize);
//         }
//         catch (const boost::property_tree::ptree_error &e)
//         {
//             spdlog::error("Failed to read datasize from configuration file: {}", e.what());
//             throw;
//         }
//     }

//     void run()
//     {
//         // register prefix
//         m_face.setInterestFilter(m_prefix,
//                                  std::bind(&ProducerTest::onInterest, this, std::placeholders::_1, std::placeholders::_2),
//                                  std::bind(&ProducerTest::onRegisterSuccess, this, std::placeholders::_1),
//                                  std::bind(&ProducerTest::onRegisterFailure, this, std::placeholders::_1, std::placeholders::_2));
//         spdlog::info("Registering prefix {}", m_prefix.toUri());

//         while (true)
//         {
//             m_face.processEvents();
//         }
//     }

// private:
//     void onInterest(const ndn::InterestFilter &filter, const ndn::Interest &interest)
//     {
//         spdlog::info("Received Interest: {}", interest.getName().toUri());

//         // 创建 Data 包
//         auto data = std::make_shared<ndn::Data>(interest.getName());
//         data->setFreshnessPeriod(ndn::time::seconds(10));

//         // 设置自定义大小的内容
//         std::string content(m_contentSize, 'A'); // 使用字符 'A' 填充内容
//         data->setContent(ndn::make_span(reinterpret_cast<const uint8_t *>(content.data()), content.size()));
//         // 使用 KeyChain 对 Data 包进行签名
//         m_keyChain.sign(*data);

//         // 发送 Data 包
//         m_face.put(*data);
//         spdlog::info("Sent Data: {}", data->getName().toUri());
//     }

//     void onRegisterSuccess(const ndn::Name &prefix)
//     {
//         spdlog::info("Successfully registered prefix: {}", prefix.toUri());
//     }

//     void onRegisterFailure(const ndn::Name &prefix, const std::string &reason)
//     {
//         spdlog::error("Failed to register prefix: {} ({})", prefix.toUri(), reason);
//     }

// private:
//     ndn::Face m_face;
//     ndn::KeyChain m_keyChain;
//     ndn::Name m_prefix;
//     size_t m_contentSize;
// };

// int main(int argc, char **argv)
// {
//     if (argc != 2)
//     {
//         std::cerr << "Usage: " << argv[0] << " <prefix>" << std::endl;
//         return 1;
//     }
//     try
//     {
//         ProducerTest producer(argv[1]);
//         producer.run();
//     }
//     catch (const std::exception &e)
//     {
//         std::cerr << "Error: " << e.what() << std::endl;
//         return 1;
//     }
//     return 0;
// }

#include <iostream>
#include <ndn-cxx/face.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/security/signing-info.hpp>
#include <ndn-cxx/encoding/block.hpp>
#include <ndn-cxx/util/segmenter.hpp>
#include <memory>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
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

        // 读取数据大小配置
        boost::property_tree::ptree pt;
        try
        {
            boost::property_tree::ini_parser::read_ini("../experiments/experiment.ini", pt);
            m_contentSize = pt.get<size_t>("Datasize.datasize");
            spdlog::info("Content size set to {} bytes", m_contentSize);
        }
        catch (const boost::property_tree::ptree_error &e)
        {
            spdlog::error("Failed to read datasize from configuration file: {}", e.what());
            throw;
        }
    }

    void run()
    {
        // register prefix
        m_face.setInterestFilter(m_prefix,
                                 std::bind(&ProducerTest::onInterest, this, std::placeholders::_1, std::placeholders::_2),
                                 std::bind(&ProducerTest::onRegisterSuccess, this, std::placeholders::_1),
                                 std::bind(&ProducerTest::onRegisterFailure, this, std::placeholders::_1, std::placeholders::_2));
        spdlog::info("Registering prefix {}", m_prefix.toUri());

        while (true)
        {
            m_face.processEvents();
        }
    }

private:
private:
    void onInterest(const ndn::InterestFilter &filter, const ndn::Interest &interest)
    {
        spdlog::info("Received Interest: {}", interest.getName().toUri());

        // 检查 Interest 名称是否包含分段号
        ndn::Name interestName = interest.getName();
        if (interestName.size() < m_prefix.size() + 1)
        {
            spdlog::warn("Invalid interest format: {}", interestName.toUri());
            return;
        }

        // 生成内容（保持原有逻辑）
        std::string content(m_contentSize, 'A');

        // 分片处理
        const size_t SEGMENT_SIZE = 4000; // 分片大小（根据MTU调整）
        ndn::Segmenter segmenter(m_keyChain, m_signingInfo);
        std::vector<std::shared_ptr<ndn::Data>> segments = segmenter.segment(
            ndn::make_span(reinterpret_cast<const uint8_t *>(content.data()), content.size()),
            interest.getName().getPrefix(-1), SEGMENT_SIZE, ndn::time::seconds(20), ndn::tlv::ContentType_Blob);

        // 检查请求的是否是特定分段
        if (interestName.size() > m_prefix.size() + 1 && interestName[-1].isSegment())
        {
            // 请求的是特定分段（如 seg=3）
            uint64_t requestedSegNo = interestName[-1].toSegment();
            if (requestedSegNo < segments.size())
            {
                // 设置分段名称和 FinalBlockId
                segments[requestedSegNo]->setName(interestName);
                segments[requestedSegNo]->setFreshnessPeriod(ndn::time::seconds(10));
                segments[requestedSegNo]->setFinalBlock(ndn::name::Component::fromSegment(segments.size() - 1));

                // 签名并发送
                m_keyChain.sign(*segments[requestedSegNo]);
                m_face.put(*segments[requestedSegNo]);
                spdlog::info("Sent segment {}: {}", requestedSegNo, interestName.toUri());
            }
            else
            {
                spdlog::warn("Requested segment {} is out of range", requestedSegNo);
            }
        }
        else
        {
            // 初始请求（无 seg= 后缀）：返回第一个分段 seg=0
            ndn::Name firstSegmentName = interestName;
            firstSegmentName.appendSegment(0);

            // 设置第一个分段
            segments[0]->setName(firstSegmentName);
            segments[0]->setFreshnessPeriod(ndn::time::seconds(10));
            segments[0]->setFinalBlock(ndn::name::Component::fromSegment(segments.size() - 1));

            // 签名并发送
            m_keyChain.sign(*segments[0]);
            m_face.put(*segments[0]);
            spdlog::info("Sent initial segment 0: {}", firstSegmentName.toUri());
        }
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
    size_t m_contentSize;
    ndn::security::SigningInfo m_signingInfo;
};

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <prefix>" << std::endl;
        return 1;
    }
    try
    {
        ProducerTest producer(argv[1]);
        producer.run();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
