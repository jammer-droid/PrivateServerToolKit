#include "TkPacketCompiler.h"

#include "TkPacketGeneratedFileCommitter.h"
#include "schema/TkPacketSchemaCompiler.h"

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
} // namespace

PacketCompiler::PacketCompiler(const TkDiagnosticCallbackInfo diagnosticCallback)
    : diagnosticCallback_(diagnosticCallback)
{
}

TkResult PacketCompiler::Compile(const char *const *const inputPaths, const std::size_t inputPathCount,
                                 const char *const outputDirectory, const PacketCodeGenerator &codeGenerator) const
{
    if (!IsNonEmpty(outputDirectory))
    {
        return TK_ERROR_INVALID_ARGUMENT;
    }

    PacketDescriptorSet descriptorSet;
    PacketSchemaCompiler schemaCompiler(diagnosticCallback_);
    TkResult result = schemaCompiler.Compile(inputPaths, inputPathCount, &descriptorSet);
    if (result != TK_SUCCESS)
    {
        return result;
    }

    std::vector<GeneratedFile> generatedFiles;
    generatedFiles.reserve(descriptorSet.packets.size());
    for (const PacketDescriptor &descriptor : descriptorSet.packets)
    {
        GeneratedFile generatedFile;
        result = codeGenerator.Generate(descriptor, &generatedFile);
        if (result != TK_SUCCESS)
        {
            return result;
        }

        generatedFiles.push_back(std::move(generatedFile));
    }

    const std::filesystem::path outputPath = std::filesystem::u8path(outputDirectory);
    for (const GeneratedFile &generatedFile : generatedFiles)
    {
        const std::filesystem::path finalPath = outputPath / generatedFile.fileName;
        result = CommitGeneratedFile(finalPath, generatedFile.contents, diagnosticCallback_);
        if (result != TK_SUCCESS)
        {
            return result;
        }
    }

    return TK_SUCCESS;
}
} // namespace pstk::packet
