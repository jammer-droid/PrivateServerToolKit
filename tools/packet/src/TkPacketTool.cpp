#include "pstk/packet/TkPacketTool.h"

TkResult TkPacketGetApiVersion(uint32_t *const outVersion)
{
    if (outVersion == nullptr)
    {
        return TK_ERROR_INVALID_ARGUMENT;
    }

    *outVersion = 1U;

    return TK_SUCCESS;
}
