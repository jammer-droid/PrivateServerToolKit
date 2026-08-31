#include "TkPacketCppCompiler.h"

#include "TkPacketCppGenerator.h"
#include "generator/TkPacketCompiler.h"

#include <pstk/packet/TkPacketTool.h>

namespace pstk::packet
{
TkResult CompileCpp(const TkPacketCompileInfo &compileInfo)
{
    CppPacketCodeGenerator codeGenerator(compileInfo.namespaceName);
    PacketCompiler compiler(compileInfo.diagnosticCallback);

    return compiler.Compile(compileInfo.inputPaths, compileInfo.inputPathCount, compileInfo.outputDirectory,
                            codeGenerator);
}
} // namespace pstk::packet
