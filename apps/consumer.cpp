#include <iostream>
#include <ndn-cxx/face.hpp>
#include <ndn-cxx/security/key-chain.hpp>

class Consumer
{
public:
    void run()
    {
        // 创建一个 Interest 数据包
        ndn::Interest interest("/example/testApp");
        interest.setCanBePrefix(false);
        interest.setInterestLifetime(ndn::time::seconds(2));

        std::cout << "Sending Interest: " << interest << std::endl;

        // 发送 Interest 数据包
        m_face.expressInterest(interest,
                               bind(&Consumer::onData, this, _1, _2),
                               bind(&Consumer::onNack, this, _1, _2),
                               bind(&Consumer::onTimeout, this, _1));
        // 处理事件循环
        m_face.processEvents();
    }

private:
    void onData(const ndn::Interest &interest, const ndn::Data &data)
    {
        std::cout << "Received Data: " << data << std::endl;
    }

    void onNack(const ndn::Interest &interest, const ndn::lp::Nack &nack)
    {
        std::cerr << "Received Nack for Interest: " << interest << " (" << nack.getReason() << ")" << std::endl;
    }

    void onTimeout(const ndn::Interest &interest)
    {
        std::cerr << "Interest timeout for: " << interest << std::endl;
    }

private:
    ndn::Face m_face;
    ndn::KeyChain m_keyChain;
};

int main()
{
    Consumer consumer;
    consumer.run();
    return 0;
}