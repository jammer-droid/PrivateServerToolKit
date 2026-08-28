#pragma once

#include "TkPacketSchema.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace pstk::packet
{
struct FieldDescriptor
{
    std::string name;
    PacketPrimitiveType type;
    std::size_t offset;
    std::size_t size;
};

/*
 * Payload layout (transport header excluded):
 *
 *   offset=0,size=2                 declared fields (declaration order)                  total
 *   +-------------------------------+--------------------------------------------------+------------+
 *   | payloadVersion                 | field[i]: descriptor-computed offset/size       | payloadBytes|
 *   +-------------------------------+--------------------------------------------------+------------+
 *   NetworkRuntime transport header is outside this layout.
 */
struct PacketDescriptor
{
    std::string sourceName;
    std::string name;
    std::uint16_t id;
    std::uint16_t payloadVersion;
    std::size_t payloadVersionOffset;
    std::size_t payloadVersionSize;
    std::size_t payloadBytes;
    std::vector<FieldDescriptor> fields;
};

struct PacketDescriptorSet
{
    std::vector<PacketDescriptor> packets;
};
} // namespace pstk::packet
