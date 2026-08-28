#pragma once

#include "schema/TkPacketDescriptor.h"

#include <pstk/TkResult.h>

#include <string>

namespace pstk::packet
{
TkResult GenerateCppHeader(const PacketDescriptor &descriptor, const char *namespaceName, std::string *outHeader);
} // namespace pstk::packet
