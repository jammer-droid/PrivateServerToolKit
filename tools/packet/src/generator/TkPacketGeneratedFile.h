#pragma once

#include <filesystem>
#include <string>

namespace pstk::packet
{
struct GeneratedFile
{
    std::filesystem::path fileName;
    std::string contents;
};
} // namespace pstk::packet
