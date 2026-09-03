#pragma once

#include <pstk/TkResult.h>

struct TkPacketCompileInfo;

namespace pstk::packet
{
TkResult CompileCpp(const TkPacketCompileInfo &compileInfo);
} // namespace pstk::packet
