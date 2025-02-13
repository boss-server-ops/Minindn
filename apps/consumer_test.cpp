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
#include <boost/asio/io_context.hpp>
#include <boost/property_tree/ini_parser.hpp>

class ConsumerTest
{
public:
    ConsumerTest(const std::string &prefix)
        : m_prefix(prefix), m_firstRequest(true)
    {
        // 初始化 spdlog (保持原有日志配置)
        auto logger = spdlog::basic_logger_mt("consumer_logger", "logs/consumer.log");
        spdlog::set_default_logger(logger);
        spdlog::set_level(spdlog::level::info);
        spdlog::flush_on(spdlog::level::info);

        // 读取配置 (保持原有配置读取方式)
        boost::property_tree::ptree pt;
        try
        {
            boost::property_tree::ini_parser::read_ini("../experiments/experiment.ini", pt);
            m_dataSize = pt.get<size_t>("Datasize.datasize");
            int experimentTime = pt.get<int>("ExperimentTime.experimenttime");
            m_experimentDuration = std::chrono::seconds(experimentTime);
        }
        catch (const boost::property_tree::ptree_error &e)
        {
            spdlog::error("Failed to read configuration: {}", e.what());
            throw;
        }

        // 保持原有文件名生成逻辑
        m_rttFileName = generateFileName(m_dataSize);
        spdlog::info("RTT file: {}", m_rttFileName);
    }

    void run()
    {
        m_experimentStart = std::chrono::high_resolution_clock::now();
        sendNextInterest(0); // 启动第一个请求
        m_face.processEvents();
    }

private:
    void sendNextInterest(int seq)
    {
        // 检查实验是否超时
        if (std::chrono::high_resolution_clock::now() - m_experimentStart >= m_experimentDuration)
        {
            spdlog::info("Experiment duration reached, stopping");
            m_face.shutdown();
            return;
        }

        // 保持原有兴趣包构造方式
        ndn::Name interestName = m_prefix;
        interestName.append("version").append(std::to_string(seq));

        spdlog::info("Sending interest: {}", interestName.toUri());

        // 保持原有时间记录方式
        m_startTime = std::chrono::high_resolution_clock::now();

        // 保持原有 SegmentFetcher 参数
        ndn::SegmentFetcher::Options options;
        options.interestLifetime = ndn::time::milliseconds(10000); // 保持 10 秒
        options.maxTimeout = ndn::time::milliseconds(120000);      // 保持 120 秒

        auto fetcher = ndn::SegmentFetcher::start(m_face, ndn::Interest(interestName), m_validator, options);

        // 保持原有回调结构，添加序列号跟踪
        fetcher->onComplete.connect([this, seq](const ndn::ConstBufferPtr &content)
                                    {
                                        onSegmentedData(content);
                                        scheduleNext(seq + 1); // 成功时发送下一个
                                    });

        fetcher->onError.connect([this, seq](uint32_t errorCode, const std::string &errorMsg)
                                 {
                                     spdlog::error("Request {} failed: {} - {}", seq, errorCode, errorMsg);
                                     scheduleNext(seq + 1); // 即使失败也继续发送
                                 });
    }

    void scheduleNext(int nextSeq)
    {
        // 通过IO服务确保顺序执行
        m_face.getIoContext().post([this, nextSeq]()
                                   { sendNextInterest(nextSeq); });
    }

    void onSegmentedData(const ndn::ConstBufferPtr &buffer)
    {
        // 保持原有RTT计算方式
        auto endTime = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> rtt = endTime - m_startTime;

        // 保持原有文件写入方式
        std::ofstream rtt_file;
        if (m_firstRequest)
        {
            rtt_file.open(m_rttFileName, std::ios::out);
            m_firstRequest = false;
        }
        else
        {
            rtt_file.open(m_rttFileName, std::ios::app);
        }

        if (rtt_file.is_open())
        {
            auto timestamp = std::chrono::duration_cast<std::chrono::duration<double>>(m_startTime - m_experimentStart).count();
            rtt_file << "Timestamp: " << std::fixed << std::setprecision(6) << timestamp
                     << " RTT: " << std::fixed << std::setprecision(6) << rtt.count() << " seconds\n";
            rtt_file.close();
        }

        spdlog::info("Received {} bytes, RTT: {:.3f}s", buffer->size(), rtt.count());
    }

    // 保持原有文件名生成逻辑
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
        oss << "ndn.txt";
        return oss.str();
    }

private:
    ndn::Face m_face;
    ndn::security::ValidatorNull m_validator;
    ndn::Name m_prefix;

    // 保持原有时间记录变量
    std::chrono::high_resolution_clock::time_point m_startTime;
    std::chrono::high_resolution_clock::time_point m_experimentStart;
    std::chrono::seconds m_experimentDuration;

    // 保持原有文件相关变量
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