#include <iostream>
#include <ndn-cxx/face.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/encoding/block.hpp>
#include <memory>

class Producer
{
public:
    void run()
    {
        // 注册前缀
        m_face.setInterestFilter("/example/testApp",
                                 std::bind(&Producer::onInterest, this, std::placeholders::_1, std::placeholders::_2),
                                 std::bind(&Producer::onRegisterSuccess, this, std::placeholders::_1),
                                 std::bind(&Producer::onRegisterFailure, this, std::placeholders::_1, std::placeholders::_2));
        std::cout << "Registering prefix /example/testApp" << std::endl;

        // 处理事件循环
        m_face.processEvents();
    }

private:
    void onInterest(const ndn::InterestFilter &filter, const ndn::Interest &interest)
    {
        std::cout << "Received Interest: " << interest << std::endl;

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
        std::cout << "Successfully registered prefix: " << prefix << std::endl;
    }

    void onRegisterFailure(const ndn::Name &prefix, const std::string &reason)
    {
        std::cerr << "Failed to register prefix: " << prefix << " (" << reason << ")" << std::endl;
    }

private:
    ndn::Face m_face;
    ndn::KeyChain m_keyChain;
};

int main()
{
    Producer producer;
    producer.run();
    return 0;
}