#include <gtest/gtest.h>

#include "schema/TkPacketDescriptor.h"
#include "schema/TkPacketSchemaCompiler.h"

#include <pstk/TkResult.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace
{
class TemporaryDirectory
{
  public:
    TemporaryDirectory()
    {
        const std::string uniqueName = "pstk-packet-schema-compiler-tests-" +
                                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        path_ = std::filesystem::temp_directory_path() / uniqueName;
        std::filesystem::create_directories(path_);
    }

    ~TemporaryDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    std::filesystem::path Write(const std::string &fileName, const std::string &contents) const
    {
        const std::filesystem::path filePath = path_ / fileName;
        std::ofstream output(filePath, std::ios::binary);
        output << contents;
        output.close();
        return filePath;
    }

  private:
    std::filesystem::path path_;
};

TkResult Compile(const std::filesystem::path &schemaPath, pstk::packet::PacketDescriptorSet *const outDescriptorSet)
{
    const std::string pathString = schemaPath.string();
    const char *const inputPaths[] = {pathString.c_str()};
    const TkDiagnosticCallbackInfo diagnosticCallback = {nullptr, nullptr};
    const pstk::packet::PacketSchemaCompiler compiler(diagnosticCallback);
    return compiler.Compile(inputPaths, 1, outDescriptorSet);
}
} // namespace

TEST(PacketSchemaCompiler, BuildsEmptyPacketDescriptor)
{
    const TemporaryDirectory directory;
    const std::filesystem::path schemaPath = directory.Write(
        "empty.packet.json", R"({"schemaVersion":1,"packet":{"name":"Empty","id":1,"payloadVersion":1,"fields":[]}})");
    pstk::packet::PacketDescriptorSet descriptors;

    ASSERT_EQ(Compile(schemaPath, &descriptors), TK_SUCCESS);
    ASSERT_EQ(descriptors.packets.size(), 1U);

    const pstk::packet::PacketDescriptor &descriptor = descriptors.packets.front();
    EXPECT_EQ(descriptor.name, "Empty");
    EXPECT_EQ(descriptor.payloadVersionOffset, 0U);
    EXPECT_EQ(descriptor.payloadVersionSize, 2U);
    EXPECT_EQ(descriptor.payloadBytes, 2U);
    EXPECT_TRUE(descriptor.fields.empty());
}

TEST(PacketSchemaCompiler, BuildsAllPrimitiveTypeLayout)
{
    const TemporaryDirectory directory;
    const std::filesystem::path schemaPath =
        directory.Write("all-types.packet.json",
                        R"({"schemaVersion":1,"packet":{"name":"AllTypes","id":2,"payloadVersion":1,"fields":[)"
                        R"({"name":"int8Value","type":"int8"},)"
                        R"({"name":"uint8Value","type":"uint8"},)"
                        R"({"name":"int16Value","type":"int16"},)"
                        R"({"name":"uint16Value","type":"uint16"},)"
                        R"({"name":"int32Value","type":"int32"},)"
                        R"({"name":"uint32Value","type":"uint32"},)"
                        R"({"name":"int64Value","type":"int64"},)"
                        R"({"name":"uint64Value","type":"uint64"}]}})");
    pstk::packet::PacketDescriptorSet descriptors;

    ASSERT_EQ(Compile(schemaPath, &descriptors), TK_SUCCESS);
    ASSERT_EQ(descriptors.packets.size(), 1U);

    const pstk::packet::PacketDescriptor &descriptor = descriptors.packets.front();
    const std::array<pstk::packet::PacketPrimitiveType, 8> expectedTypes = {
        pstk::packet::PacketPrimitiveType::Int8,  pstk::packet::PacketPrimitiveType::UInt8,
        pstk::packet::PacketPrimitiveType::Int16, pstk::packet::PacketPrimitiveType::UInt16,
        pstk::packet::PacketPrimitiveType::Int32, pstk::packet::PacketPrimitiveType::UInt32,
        pstk::packet::PacketPrimitiveType::Int64, pstk::packet::PacketPrimitiveType::UInt64,
    };
    const std::array<std::size_t, 8> expectedOffsets = {2, 3, 4, 6, 8, 12, 16, 24};
    const std::array<std::size_t, 8> expectedSizes = {1, 1, 2, 2, 4, 4, 8, 8};

    ASSERT_EQ(descriptor.fields.size(), expectedTypes.size());
    for (std::size_t fieldIndex = 0; fieldIndex < expectedTypes.size(); ++fieldIndex)
    {
        EXPECT_EQ(descriptor.fields[fieldIndex].type, expectedTypes[fieldIndex]);
        EXPECT_EQ(descriptor.fields[fieldIndex].offset, expectedOffsets[fieldIndex]);
        EXPECT_EQ(descriptor.fields[fieldIndex].size, expectedSizes[fieldIndex]);
    }
    EXPECT_EQ(descriptor.payloadBytes, 32U);
}

TEST(PacketSchemaCompiler, BuildsMovementInputLayout)
{
    const TemporaryDirectory directory;
    const std::filesystem::path schemaPath =
        directory.Write("movement-input.packet.json",
                        R"({"schemaVersion":1,"packet":{"name":"MovementInput","id":257,"payloadVersion":1,"fields":[)"
                        R"({"name":"controlledEntityGeneration","type":"uint32"},)"
                        R"({"name":"targetServerTick","type":"uint32"},)"
                        R"({"name":"moveX","type":"int16"},)"
                        R"({"name":"moveY","type":"int16"}]}})");
    pstk::packet::PacketDescriptorSet descriptors;

    ASSERT_EQ(Compile(schemaPath, &descriptors), TK_SUCCESS);
    ASSERT_EQ(descriptors.packets.size(), 1U);

    const pstk::packet::PacketDescriptor &descriptor = descriptors.packets.front();
    const std::array<std::size_t, 4> expectedOffsets = {2, 6, 10, 12};
    const std::array<std::size_t, 4> expectedSizes = {4, 4, 2, 2};

    ASSERT_EQ(descriptor.fields.size(), expectedOffsets.size());
    for (std::size_t fieldIndex = 0; fieldIndex < expectedOffsets.size(); ++fieldIndex)
    {
        EXPECT_EQ(descriptor.fields[fieldIndex].offset, expectedOffsets[fieldIndex]);
        EXPECT_EQ(descriptor.fields[fieldIndex].size, expectedSizes[fieldIndex]);
    }
    EXPECT_EQ(descriptor.payloadBytes, 14U);
}
