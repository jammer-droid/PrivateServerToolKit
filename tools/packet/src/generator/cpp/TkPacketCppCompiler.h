#pragma once

#include <pstk/TkResult.h>

struct TkPacketCppCompileInfo;

namespace pstk::packet
{
TkResult CompileCpp(const TkPacketCppCompileInfo &compileInfo);
} // namespace pstk::packet
