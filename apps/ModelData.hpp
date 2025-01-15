#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <iostream>

struct ModelData
{
    std::vector<double> parameters; // Model parameters
    double qsf;
    std::vector<std::string> congestedNodes;

    ModelData();
};
/**
 * @brief Serialize a ModelData object
 *
 * @param modelData The ModelData object to serialize
 * @param buffer The buffer to store the serialized data
 */
void serializeModelData(const ModelData &modelData, std::vector<uint8_t> &buffer);

/**
 * @brief Deserialize buffer data into a ModelData object
 *
 * @param buffer The buffer containing the serialized data
 * @param modelData The ModelData object to store the deserialized data
 * @return True if deserialization is successful, otherwise false
 */
bool deserializeModelData(const std::vector<uint8_t> &buffer, ModelData &modelData);