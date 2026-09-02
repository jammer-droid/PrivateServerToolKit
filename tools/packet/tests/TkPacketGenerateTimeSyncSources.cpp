#include "generator/cpp/TkPacketCppCompiler.h"

#include <pstk/packet/TkPacketTool.h>

#include <cstddef>

int main(const int argumentCount, const char *const *const arguments)
{
    if (argumentCount != 4)
    {
        return 1;
    }

    const char *const inputPaths[] = {arguments[1], arguments[2]};
    const TkPacketCompileInfo compileInfo = {
        inputPaths,         sizeof(inputPaths) / sizeof(inputPaths[0]),
        arguments[3],       "pstk::packet::generated_time_sync",
        {nullptr, nullptr},
    };

    return pstk::packet::CompileCpp(compileInfo) == TK_SUCCESS ? 0 : 1;
}
