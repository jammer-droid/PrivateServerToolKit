#pragma once

#include <pstk/TkDiagnostic.h>
#include <pstk/TkResult.h>

#include <filesystem>
#include <string>

namespace pstk::packet
{
TkResult CommitGeneratedFile(const std::filesystem::path &finalPath, const std::string &contents,
                             TkDiagnosticCallbackInfo diagnosticCallback);
} // namespace pstk::packet
