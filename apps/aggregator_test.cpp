#include <iostream>
#include <ndn-cxx/face.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/security/validator-null.hpp>
#include <ndn-cxx/util/segment-fetcher.hpp>
#include <ndn-cxx/security/signing-info.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <boost/asio/io_context.hpp>
#include <ndn-cxx/util/segmenter.hpp>

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

        spdlog::info("Sending interest: {}", newName.toUri());

        ndn::SegmentFetcher::Options options;
        options.interestLifetime = ndn::time::milliseconds(10000); // 保持 10 秒
        options.maxTimeout = ndn::time::milliseconds(120000);      // 保持 120 秒

        auto fetcher = ndn::SegmentFetcher::start(m_face, ndn::Interest(newName), m_validator, options);

        fetcher->onComplete.connect([this, interest](const ndn::ConstBufferPtr &content)
                                    { onSegmentedData(content, interest); });

        fetcher->onError.connect([this](uint32_t errorCode, const std::string &errorMsg)
                                 { spdlog::error("Request failed: {} - {}", errorCode, errorMsg); });
    }

    void onSegmentedData(const ndn::ConstBufferPtr &buffer, const ndn::Interest &interest)
    {
        spdlog::info("Received Data from Producer: {}", buffer->size());
        ndn::Name newName = interest.getName();
        // 将数据包发送回 Consumer
        // 分片处理
        const size_t SEGMENT_SIZE = 4000; // 分片大小（根据MTU调整）
        ndn::Segmenter segmenter(m_keyChain, m_signingInfo);
        std::vector<std::shared_ptr<ndn::Data>> segments = segmenter.segment(
            ndn::make_span(reinterpret_cast<const uint8_t *>(buffer->data()), buffer->size()),
            newName, SEGMENT_SIZE, ndn::time::seconds(20), ndn::tlv::ContentType_Blob);
        // 检查请求的是否是特定分段
        if (newName.size() > m_prefix.size() + 1 && newName[-1].isSegment())
        {
            // 请求的是特定分段（如 seg=3）
            uint64_t requestedSegNo = newName[-1].toSegment();
            if (requestedSegNo < segments.size())
            {
                // 设置分段名称和 FinalBlockId
                segments[requestedSegNo]->setName(newName);
                segments[requestedSegNo]->setFreshnessPeriod(ndn::time::seconds(10));
                segments[requestedSegNo]->setFinalBlock(ndn::name::Component::fromSegment(segments.size() - 1));

                // 签名并发送
                m_keyChain.sign(*segments[requestedSegNo]);
                m_face.put(*segments[requestedSegNo]);
                spdlog::info("Sent segment {}: {}", requestedSegNo, newName.toUri());
            }
            else
            {
                spdlog::warn("Requested segment {} is out of range", requestedSegNo);
            }
        }
        else
        {
            // 初始请求（无 seg= 后缀）：返回第一个分段 seg=0
            ndn::Name firstSegmentName = newName;
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
        // auto newData = std::make_shared<ndn::Data>(newName);
        // newData->setFreshnessPeriod(ndn::time::seconds(10));
        // newData->setContent(buffer);
        // m_keyChain.sign(*newData);
        // m_face.put(*newData);
        spdlog::info("Sent Data: {}", newName.toUri());
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
    ndn::security::ValidatorNull m_validator;
    ndn::Name m_prefix;
    ndn::security::SigningInfo m_signingInfo;
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