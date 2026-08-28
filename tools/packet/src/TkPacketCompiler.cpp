#include "TkPacketCompiler.h"

#include <nlohmann/json.hpp>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <string>
#include <unordered_set>
#include <vector>
#include <fstream>

namespace pstk::packet
{
namespace
{
using Json = nlohmann::json;

inline constexpr const char *SourceReadFailedId = "PSTK-PACKET-SOURCE-READ-FAILED";
inline constexpr const char *InvalidJsonId = "PSTK-PACKET-INVALID-JSON";
inline constexpr const char *InvalidSchemaId = "PSTK-PACKET-INVALID-SCHEMA";
inline constexpr const char *UnsupportedSchemaVersionId = "PSTK-PACKET-UNSUPPORTED-SCHEMA-VERSION";
inline constexpr const char *DuplicateKeyId = "PSTK-PACKET-DUPLICATE-KEY";
inline constexpr const char *DuplicatePacketIdId = "PSTK-PACKET-DUPLICATE-PACKET-ID";
inline constexpr const char *DuplicatePacketNameId = "PSTK-PACKET-DUPLICATE-PACKET-NAME";
inline constexpr const char *DuplicateFieldNameId = "PSTK-PACKET-DUPLICATE-FIELD-NAME";
inline constexpr const char *UnknownFieldTypeId = "PSTK-PACKET-UNKNOWN-FIELD-TYPE";

struct PacketSource
{
    std::string sourceName;
    std::string contents;
};

struct ParsedFieldSchema
{
    std::string name;
    std::string type;
};

struct ParsedPacketSource
{
    std::string sourceName;
    std::string name;
    std::uint16_t id;
    std::uint16_t payloadVersion;
    std::vector<ParsedFieldSchema> fields;
};

class DuplicateKeyDetector final : public nlohmann::json_sax<Json>
{
    struct SaxContainer
    {
        enum class SaxType
        {
            JsonObject, // property 접근 가능
            JsonArray   // index 로 접근 가능
        };

        SaxType type;
        std::string path;

        // for SaxType::JsonObject
        std::unordered_set<std::string> objectKeys;
        std::string pendingKey;

        // for SaxType::JsonArray
        std::size_t nextIndex = 0;
    };

  public:
    bool null() override
    {
        ConsumeValue();
        return true;
    }

    bool boolean(bool) override
    {
        ConsumeValue();
        return true;
    }

    bool number_integer(number_integer_t) override
    {
        ConsumeValue();
        return true;
    }

    bool number_unsigned(number_unsigned_t) override
    {
        ConsumeValue();
        return true;
    }

    bool number_float(number_float_t, const string_t &) override
    {
        ConsumeValue();
        return true;
    }

    bool string(string_t &) override
    {
        ConsumeValue();
        return true;
    }

    bool binary(binary_t &) override
    {
        ConsumeValue();
        return true;
    }

    bool start_object(std::size_t) override
    {
        containerStack_.push_back({SaxContainer::SaxType::JsonObject, TakeValuePath()});
        return true;
    }

    bool key(string_t &key) override
    {
        SaxContainer &object = containerStack_.back();
        if (!object.objectKeys.insert(key).second)
        {
            hasDuplicateKey_ = true;
            duplicatePath_ = AppendProperty(object.path, key);
            return false;
        }

        object.pendingKey = key;
        return true;
    }

    bool end_object() override
    {
        containerStack_.pop_back();
        return true;
    }

    bool start_array(std::size_t) override
    {
        containerStack_.push_back({SaxContainer::SaxType::JsonArray, TakeValuePath()});
        return true;
    }

    bool end_array() override
    {
        containerStack_.pop_back();
        return true;
    }

    bool parse_error(std::size_t, const std::string &, const nlohmann::detail::exception &) override
    {
        return false;
    }

    bool HasDuplicateKey() const
    {
        return hasDuplicateKey_;
    }

    const std::string &DuplicatePath() const
    {
        return duplicatePath_;
    }

  private:
    static std::string AppendProperty(const std::string &path, const std::string &property)
    {
        if (path.empty())
        {
            return property;
        }

        return path + "." + property;
    }

    std::string TakeValuePath()
    {
        if (containerStack_.empty())
        {
            return {};
        }

        SaxContainer &parent = containerStack_.back();
        if (parent.type == SaxContainer::SaxType::JsonObject)
        {
            std::string path = AppendProperty(parent.path, parent.pendingKey);
            parent.pendingKey.clear();
            return path;
        }

        return parent.path + "[" + std::to_string(parent.nextIndex++) + "]";
    }

