#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct TkByteView
{
    const uint8_t *data;
    size_t size;
} TkByteView;

typedef struct TkMutableByteView
{
    uint8_t *data;
    size_t size;
} TkMutableByteView;

static inline bool TkIsValidByteRange(const void *data, size_t size)
{
    return (size == 0) || (data != NULL);
}
