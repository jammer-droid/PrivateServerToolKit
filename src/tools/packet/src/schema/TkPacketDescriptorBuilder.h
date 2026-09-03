#pragma once

#include "TkPacketDescriptor.h"

#include <pstk/TkDiagnostic.h>
#include <pstk/TkResult.h>

namespace pstk::packet
{
class PacketDescriptorBuilder final
{
  public:
    explicit PacketDescriptorBuilder(TkDiagnosticCallbackInfo diagnosticCallback);

    TkResult Build(const PacketSchema &schema, PacketDescriptor *outDescriptor) const;

  private:
    TkDiagnosticCallbackInfo diagnosticCallback_;
};
} // namespace pstk::packet
