using System;
using System.Buffers.Binary;

namespace Pstk.Packet;

internal static class TkPacketCodecSupport
{
    public static void WriteInt8(Span<byte> output, int offset, sbyte value)
    {
        output[offset] = unchecked((byte)value);
    }

    public static sbyte ReadInt8(ReadOnlySpan<byte> input, int offset)
    {
        return unchecked((sbyte)input[offset]);
    }

    public static void WriteUInt8(Span<byte> output, int offset, byte value)
    {
        output[offset] = value;
    }

    public static byte ReadUInt8(ReadOnlySpan<byte> input, int offset)
    {
        return input[offset];
    }

    public static void WriteInt16LittleEndian(Span<byte> output, int offset, short value)
    {
        BinaryPrimitives.WriteInt16LittleEndian(output.Slice(offset, 2), value);
    }

    public static short ReadInt16LittleEndian(ReadOnlySpan<byte> input, int offset)
    {
        return BinaryPrimitives.ReadInt16LittleEndian(input.Slice(offset, 2));
    }

    public static void WriteUInt16LittleEndian(Span<byte> output, int offset, ushort value)
    {
        BinaryPrimitives.WriteUInt16LittleEndian(output.Slice(offset, 2), value);
    }

    public static ushort ReadUInt16LittleEndian(ReadOnlySpan<byte> input, int offset)
    {
        return BinaryPrimitives.ReadUInt16LittleEndian(input.Slice(offset, 2));
    }

    public static void WriteInt32LittleEndian(Span<byte> output, int offset, int value)
    {
        BinaryPrimitives.WriteInt32LittleEndian(output.Slice(offset, 4), value);
    }

    public static int ReadInt32LittleEndian(ReadOnlySpan<byte> input, int offset)
    {
        return BinaryPrimitives.ReadInt32LittleEndian(input.Slice(offset, 4));
    }

    public static void WriteUInt32LittleEndian(Span<byte> output, int offset, uint value)
    {
        BinaryPrimitives.WriteUInt32LittleEndian(output.Slice(offset, 4), value);
    }

    public static uint ReadUInt32LittleEndian(ReadOnlySpan<byte> input, int offset)
    {
        return BinaryPrimitives.ReadUInt32LittleEndian(input.Slice(offset, 4));
    }

    public static void WriteInt64LittleEndian(Span<byte> output, int offset, long value)
    {
        BinaryPrimitives.WriteInt64LittleEndian(output.Slice(offset, 8), value);
    }

    public static long ReadInt64LittleEndian(ReadOnlySpan<byte> input, int offset)
    {
        return BinaryPrimitives.ReadInt64LittleEndian(input.Slice(offset, 8));
    }

    public static void WriteUInt64LittleEndian(Span<byte> output, int offset, ulong value)
    {
        BinaryPrimitives.WriteUInt64LittleEndian(output.Slice(offset, 8), value);
    }

    public static ulong ReadUInt64LittleEndian(ReadOnlySpan<byte> input, int offset)
    {
        return BinaryPrimitives.ReadUInt64LittleEndian(input.Slice(offset, 8));
    }
}
