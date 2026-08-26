#pragma once

typedef enum TkResult
{
    TK_SUCCESS = 0,

    TK_ERROR_UNKNOWN = -1,
    TK_ERROR_INVALID_ARGUMENT = -2,
    TK_ERROR_BUFFER_TOO_SMALL = -3,
    TK_ERROR_OUT_OF_MEMORY = -4,
    TK_ERROR_INVALID_DATA = -5,
    TK_ERROR_IO = -6,

    TK_RESULT_MAX_ENUM = 0x7FFFFFFF

} TkResult;

// enum TkResult result;
// TkResult result;
