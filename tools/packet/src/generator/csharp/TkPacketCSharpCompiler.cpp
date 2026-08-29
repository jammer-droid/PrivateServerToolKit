#include "TkPacketCSharpCompiler.h"

#include "TkPacketCSharpGenerator.h"
#include "generator/TkPacketCompiler.h"

#include <pstk/packet/TkPacketTool.h>

namespace pstk::packet
{
TkResult CompileCSharp(const TkPacketCompileInfo &compileInfo)
{
    if (compileInfo.namespaceName == nullptr || compileInfo.namespaceName[0] == '\0')
    {
        return TK_ERROR_INVALID_ARGUMENT;
    }

    CSharpPacketCodeGenerator codeGenerator(compileInfo.namespaceName);
    PacketCompiler compiler(compileInfo.diagnosticCallback);

    return compiler.Compile(compileInfo.inputPaths, compileInfo.inputPathCount, compileInfo.outputDirectory,
                            codeGenerator);
}
} // namespace pstk::packet
