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

typedef struct TkDiagnosticCallbacks
{
    TkDiagnosticCallback callback;
    void *userData;
} TkDiagnosticCallbacks;
