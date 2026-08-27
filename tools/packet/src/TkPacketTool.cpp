#include "pstk/packet/TkPacketTool.h"
#include "TkPacketCompiler.h"

#include <new>

TkResult TkPacketGetApiVersion(uint32_t *const outVersion)
{
    if (outVersion == nullptr)
    {
        return TK_ERROR_INVALID_ARGUMENT;
    }

    *outVersion = 1U;

    return TK_SUCCESS;
}

TkResult TkPacketCompileCpp(const TkPacketCppCompileInfo *const compileInfo)
{
    if (compileInfo == nullptr)
    {
        return TK_ERROR_INVALID_ARGUMENT;
    }

    try
    {
        return pstk::packet::CompileCpp(*compileInfo);
    }
    catch (const std::bad_alloc &)
    {
        return TK_ERROR_OUT_OF_MEMORY;
    }
    catch (...)
    {
        return TK_ERROR_UNKNOWN;
    }
}
