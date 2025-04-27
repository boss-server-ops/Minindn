#include "consumer.hpp"

#include <ndn-cxx/util/exception.hpp>
#include <spdlog/spdlog.h>
#include <fstream>

namespace ndn::chunks
{

    Consumer::Consumer(security::Validator &validator, std::ostream &os)
        : m_validator(validator), m_outputStream(os)
    {
    }

    void
    Consumer::run(std::unique_ptr<DiscoverVersion> discover, std::unique_ptr<ChunksInterests> chunks)
    {
        m_discover = std::move(discover);
        m_chunks = std::move(chunks);
        m_nextToPrint = 0;
        m_bufferedData.clear();

        m_discover->onDiscoverySuccess.connect([this](const Name &versionedName)
                                               { m_chunks->run(versionedName,
                                                               FORWARD_TO_MEM_FN(handleData),
                                                               [](const std::string &msg)
                                                               { NDN_THROW(std::runtime_error(msg)); }); });
        m_discover->onDiscoveryFailure.connect([](const std::string &msg)
                                               { NDN_THROW(std::runtime_error(msg)); });
        m_discover->run();
        spdlog::debug("Consumer::run() finished");
    }
    void
    Consumer::handleData(std::map<uint64_t, std::shared_ptr<const Data>> &data)
    {
        if (data.empty())
        {
            spdlog::warn("Received empty data map in handleData");
            return;
        }

        // 获取第一个 Data 的名字，用于构造文件路径
        auto firstData = data.begin()->second;
        if (!firstData)
        {
            spdlog::error("First data is null");
            return;
        }

        const Name &dataName = firstData->getName();

        // 构造文件路径
        if (dataName.size() < 2)
        {
            spdlog::error("Data name has insufficient components to construct file path");
            return;
        }

        // 去掉第一个组件和最后一个组件
        Name filePathName = dataName.getSubName(1, dataName.size() - 2);

        // 添加 "../experiments" 前缀
        std::string filePath = "../conreceived" + filePathName.toUri();

        spdlog::info("Writing data to file: {}", filePath);

        // 打开文件（追加模式）并写入内容
        std::ofstream outputFile(filePath, std::ios::binary | std::ios::app);
        if (!outputFile.is_open())
        {
            spdlog::error("Failed to open file: {}", filePath);
            return;
        }

        // 遍历所有段并写入文件
        for (const auto &entry : data)
        {
            const auto &segmentData = entry.second;
            if (!segmentData)
            {
                spdlog::warn("Encountered null Data segment, skipping");
                continue;
            }

            const Block &content = segmentData->getContent();
            outputFile.write(reinterpret_cast<const char *>(content.value()), content.value_size());
        }

        outputFile.close();
        spdlog::info("All data segments successfully written to file: {}", filePath);
    }

    // void
    // Consumer::writeInOrderData()
    // {
    //     for (auto it = m_bufferedData.begin();
    //          it != m_bufferedData.end() && it->first == m_nextToPrint;
    //          it = m_bufferedData.erase(it), ++m_nextToPrint)
    //     {
    //         const Block &content = it->second->getContent();
    //         m_outputStream.write(reinterpret_cast<const char *>(content.value()), content.value_size());
    //     }
    // }

} // namespace ndn::chunks
