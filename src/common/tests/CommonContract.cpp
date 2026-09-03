#include <gtest/gtest.h>

#include <pstk/TkByteView.h>
#include <pstk/TkDiagnostic.h>
#include <pstk/TkResult.h>

#include <cstdint>
#include <type_traits>

static_assert(std::is_standard_layout<TkByteView>::value, "TkByteView must have standard layout");
static_assert(std::is_trivially_copyable<TkByteView>::value, "TkByteView must be trivially copyable");
static_assert(std::is_copy_assignable<TkByteView>::value, "TkByteView must be copy-assignable");

static_assert(std::is_standard_layout<TkMutableByteView>::value, "TkMutableByteView must have standard layout");
static_assert(std::is_trivially_copyable<TkMutableByteView>::value, "TkMutableByteView must be trivially copyable");
static_assert(std::is_copy_assignable<TkMutableByteView>::value, "TkMutableByteView must be copy-assignable");

static_assert(TK_SUCCESS == 0, "TK_SUCCESS value changed");
static_assert(TK_ERROR_UNKNOWN == -1, "TK_ERROR_UNKNOWN value changed");
static_assert(TK_ERROR_INVALID_ARGUMENT == -2, "TK_ERROR_INVALID_ARGUMENT value changed");
static_assert(TK_ERROR_BUFFER_TOO_SMALL == -3, "TK_ERROR_BUFFER_TOO_SMALL value changed");
static_assert(TK_ERROR_OUT_OF_MEMORY == -4, "TK_ERROR_OUT_OF_MEMORY value changed");
static_assert(TK_ERROR_INVALID_DATA == -5, "TK_ERROR_INVALID_DATA value changed");
static_assert(TK_ERROR_IO == -6, "TK_ERROR_IO value changed");
static_assert(TK_ERROR_INVALID_STATE == -7, "TK_ERROR_INVALID_STATE value changed");
static_assert(TK_ERROR_CAPACITY_EXCEEDED == -8, "TK_ERROR_CAPACITY_EXCEEDED value changed");
static_assert(TK_ERROR_REJECTED == -9, "TK_ERROR_REJECTED value changed");

TEST(TkByteRangeContract, ValidatesNullAndNonNullRanges)
{
    std::uint8_t byte = 0;

    EXPECT_TRUE(TkIsValidByteRange(nullptr, 0));
    EXPECT_TRUE(TkIsValidByteRange(&byte, 0));
    EXPECT_FALSE(TkIsValidByteRange(nullptr, 1));
    EXPECT_TRUE(TkIsValidByteRange(&byte, 1));
}

TEST(TkByteViewContract, SupportsCopyAssignment)
{
    std::uint8_t byte = 0;
    TkByteView source{&byte, 1};
    TkByteView copy{};

    copy = source;

    EXPECT_EQ(copy.data, source.data);
    EXPECT_EQ(copy.size, source.size);
}

TEST(TkMutableByteViewContract, SupportsCopyAssignmentAndWriteAccess)
{
    std::uint8_t byte = 0;
    TkMutableByteView source{&byte, 1};
    TkMutableByteView copy{};

    copy = source;
    copy.data[0] = 42;

    EXPECT_EQ(copy.data, source.data);
    EXPECT_EQ(copy.size, source.size);
    EXPECT_EQ(byte, 42);
}
