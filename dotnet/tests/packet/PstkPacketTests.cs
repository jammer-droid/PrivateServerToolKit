using Pstk.Generated;

namespace PstkPacketTests;

public class PacketCodecTests // xUnit용 테스트 클래스
{
    // [Fact] 를 이용하여 메서드를 xUnit 테스트로 등록
    // - 입력 매개변수가 없는 단일 테스트
    // - 입력이 필요하면 [Theory]
    [Fact]
    public void RoundTripsSignedBoundaries()
    {
        AssertRoundTrip(
            new AllTypes(
                sbyte.MinValue,
                0,
                short.MinValue,
                0,
                int.MinValue,
                0,
                long.MinValue,
                0));

        AssertRoundTrip(
            new AllTypes(
                -1,
                byte.MaxValue,
                -1,
                ushort.MaxValue,
                -1,
                uint.MaxValue,
                -1,
                ulong.MaxValue));

        AssertRoundTrip(
            new AllTypes(
                0,
                byte.MaxValue,
                0,
                ushort.MaxValue,
                0,
                uint.MaxValue,
                0,
                ulong.MaxValue));

        AssertRoundTrip(
            new AllTypes(
                sbyte.MaxValue,
                byte.MaxValue,
                short.MaxValue,
                ushort.MaxValue,
                int.MaxValue,
                uint.MaxValue,
                long.MaxValue,
                ulong.MaxValue));
    }

    [Fact]
    public void MatchesGoldenWireFormat()
    {
        AllTypes source = new AllTypes(
            -1,
            0x12,
            -2,
            0x3456,
            0x01234567,
            0x89ABCDEFU,
            -2,
            0x0123456789ABCDEFUL);
        byte[] expected =
        {
            0x01, 0x00, 0xFF, 0x12, 0xFE, 0xFF, 0x56, 0x34,
            0x67, 0x45, 0x23, 0x01, 0xEF, 0xCD, 0xAB, 0x89,
            0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
            0xEF, 0xCD, 0xAB, 0x89, 0x67, 0x45, 0x23, 0x01,
        };
        byte[] encoded = new byte[AllTypes.PayloadBytes];

        Assert.True(AllTypes.TryEncode(source, encoded));
        Assert.Equal(expected, encoded);
        Assert.True(AllTypes.TryDecode(expected, out AllTypes decoded));
        Assert.Equal(source, decoded);
    }

    [Fact]
    public void RejectsInvalidOutputSizesWithoutMutation()
    {
        AllTypes source = default;
        AssertEncodeRejectedWithoutMutation(source, AllTypes.PayloadBytes - 1);
        AssertEncodeRejectedWithoutMutation(source, AllTypes.PayloadBytes + 1);
    }

    [Fact]
    public void RejectsInvalidInputSizeAndVersion()
    {
        byte[] shortInput = new byte[AllTypes.PayloadBytes - 1];
        Assert.False(AllTypes.TryDecode(shortInput, out AllTypes shortDecoded));
        Assert.Equal(default(AllTypes), shortDecoded);

        byte[] longInput = new byte[AllTypes.PayloadBytes + 1];
        Assert.False(AllTypes.TryDecode(longInput, out AllTypes longDecoded));
        Assert.Equal(default(AllTypes), longDecoded);

        byte[] unsupportedVersion = new byte[AllTypes.PayloadBytes];
        unsupportedVersion[0] = 2;
        Assert.False(AllTypes.TryDecode(unsupportedVersion, out AllTypes versionDecoded));
        Assert.Equal(default(AllTypes), versionDecoded);
    }

    // Test Helper
    private static void AssertRoundTrip(AllTypes source)
    {
        byte[] encoded = new byte[AllTypes.PayloadBytes];

        Assert.True(AllTypes.TryEncode(source, encoded));
        Assert.True(AllTypes.TryDecode(encoded, out AllTypes decoded));
        Assert.Equal(source, decoded);
    }

    private static void AssertEncodeRejectedWithoutMutation(AllTypes source, int outputSize)
    {
        byte[] output = new byte[outputSize];
        Array.Fill(output, (byte)0xA5);
        byte[] before = (byte[])output.Clone();

        Assert.False(AllTypes.TryEncode(source, output));
        Assert.Equal(before, output);
    }
}
