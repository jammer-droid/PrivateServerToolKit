#include "TkPacketJsonParser.h"

#include "TkPacketDiagnostic.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace pstk::packet
{
namespace
{
using Json = nlohmann::json;

class DuplicateKeyDetector final : public nlohmann::json_sax<Json>
{
    struct SaxContainer
    {
        enum class SaxType
        {
            JsonObject,
            JsonArray,
        };

        SaxType type;
        std::string path;
        std::unordered_set<std::string> objectKeys;
        std::string pendingKey;
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

bool TryGetUnsigned16(const Json &value, const std::uint16_t minimum, std::uint16_t *outValue)
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

    *outValue = static_cast<std::uint16_t>(parsedValue);
    return true;
}

bool TryParsePrimitiveType(const std::string &type, PacketPrimitiveType *outType)
{
    if (type == "int8")
    {
        *outType = PacketPrimitiveType::Int8;
    }
    else if (type == "uint8")
    {
        *outType = PacketPrimitiveType::UInt8;
    }
    else if (type == "int16")
    {
        *outType = PacketPrimitiveType::Int16;
    }
    else if (type == "uint16")
    {
        *outType = PacketPrimitiveType::UInt16;
    }
    else if (type == "int32")
    {
        *outType = PacketPrimitiveType::Int32;
    }
    else if (type == "uint32")
    {
        *outType = PacketPrimitiveType::UInt32;
    }
    else if (type == "int64")
    {
        *outType = PacketPrimitiveType::Int64;
    }
    else if (type == "uint64")
    {
        *outType = PacketPrimitiveType::UInt64;
    }
    else
    {
        return false;
    }

    return true;
}

TkResult ParseField(const Json &field, const std::size_t fieldIndex, const PacketSource &source,
                    const TkDiagnosticCallbackInfo &callbackInfo, PacketFieldSchema *const outField)
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
    PacketPrimitiveType primitiveType = PacketPrimitiveType::Int8;
    if (!TryParsePrimitiveType(type, &primitiveType))
    {
        return ReportFailure(callbackInfo, TK_ERROR_INVALID_DATA, UnknownFieldTypeId, source.sourceName,
                             fieldPath + ".type", "unknown field type '" + type + "'");
    }

    *outField = {
        field["name"].get<std::string>(),
        primitiveType,
    };
    return TK_SUCCESS;
}
} // namespace

PacketJsonParser::PacketJsonParser(const TkDiagnosticCallbackInfo diagnosticCallback)
    : diagnosticCallback_(diagnosticCallback)
{
}

TkResult PacketJsonParser::Parse(const PacketSource &source, PacketSchema *const outSchema) const
{
    if (outSchema == nullptr)
    {
        return TK_ERROR_INVALID_ARGUMENT;
    }

    DuplicateKeyDetector duplicateDetector;
    if (!Json::sax_parse(source.contents, &duplicateDetector))
    {
        if (duplicateDetector.HasDuplicateKey())
        {
            return ReportFailure(diagnosticCallback_, TK_ERROR_INVALID_DATA, DuplicateKeyId, source.sourceName,
                                 duplicateDetector.DuplicatePath(), "duplicate JSON property");
        }

        return ReportFailure(diagnosticCallback_, TK_ERROR_INVALID_DATA, InvalidJsonId, source.sourceName, {},
                             "invalid JSON");
    }

    Json root = Json::parse(source.contents, nullptr, false);
    if (root.is_discarded())
    {
        return ReportFailure(diagnosticCallback_, TK_ERROR_INVALID_DATA, InvalidJsonId, source.sourceName, {},
                             "invalid JSON");
    }

    if (!root.is_object())
    {
        return ReportFailure(diagnosticCallback_, TK_ERROR_INVALID_DATA, InvalidSchemaId, source.sourceName, "$",
                             "schema root must be an object");
    }

    TkResult result = ValidateProperties(root, {"schemaVersion", "packet"}, {"schemaVersion", "packet"}, source, {},
                                         diagnosticCallback_);
    if (result != TK_SUCCESS)
    {
        return result;
    }

    std::uint16_t schemaVersion = 0;
    if (!TryGetUnsigned16(root["schemaVersion"], 1, &schemaVersion))
    {
        return ReportFailure(diagnosticCallback_, TK_ERROR_INVALID_DATA, InvalidSchemaId, source.sourceName,
                             "schemaVersion", "schema version must be an integer from 1 to 65535");
    }
    if (schemaVersion != 1)
    {
        return ReportFailure(diagnosticCallback_, TK_ERROR_INVALID_DATA, UnsupportedSchemaVersionId, source.sourceName,
                             "schemaVersion", "only schema version 1 is supported");
    }

    const Json &packet = root["packet"];
    if (!packet.is_object())
    {
        return ReportFailure(diagnosticCallback_, TK_ERROR_INVALID_DATA, InvalidSchemaId, source.sourceName, "packet",
                             "packet must be an object");
    }

    result = ValidateProperties(packet, {"name", "id", "payloadVersion", "fields"},
                                {"name", "id", "payloadVersion", "fields"}, source, "packet", diagnosticCallback_);
    if (result != TK_SUCCESS)
    {
        return result;
    }

    if (!packet["name"].is_string() || packet["name"].get_ref<const std::string &>().empty())
    {
        return ReportFailure(diagnosticCallback_, TK_ERROR_INVALID_DATA, InvalidSchemaId, source.sourceName,
                             "packet.name", "packet name must be a non-empty string");
    }

    std::uint16_t packetId = 0;
    if (!TryGetUnsigned16(packet["id"], 0, &packetId))
    {
        return ReportFailure(diagnosticCallback_, TK_ERROR_INVALID_DATA, InvalidSchemaId, source.sourceName,
                             "packet.id", "packet ID must be an integer from 0 to 65535");
    }

    std::uint16_t payloadVersion = 0;
    if (!TryGetUnsigned16(packet["payloadVersion"], 1, &payloadVersion))
    {
        return ReportFailure(diagnosticCallback_, TK_ERROR_INVALID_DATA, InvalidSchemaId, source.sourceName,
                             "packet.payloadVersion", "payload version must be an integer from 1 to 65535");
    }

    if (!packet["fields"].is_array())
    {
        return ReportFailure(diagnosticCallback_, TK_ERROR_INVALID_DATA, InvalidSchemaId, source.sourceName,
                             "packet.fields", "fields must be an array");
    }

    PacketSchema parsedSchema = {
        source.sourceName, packet["name"].get<std::string>(), packetId, payloadVersion, {},
    };
    parsedSchema.fields.reserve(packet["fields"].size());

    std::unordered_set<std::string> fieldNames;
    for (std::size_t fieldIndex = 0; fieldIndex < packet["fields"].size(); ++fieldIndex)
    {
        PacketFieldSchema parsedField;
        result = ParseField(packet["fields"][fieldIndex], fieldIndex, source, diagnosticCallback_, &parsedField);
        if (result != TK_SUCCESS)
        {
            return result;
        }

        if (!fieldNames.insert(parsedField.name).second)
        {
            return ReportFailure(diagnosticCallback_, TK_ERROR_INVALID_DATA, DuplicateFieldNameId, source.sourceName,
                                 "packet.fields[" + std::to_string(fieldIndex) + "].name",
                                 "duplicate field name '" + parsedField.name + "'");
        }

        parsedSchema.fields.push_back(std::move(parsedField));
    }

    *outSchema = std::move(parsedSchema);
    return TK_SUCCESS;
}
} // namespace pstk::packet
