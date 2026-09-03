#pragma once

#include "TkPacketGeneratedFile.h"
#include "schema/TkPacketDescriptor.h"

#include <pstk/TkResult.h>

namespace pstk::packet
{
class PacketCodeGenerator
{
  public:
    virtual ~PacketCodeGenerator() = default;

    virtual TkResult Generate(const PacketDescriptor &descriptor, GeneratedFile *outFile) const = 0;
};
} // namespace pstk::packet
