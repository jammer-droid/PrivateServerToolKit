#include <gtest/gtest.h>

#include "AllTypes.generated.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>

namespace
{
using AllTypes = pstk::packet::generated_test::AllTypes;

struct CapturedDiagnostic
{
    std::string id;
    std::string message;
    TkDiagnosticSeverity severity{};
    bool hasSourceName{};
    std::size_t byteOffset{};
    std::uint32_t line{};
    std::uint32_t column{};
};

void CaptureDiagnostic(const TkDiagnostic *const diagnostic, void *const userData)
{
    CapturedDiagnostic &captured = *static_cast<CapturedDiagnostic *>(userData);
    captured.id = diagnostic->id;
    captured.message = diagnostic->message;
    captured.severity = diagnostic->severity;
    captured.hasSourceName = diagnostic->location.sourceName != nullptr;
    captured.byteOffset = diagnostic->location.byteOffset;
    captured.line = diagnostic->location.line;
    captured.column = diagnostic->location.column;
}

bool Matches(const AllTypes &left, const AllTypes &right)
{
    return left.int8Value == right.int8Value && left.uint8Value == right.uint8Value &&
           left.int16Value == right.int16Value && left.uint16Value == right.uint16Value &&
           left.int32Value == right.int32Value && left.uint32Value == right.uint32Value &&
           left.int64Value == right.int64Value && left.uint64Value == right.uint64Value;
}

template <typename SignedValue> AllTypes MakeSignedCase(const SignedValue value)
{
    AllTypes packet{};
    packet.int8Value = static_cast<std::int8_t>(value);
    packet.uint8Value = std::numeric_limits<std::uint8_t>::max();
    packet.int16Value = static_cast<std::int16_t>(value);
    packet.uint16Value = std::numeric_limits<std::uint16_t>::max();
    packet.int32Value = static_cast<std::int32_t>(value);
    packet.uint32Value = std::numeric_limits<std::uint32_t>::max();
    packet.int64Value = static_cast<std::int64_t>(value);
    packet.uint64Value = std::numeric_limits<std::uint64_t>::max();
    return packet;
}

void ExpectRoundTrip(const AllTypes &source)
{
    std::array<std::uint8_t, AllTypes::PayloadBytes> encoded{};
    ASSERT_EQ(source.Encode({encoded.data(), encoded.size()}), TK_SUCCESS);

    AllTypes decoded{};
    ASSERT_EQ(decoded.Decode({encoded.data(), encoded.size()}), TK_SUCCESS);
    EXPECT_TRUE(Matches(decoded, source));
}
} // namespace

TEST(TkPacketGeneratedCodec, RoundTripsSignedBoundaries)
{
    AllTypes minimum{};
    minimum.int8Value = std::numeric_limits<std::int8_t>::min();
    minimum.int16Value = std::numeric_limits<std::int16_t>::min();
    minimum.int32Value = std::numeric_limits<std::int32_t>::min();
    minimum.int64Value = std::numeric_limits<std::int64_t>::min();
    ExpectRoundTrip(minimum);

    ExpectRoundTrip(MakeSignedCase(-1));
    ExpectRoundTrip(MakeSignedCase(0));

    AllTypes maximum{};
    maximum.int8Value = std::numeric_limits<std::int8_t>::max();
    maximum.int16Value = std::numeric_limits<std::int16_t>::max();
    maximum.int32Value = std::numeric_limits<std::int32_t>::max();
    maximum.int64Value = std::numeric_limits<std::int64_t>::max();
    maximum.uint8Value = std::numeric_limits<std::uint8_t>::max();
    maximum.uint16Value = std::numeric_limits<std::uint16_t>::max();
    maximum.uint32Value = std::numeric_limits<std::uint32_t>::max();
    maximum.uint64Value = std::numeric_limits<std::uint64_t>::max();
    ExpectRoundTrip(maximum);
}

