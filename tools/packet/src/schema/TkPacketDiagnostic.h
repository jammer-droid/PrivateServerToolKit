#pragma once

#include <pstk/TkDiagnostic.h>
#include <pstk/TkResult.h>

#include <string>

namespace pstk::packet
{
inline constexpr const char *SourceReadFailedId = "PSTK-PACKET-SOURCE-READ-FAILED";
inline constexpr const char *InvalidJsonId = "PSTK-PACKET-INVALID-JSON";
inline constexpr const char *InvalidSchemaId = "PSTK-PACKET-INVALID-SCHEMA";
inline constexpr const char *UnsupportedSchemaVersionId = "PSTK-PACKET-UNSUPPORTED-SCHEMA-VERSION";
inline constexpr const char *DuplicateKeyId = "PSTK-PACKET-DUPLICATE-KEY";
inline constexpr const char *DuplicatePacketIdId = "PSTK-PACKET-DUPLICATE-PACKET-ID";
inline constexpr const char *DuplicatePacketNameId = "PSTK-PACKET-DUPLICATE-PACKET-NAME";
inline constexpr const char *DuplicateFieldNameId = "PSTK-PACKET-DUPLICATE-FIELD-NAME";
inline constexpr const char *UnknownFieldTypeId = "PSTK-PACKET-UNKNOWN-FIELD-TYPE";
inline constexpr const char *LayoutOverflowId = "PSTK-PACKET-LAYOUT-OVERFLOW";
inline constexpr const char *OutputWriteFailedId = "PSTK-PACKET-OUTPUT-WRITE-FAILED";

void EmitDiagnostic(const TkDiagnosticCallbackInfo &callbackInfo, const char *id, const std::string &message,
                    const std::string &sourceName);

TkResult ReportFailure(const TkDiagnosticCallbackInfo &callbackInfo, TkResult result, const char *id,
                       const std::string &sourceName, const std::string &logicalPath, const std::string &detail);
} // namespace pstk::packet
