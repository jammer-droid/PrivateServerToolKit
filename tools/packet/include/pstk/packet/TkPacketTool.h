#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pstk/TkDiagnostic.h>
#include <pstk/TkResult.h>
#include "pstk_packet_export.h"

typedef struct TkPacketCppCompileInfo
{
    const char *const *inputPaths;
    size_t inputPathCount;
    const char *outputDirectory;
    const char *namespaceName;
    TkDiagnosticCallbackInfo diagnosticCallback;
} TkPacketCppCompileInfo;

#ifdef __cplusplus
extern "C"
{
#endif

    PSTK_PACKET_API TkResult TkPacketGetApiVersion(uint32_t *outVersion);

    PSTK_PACKET_API TkResult TkPacketCompileCpp(const TkPacketCppCompileInfo *compileInfo);

#ifdef __cplusplus
}
#endif
