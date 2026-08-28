#include "TkPacketCppCompiler.h"

#include "TkPacketCppGenerator.h"
#include "TkPacketGeneratedFileCommitter.h"

#include "schema/TkPacketSchemaCompiler.h"

#include <pstk/packet/TkPacketTool.h>

#include <cstddef>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace pstk::packet
{
namespace
{
bool IsNonEmpty(const char *const value)
{
    return value != nullptr && value[0] != '\0';
}

bool IsValidCppCompileInfo(const TkPacketCppCompileInfo &compileInfo)
{
    return IsNonEmpty(compileInfo.outputDirectory) && IsNonEmpty(compileInfo.namespaceName);
}
} // namespace

TkResult CompileCpp(const TkPacketCppCompileInfo &compileInfo)
{
    if (!IsValidCppCompileInfo(compileInfo))
    {
        return TK_ERROR_INVALID_ARGUMENT;
    }

    PacketDescriptorSet descriptorSet;
    PacketSchemaCompiler schemaCompiler(compileInfo.diagnosticCallback);
    TkResult result = schemaCompiler.Compile(compileInfo.inputPaths, compileInfo.inputPathCount, &descriptorSet);
    if (result != TK_SUCCESS)
    {
        return result;
    }

    std::vector<std::string> generatedHeaders;
    generatedHeaders.reserve(descriptorSet.packets.size());
    for (const PacketDescriptor &descriptor : descriptorSet.packets)
    {
        std::string generatedHeader;
        result = GenerateCppHeader(descriptor, compileInfo.namespaceName, &generatedHeader);
        if (result != TK_SUCCESS)
        {
            return result;
        }

        generatedHeaders.push_back(std::move(generatedHeader));
    }

    const std::filesystem::path outputDirectory = std::filesystem::u8path(compileInfo.outputDirectory);
    for (std::size_t packetIndex = 0; packetIndex < descriptorSet.packets.size(); ++packetIndex)
    {
        const PacketDescriptor &descriptor = descriptorSet.packets[packetIndex];
        const std::filesystem::path fileName = std::filesystem::u8path(descriptor.name + ".generated.h");
        const std::filesystem::path finalPath = outputDirectory / fileName;

        result = CommitGeneratedFile(finalPath, generatedHeaders[packetIndex], compileInfo.diagnosticCallback);
        if (result != TK_SUCCESS)
        {
            return result;
        }
    }

    return TK_SUCCESS;
}
} // namespace pstk::packet
