#include "ModelData.hpp"
#include <cstring> // for memcpy
#include <spdlog/spdlog.h>

/**
 * Constructor
 */
ModelData::ModelData()
    : qsf(-1.0) // Initialize qsf as a double
{
    // waiting for modify path
    int parameterSize = readDataSizeFromConfig("experiments/config.ini");
    parameters.resize(parameterSize, 0.0);
}

// Function to read DataSize from the [General] section of a configuration file
int readDataSizeFromConfig(const std::string &filename)
{
    boost::property_tree::ptree pt;
    try
    {
        boost::property_tree::ini_parser::read_ini(filename, pt);
        int dataSize = pt.get<int>("General.DataSize"); // Using get to fetch the value and convert to int
        return dataSize;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Exception caught: " << e.what() << std::endl;
        return 150; // Default value on error
    }
}

/**
 * @brief Serialize a ModelData object
 *
 * @param modelData The ModelData object to serialize
 * @param buffer The buffer to store the serialized data
 */
void serializeModelData(const ModelData &modelData, std::vector<uint8_t> &buffer)
{
    // Clear the buffer first
    buffer.clear();

    // Transfer ModelData.parameters into bytes
    size_t paramSize = modelData.parameters.size() * sizeof(double);
    buffer.resize(paramSize);
    std::copy(reinterpret_cast<const uint8_t *>(modelData.parameters.data()),
              reinterpret_cast<const uint8_t *>(modelData.parameters.data()) + paramSize,
              buffer.data());

    // Transfer ModelData.qsf into bytes (now double instead of int)
    buffer.insert(buffer.end(), reinterpret_cast<const uint8_t *>(&modelData.qsf), reinterpret_cast<const uint8_t *>(&modelData.qsf + 1));

    // Serialize ModelData.congestedNodes into bytes
    for (const auto &str : modelData.congestedNodes)
    {
        uint32_t strLength = static_cast<uint32_t>(str.size());
        buffer.insert(buffer.end(), reinterpret_cast<uint8_t *>(&strLength), reinterpret_cast<uint8_t *>(&strLength + 1)); // Insert the length of each string within the vector
        buffer.insert(buffer.end(), str.begin(), str.end());                                                               // Insert the string
    }

    spdlog::info("Serialized ModelData with {} parameters and {} congested nodes", modelData.parameters.size(), modelData.congestedNodes.size());
}

/**
 * @brief Deserialize buffer data into a ModelData object
 *
 * @param buffer The buffer containing the serialized data
 * @param modelData The ModelData object to store the deserialized data
 * @return True if deserialization is successful, otherwise false
 */
bool deserializeModelData(const std::vector<uint8_t> &buffer, ModelData &modelData)
{
    // Transfer ModelData.parameters back
    size_t paramSize = modelData.parameters.size() * sizeof(double);
    if (buffer.size() < paramSize)
    {
        spdlog::error("Buffer size is smaller than expected!");
        return false;
    }
    std::copy(buffer.data(), buffer.data() + paramSize, reinterpret_cast<uint8_t *>(modelData.parameters.data()));

    // Transfer ModelData.qsf back (now double instead of int)
    size_t currentIndex = paramSize;
    if (currentIndex + sizeof(double) > buffer.size())
    {
        std::cout << "Buffer size can't hold qsf value!" << std::endl;
        return false;
    }
    std::copy(buffer.data() + currentIndex, buffer.data() + currentIndex + sizeof(double), reinterpret_cast<uint8_t *>(&modelData.qsf));
    currentIndex += sizeof(double);
    // Deserialize ModelData.congestedNodes
    while (currentIndex < buffer.size())
    {
        if (currentIndex + sizeof(uint32_t) > buffer.size())
        {
            spdlog::error("Buffer size can't hold string length!");
            return false;
        }

        uint32_t strLength;
        std::copy(buffer.data() + currentIndex, buffer.data() + currentIndex + sizeof(uint32_t), reinterpret_cast<uint8_t *>(&strLength)); // Copy memory of the string's length
        currentIndex += sizeof(uint32_t);

        if (currentIndex + strLength > buffer.size())
        {
            spdlog::error("Buffer size can't hold string content!");
            return false;
        }
        std::string str(reinterpret_cast<const char *>(buffer.data() + currentIndex), strLength);
        modelData.congestedNodes.push_back(str);
        currentIndex += strLength;
    }

    spdlog::info("Deserialized ModelData with {} parameters and {} congested nodes", modelData.parameters.size(), modelData.congestedNodes.size());
    return true;
}