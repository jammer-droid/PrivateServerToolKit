#pragma once

#include <stdint.h>

#include <pstk/TkResult.h>
#include "pstk_packet_export.h"

#ifdef __cplusplus
extern "C"
{
#endif

    PSTK_PACKET_API TkResult TkPacketGetApiVersion(uint32_t *outVersion);

#ifdef __cplusplus
}
#endif
