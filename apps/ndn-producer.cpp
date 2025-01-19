#include "ndn-producer.hpp"

#include "ModelData.hpp"

Producer::Producer(const std::string &prefix)
    : m_virtualPayloadSize(1024),
      m_freshness(ndn::time::milliseconds(1000)),
      m_prefixnum(0),
      m_prefix(prefix)
{
    // 初始化 spdlog
    m_logger = spdlog::basic_logger_mt("producer_logger", "logs/producer.log");
    spdlog::set_default_logger(m_logger);
    spdlog::set_level(spdlog::level::info); // 设置日志级别
    spdlog::flush_on(spdlog::level::info);  // 每条 info 级别及以上的日志后刷新

    spdlog::info("Producer initialized");
}

Producer::Producer()
    : m_virtualPayloadSize(1024),
      m_freshness(ndn::time::milliseconds(1000)),
      m_prefixnum(0)
{
    // 初始化 spdlog
    m_logger = spdlog::basic_logger_mt("producer_logger", "logs/producer.log");
    spdlog::set_default_logger(m_logger);
    spdlog::set_level(spdlog::level::info); // 设置日志级别
    spdlog::flush_on(spdlog::level::warn);  // 每条 warn 级别及以上的日志后刷新

    spdlog::info("Producer initialized");
}

void Producer::OnInterest(const ndn::InterestFilter &filter, const ndn::Interest &interest)
{
    spdlog::info("Received Interest: {}", interest.getName().toUri());
    App::OnInterest(filter, interest);

    if (!m_active)
        return;
    // create Data packet
    auto data = std::make_shared<ndn::Data>(interest.getName());
    data->setFreshnessPeriod(m_freshness);

    // generate new data content
    ModelData modelData;
    std::default_random_engine generator(std::random_device{}());   // reate random generator
    std::uniform_real_distribution<double> distribution(0.0, 10.0); // define range (0.0, 10.0)
    modelData.parameters.clear();
    for (int i = 0; i < m_dataSize; ++i)
    {
        modelData.parameters.push_back(distribution(generator)); // generate random double range (0.0, 10.0)
    }

    std::vector<uint8_t> buffer;
    serializeModelData(modelData, buffer); // 序列化数据包
    auto bufferPtr = std::make_shared<ndn::Buffer>(buffer.data(), buffer.data() + buffer.size());
    ndn::Block contentBlock(ndn::tlv::Content, bufferPtr);
    data->setContent(contentBlock);

    // use KeyChain to sign Data packet
    m_keyChain.sign(*data);
    // send Data packet
    m_face.put(*data);
}

void Producer::StartApplication()
{

    App::StartApplication();
    spdlog::info("Producer application started");
    m_face.setInterestFilter(m_prefix.toUri(),
                             std::bind(&Producer::OnInterest, this, std::placeholders::_1, std::placeholders::_2),
                             std::bind(&App::OnRegisterSuccess, this, std::placeholders::_1),
                             std::bind(&App::OnRegisterFailure, this, std::placeholders::_1, std::placeholders::_2));
    m_face.processEvents();
    spdlog::info("Producer finished processing events");
}

void Producer::StopApplication()
{
    App::StopApplication();
    spdlog::info("Producer application stopped");
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        spdlog::error("Usage: {} <prefix>", argv[0]);
        return 1;
    }

    // 创建 Producer 对象
    Producer producer(argv[1]);

    // 启动应用程序
    producer.StartApplication();

    return 0;
}