#include "ShaderModule.h"

#include "common/VulkanException.h"

#include <vector>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <ios>
#include <string>

namespace
{

std::vector<std::uint32_t> ReadSpirV(const std::filesystem::path &shaderPath)
{
    std::string pathString(shaderPath.c_str());
    std::ifstream ifs(shaderPath, std::ios::binary | std::ios::ate);
    if (!ifs.is_open())
    {
        throw std::runtime_error("File Open Failed: " + pathString);
    }

    std::streamsize fileSize = ifs.tellg();

#ifndef NDEBUG
    std::cout << "File Open: " << pathString << "(" << fileSize << "B)\n";
#endif

    if (!(fileSize > 0 && fileSize % 4 == 0))
    {
        throw std::runtime_error("Invalid File size");
    }

    std::vector<std::uint32_t> shaderBytes(static_cast<std::uint32_t>(fileSize) / sizeof(std::uint32_t));
    ifs.seekg(0, std::ios::beg);

    ifs.read(reinterpret_cast<char *>(shaderBytes.data()), fileSize);

    ifs.close();

    return shaderBytes;
}

}; // namespace

ShaderModule::ShaderModule(VkDevice device, const std::filesystem::path &shaderPath)
{
    const std::vector<std::uint32_t> words = ReadSpirV(shaderPath);

    VkShaderModuleCreateInfo moduleCreateInfo{};
    moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleCreateInfo.codeSize = words.size() * sizeof(std::uint32_t);
    moduleCreateInfo.pCode = words.data();

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    VK_CHECK(vkCreateShaderModule(device, &moduleCreateInfo, nullptr, &shaderModule));

    shaderHandle_.Adopt(shaderModule, deleter::VkShaderModuleDeleter{device});
}