    void ConsumeValue()
    {
        (void)TakeValuePath();
    }

    std::vector<SaxContainer> containerStack_;
    bool hasDuplicateKey_ = false;
    std::string duplicatePath_;
};

void EmitDiagnostic(const TkDiagnosticCallbackInfo &callbackInfo, const char *const id, const std::string &message,
                    const std::string &sourceName)
{
    if (callbackInfo.callback == nullptr)
    {
        return;
    }

    const TkDiagnostic diagnostic = {
        TK_DIAGNOSTIC_ERROR,
        id,
        message.c_str(),
        {sourceName.c_str(), 0, 0, 0},
    };
    callbackInfo.callback(&diagnostic, callbackInfo.userData);
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

bool IsNonEmpty(const char *const value)
{
    return value != nullptr && value[0] != '\0';
}

bool IsValidCompileInfo(const TkPacketCppCompileInfo &compileInfo)
{
    if (compileInfo.inputPaths == nullptr || compileInfo.inputPathCount == 0 ||
        !IsNonEmpty(compileInfo.outputDirectory) || !IsNonEmpty(compileInfo.namespaceName))
    {
        return false;
    }

    for (std::size_t index = 0; index < compileInfo.inputPathCount; ++index)
    {
        if (!IsNonEmpty(compileInfo.inputPaths[index]))
        {
            return false;
        }
    }

    return true;
}

TkResult ReadPacketSource(const char *const inputPath, const TkDiagnosticCallbackInfo &callbackInfo,
                          PacketSource *const outSource)
{
    std::ifstream input(inputPath, std::ios::binary);
    if (!input)
    {
        return ReportFailure(callbackInfo, TK_ERROR_IO, SourceReadFailedId, inputPath, {},
                             "failed to open packet schema");
    }

    std::string contents{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    if (input.bad())
    {
        return ReportFailure(callbackInfo, TK_ERROR_IO, SourceReadFailedId, inputPath, {},
                             "failed to read packet schema");
    }

    *outSource = {inputPath, std::move(contents)};
    return TK_SUCCESS;
}

bool IsAllowedProperty(const std::string &property, const std::initializer_list<const char *> allowed)
{
    return std::find_if(allowed.begin(), allowed.end(),
                        [&property](const char *const candidate) { return property == candidate; }) != allowed.end();
}

TkResult ValidateProperties(const Json &object, const std::initializer_list<const char *> required,
                            const std::initializer_list<const char *> allowed, const PacketSource &source,
                            const std::string &logicalPath, const TkDiagnosticCallbackInfo &callbackInfo)
{
    for (const char *const property : required)
    {
        if (!object.contains(property))
        {
            const std::string propertyPath = logicalPath.empty() ? property : logicalPath + "." + property;
            return ReportFailure(callbackInfo, TK_ERROR_INVALID_DATA, InvalidSchemaId, source.sourceName, propertyPath,
                                 "required property is missing");
        }
    }

    for (auto property = object.cbegin(); property != object.cend(); ++property)
    {
        if (!IsAllowedProperty(property.key(), allowed))
        {
            const std::string propertyPath = logicalPath.empty() ? property.key() : logicalPath + "." + property.key();
            return ReportFailure(callbackInfo, TK_ERROR_INVALID_DATA, InvalidSchemaId, source.sourceName, propertyPath,
                                 "unknown property");
        }
    }

    return TK_SUCCESS;
}

bool TryGetUnsigned16(const Json &value, const std::uint16_t minimum, std::uint16_t &outValue)
{
    std::uint64_t parsedValue = 0;
    if (value.is_number_unsigned())
    {
        parsedValue = value.get<std::uint64_t>();
    }
    else if (value.is_number_integer())
    {
        const std::int64_t signedValue = value.get<std::int64_t>();
        if (signedValue < 0)
        {
            return false;
        }
        parsedValue = static_cast<std::uint64_t>(signedValue);
    }
    else
    {
        return false;
    }

    if (parsedValue < minimum || parsedValue > std::numeric_limits<std::uint16_t>::max())
    {
        return false;
    }

    outValue = static_cast<std::uint16_t>(parsedValue);
    return true;
}

bool IsKnownFieldType(const std::string &type)
{
    static constexpr const char *KnownTypes[] = {
        "int8", "uint8", "int16", "uint16", "int32", "uint32", "int64", "uint64",
    };

    return std::find_if(std::begin(KnownTypes), std::end(KnownTypes),
                        [&type](const char *const knownType) { return type == knownType; }) != std::end(KnownTypes);
}

TkResult ParseField(const Json &field, const std::size_t fieldIndex, const PacketSource &source,
                    const TkDiagnosticCallbackInfo &callbackInfo, ParsedFieldSchema *const outField)
{
    const std::string fieldPath = "packet.fields[" + std::to_string(fieldIndex) + "]";
    if (!field.is_object())
    {
        return ReportFailure(callbackInfo, TK_ERROR_INVALID_DATA, InvalidSchemaId, source.sourceName, fieldPath,
                             "field must be an object");
    }

    const TkResult propertiesResult =
        ValidateProperties(field, {"name", "type"}, {"name", "type"}, source, fieldPath, callbackInfo);
    if (propertiesResult != TK_SUCCESS)
    {
        return propertiesResult;
    }

    if (!field["name"].is_string() || field["name"].get_ref<const std::string &>().empty())
    {
        return ReportFailure(callbackInfo, TK_ERROR_INVALID_DATA, InvalidSchemaId, source.sourceName,
                             fieldPath + ".name", "field name must be a non-empty string");
    }

    if (!field["type"].is_string())
    {
        return ReportFailure(callbackInfo, TK_ERROR_INVALID_DATA, InvalidSchemaId, source.sourceName,
                             fieldPath + ".type", "field type must be a string");
    }

    const std::string &type = field["type"].get_ref<const std::string &>();
    if (!IsKnownFieldType(type))
    {
        return ReportFailure(callbackInfo, TK_ERROR_INVALID_DATA, UnknownFieldTypeId, source.sourceName,
                             fieldPath + ".type", "unknown field type '" + type + "'");
    }

    *outField = {
        field["name"].get<std::string>(),
        type,
    };
    return TK_SUCCESS;
}

TkResult ParsePacketSource(const PacketSource &source, const TkDiagnosticCallbackInfo &callbackInfo,
                           ParsedPacketSource *const outPacket)
{
    DuplicateKeyDetector duplicateDetector;
    if (!Json::sax_parse(source.contents, &duplicateDetector))
    {
        if (duplicateDetector.HasDuplicateKey())
        {
            return ReportFailure(callbackInfo, TK_ERROR_INVALID_DATA, DuplicateKeyId, source.sourceName,
                                 duplicateDetector.DuplicatePath(), "duplicate JSON property");
        }

        return ReportFailure(callbackInfo, TK_ERROR_INVALID_DATA, InvalidJsonId, source.sourceName, {}, "invalid JSON");
    }

    Json root = Json::parse(source.contents, nullptr, false);
    if (root.is_discarded())
    {
        return ReportFailure(callbackInfo, TK_ERROR_INVALID_DATA, InvalidJsonId, source.sourceName, {}, "invalid JSON");
    }

    if (!root.is_object())
    {
        return ReportFailure(callbackInfo, TK_ERROR_INVALID_DATA, InvalidSchemaId, source.sourceName, "$",
                             "schema root must be an object");
    }

    TkResult result =
        ValidateProperties(root, {"schemaVersion", "packet"}, {"schemaVersion", "packet"}, source, {}, callbackInfo);
    if (result != TK_SUCCESS)
    {
        return result;
    }

    std::uint16_t schemaVersion = 0;
    if (!TryGetUnsigned16(root["schemaVersion"], 1, schemaVersion))
    {
        return ReportFailure(callbackInfo, TK_ERROR_INVALID_DATA, InvalidSchemaId, source.sourceName, "schemaVersion",
                             "schema version must be an integer from 1 to 65535");
    }
    if (schemaVersion != 1)
    {
        return ReportFailure(callbackInfo, TK_ERROR_INVALID_DATA, UnsupportedSchemaVersionId, source.sourceName,
                             "schemaVersion", "only schema version 1 is supported");
    }

    const Json &packet = root["packet"];
    if (!packet.is_object())
    {
        return ReportFailure(callbackInfo, TK_ERROR_INVALID_DATA, InvalidSchemaId, source.sourceName, "packet",
                             "packet must be an object");
    }

    result = ValidateProperties(packet, {"name", "id", "payloadVersion", "fields"},
                                {"name", "id", "payloadVersion", "fields"}, source, "packet", callbackInfo);
    if (result != TK_SUCCESS)
    {
        return result;
    }

    if (!packet["name"].is_string() || packet["name"].get_ref<const std::string &>().empty())
    {
        return ReportFailure(callbackInfo, TK_ERROR_INVALID_DATA, InvalidSchemaId, source.sourceName, "packet.name",
                             "packet name must be a non-empty string");
    }

    std::uint16_t packetId = 0;
    if (!TryGetUnsigned16(packet["id"], 0, packetId))
    {
        return ReportFailure(callbackInfo, TK_ERROR_INVALID_DATA, InvalidSchemaId, source.sourceName, "packet.id",
                             "packet ID must be an integer from 0 to 65535");
    }

    std::uint16_t payloadVersion = 0;
    if (!TryGetUnsigned16(packet["payloadVersion"], 1, payloadVersion))
    {
        return ReportFailure(callbackInfo, TK_ERROR_INVALID_DATA, InvalidSchemaId, source.sourceName,
                             "packet.payloadVersion", "payload version must be an integer from 1 to 65535");
    }

    if (!packet["fields"].is_array())
    {
        return ReportFailure(callbackInfo, TK_ERROR_INVALID_DATA, InvalidSchemaId, source.sourceName, "packet.fields",
                             "fields must be an array");
    }

    ParsedPacketSource parsedPacket = {
        source.sourceName, packet["name"].get<std::string>(), packetId, payloadVersion, {},
    };
    parsedPacket.fields.reserve(packet["fields"].size());

    std::unordered_set<std::string> fieldNames;
    for (std::size_t fieldIndex = 0; fieldIndex < packet["fields"].size(); ++fieldIndex)
    {
        ParsedFieldSchema parsedField;
        result = ParseField(packet["fields"][fieldIndex], fieldIndex, source, callbackInfo, &parsedField);
        if (result != TK_SUCCESS)
        {
            return result;
        }

        if (!fieldNames.insert(parsedField.name).second)
        {
            return ReportFailure(callbackInfo, TK_ERROR_INVALID_DATA, DuplicateFieldNameId, source.sourceName,
                                 "packet.fields[" + std::to_string(fieldIndex) + "].name",
                                 "duplicate field name '" + parsedField.name + "'");
        }

        parsedPacket.fields.push_back(std::move(parsedField));
    }

    *outPacket = std::move(parsedPacket);
    return TK_SUCCESS;
}

TkResult ValidateBatchPacket(const ParsedPacketSource &packet, std::unordered_set<std::uint16_t> *const packetIds,
                             std::unordered_set<std::string> *const packetNames,
                             const TkDiagnosticCallbackInfo &callbackInfo)
{
    if (!packetIds->insert(packet.id).second)
    {
        return ReportFailure(callbackInfo, TK_ERROR_INVALID_DATA, DuplicatePacketIdId, packet.sourceName, "packet.id",
                             "duplicate packet ID " + std::to_string(packet.id));
    }

    if (!packetNames->insert(packet.name).second)
    {
        return ReportFailure(callbackInfo, TK_ERROR_INVALID_DATA, DuplicatePacketNameId, packet.sourceName,
                             "packet.name", "duplicate packet name '" + packet.name + "'");
    }

    return TK_SUCCESS;
}
} // namespace

TkResult CompileCpp(const TkPacketCppCompileInfo &compileInfo)
{
    if (!IsValidCompileInfo(compileInfo))
    {
        return TK_ERROR_INVALID_ARGUMENT;
    }

    std::unordered_set<std::uint16_t> packetIds;
    std::unordered_set<std::string> packetNames;

    for (std::size_t inputIndex = 0; inputIndex < compileInfo.inputPathCount; ++inputIndex)
    {
        PacketSource source;
        TkResult result = ReadPacketSource(compileInfo.inputPaths[inputIndex], compileInfo.diagnosticCallback, &source);
        if (result != TK_SUCCESS)
        {
            return result;
        }

        ParsedPacketSource packet;
        result = ParsePacketSource(source, compileInfo.diagnosticCallback, &packet);
        if (result != TK_SUCCESS)
        {
            return result;
        }

        result = ValidateBatchPacket(packet, &packetIds, &packetNames, compileInfo.diagnosticCallback);
        if (result != TK_SUCCESS)
        {
            return result;
        }
    }

    return TK_SUCCESS;
}
} // namespace pstk::packet
