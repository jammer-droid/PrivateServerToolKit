#include <gtest/gtest.h>

#include <pstk/packet/TkPacketCodecSupport.h>

#include <array>
#include <cstdint>
#include <limits>

namespace
{
template <typename Integer> void ExpectRoundTrip(const Integer value)
{
    std::array<std::uint8_t, sizeof(Integer)> bytes{};

    pstk::packet::detail::WriteLittleEndian(value, bytes.data());

    EXPECT_EQ(pstk::packet::detail::ReadLittleEndian<Integer>(bytes.data()), value);
}

template <typename Integer> void ExpectSignedBoundariesRoundTrip()
{
    ExpectRoundTrip(std::numeric_limits<Integer>::min());
    ExpectRoundTrip(static_cast<Integer>(-1));
    ExpectRoundTrip(static_cast<Integer>(0));
    ExpectRoundTrip(std::numeric_limits<Integer>::max());
}
} // namespace

TEST(TkPacketCodecSupport, WritesLittleEndianBytes)
{
    std::array<std::uint8_t, 8> bytes{};

    pstk::packet::detail::WriteLittleEndian(UINT64_C(0x0123456789ABCDEF), bytes.data());

    const std::array<std::uint8_t, 8> expected = {0xEF, 0xCD, 0xAB, 0x89, 0x67, 0x45, 0x23, 0x01};
    EXPECT_EQ(bytes, expected);
}

TEST(TkPacketCodecSupport, RoundTripsSignedBoundaries)
{
    ExpectSignedBoundariesRoundTrip<std::int8_t>();
    ExpectSignedBoundariesRoundTrip<std::int16_t>();
    ExpectSignedBoundariesRoundTrip<std::int32_t>();
    ExpectSignedBoundariesRoundTrip<std::int64_t>();
}

TEST(TkPacketCodecSupport, SupportsUnsignedPrimitiveWidths)
{
    ExpectRoundTrip(std::numeric_limits<std::uint8_t>::max());
    ExpectRoundTrip(std::numeric_limits<std::uint16_t>::max());
    ExpectRoundTrip(std::numeric_limits<std::uint32_t>::max());
    ExpectRoundTrip(std::numeric_limits<std::uint64_t>::max());
}
