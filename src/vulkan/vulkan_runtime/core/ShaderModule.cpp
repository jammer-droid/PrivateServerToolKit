#include "ShaderModule.h"

#include "common/VulkanException.h"

#include <vector>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <ios>
#include <string>
#include <limits>

namespace
{

std::vector<std::uint32_t> ReadSpirV(const std::filesystem::path &shaderPath)
{
    const std::string pathString = shaderPath.string();
    std::ifstream ifs(shaderPath, std::ios::binary | std::ios::ate);
    if (!ifs.is_open())
    {
        throw std::runtime_error("File Open Failed: " + pathString);
    }

    const std::streamoff fileSize = ifs.tellg();

#ifndef NDEBUG
    std::cout << "File Open: " << pathString << "(" << fileSize << "B)\n";
#endif

    if (!(fileSize > 0 && fileSize % 4 == 0))
    {
        throw std::runtime_error("Invalid SPIR-V file size: " + pathString);
    }

    const std::uintmax_t unsignedFileSize = static_cast<std::uintmax_t>(fileSize);
    if (unsignedFileSize > std::numeric_limits<std::size_t>::max() ||
        unsignedFileSize > static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max()))
    {
        throw std::runtime_error("SPIR-V file is too large: " + pathString);
    }

    const std::size_t byteSize = static_cast<std::size_t>(fileSize);
    std::vector<std::uint32_t> shaderBytes(byteSize / sizeof(std::uint32_t));
    ifs.seekg(0, std::ios::beg);

    if (!ifs.read(reinterpret_cast<char *>(shaderBytes.data()), static_cast<std::streamsize>(byteSize)))
    {
        throw std::runtime_error("Failed to read SPIR-V: " + pathString);
    }

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
