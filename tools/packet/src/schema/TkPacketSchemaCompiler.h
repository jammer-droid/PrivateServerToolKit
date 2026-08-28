#pragma once

#include "TkPacketDescriptor.h"

#include <pstk/TkDiagnostic.h>
#include <pstk/TkResult.h>

#include <cstddef>

namespace pstk::packet
{
class PacketSchemaCompiler final
{
  public:
    explicit PacketSchemaCompiler(TkDiagnosticCallbackInfo diagnosticCallback);

    TkResult Compile(const char *const *inputPaths, std::size_t inputPathCount,
                     PacketDescriptorSet *outDescriptorSet) const;

  private:
    TkDiagnosticCallbackInfo diagnosticCallback_;
};
} // namespace pstk::packet
