#include "TkPacketDescriptorBuilder.h"

#include "TkPacketDiagnostic.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>

namespace pstk::packet
{
namespace
{
constexpr std::size_t PayloadVersionOffset = 0;
constexpr std::size_t PayloadVersionSize = 2;

bool TryGetPrimitiveTypeSize(const PacketPrimitiveType type, std::size_t *outSize)
{
    switch (type)
    {
    case PacketPrimitiveType::Int8:
    case PacketPrimitiveType::UInt8:
        *outSize = 1;
        return true;
    case PacketPrimitiveType::Int16:
    case PacketPrimitiveType::UInt16:
        *outSize = 2;
        return true;
    case PacketPrimitiveType::Int32:
    case PacketPrimitiveType::UInt32:
        *outSize = 4;
        return true;
    case PacketPrimitiveType::Int64:
    case PacketPrimitiveType::UInt64:
        *outSize = 8;
        return true;
    }

    return false;
}
} // namespace

PacketDescriptorBuilder::PacketDescriptorBuilder(const TkDiagnosticCallbackInfo diagnosticCallback)
    : diagnosticCallback_(diagnosticCallback)
{
}

TkResult PacketDescriptorBuilder::Build(const PacketSchema &schema, PacketDescriptor *const outDescriptor) const
{
    if (outDescriptor == nullptr)
    {
        return TK_ERROR_INVALID_ARGUMENT;
    }

    PacketDescriptor descriptor = {
        schema.sourceName,    schema.name,        schema.id,          schema.payloadVersion,
        PayloadVersionOffset, PayloadVersionSize, PayloadVersionSize, {},
    };
    descriptor.fields.reserve(schema.fields.size());

    for (std::size_t fieldIndex = 0; fieldIndex < schema.fields.size(); ++fieldIndex)
    {
        const PacketFieldSchema &field = schema.fields[fieldIndex];
        std::size_t fieldSize = 0;
        if (!TryGetPrimitiveTypeSize(field.type, &fieldSize))
        {
            return ReportFailure(diagnosticCallback_, TK_ERROR_INVALID_DATA, InvalidSchemaId, schema.sourceName,
                                 "packet.fields[" + std::to_string(fieldIndex) + "].type",
                                 "field type is not supported");
        }

        if (descriptor.payloadBytes > std::numeric_limits<std::size_t>::max() - fieldSize)
        {
            return ReportFailure(diagnosticCallback_, TK_ERROR_INVALID_DATA, LayoutOverflowId, schema.sourceName,
                                 "packet.fields[" + std::to_string(fieldIndex) + "]", "payload layout size overflow");
        }

        descriptor.fields.push_back({field.name, field.type, descriptor.payloadBytes, fieldSize});
        descriptor.payloadBytes += fieldSize;
    }

    *outDescriptor = std::move(descriptor);
    return TK_SUCCESS;
}
} // namespace pstk::packet
