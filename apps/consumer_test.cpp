#include <iostream>
#include <ndn-cxx/face.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/security/validator-null.hpp>
#include <ndn-cxx/util/segment-fetcher.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <chrono>
#include <fstream>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

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

        // 读取配置
        boost::property_tree::ptree pt;
        try
        {
            boost::property_tree::ini_parser::read_ini("../experiments/experiment.ini", pt);
            m_dataSize = pt.get<size_t>("Datasize.datasize");
            spdlog::info("Data size set to {} bytes", m_dataSize);

            int experimentTime = pt.get<int>("ExperimentTime.experimenttime");
            ExperimentDuration = std::chrono::seconds(experimentTime);
            spdlog::info("Experiment duration set to {} seconds", experimentTime);
        }
        catch (const boost::property_tree::ptree_error &e)
        {
            spdlog::error("Failed to read configuration file: {}", e.what());
            throw;
        }

        // 生成文件名
        m_rttFileName = generateFileName(m_dataSize);
        spdlog::info("RTT file name set to {}", m_rttFileName);
    }

    void run()
    {
        StartTime = std::chrono::high_resolution_clock::now();
        int i = 0;
        while (std::chrono::high_resolution_clock::now() - StartTime < ExperimentDuration)
        {

            ndn::Name interestName = m_prefix;
            interestName.append("version").append(std::to_string(i++));

            spdlog::info("Starting SegmentFetcher for: {}", interestName.toUri());

            // 记录开始时间
            m_startTime = std::chrono::high_resolution_clock::now();

            // 设置 SegmentFetcher 选项
            ndn::SegmentFetcher::Options options;
            options.interestLifetime = ndn::time::milliseconds(10000); // 10 秒
            options.maxTimeout = ndn::time::milliseconds(120000);      // 120 秒

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
    void
    onSegmentedData(const ndn::ConstBufferPtr &buffer)
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
            rtt_file.open(m_rttFileName, std::ios::out); // 清空文件
            m_firstRequest = false;
        }
        else
        {
            rtt_file.open(m_rttFileName, std::ios::app); // 追加内容
        }

        if (rtt_file.is_open())
        {
            rtt_file << "RTT: " << rtt.count() << " seconds, starttime: " << std::chrono::duration_cast<std::chrono::seconds>(m_startTime - StartTime).count() << " seconds\n";
            rtt_file.close();
        }
        else
        {
            spdlog::error("Failed to open RTT file!");
        }
    }

    std::string generateFileName(size_t dataSize)
    {
        std::ostringstream oss;
        oss << "../results/rtt_";
        if (dataSize >= 1e9)
        {
            oss << dataSize / 1e9 << "GB";
        }
        else if (dataSize >= 1e6)
        {
            oss << dataSize / 1e6 << "MB";
        }
        else if (dataSize >= 1e3)
        {
            oss << dataSize / 1e3 << "KB";
        }
        else
        {
            oss << dataSize << "B";
        }
        oss << ".txt";
        return oss.str();
    }

private:
    ndn::Face m_face;
    ndn::KeyChain m_keyChain;
    ndn::Name m_prefix;
    ndn::security::ValidatorNull m_validator; // 使用 ValidatorNull 进行简单验证
    std::chrono::high_resolution_clock::time_point m_startTime;
    std::chrono::high_resolution_clock::time_point StartTime;
    std::chrono::seconds ExperimentDuration;
    bool m_firstRequest;
    size_t m_dataSize;
    std::string m_rttFileName;
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
        ConsumerTest consumer(argv[1]);
        consumer.run();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}