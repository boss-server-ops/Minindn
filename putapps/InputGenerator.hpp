#ifndef IMA_INPUT_GENERATOR_HPP
#define IMA_INPUT_GENERATOR_HPP

#include <string>
#include <vector>
#include <fstream>
#include <memory>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

class InputGenerator
{
public:
    InputGenerator(const std::string &configFilePath, const std::string &inputFilePath);

    // read the configuration file and calculate how many chunks the file can be divided into
    size_t readFile();

    std::unique_ptr<std::istream> getChunk(size_t chunkNumber);

private:
    std::string m_inputFilePath;
    size_t m_chunkSize;
    size_t m_totalChunks;
    std::vector<std::streampos> m_chunkOffsets;
    size_t m_fileSize;
    void calculateChunkOffsets();
};

#endif // IMA_INPUT_GENERATOR_HPP