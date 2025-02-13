#include "ndn-app.hpp"
#include <experimental/filesystem>
namespace std
{
    namespace fs = experimental::filesystem;
}
#include <fstream>
#include <sstream>
#include <cstdlib> // for system()
#include <limits>  // for std::numeric_limits
#include "algorithm/include/AggregationTree.hpp"
#include "algorithm/utility/utility.hpp"
App::App()
    : m_active(false),
      m_face(),
      m_appId(std::numeric_limits<uint32_t>::max())
{
    // initialize spdlog
    m_logger = spdlog::basic_logger_mt("app_logger", "logs/app.log");
    spdlog::set_default_logger(m_logger);
    spdlog::set_level(spdlog::level::info); // set log level
    spdlog::flush_on(spdlog::level::info);  // flush after each log

    spdlog::info("App initialized");
}

App::~App()
{
    spdlog::info("App destroyed");
}

uint32_t App::GetId() const
{
    return m_appId;
}

void App::OnInterest(const ndn::InterestFilter &filter, const ndn::Interest &interest)
{
    spdlog::info("Received Interest: {}", interest.getName().toUri());
}

void App::OnRegisterSuccess(const ndn::Name &prefix)
{
    spdlog::info("Successfully registered prefix: {}", prefix.toUri());
}

void App::OnRegisterFailure(const ndn::Name &prefix, const std::string &reason)
{
    spdlog::error("Failed to register prefix: {} ({})", prefix.toUri(), reason);
}

void App::OnData(const ndn::Interest &interest, const ndn::Data &data)
{
    spdlog::info("Received Data: {}", data.getName().toUri());
}

void App::OnNack(const ndn::Interest &interest, const ndn::lp::Nack &nack)
{
    spdlog::error("Received Nack for Interest: {} ({})", interest.getName().toUri(), static_cast<int>(nack.getReason()));
}

void App::OnTimeout(const ndn::Interest &interest)
{
    spdlog::error("Interest timeout for: {}", interest.getName().toUri());
}

void App::ConstructAggregationTree()
{
    // Actual function is implemented in consumer
}

void App::StartApplication()
{
    if (m_active)
        return;

    m_active = true;
    spdlog::info("Application started");
}

void App::StopApplication()
{
    if (!m_active)
        return;
    m_active = false;
    m_face.shutdown();
    spdlog::info("Application stopped");
}

std::set<std::string> App::findLeafNodes(const std::string &key, const std::map<std::string, std::vector<std::string>> &treeMap)
{
    std::set<std::string> result;
    auto it = treeMap.find(key);
    if (it != treeMap.end())
    {
        for (const auto &subkey : it->second)
        {
            if (treeMap.find(subkey) != treeMap.end())
            {
                auto subResult = findLeafNodes(subkey, treeMap);
                result.insert(subResult.begin(), subResult.end());
            }
            else
            {
                result.insert(subkey);
            }
        }
    }
    return result;
}

std::map<std::string, std::set<std::string>> App::getLeafNodes(const std::string &key, const std::map<std::string, std::vector<std::string>> &treeMap)
{
    std::map<std::string, std::set<std::string>> result;
    auto it = treeMap.find(key);
    if (it != treeMap.end())
    {
        for (const auto &subkey : it->second)
        {
            if (treeMap.find(subkey) != treeMap.end())
            {
                result[subkey] = findLeafNodes(subkey, treeMap);
            }
            else
            {
                result[subkey].insert(subkey);
            }
        }
    }
    return result;
}

int App::findRoundIndex(const std::vector<std::vector<std::string>> &roundVec, const std::string &target)
{
    for (int i = 0; i < roundVec.size(); ++i)
    {
        for (int j = 0; j < roundVec[i].size(); ++j)
        {
            if (roundVec[i][j] == target)
            {
                return i; // Return index
            }
        }
    }
    spdlog::error("Error! Can't find round index");
    return -1;
}

void App::CheckDirectoryExist(const std::string &path)
{
    if (!std::fs::exists(path))
    {
        if (!std::fs::create_directories(path))
        {
            spdlog::error("Failed to create directory: {}", path);
            exit(EXIT_FAILURE); // Stop execution if unable to create directory
        }
    }
}

void App::OpenFile(const std::string &filename)
{
    std::ofstream file(filename, std::ofstream::out | std::ofstream::trunc);
    if (!file.is_open())
    {
        spdlog::error("Failed to open the file: {}", filename);
    }
    file.close();
}