TEST(TkPacketGeneratedCodec, MatchesGoldenWireFormat)
{
    AllTypes source{};
    source.int8Value = -1;
    source.uint8Value = 0x12;
    source.int16Value = static_cast<std::int16_t>(-2);
    source.uint16Value = 0x3456;
    source.int32Value = static_cast<std::int32_t>(0x01234567);
    source.uint32Value = UINT32_C(0x89ABCDEF);
    source.int64Value = INT64_C(-2);
    source.uint64Value = UINT64_C(0x0123456789ABCDEF);
    std::array<std::uint8_t, AllTypes::PayloadBytes> encoded{};

    ASSERT_EQ(source.Encode({encoded.data(), encoded.size()}), TK_SUCCESS);

    const std::array<std::uint8_t, AllTypes::PayloadBytes> expected = {
        0x01, 0x00, 0xFF, 0x12, 0xFE, 0xFF, 0x56, 0x34, 0x67, 0x45, 0x23, 0x01, 0xEF, 0xCD, 0xAB, 0x89,
        0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xEF, 0xCD, 0xAB, 0x89, 0x67, 0x45, 0x23, 0x01,
    };
    EXPECT_EQ(encoded, expected);

    AllTypes decoded{};
    ASSERT_EQ(decoded.Decode({expected.data(), expected.size()}), TK_SUCCESS);
    EXPECT_TRUE(Matches(decoded, source));
}

TEST(TkPacketGeneratedCodec, PreservesEncodeOutputForInvalidViews)
{
    const AllTypes source = MakeSignedCase(-1);
    std::array<std::uint8_t, AllTypes::PayloadBytes + 1> output{};
    output.fill(0xA5);
    const std::array<std::uint8_t, AllTypes::PayloadBytes + 1> before = output;

    EXPECT_EQ(source.Encode({nullptr, AllTypes::PayloadBytes}), TK_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(source.Encode({output.data(), AllTypes::PayloadBytes - 1}), TK_ERROR_BUFFER_TOO_SMALL);
    EXPECT_EQ(output, before);
    EXPECT_EQ(source.Encode({output.data(), output.size()}), TK_ERROR_INVALID_ARGUMENT);
    EXPECT_EQ(output, before);
}

TEST(TkPacketGeneratedCodec, PreservesObjectForInvalidViewsAndVersion)
{
    AllTypes target = MakeSignedCase(-1);
    const AllTypes before = target;
    std::array<std::uint8_t, AllTypes::PayloadBytes + 1> input{};

    EXPECT_EQ(target.Decode({nullptr, AllTypes::PayloadBytes}), TK_ERROR_INVALID_ARGUMENT);
    EXPECT_TRUE(Matches(target, before));
    EXPECT_EQ(target.Decode({input.data(), AllTypes::PayloadBytes - 1}), TK_ERROR_INVALID_DATA);
    EXPECT_TRUE(Matches(target, before));
    EXPECT_EQ(target.Decode({input.data(), input.size()}), TK_ERROR_INVALID_DATA);
    EXPECT_TRUE(Matches(target, before));

    input[0] = 2;
    EXPECT_EQ(target.Decode({input.data(), AllTypes::PayloadBytes}), TK_ERROR_INVALID_DATA);
    EXPECT_TRUE(Matches(target, before));
}

TEST(TkPacketGeneratedCodec, ReportsRuntimeDiagnostics)
{
    AllTypes packet{};
    std::array<std::uint8_t, AllTypes::PayloadBytes> bytes{};
    CapturedDiagnostic captured;
    const TkDiagnosticCallbackInfo callbackInfo = {CaptureDiagnostic, &captured};

    EXPECT_EQ(packet.Encode({bytes.data(), bytes.size() - 1}, callbackInfo), TK_ERROR_BUFFER_TOO_SMALL);
    EXPECT_EQ(captured.id, "PSTK-PACKET-INVALID-PAYLOAD-SIZE");
    EXPECT_EQ(captured.severity, TK_DIAGNOSTIC_ERROR);
    EXPECT_FALSE(captured.hasSourceName);
    EXPECT_EQ(captured.byteOffset, 0U);
    EXPECT_EQ(captured.line, 0U);
    EXPECT_EQ(captured.column, 0U);
    EXPECT_NE(captured.message.find("expected 32"), std::string::npos);
    EXPECT_NE(captured.message.find("actual 31"), std::string::npos);

    bytes[0] = 2;
    captured = {};
    EXPECT_EQ(packet.Decode({bytes.data(), bytes.size()}, callbackInfo), TK_ERROR_INVALID_DATA);
    EXPECT_EQ(captured.id, "PSTK-PACKET-UNSUPPORTED-PAYLOAD-VERSION");
    EXPECT_NE(captured.message.find("expected 1"), std::string::npos);
    EXPECT_NE(captured.message.find("actual 2"), std::string::npos);
}
