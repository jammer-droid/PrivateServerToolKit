#include <gtest/gtest.h>

#include <pstk/packet/TkPacketTool.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace
{
struct CapturedDiagnostic
{
    TkDiagnosticSeverity severity;
    std::string id;
    std::string message;
    std::string sourceName;
    std::size_t byteOffset;
    std::uint32_t line;
    std::uint32_t column;
};

struct DiagnosticCapture
{
    std::vector<CapturedDiagnostic> diagnostics;
};

void CaptureDiagnostic(const TkDiagnostic *const diagnostic, void *const userData)
{
    DiagnosticCapture &capture = *static_cast<DiagnosticCapture *>(userData);
    capture.diagnostics.push_back({
        diagnostic->severity,
        diagnostic->id,
        diagnostic->message,
        diagnostic->location.sourceName == nullptr ? "" : diagnostic->location.sourceName,
        diagnostic->location.byteOffset,
        diagnostic->location.line,
        diagnostic->location.column,
    });
}

class TemporaryDirectory
{
  public:
    TemporaryDirectory()
    {
        const std::string uniqueName =
            "pstk-packet-tests-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        path_ = std::filesystem::temp_directory_path() / uniqueName;
        outputDirectory_ = path_ / "output";
        std::filesystem::create_directories(outputDirectory_);
    }

    ~TemporaryDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    std::filesystem::path Write(const std::string &fileName, const std::string &contents) const
    {
        const std::filesystem::path filePath = path_ / fileName;
        std::ofstream output(filePath, std::ios::binary);
        output << contents;
        output.close();
        return filePath;
    }

    const std::filesystem::path &Path() const
    {
        return path_;
    }

    const std::filesystem::path &OutputDirectory() const
    {
        return outputDirectory_;
    }

  private:
    std::filesystem::path path_;
    std::filesystem::path outputDirectory_;
};

TkResult Compile(const std::vector<std::filesystem::path> &schemaPaths, const std::filesystem::path &outputDirectory,
                 DiagnosticCapture &capture)
{
    std::vector<std::string> pathStrings;
    pathStrings.reserve(schemaPaths.size());
    for (const std::filesystem::path &path : schemaPaths)
    {
        pathStrings.push_back(path.string());
    }

    std::vector<const char *> inputPaths;
    inputPaths.reserve(pathStrings.size());
    for (const std::string &path : pathStrings)
    {
        inputPaths.push_back(path.c_str());
    }

    const std::string outputPath = outputDirectory.string();
    const TkPacketCppCompileInfo compileInfo = {
        inputPaths.data(), inputPaths.size(), outputPath.c_str(), "PrivateServer", {CaptureDiagnostic, &capture},
    };

    return TkPacketCompileCpp(&compileInfo);
}

std::string ReadFile(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void ExpectSingleDiagnostic(const DiagnosticCapture &capture, const std::string &expectedId,
                            const std::filesystem::path &expectedSource, const std::string &expectedLogicalPath)
{
    ASSERT_EQ(capture.diagnostics.size(), 1U);
    const CapturedDiagnostic &diagnostic = capture.diagnostics.front();
    EXPECT_EQ(diagnostic.severity, TK_DIAGNOSTIC_ERROR);
    EXPECT_EQ(diagnostic.id, expectedId);
    EXPECT_EQ(diagnostic.sourceName, expectedSource.string());
    EXPECT_EQ(diagnostic.byteOffset, 0U);
    EXPECT_EQ(diagnostic.line, 0U);
    EXPECT_EQ(diagnostic.column, 0U);
    EXPECT_NE(diagnostic.message.find(expectedSource.string()), std::string::npos);
    if (!expectedLogicalPath.empty())
    {
        EXPECT_NE(diagnostic.message.find(expectedLogicalPath), std::string::npos);
    }
}

TEST(TkPacketCompiler, RejectsInvalidCompileInfoWithoutDiagnostic)
{
    DiagnosticCapture capture;
    const TkPacketCppCompileInfo compileInfo = {
        nullptr, 0, "output", "PrivateServer", {CaptureDiagnostic, &capture},
    };

    EXPECT_EQ(TkPacketCompileCpp(&compileInfo), TK_ERROR_INVALID_ARGUMENT);
    EXPECT_TRUE(capture.diagnostics.empty());
}

TEST(TkPacketCompiler, GeneratesHeaderForValidSchema)
{
    const TemporaryDirectory directory;
    const std::filesystem::path schemaPath = directory.Write(
        "MovementInput.packet.json",
        R"({"schemaVersion":1,"packet":{"name":"MovementInput","id":257,"payloadVersion":1,"fields":[{"name":"moveX","type":"int16"}]}})");
    DiagnosticCapture capture;

    EXPECT_EQ(Compile({schemaPath}, directory.OutputDirectory(), capture), TK_SUCCESS);
    EXPECT_TRUE(capture.diagnostics.empty());
    EXPECT_TRUE(std::filesystem::is_regular_file(directory.OutputDirectory() / "MovementInput.generated.h"));
}

TEST(TkPacketCompiler, DoesNotRewriteIdenticalGeneratedHeader)
{
    const TemporaryDirectory directory;
    const std::filesystem::path schemaPath = directory.Write(
        "MovementInput.packet.json",
        R"({"schemaVersion":1,"packet":{"name":"MovementInput","id":257,"payloadVersion":1,"fields":[{"name":"moveX","type":"int16"}]}})");
    const std::filesystem::path generatedHeader = directory.OutputDirectory() / "MovementInput.generated.h";
    DiagnosticCapture capture;

    ASSERT_EQ(Compile({schemaPath}, directory.OutputDirectory(), capture), TK_SUCCESS);

    // 새 생성 내용이 기존 final과 같으면 기존 파일을 다시 쓰지 않고 보존해야 한다.
    const std::filesystem::file_time_type pastTime =
        std::filesystem::file_time_type::clock::now() - std::chrono::hours(1);

    // generatedHeader 파일의 마지막 수정 시간을 pastTime으로 변경
    std::filesystem::last_write_time(generatedHeader, pastTime);

    // generatedHeader의 수정 시간 불러오기
    const std::filesystem::file_time_type storedTime = std::filesystem::last_write_time(generatedHeader);

    // Compile을 했을 때, Write 까지 진행하지 않으면
    // generatedHeader의 마지막 수정 시간이 storedTime과 같음
    EXPECT_EQ(Compile({schemaPath}, directory.OutputDirectory(), capture), TK_SUCCESS);
    EXPECT_TRUE(capture.diagnostics.empty());
    EXPECT_EQ(std::filesystem::last_write_time(generatedHeader), storedTime);
}

TEST(TkPacketCompiler, PreservesExistingHeaderWhenTemporaryWriteFails)
{
    const TemporaryDirectory directory;
    const std::filesystem::path schemaPath = directory.Write(
        "MovementInput.packet.json",
        R"({"schemaVersion":1,"packet":{"name":"MovementInput","id":257,"payloadVersion":1,"fields":[{"name":"moveX","type":"int16"}]}})");
    const std::filesystem::path generatedHeader = directory.OutputDirectory() / "MovementInput.generated.h";
    DiagnosticCapture capture;

    // 정상 Compile 실행
    ASSERT_EQ(Compile({schemaPath}, directory.OutputDirectory(), capture), TK_SUCCESS);
    const std::string originalContents = ReadFile(generatedHeader);

    // 새 생성 내용이 달라도 임시 쓰기가 실패하면 기존 final을 보존하고 임시 산출물을 남기지 않아야 한다.
    directory.Write(
        "MovementInput.packet.json",
        R"({"schemaVersion":1,"packet":{"name":"MovementInput","id":258,"payloadVersion":1,"fields":[{"name":"moveX","type":"int16"}]}})");

    std::filesystem::path temporaryPath = generatedHeader;
    temporaryPath += ".tmp";
    ASSERT_TRUE(std::filesystem::create_directory(temporaryPath)); // .tmp 경로 생성

    // 다시 Compile 진행하면
    // generatedHeader.h.tmp 가 파일이 아닌 디렉터리로 존재하기 때문에
    // 변경된 내용을 작성하기 위한 .h.tmp 를 만들기 위한 ofstream을 여는 과정에서 실패
    //      - 테스트를 위한 실패 path
    //      - 정상 path에서는 .h.tmp 파일을 만들고, ofstream으로 파일 쓰기 진행
    EXPECT_EQ(Compile({schemaPath}, directory.OutputDirectory(), capture), TK_ERROR_IO);
    ExpectSingleDiagnostic(capture, "PSTK-PACKET-OUTPUT-WRITE-FAILED", generatedHeader, "");

    // 재 Compile이 실패했기 때문에 원본 content의 내용은 변경되지 않아야 함
    EXPECT_EQ(ReadFile(generatedHeader), originalContents);
    EXPECT_FALSE(std::filesystem::exists(temporaryPath));
}

TEST(TkPacketCompiler, ReportsSourceReadFailure)
{
    const TemporaryDirectory directory;
    const std::filesystem::path missingPath = directory.Path() / "missing.packet.json";
    DiagnosticCapture capture;

    EXPECT_EQ(Compile({missingPath}, directory.OutputDirectory(), capture), TK_ERROR_IO);
    ExpectSingleDiagnostic(capture, "PSTK-PACKET-SOURCE-READ-FAILED", missingPath, "");
    EXPECT_TRUE(std::filesystem::is_empty(directory.OutputDirectory()));
}

TEST(TkPacketCompiler, ReportsInvalidJson)
{
    const TemporaryDirectory directory;
    const std::filesystem::path schemaPath = directory.Write("invalid.packet.json", R"({"schemaVersion":)");
    DiagnosticCapture capture;

    EXPECT_EQ(Compile({schemaPath}, directory.OutputDirectory(), capture), TK_ERROR_INVALID_DATA);
    ExpectSingleDiagnostic(capture, "PSTK-PACKET-INVALID-JSON", schemaPath, "");
    EXPECT_TRUE(std::filesystem::is_empty(directory.OutputDirectory()));
}

TEST(TkPacketCompiler, ReportsDuplicateJsonProperty)
{
    const TemporaryDirectory directory;
    const std::filesystem::path schemaPath =
        directory.Write("duplicate-key.packet.json",
                        R"({"schemaVersion":1,"packet":{"name":"One","id":1,"id":2,"payloadVersion":1,"fields":[]}})");
    DiagnosticCapture capture;

    EXPECT_EQ(Compile({schemaPath}, directory.OutputDirectory(), capture), TK_ERROR_INVALID_DATA);
    ExpectSingleDiagnostic(capture, "PSTK-PACKET-DUPLICATE-KEY", schemaPath, "packet.id");
    EXPECT_TRUE(std::filesystem::is_empty(directory.OutputDirectory()));
}

TEST(TkPacketCompiler, ReportsUnsupportedSchemaVersionBeforePacketErrors)
{
    const TemporaryDirectory directory;
    const std::filesystem::path schemaPath =
        directory.Write("version.packet.json", R"({"schemaVersion":2,"packet":{}})");
    DiagnosticCapture capture;

    EXPECT_EQ(Compile({schemaPath}, directory.OutputDirectory(), capture), TK_ERROR_INVALID_DATA);
    ExpectSingleDiagnostic(capture, "PSTK-PACKET-UNSUPPORTED-SCHEMA-VERSION", schemaPath, "schemaVersion");
    EXPECT_TRUE(std::filesystem::is_empty(directory.OutputDirectory()));
}

TEST(TkPacketCompiler, ReportsUnknownSchemaProperty)
{
    const TemporaryDirectory directory;
    const std::filesystem::path schemaPath = directory.Write(
        "unknown-property.packet.json",
        R"({"schemaVersion":1,"packet":{"name":"One","id":1,"payloadVersion":1,"fields":[],"direction":"clientToServer"}})");
    DiagnosticCapture capture;

    EXPECT_EQ(Compile({schemaPath}, directory.OutputDirectory(), capture), TK_ERROR_INVALID_DATA);
    ExpectSingleDiagnostic(capture, "PSTK-PACKET-INVALID-SCHEMA", schemaPath, "packet.direction");
    EXPECT_TRUE(std::filesystem::is_empty(directory.OutputDirectory()));
}

TEST(TkPacketCompiler, ReportsUnknownFieldType)
{
    const TemporaryDirectory directory;
    const std::filesystem::path schemaPath = directory.Write(
        "unknown-type.packet.json",
        R"({"schemaVersion":1,"packet":{"name":"One","id":1,"payloadVersion":1,"fields":[{"name":"value","type":"float32"}]}})");
    DiagnosticCapture capture;

    EXPECT_EQ(Compile({schemaPath}, directory.OutputDirectory(), capture), TK_ERROR_INVALID_DATA);
    ExpectSingleDiagnostic(capture, "PSTK-PACKET-UNKNOWN-FIELD-TYPE", schemaPath, "packet.fields[0].type");
    EXPECT_TRUE(std::filesystem::is_empty(directory.OutputDirectory()));
}

TEST(TkPacketCompiler, ReportsDuplicateFieldName)
{
    const TemporaryDirectory directory;
    const std::filesystem::path schemaPath = directory.Write(
        "duplicate-field.packet.json",
        R"({"schemaVersion":1,"packet":{"name":"One","id":1,"payloadVersion":1,"fields":[{"name":"value","type":"int8"},{"name":"value","type":"uint8"}]}})");
    DiagnosticCapture capture;

    EXPECT_EQ(Compile({schemaPath}, directory.OutputDirectory(), capture), TK_ERROR_INVALID_DATA);
    ExpectSingleDiagnostic(capture, "PSTK-PACKET-DUPLICATE-FIELD-NAME", schemaPath, "packet.fields[1].name");
    EXPECT_TRUE(std::filesystem::is_empty(directory.OutputDirectory()));
}

TEST(TkPacketCompiler, ReportsDuplicatePacketIdInInputOrder)
{
    const TemporaryDirectory directory;
    const std::filesystem::path firstPath = directory.Write(
        "first.packet.json", R"({"schemaVersion":1,"packet":{"name":"One","id":1,"payloadVersion":1,"fields":[]}})");
    const std::filesystem::path secondPath = directory.Write(
        "second.packet.json", R"({"schemaVersion":1,"packet":{"name":"Two","id":1,"payloadVersion":1,"fields":[]}})");
    DiagnosticCapture capture;

    EXPECT_EQ(Compile({firstPath, secondPath}, directory.OutputDirectory(), capture), TK_ERROR_INVALID_DATA);
    ExpectSingleDiagnostic(capture, "PSTK-PACKET-DUPLICATE-PACKET-ID", secondPath, "packet.id");
    EXPECT_TRUE(std::filesystem::is_empty(directory.OutputDirectory()));
}

TEST(TkPacketCompiler, ReportsDuplicatePacketNameInInputOrder)
{
    const TemporaryDirectory directory;
    const std::filesystem::path firstPath = directory.Write(
        "first.packet.json", R"({"schemaVersion":1,"packet":{"name":"One","id":1,"payloadVersion":1,"fields":[]}})");
    const std::filesystem::path secondPath = directory.Write(
        "second.packet.json", R"({"schemaVersion":1,"packet":{"name":"One","id":2,"payloadVersion":1,"fields":[]}})");
    DiagnosticCapture capture;

    EXPECT_EQ(Compile({firstPath, secondPath}, directory.OutputDirectory(), capture), TK_ERROR_INVALID_DATA);
    ExpectSingleDiagnostic(capture, "PSTK-PACKET-DUPLICATE-PACKET-NAME", secondPath, "packet.name");
    EXPECT_TRUE(std::filesystem::is_empty(directory.OutputDirectory()));
}
} // namespace
