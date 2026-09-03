#include "TkPacketCSharpCompiler.h"

#include "TkPacketCSharpGenerator.h"
#include "generator/TkPacketCompiler.h"

#include <pstk/packet/TkPacketTool.h>

namespace pstk::packet
{
TkResult CompileCSharp(const TkPacketCompileInfo &compileInfo)
{
    CSharpPacketCodeGenerator codeGenerator(compileInfo.namespaceName);
    PacketCompiler compiler(compileInfo.diagnosticCallback);

    return compiler.Compile(compileInfo.inputPaths, compileInfo.inputPathCount, compileInfo.outputDirectory,
                            codeGenerator);
}
} // namespace pstk::packet
