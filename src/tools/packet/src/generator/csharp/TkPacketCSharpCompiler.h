#pragma once

#include <pstk/TkResult.h>

struct TkPacketCompileInfo;

namespace pstk::packet
{
TkResult CompileCSharp(const TkPacketCompileInfo &compileInfo);
} // namespace pstk::packet
