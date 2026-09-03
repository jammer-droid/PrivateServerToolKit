#pragma once

#include <pstk/TkByteView.h>
#include <pstk/TkDiagnostic.h>
#include <pstk/TkResult.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <type_traits>

namespace pstk::packet::detail
{
inline constexpr const char *InvalidPayloadSizeId = "PSTK-PACKET-INVALID-PAYLOAD-SIZE";
inline constexpr const char *UnsupportedPayloadVersionId = "PSTK-PACKET-UNSUPPORTED-PAYLOAD-VERSION";

inline void EmitRuntimeDiagnostic(const TkDiagnosticCallbackInfo diagnosticCallback, const char *const id,
                                  const char *const message)
{
    const TkDiagnostic diagnostic = {
        TK_DIAGNOSTIC_ERROR,
        id,
        message,
        {nullptr, 0, 0, 0},
    };
    TkEmitDiagnostic(diagnosticCallback, &diagnostic);
}

inline void EmitInvalidPayloadSize(const std::size_t expectedSize, const std::size_t actualSize,
                                   const char *const detail, const TkDiagnosticCallbackInfo diagnosticCallback)
{
    char message[192]{};
    std::snprintf(message, sizeof(message), "%s; expected %zu bytes, actual %zu bytes", detail, expectedSize,
                  actualSize);
    EmitRuntimeDiagnostic(diagnosticCallback, InvalidPayloadSizeId, message);
}

inline TkResult ValidateEncodeView(const TkMutableByteView output, const std::size_t expectedSize,
                                   const TkDiagnosticCallbackInfo diagnosticCallback)
{
    if (!TkIsValidByteRange(output.data, output.size))
    {
        EmitInvalidPayloadSize(expectedSize, output.size, "encode output data is null", diagnosticCallback);
        return TK_ERROR_INVALID_ARGUMENT;
    }

    if (output.size < expectedSize)
    {
        EmitInvalidPayloadSize(expectedSize, output.size, "encode output is too small", diagnosticCallback);
        return TK_ERROR_BUFFER_TOO_SMALL;
    }

    if (output.size > expectedSize)
    {
        EmitInvalidPayloadSize(expectedSize, output.size, "encode output size must match exactly", diagnosticCallback);
        return TK_ERROR_INVALID_ARGUMENT;
    }

    return TK_SUCCESS;
}

inline TkResult ValidateDecodeView(const TkByteView input, const std::size_t expectedSize,
                                   const TkDiagnosticCallbackInfo diagnosticCallback)
{
    if (!TkIsValidByteRange(input.data, input.size))
    {
        EmitInvalidPayloadSize(expectedSize, input.size, "decode input data is null", diagnosticCallback);
        return TK_ERROR_INVALID_ARGUMENT;
    }

    if (input.size != expectedSize)
    {
        EmitInvalidPayloadSize(expectedSize, input.size, "decode input size must match exactly", diagnosticCallback);
        return TK_ERROR_INVALID_DATA;
    }

    return TK_SUCCESS;
}

inline TkResult ValidatePayloadVersion(const std::uint16_t actualVersion, const std::uint16_t expectedVersion,
                                       const TkDiagnosticCallbackInfo diagnosticCallback)
{
    if (actualVersion == expectedVersion)
    {
        return TK_SUCCESS;
    }

    char message[160]{};
    std::snprintf(message, sizeof(message), "unsupported payload version; expected %u, actual %u",
                  static_cast<unsigned int>(expectedVersion), static_cast<unsigned int>(actualVersion));
    EmitRuntimeDiagnostic(diagnosticCallback, UnsupportedPayloadVersionId, message);
    return TK_ERROR_INVALID_DATA;
}

template <typename Integer> using UnsignedInteger = std::make_unsigned_t<Integer>;

template <typename Integer> constexpr UnsignedInteger<Integer> ToUnsignedBits(const Integer value)
{
    static_assert(std::is_integral_v<Integer> && !std::is_same_v<Integer, bool>);
    static_assert(std::numeric_limits<Integer>::digits + (std::is_signed_v<Integer> ? 1 : 0) == sizeof(Integer) * 8);

    // C++ defines signed-to-unsigned conversion modulo 2^N.
    // Therefore, every signed value has a corresponding same-width unsigned value
    //  even when the source value is negative.
    return static_cast<UnsignedInteger<Integer>>(value);
}

template <typename Integer> constexpr Integer FromUnsignedBits(const UnsignedInteger<Integer> rawValue)
{
    static_assert(std::is_integral_v<Integer> && !std::is_same_v<Integer, bool>);
    static_assert(std::numeric_limits<Integer>::digits + (std::is_signed_v<Integer> ? 1 : 0) == sizeof(Integer) * 8);

    if constexpr (!std::is_signed_v<Integer>)
    {
        return rawValue;
    }
    else
    {
        using Unsigned = UnsignedInteger<Integer>;
        constexpr Unsigned signBit = static_cast<Unsigned>(Unsigned{1} << std::numeric_limits<Integer>::digits);

        // The unsigned range cannot be represented completely by the corresponding signed type.
        // Therefore, C++17 leaves out-of-range unsigned-to-signed conversion implementation-defined.
        // PrivateServerToolKit supports MSVC, Clang, and GCC targets
        //  whose same-width conversion preserves the two's-complement bit pattern.
        // A new conversion path is required for a toolchain that does not satisfy this contract.
        static_assert(static_cast<Integer>(std::numeric_limits<Unsigned>::max()) == Integer{-1});
        static_assert(static_cast<Integer>(signBit) == std::numeric_limits<Integer>::min());

        return static_cast<Integer>(rawValue);
    }
}

template <typename Integer> inline void WriteLittleEndian(const Integer value, std::uint8_t *const output)
{
    using Unsigned = UnsignedInteger<Integer>;
    Unsigned rawValue = ToUnsignedBits(value);
    for (std::size_t byteIndex = 0; byteIndex < sizeof(Integer); ++byteIndex)
    {
        output[byteIndex] = static_cast<std::uint8_t>(rawValue & Unsigned{0xFF});
        rawValue = static_cast<Unsigned>(rawValue >> 8);
    }
}

template <typename Integer> inline Integer ReadLittleEndian(const std::uint8_t *const input)
{
    using Unsigned = UnsignedInteger<Integer>;
    Unsigned rawValue = 0;
    for (std::size_t byteIndex = 0; byteIndex < sizeof(Integer); ++byteIndex)
    {
        const Unsigned byteValue = static_cast<Unsigned>(input[byteIndex]);
        rawValue = static_cast<Unsigned>(rawValue | static_cast<Unsigned>(byteValue << (byteIndex * 8)));
    }

    return FromUnsignedBits<Integer>(rawValue);
}
} // namespace pstk::packet::detail
