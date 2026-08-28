#pragma once

#include "TkPacketSchema.h"

#include <pstk/TkDiagnostic.h>
#include <pstk/TkResult.h>

namespace pstk::packet
{
class PacketJsonParser final
{
  public:
    explicit PacketJsonParser(TkDiagnosticCallbackInfo diagnosticCallback);

    TkResult Parse(const PacketSource &source, PacketSchema *outSchema) const;

  private:
    TkDiagnosticCallbackInfo diagnosticCallback_;
};
} // namespace pstk::packet
