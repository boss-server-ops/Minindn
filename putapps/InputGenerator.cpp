#include "InputGenerator.hpp"
#include <iostream>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <spdlog/spdlog.h>

InputGenerator::InputGenerator(const std::string &configFilePath, const std::string &inputFilePath)
    : m_inputFilePath(inputFilePath), m_chunkSize(0), m_totalChunks(0), m_fileSize(0)
{
    // 读取配置文件
    boost::property_tree::ptree pt;
    boost::property_tree::ini_parser::read_ini(configFilePath, pt);

    // 获取块大小
    m_chunkSize = pt.get<size_t>("General.chunk-size", 20);
}

size_t InputGenerator::readFile()
{
    // 打开输入文件
    std::ifstream inputFile(m_inputFilePath, std::ios::binary | std::ios::ate);
    if (!inputFile)
    {
        throw std::runtime_error("Failed to open input file");
    }

    // 获取文件大小并保存
    m_fileSize = inputFile.tellg();
    inputFile.close(); // 关闭文件，后续再打开计算偏移量

    // 计算总块数
    m_totalChunks = (m_fileSize + m_chunkSize - 1) / m_chunkSize;

    // 计算每个块的偏移量
    calculateChunkOffsets();

    return m_totalChunks;
}

void InputGenerator::calculateChunkOffsets()
{
    m_chunkOffsets.clear();
    for (std::streampos offset = 0; offset < m_fileSize; offset += m_chunkSize)
    {
        m_chunkOffsets.push_back(offset);
    }
}

std::unique_ptr<std::istream> InputGenerator::getChunk(size_t chunkNumber)
{
    if (chunkNumber >= m_totalChunks)
    {
        spdlog::debug("Chunk number out of range");
        throw std::out_of_range("Chunk number out of range");
    }

    std::ifstream *inputFile = new std::ifstream(m_inputFilePath, std::ios::binary);
    if (!inputFile->is_open())
    {
        spdlog::debug("Failed to open input file");
        throw std::runtime_error("Failed to open input file");
    }

    spdlog::debug("Starting seekg()");
    inputFile->seekg(m_chunkOffsets[chunkNumber], std::ios::beg);
    if (!inputFile->good())
    {
        spdlog::error("seekg failed for chunk {}", chunkNumber);
        throw std::runtime_error("seekg failed");
    }
    spdlog::debug("Finished seekg()");

    return std::unique_ptr<std::istream>(inputFile);
}