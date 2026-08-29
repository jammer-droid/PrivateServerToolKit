#pragma once

#include "../TkPacketCodeGenerator.h"
#include "schema/TkPacketDescriptor.h"

#include <pstk/TkResult.h>

#include <string>

namespace pstk::packet
{
TkResult GenerateCppHeader(const PacketDescriptor &descriptor, const char *namespaceName, std::string *outHeader);

class CppPacketCodeGenerator final : public PacketCodeGenerator
{
  public:
    explicit CppPacketCodeGenerator(const char *namespaceName);

    TkResult Generate(const PacketDescriptor &descriptor, GeneratedFile *outFile) const override;

  private:
    std::string namespaceName_;
};
} // namespace pstk::packet
