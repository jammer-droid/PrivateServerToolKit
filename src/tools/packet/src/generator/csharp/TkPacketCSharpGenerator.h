#pragma once

#include "generator/TkPacketCodeGenerator.h"
#include "schema/TkPacketDescriptor.h"

#include <pstk/TkResult.h>

#include <string>

namespace pstk::packet
{
TkResult GenerateCSharpSource(const PacketDescriptor &descriptor, const char *namespaceName, std::string *outSource);

class CSharpPacketCodeGenerator final : public PacketCodeGenerator
{
  public:
    explicit CSharpPacketCodeGenerator(const char *namespaceName);

    TkResult Generate(const PacketDescriptor &descriptor, GeneratedFile *outFile) const override;

  private:
    std::string namespaceName_;
};
} // namespace pstk::packet
