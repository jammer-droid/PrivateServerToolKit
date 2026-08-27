#include <gtest/gtest.h>

#include <pstk/packet/TkPacketTool.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
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

TEST(TkPacketCompiler, ParsesValidSchemaWithoutWritingOutput)
{
    const TemporaryDirectory directory;
    const std::filesystem::path schemaPath = directory.Write(
        "MovementInput.packet.json",
        R"({"schemaVersion":1,"packet":{"name":"MovementInput","id":257,"payloadVersion":1,"fields":[{"name":"moveX","type":"int16"}]}})");
    DiagnosticCapture capture;

    EXPECT_EQ(Compile({schemaPath}, directory.OutputDirectory(), capture), TK_SUCCESS);
    EXPECT_TRUE(capture.diagnostics.empty());
    EXPECT_TRUE(std::filesystem::is_empty(directory.OutputDirectory()));
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
