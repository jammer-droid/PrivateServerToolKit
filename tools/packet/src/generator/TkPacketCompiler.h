#pragma once

#include "TkPacketCodeGenerator.h"

#include <pstk/TkDiagnostic.h>
#include <pstk/TkResult.h>

#include <cstddef>

namespace pstk::packet
{
class PacketCompiler final
{
  public:
    explicit PacketCompiler(TkDiagnosticCallbackInfo diagnosticCallback);

    TkResult Compile(const char *const *inputPaths, std::size_t inputPathCount, const char *outputDirectory,
                     const PacketCodeGenerator &codeGenerator) const;

  private:
    TkDiagnosticCallbackInfo diagnosticCallback_;
};
} // namespace pstk::packet
