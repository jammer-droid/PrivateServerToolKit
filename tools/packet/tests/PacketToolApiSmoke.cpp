#include <cstdint>
#include <cstdio>

#include <pstk/packet/PacketToolApi.h>

int main()
{
    const std::uint32_t apiVersion = PstkPacketGetApiVersion();

    if (apiVersion != 1U)
    {
        std::fprintf(stderr, "Unexpected Packet API version: %u\n", static_cast<unsigned int>(apiVersion));

        return 1;
    }

    return 0;
}
