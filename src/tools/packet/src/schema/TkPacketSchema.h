#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace pstk::packet
{
struct PacketSource
{
    std::string sourceName;
    std::string contents;
};

enum class PacketPrimitiveType
{
    Int8,
    UInt8,
    Int16,
    UInt16,
    Int32,
    UInt32,
    Int64,
    UInt64,
};

struct PacketFieldSchema
{
    std::string name;
    PacketPrimitiveType type;
};

struct PacketSchema
{
    std::string sourceName;
    std::string name;
    std::uint16_t id;
    std::uint16_t payloadVersion;
    std::vector<PacketFieldSchema> fields;
};
} // namespace pstk::packet
