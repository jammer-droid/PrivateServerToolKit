#include <pstk/packet/TkPacketTool.h>

#include <SimpleIni.h>

#include <algorithm>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <new>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace
{
enum class PacketLanguage
{
    Cpp,
    CSharp,
};

struct PacketConfiguration
{
    PacketLanguage language;
    std::filesystem::path inputPath;
    std::filesystem::path outputDirectory;
    std::string namespaceName;
};

const char *DiagnosticSeverityName(const TkDiagnosticSeverity severity) noexcept
{
    switch (severity)
    {
    case TK_DIAGNOSTIC_INFO:
        return "info";
    case TK_DIAGNOSTIC_WARNING:
        return "warning";
    case TK_DIAGNOSTIC_ERROR:
        return "error";
    default:
        return "unknown";
    }
}

void PrintDiagnostic(const TkDiagnostic *const diagnostic, void *const) noexcept
{
    const char *const sourceName =
        diagnostic->location.sourceName == nullptr ? "<none>" : diagnostic->location.sourceName;

    std::fprintf(stderr, "pstk-packet: severity=%s(%d) id=%s source=%s message=%s\n",
                 DiagnosticSeverityName(diagnostic->severity), static_cast<int>(diagnostic->severity), diagnostic->id,
                 sourceName, diagnostic->message);
}

void PrintUsage(const char *const programName) noexcept
{
    std::fprintf(stderr, "Usage: %s <config.ini>\n", programName == nullptr ? "pstk-packet" : programName);
}

bool IsNonEmpty(const char *const value) noexcept
{
    return value != nullptr && value[0] != '\0';
}

bool HasJsonExtension(const std::filesystem::path &path)
{
    return path.extension().u8string() == ".json";
}

TkResult LoadConfiguration(const std::filesystem::path &configPath, PacketConfiguration *const outConfiguration)
{
    if (configPath.empty())
    {
        std::fprintf(stderr, "pstk-packet: configuration path is empty\n");
        return TK_ERROR_INVALID_ARGUMENT;
    }

    CSimpleIniA ini;
    ini.SetUnicode();
    const SI_Error loadResult = ini.LoadFile(configPath.c_str());
    if (loadResult != SI_OK)
    {
        const std::string configName = configPath.u8string();
        std::fprintf(stderr, "pstk-packet: failed to load configuration '%s' (SimpleIni error %d)\n",
                     configName.c_str(), loadResult);

        if (loadResult == SI_NOMEM)
        {
            return TK_ERROR_OUT_OF_MEMORY;
        }
        if (loadResult == SI_FILE)
        {
            return TK_ERROR_IO;
        }
        return TK_ERROR_INVALID_DATA;
    }

    const char *const languageValue = ini.GetValue("packet", "language", nullptr);
    const char *const inputValue = ini.GetValue("packet", "input", nullptr);
    const char *const outputValue = ini.GetValue("packet", "output", nullptr);
    const char *const namespaceValue = ini.GetValue("packet", "namespace", nullptr);

    if (!IsNonEmpty(languageValue))
    {
        std::fprintf(stderr, "pstk-packet: missing required [packet] language\n");
        return TK_ERROR_INVALID_ARGUMENT;
    }
    if (!IsNonEmpty(inputValue))
    {
        std::fprintf(stderr, "pstk-packet: missing required [packet] input\n");
        return TK_ERROR_INVALID_ARGUMENT;
    }
    if (!IsNonEmpty(outputValue))
    {
        std::fprintf(stderr, "pstk-packet: missing required [packet] output\n");
        return TK_ERROR_INVALID_ARGUMENT;
    }

    PacketLanguage language;
    const std::string languageText = languageValue;
    if (languageText == "cpp")
    {
        language = PacketLanguage::Cpp;
    }
    else if (languageText == "csharp")
    {
        language = PacketLanguage::CSharp;
    }
    else
    {
        std::fprintf(stderr, "pstk-packet: unsupported [packet] language '%s'\n", languageValue);
        return TK_ERROR_INVALID_ARGUMENT;
    }

    const std::filesystem::path absoluteConfigPath = std::filesystem::absolute(configPath);
    const std::filesystem::path configDirectory = absoluteConfigPath.parent_path();
    const std::filesystem::path inputPath = std::filesystem::u8path(inputValue);
    const std::filesystem::path outputPath = std::filesystem::u8path(outputValue);

    PacketConfiguration configuration;
    configuration.language = language;
    configuration.inputPath = inputPath.is_absolute() ? inputPath : configDirectory / inputPath;
    configuration.outputDirectory = outputPath.is_absolute() ? outputPath : configDirectory / outputPath;
    configuration.namespaceName = namespaceValue == nullptr ? "" : namespaceValue;
    *outConfiguration = std::move(configuration);
    return TK_SUCCESS;
}

TkResult CollectInputPaths(const std::filesystem::path &inputPath, std::vector<std::string> *const outInputPaths)
{
    std::error_code error;
    const std::filesystem::file_status inputStatus = std::filesystem::status(inputPath, error);
    if (error)
    {
        const std::string inputName = inputPath.u8string();
        std::fprintf(stderr, "pstk-packet: failed to inspect input '%s': %s\n", inputName.c_str(),
                     error.message().c_str());
        return TK_ERROR_IO;
    }
    if (!std::filesystem::exists(inputStatus))
    {
        const std::string inputName = inputPath.u8string();
        std::fprintf(stderr, "pstk-packet: input does not exist: '%s'\n", inputName.c_str());
        return TK_ERROR_INVALID_ARGUMENT;
    }

    std::vector<std::string> inputPaths;
    if (std::filesystem::is_regular_file(inputStatus))
    {
        if (!HasJsonExtension(inputPath))
        {
            const std::string inputName = inputPath.u8string();
            std::fprintf(stderr, "pstk-packet: input file is not a .json schema: '%s'\n", inputName.c_str());
            return TK_ERROR_INVALID_ARGUMENT;
        }

        inputPaths.push_back(inputPath.u8string());
    }
    else if (std::filesystem::is_directory(inputStatus))
    {
        std::filesystem::recursive_directory_iterator iterator(inputPath, std::filesystem::directory_options::none,
                                                               error);
        if (error)
        {
            const std::string inputName = inputPath.u8string();
            std::fprintf(stderr, "pstk-packet: failed to inspect input directory '%s': %s\n", inputName.c_str(),
                         error.message().c_str());
            return TK_ERROR_IO;
        }

        const std::filesystem::recursive_directory_iterator end;
        while (iterator != end)
        {
            const std::filesystem::directory_entry &entry = *iterator;
            std::error_code entryError;
            const bool isRegularFile = entry.is_regular_file(entryError);
            if (entryError)
            {
                const std::string entryName = entry.path().u8string();
                std::fprintf(stderr, "pstk-packet: failed to inspect input '%s': %s\n", entryName.c_str(),
                             entryError.message().c_str());
                return TK_ERROR_IO;
            }
            if (isRegularFile && HasJsonExtension(entry.path()))
            {
                inputPaths.push_back(entry.path().u8string());
            }

            iterator.increment(error);
            if (error)
            {
                const std::string inputName = inputPath.u8string();
                std::fprintf(stderr, "pstk-packet: failed to traverse input directory '%s': %s\n", inputName.c_str(),
                             error.message().c_str());
                return TK_ERROR_IO;
            }
        }
    }
    else
    {
        const std::string inputName = inputPath.u8string();
        std::fprintf(stderr, "pstk-packet: input is neither a regular file nor a directory: '%s'\n", inputName.c_str());
        return TK_ERROR_INVALID_ARGUMENT;
    }

    if (inputPaths.empty())
    {
        const std::string inputName = inputPath.u8string();
        std::fprintf(stderr, "pstk-packet: no .json schemas found under '%s'\n", inputName.c_str());
        return TK_ERROR_INVALID_DATA;
    }
    std::sort(inputPaths.begin(), inputPaths.end());

    *outInputPaths = std::move(inputPaths);
    return TK_SUCCESS;
}

TkResult EnsureOutputDirectory(const std::filesystem::path &outputPath)
{
    std::error_code error;
    std::filesystem::create_directories(outputPath, error);
    if (error)
    {
        const std::string outputName = outputPath.u8string();
        std::fprintf(stderr, "pstk-packet: failed to create output directory '%s': %s\n", outputName.c_str(),
                     error.message().c_str());
        return TK_ERROR_IO;
    }

    return TK_SUCCESS;
}

TkResult Run(const std::filesystem::path &configPath)
{
    PacketConfiguration configuration;
    TkResult result = LoadConfiguration(configPath, &configuration);
    if (result != TK_SUCCESS)
    {
        return result;
    }

    std::vector<std::string> inputPathStrings;
    result = CollectInputPaths(configuration.inputPath, &inputPathStrings);
    if (result != TK_SUCCESS)
    {
        return result;
    }

    result = EnsureOutputDirectory(configuration.outputDirectory);
    if (result != TK_SUCCESS)
    {
        return result;
    }

    // Keep the owning strings unchanged while the synchronous compiler borrows their c_str() pointers.
    std::vector<const char *> inputPathPointers;
    inputPathPointers.reserve(inputPathStrings.size());
    for (const std::string &inputPath : inputPathStrings)
    {
        inputPathPointers.push_back(inputPath.c_str());
    }

    const std::string outputDirectory = configuration.outputDirectory.u8string();
    const char *const namespaceName =
        configuration.namespaceName.empty() ? nullptr : configuration.namespaceName.c_str();
    const TkPacketCompileInfo compileInfo = {
        inputPathPointers.data(),   inputPathPointers.size(), outputDirectory.c_str(), namespaceName,
        {PrintDiagnostic, nullptr},
    };

    if (configuration.language == PacketLanguage::Cpp)
    {
        result = TkPacketCompileCpp(&compileInfo);
    }
    else
    {
        result = TkPacketCompileCSharp(&compileInfo);
    }

    if (result != TK_SUCCESS)
    {
        std::fprintf(stderr, "pstk-packet: compiler failed with TkResult %d\n", static_cast<int>(result));
    }
    return result;
}
} // namespace

int main(const int argc, char *argv[])
{
    try
    {
        if (argc != 2)
        {
            PrintUsage(argc > 0 && argv != nullptr ? argv[0] : nullptr);
            return 1;
        }

        const std::filesystem::path configPath(argv[1]);
        const TkResult result = Run(configPath);
        return result == TK_SUCCESS ? 0 : 1;
    }
    catch (const std::bad_alloc &)
    {
        std::fprintf(stderr, "pstk-packet: out of memory\n");
        return 1;
    }
    catch (const std::exception &exception)
    {
        std::fprintf(stderr, "pstk-packet: unexpected failure: %s\n", exception.what());
        return 1;
    }
    catch (...)
    {
        std::fprintf(stderr, "pstk-packet: unexpected unknown failure\n");
        return 1;
    }
}
