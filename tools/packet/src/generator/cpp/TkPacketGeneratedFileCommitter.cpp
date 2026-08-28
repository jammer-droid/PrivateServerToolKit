#include "TkPacketGeneratedFileCommitter.h"

#include "schema/TkPacketDiagnostic.h"

#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace pstk::packet
{
namespace
{
std::filesystem::path GetTemporaryPath(const std::filesystem::path &finalPath)
{
    std::filesystem::path temporaryPath = finalPath;
    temporaryPath += ".tmp";
    return temporaryPath;
}

void RemoveTemporaryFile(const std::filesystem::path &finalPath)
{
    std::error_code error;
    std::filesystem::remove(GetTemporaryPath(finalPath), error);
}

TkResult ReportOutputFailure(const TkDiagnosticCallbackInfo &diagnosticCallback, const std::filesystem::path &finalPath,
                             const std::string &operation)
{
    RemoveTemporaryFile(finalPath);
    return ReportFailure(diagnosticCallback, TK_ERROR_IO, OutputWriteFailedId, finalPath.u8string(), {}, operation);
}

TkResult ReadExistingFile(const std::filesystem::path &finalPath, const TkDiagnosticCallbackInfo &diagnosticCallback,
                          std::string *const outContents)
{
    std::ifstream input(finalPath, std::ios::binary);
    if (!input)
    {
        return ReportOutputFailure(diagnosticCallback, finalPath, "failed to open existing generated file");
    }

    std::string contents{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    if (input.bad())
    {
        return ReportOutputFailure(diagnosticCallback, finalPath, "failed to read existing generated file");
    }

    input.close();
    if (input.fail())
    {
        return ReportOutputFailure(diagnosticCallback, finalPath, "failed to close existing generated file");
    }

    *outContents = std::move(contents);
    return TK_SUCCESS;
}

TkResult WriteTemporaryFile(const std::filesystem::path &temporaryPath, const std::filesystem::path &finalPath,
                            const std::string &contents, const TkDiagnosticCallbackInfo &diagnosticCallback)
{
    std::ofstream output(temporaryPath, std::ios::binary | std::ios::trunc);
    if (!output)
    {
        return ReportOutputFailure(diagnosticCallback, finalPath, "failed to open temporary generated file");
    }

    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    if (!output)
    {
        output.close();
        return ReportOutputFailure(diagnosticCallback, finalPath, "failed to write temporary generated file");
    }

    output.flush();
    if (!output)
    {
        output.close();
        return ReportOutputFailure(diagnosticCallback, finalPath, "failed to flush temporary generated file");
    }

    output.close();
    if (output.fail())
    {
        return ReportOutputFailure(diagnosticCallback, finalPath, "failed to close temporary generated file");
    }

    return TK_SUCCESS;
}

bool ReplaceFile(const std::filesystem::path &temporaryPath, const std::filesystem::path &finalPath)
{
#ifdef _WIN32
    // Windows에서 std::filesystem::rename을 사용하면
    // finalPath 파일이 이미 존재하는 경우에는 오류가 발생할 수 있음
    // MoveFileExW로
    //      - 교체 성공 -> 신규 finalPath 존재
    //      - 교체 실패 -> 기존 finalPath 유지, temp 정리
    return MoveFileExW(temporaryPath.c_str(), finalPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) !=
           FALSE;
#else
    std::error_code error;
    std::filesystem::rename(temporaryPath, finalPath, error);
    return !error;
#endif
}
} // namespace

TkResult CommitGeneratedFile(const std::filesystem::path &finalPath, const std::string &contents,
                             const TkDiagnosticCallbackInfo diagnosticCallback)
{
    if (finalPath.empty())
    {
        return TK_ERROR_INVALID_ARGUMENT;
    }

    std::error_code error;
    const bool finalFileExists = std::filesystem::exists(finalPath, error);
    if (error)
    {
        return ReportOutputFailure(diagnosticCallback, finalPath, "failed to inspect existing generated file");
    }

    if (finalFileExists)
    {
        std::string existingContents;
        const TkResult result = ReadExistingFile(finalPath, diagnosticCallback, &existingContents);
        if (result != TK_SUCCESS)
        {
            return result;
        }

        if (existingContents == contents)
        {
            return TK_SUCCESS;
        }
    }

    const std::filesystem::path temporaryPath = GetTemporaryPath(finalPath);

    const TkResult result = WriteTemporaryFile(temporaryPath, finalPath, contents, diagnosticCallback);
    if (result != TK_SUCCESS)
    {
        return result;
    }

    if (!ReplaceFile(temporaryPath, finalPath))
    {
        return ReportOutputFailure(diagnosticCallback, finalPath, "failed to replace generated file");
    }

    return TK_SUCCESS;
}
} // namespace pstk::packet
