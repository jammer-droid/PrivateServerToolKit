#include "TkPacketCppCompiler.h"

#include "schema/TkPacketSchemaCompiler.h"

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
    return schemaCompiler.Compile(compileInfo.inputPaths, compileInfo.inputPathCount, &descriptorSet);
}
} // namespace pstk::packet
