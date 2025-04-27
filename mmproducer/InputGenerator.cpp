#include "InputGenerator.hpp"
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <spdlog/spdlog.h>
#include <algorithm>

// 流包装器实现
class InputGenerator::LimitedIStream : public std::istream
{
public:
    LimitedIStream(std::unique_ptr<std::istream> src, size_t limit)
    try : std
        ::istream(new LimitedStreambuf(src->rdbuf(), limit)),
            m_src(std::move(src)) {}
    catch (...)
    {
        delete rdbuf();
        throw;
    }

    ~LimitedIStream() noexcept override = default;

private:
    std::unique_ptr<std::istream> m_src;

    class LimitedStreambuf : public std::streambuf
    {
    public:
        LimitedStreambuf(std::streambuf *src, size_t limit)
            : m_src(src), m_remaining(limit) {}

    protected:
        int_type underflow() override
        {
            if (m_remaining <= 0)
                return traits_type::eof();
            int_type ch = m_src->sbumpc();
            if (ch != traits_type::eof())
            {
                m_remaining--;
                m_current = traits_type::to_char_type(ch);
                setg(&m_current, &m_current, &m_current + 1);
            }
            return ch;
        }

        std::streamsize xsgetn(char *s, std::streamsize count) override
        {
            count = std::min(count, static_cast<std::streamsize>(m_remaining));
            auto read = m_src->sgetn(s, count);
            m_remaining -= read;
            return read;
        }

    private:
        std::streambuf *m_src;
        size_t m_remaining;
        char m_current;
    };
};

// InputGenerator 成员函数实现
InputGenerator::InputGenerator(const std::string &configFilePath, const std::string &inputFilePath)
    : m_inputFilePath(inputFilePath), m_chunkSize(0), m_totalChunks(0), m_fileSize(0)
{
    boost::property_tree::ptree pt;
    boost::property_tree::ini_parser::read_ini(configFilePath, pt);
    m_chunkSize = pt.get<size_t>("General.chunk-size", 20);
}

size_t InputGenerator::readFile()
{
    std::ifstream inputFile(m_inputFilePath, std::ios::binary | std::ios::ate);
    if (!inputFile)
        throw std::runtime_error("Failed to open input file");

    m_fileSize = inputFile.tellg();
    inputFile.close();

    m_totalChunks = (m_fileSize + m_chunkSize - 1) / m_chunkSize;
    calculateChunkOffsets();

    return m_totalChunks;
}

void InputGenerator::calculateChunkOffsets()
{
    m_chunkOffsets.clear();
    for (size_t offset = 0; offset < m_fileSize; offset += m_chunkSize)
    {
        m_chunkOffsets.push_back(offset);
    }
}

std::unique_ptr<std::istream> InputGenerator::getChunk(size_t chunkNumber)
{
    if (chunkNumber >= m_totalChunks)
    {
        throw std::out_of_range("Invalid chunk number");
    }

    auto inputFile = std::make_unique<std::ifstream>(
        m_inputFilePath, std::ios::binary);
    if (!inputFile->is_open())
    {
        throw std::runtime_error("Failed to open input file");
    }

    inputFile->seekg(m_chunkOffsets[chunkNumber], std::ios::beg);
    if (!inputFile->good())
    {
        throw std::runtime_error("seekg failed");
    }

    // 计算实际块大小
    const size_t actualChunkSize = (chunkNumber == m_totalChunks - 1)
                                       ? (m_fileSize - m_chunkOffsets[chunkNumber])
                                       : m_chunkSize;

    return std::make_unique<LimitedIStream>(
        std::move(inputFile), actualChunkSize);
}