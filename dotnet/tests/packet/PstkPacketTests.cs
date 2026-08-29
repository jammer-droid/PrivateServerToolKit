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

    // Test Helper
    private static void AssertRoundTrip(AllTypes source)
    {
        byte[] encoded = new byte[AllTypes.PayloadBytes];

        Assert.True(AllTypes.TryEncode(source, encoded));
        Assert.True(AllTypes.TryDecode(encoded, out AllTypes decoded));
        Assert.Equal(source, decoded);
    }
}
