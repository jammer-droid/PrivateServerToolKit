#include <cstdint>
#include <cstdio>

#include <pstk/packet/TkPacketTool.h>

int main()
{
    uint32_t apiVersion = 0;
    TkResult result = TkPacketGetApiVersion(&apiVersion);

    if (result != TK_SUCCESS || apiVersion != 1U)
    {
        std::fprintf(stderr, "Unexpected Packet API version: %u\n", static_cast<unsigned int>(apiVersion));

        return 1;
    }

    TkResult nullResult = TkPacketGetApiVersion(nullptr);

    if (nullResult != TK_ERROR_INVALID_ARGUMENT)
    {
        std::fprintf(stderr, "Null output was not rejected: %d\n", static_cast<int>(nullResult));

        return 1;
    }

    return 0;
}
