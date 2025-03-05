#ifndef IMA_INPUT_GENERATOR_HPP
#define IMA_INPUT_GENERATOR_HPP

#include <fstream>
#include <vector>
#include <memory>
#include <string>

class InputGenerator
{
public:
    InputGenerator(const std::string &configFilePath, const std::string &inputFilePath);
    size_t readFile();
    std::unique_ptr<std::istream> getChunk(size_t chunkNumber);

private:
    std::string m_inputFilePath;
    size_t m_chunkSize;
    size_t m_totalChunks;
    size_t m_fileSize;
    std::vector<std::streampos> m_chunkOffsets;

    void calculateChunkOffsets();

    // 流包装器定义
    class LimitedIStream;
};

#endif // IMA_INPUT_GENERATOR_HPP