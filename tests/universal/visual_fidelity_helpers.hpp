#pragma once

#include <string>
#include <filesystem>
#include <fstream>
#include "fidelity.hpp"
#include "visual_fidelity.hpp"

namespace systems::leal::campello_widgets::testing {

// Standard resolution for fidelity testing
inline constexpr float kFidelityWidth = 1280.0f;
inline constexpr float kFidelityHeight = 720.0f;

// Helper: Check if Flutter golden exists (for visual fidelity tests)
inline bool flutterGoldenExists(const std::string& name)
{
    std::filesystem::path path = std::filesystem::path(getFlutterGoldensDirectory()) / name;
    return std::filesystem::exists(path);
}

// Helper: Get path to Flutter golden file
inline std::string getFlutterGoldenPath(const std::string& name)
{
    return (std::filesystem::path(getFlutterGoldensDirectory()) / name).string();
}

// Helper: Get path for C++ output file
inline std::string getCppOutputPath(const std::string& name)
{
    return (std::filesystem::path(getCppOutputDirectory()) / name).string();
}

// Helper: Check if golden file exists (for fidelity tests using getGoldensDirectory)
inline bool goldenFileExists(const std::string& name)
{
    std::ifstream file(getGoldensDirectory() + "/" + name);
    return file.good();
}

// Helper: Load golden file content
inline std::string loadGolden(const std::string& name)
{
    return loadGoldenFile(name);
}

} // namespace systems::leal::campello_widgets::testing
