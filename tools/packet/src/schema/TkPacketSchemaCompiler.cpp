#include "TkPacketSchemaCompiler.h"

#include "TkPacketDescriptorBuilder.h"
#include "TkPacketDiagnostic.h"
#include "TkPacketJsonParser.h"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <unordered_set>
#include <utility>

namespace pstk::packet
{
namespace
{
bool IsNonEmpty(const char *const value)
{
    return value != nullptr && value[0] != '\0';
}

bool IsValidInput(const char *const *const inputPaths, const std::size_t inputPathCount,
                  const PacketDescriptorSet *const outDescriptorSet)
{
    if (inputPaths == nullptr || inputPathCount == 0 || outDescriptorSet == nullptr)
    {
        return false;
    }

    for (std::size_t inputIndex = 0; inputIndex < inputPathCount; ++inputIndex)
    {
        if (!IsNonEmpty(inputPaths[inputIndex]))
        {
            return false;
        }
    }

    return true;
}

TkResult ReadPacketSource(const char *const inputPath, const TkDiagnosticCallbackInfo &callbackInfo,
                          PacketSource *const outSource)
{
    std::ifstream input(inputPath, std::ios::binary);
    if (!input)
    {
        return ReportFailure(callbackInfo, TK_ERROR_IO, SourceReadFailedId, inputPath, {},
                             "failed to open packet schema");
    }

    std::string contents{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    if (input.bad())
    {
        return ReportFailure(callbackInfo, TK_ERROR_IO, SourceReadFailedId, inputPath, {},
                             "failed to read packet schema");
    }

    *outSource = {inputPath, std::move(contents)};
    return TK_SUCCESS;
}

TkResult ValidateBatchPacket(const PacketSchema &schema, std::unordered_set<std::uint16_t> *const packetIds,
                             std::unordered_set<std::string> *const packetNames,
                             const TkDiagnosticCallbackInfo &callbackInfo)
{
    if (!packetIds->insert(schema.id).second)
    {
        return ReportFailure(callbackInfo, TK_ERROR_INVALID_DATA, DuplicatePacketIdId, schema.sourceName, "packet.id",
                             "duplicate packet ID " + std::to_string(schema.id));
    }

    if (!packetNames->insert(schema.name).second)
    {
        return ReportFailure(callbackInfo, TK_ERROR_INVALID_DATA, DuplicatePacketNameId, schema.sourceName,
                             "packet.name", "duplicate packet name '" + schema.name + "'");
    }

    return TK_SUCCESS;
}
} // namespace

PacketSchemaCompiler::PacketSchemaCompiler(const TkDiagnosticCallbackInfo diagnosticCallback)
    : diagnosticCallback_(diagnosticCallback)
{
}

TkResult PacketSchemaCompiler::Compile(const char *const *const inputPaths, const std::size_t inputPathCount,
                                       PacketDescriptorSet *const outDescriptorSet) const
{
    if (!IsValidInput(inputPaths, inputPathCount, outDescriptorSet))
    {
        return TK_ERROR_INVALID_ARGUMENT;
    }

    PacketJsonParser parser(diagnosticCallback_);
    PacketDescriptorBuilder descriptorBuilder(diagnosticCallback_);
    PacketDescriptorSet descriptorSet;
    descriptorSet.packets.reserve(inputPathCount);

    std::unordered_set<std::uint16_t> packetIds;
    std::unordered_set<std::string> packetNames;
    packetIds.reserve(inputPathCount);
    packetNames.reserve(inputPathCount);

    for (std::size_t inputIndex = 0; inputIndex < inputPathCount; ++inputIndex)
    {
        PacketSource source;
        TkResult result = ReadPacketSource(inputPaths[inputIndex], diagnosticCallback_, &source);
        if (result != TK_SUCCESS)
        {
            return result;
        }

        PacketSchema schema;
        result = parser.Parse(source, &schema);
        if (result != TK_SUCCESS)
        {
            return result;
        }

        result = ValidateBatchPacket(schema, &packetIds, &packetNames, diagnosticCallback_);
        if (result != TK_SUCCESS)
        {
            return result;
        }

        PacketDescriptor descriptor;
        result = descriptorBuilder.Build(schema, &descriptor);
        if (result != TK_SUCCESS)
        {
            return result;
        }

        descriptorSet.packets.push_back(std::move(descriptor));
    }

    *outDescriptorSet = std::move(descriptorSet);
    return TK_SUCCESS;
}
} // namespace pstk::packet
