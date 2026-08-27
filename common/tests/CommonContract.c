#include <pstk/TkByteView.h>
#include <pstk/TkDiagnostic.h>
#include <pstk/TkResult.h>

#include <stdio.h>

static int Expect(int condition, const char *message)
{
    if (condition)
    {
        return 1;
    }

    fprintf(stderr, "Contract violation: %s\n", message);
    return 0;
}

int main(void)
{
    int passed = 1;
    uint8_t byte = 0;

    passed &= Expect(TK_SUCCESS == 0, "TK_SUCCESS value changed");
    passed &= Expect(TK_ERROR_UNKNOWN == -1, "TK_ERROR_UNKNOWN value changed");
    passed &= Expect(TK_ERROR_INVALID_ARGUMENT == -2, "TK_ERROR_INVALID_ARGUMENT value changed");
    passed &= Expect(TK_ERROR_BUFFER_TOO_SMALL == -3, "TK_ERROR_BUFFER_TOO_SMALL value changed");
    passed &= Expect(TK_ERROR_OUT_OF_MEMORY == -4, "TK_ERROR_OUT_OF_MEMORY value changed");
    passed &= Expect(TK_ERROR_INVALID_DATA == -5, "TK_ERROR_INVALID_DATA value changed");

    passed &= Expect(TkIsValidByteRange(NULL, 0), "null empty range must be valid");
    passed &= Expect(TkIsValidByteRange(&byte, 0), "non-null empty range must be valid");
    passed &= Expect(!TkIsValidByteRange(NULL, 1), "null non-empty range must be invalid");
    passed &= Expect(TkIsValidByteRange(&byte, 1), "non-null non-empty range must be valid");
    passed &= Expect(TK_ERROR_IO == -6, "TK_ERROR_IO value changed");

    {
        TkByteView source = {&byte, 1};
        TkByteView copy = {NULL, 0};

        copy = source;

        passed &= Expect(copy.data == source.data, "TkByteView data must be copy-assignable");
        passed &= Expect(copy.size == source.size, "TkByteView size must be copy-assignable");
    }

    {
        TkMutableByteView source = {&byte, 1};
        TkMutableByteView copy = {NULL, 0};

        copy = source;
        copy.data[0] = 42;

        passed &= Expect(copy.data == source.data, "TkMutableByteView data must be copy-assignable");
        passed &= Expect(copy.size == source.size, "TkMutableByteView size must be copy-assignable");
        passed &= Expect(byte == 42, "TkMutableByteView must provide writable byte access");
    }

    return passed ? 0 : 1;
}
