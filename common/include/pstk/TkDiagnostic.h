#pragma once

#include <stddef.h>
#include <stdint.h>

typedef enum TkDiagnosticSeverity
{
    TK_DIAGNOSTIC_INFO = 0,
    TK_DIAGNOSTIC_WARNING = 1,
    TK_DIAGNOSTIC_ERROR = 2,

    TK_DIAGNOSTIC_SEVERITY_MAX_ENUM = 0x7FFFFFFF
} TkDiagnosticSeverity;

/*
 * - sourceName == NULL이면 source가 없고 위치 값은 모두 0
 * - sourceName != NULL이고 위치 값이 모두 0이면 source만 알고 정확한 위치는 모름
 * - 그 외에는 byteOffset은 0-based, line과 column은 1-based
 */
typedef struct TkDiagnosticLocation
{
    const char *sourceName;
    size_t byteOffset;
    uint32_t line;
    uint32_t column;
} TkDiagnosticLocation;

typedef struct TkDiagnostic
{
    TkDiagnosticSeverity severity;
    const char *id;
    const char *message;
    TkDiagnosticLocation location;
} TkDiagnostic;

typedef void (*TkDiagnosticCallback)(const TkDiagnostic *diagnostic, void *userData);

typedef struct TkDiagnosticCallbackInfo
{
    TkDiagnosticCallback callback;
    void *userData;
} TkDiagnosticCallbackInfo;
