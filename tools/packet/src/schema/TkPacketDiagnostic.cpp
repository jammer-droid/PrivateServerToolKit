#include "TkPacketDiagnostic.h"

namespace pstk::packet
{
void EmitDiagnostic(const TkDiagnosticCallbackInfo &callbackInfo, const char *const id, const std::string &message,
                    const std::string &sourceName)
{
    const TkDiagnostic diagnostic = {
        TK_DIAGNOSTIC_ERROR,
        id,
        message.c_str(),
        {sourceName.c_str(), 0, 0, 0},
    };
    TkEmitDiagnostic(callbackInfo, &diagnostic);
}

TkResult ReportFailure(const TkDiagnosticCallbackInfo &callbackInfo, const TkResult result, const char *const id,
                       const std::string &sourceName, const std::string &logicalPath, const std::string &detail)
{
    std::string message = sourceName;
    if (!logicalPath.empty())
    {
        message += ": ";
        message += logicalPath;
    }
    message += ": ";
    message += detail;

    EmitDiagnostic(callbackInfo, id, message, sourceName);
    return result;
}
} // namespace pstk::packet
