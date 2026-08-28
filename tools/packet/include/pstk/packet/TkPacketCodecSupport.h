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
    if (diagnosticCallback.callback == nullptr)
    {
        return;
    }

    const TkDiagnostic diagnostic = {
        TK_DIAGNOSTIC_ERROR,
        id,
        message,
        {nullptr, 0, 0, 0},
    };
    diagnosticCallback.callback(&diagnostic, diagnosticCallback.userData);
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

    using Unsigned = UnsignedInteger<Integer>;
    if constexpr (!std::is_signed_v<Integer>)
    {
        return value;
    }
    else
    {
        if (value >= 0)
        {
            return static_cast<Unsigned>(value);
        }

        const Unsigned signBit = static_cast<Unsigned>(Unsigned{1} << std::numeric_limits<Integer>::digits);
        if (value == std::numeric_limits<Integer>::min())
        {
            return signBit;
        }

        const Unsigned magnitude = static_cast<Unsigned>(-value);
        return static_cast<Unsigned>(static_cast<Unsigned>(~magnitude) + Unsigned{1});
    }
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
        const Unsigned maximum = static_cast<Unsigned>(std::numeric_limits<Integer>::max());
        if (rawValue <= maximum)
        {
            return static_cast<Integer>(rawValue);
        }

        const Unsigned signBit = static_cast<Unsigned>(maximum + Unsigned{1});
        const Unsigned magnitude = static_cast<Unsigned>(static_cast<Unsigned>(~rawValue) + Unsigned{1});
        if (magnitude == signBit)
        {
            return std::numeric_limits<Integer>::min();
        }

        return static_cast<Integer>(-static_cast<Integer>(magnitude));
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
