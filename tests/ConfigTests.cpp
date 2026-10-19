#include "app/Config.h"
#include "app/Errors.h"
#include "input/HoverTracker.h"
#include "render/TextStyles.h"
#include <roapi.h>
#include <fstream>
#include <iterator>
#include <cstdio>

namespace {
using namespace caecilia;
using namespace NotchIsland;
void Require(bool condition, const char* message) {
    if (!condition) { throw std::runtime_error(message); }
}
void Defaults(const Config& config) {
    Require(config.user.version == kDefaultConfig.version && config.user.titleFont == kDefaultConfig.titleFont &&
        config.user.hoverDelayMs == kDefaultConfig.hoverDelayMs && config.user.monitorIndex == kDefaultConfig.monitorIndex &&
        std::abs(config.user.accentColor.r - kDefaultConfig.accentColor.r) < 0.002f && !config.showDebugFps,
        "Expected default config");
}
std::string Read(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    Require(input.good(), "Read test file");
    return {std::istreambuf_iterator<char>(input), {}};
}
void Write(const std::filesystem::path& path, const std::string& bytes) {
    std::ofstream output(path, std::ios::binary);
    output << bytes;
    output.close();
    Require(!output.fail(), "Write test file");
}
void CheckFont(TitleFont font, const wchar_t* expected) {
    TextStyles styles;
    styles.Initialize(font);
    const auto layout = styles.Layout(L"Glass Harbor", Typography::Role::MediaTitle, ExpandedNowPlaying::Title);
    wchar_t family[64]{};
    Check(layout->GetFontFamilyName(0, family, static_cast<UINT32>(std::size(family))), "Read title family");
    Require(std::wstring_view(family) == expected, "Wrong title font family");
    float size{};
    Check(layout->GetFontSize(0, &size), "Read title size");
    Require(size == Typography::MediaTitle.size, "Title size must not change with font choice");
}
void Run(const std::filesystem::path& path) {
    for (const auto* text : {"{}", "{", "[]", "null", "{\"hoverDelayMs\":", "{\"hoverDelayMs\":0,}",
                            "{\"titleFont\":\"sans\"} trailing", "\xFF"}) { Defaults(ParseConfig(text)); }
    Defaults(ParseConfig(R"({"version":99,"titleFont":false,"accentColor":"#12345g","hoverDelayMs":"0","monitorIndex":-1,"unknown":[1,{}]})"));
    Defaults(ParseConfig(R"({"hoverDelayMs":0.5,"monitorIndex":1.2})"));
    const auto custom = ParseConfig(R"({"version":1,"titleFont":"sans","accentColor":"#aB12EF","hoverDelayMs":800,"monitorIndex":2,"showDebugFps":true})");
    Require(custom.user.titleFont == TitleFont::Sans && custom.user.hoverDelayMs == 800 &&
        custom.user.monitorIndex == 2 && custom.showDebugFps, "Valid fields ignored");
    Require(std::abs(custom.user.accentColor.r - 171.0f / 255) < 0.0001f &&
        std::abs(custom.user.accentColor.g - 18.0f / 255) < 0.0001f &&
        std::abs(custom.user.accentColor.b - 239.0f / 255) < 0.0001f, "Hex parsing");
    Require(ParseConfig(R"({"hoverDelayMs":-999999999999})").user.hoverDelayMs == 0, "Lower clamp");
    Require(ParseConfig(R"({"hoverDelayMs":999999999999})").user.hoverDelayMs == 1000, "Upper clamp");
    Require(ParseConfig(R"({"monitorIndex":999999999999})").user.monitorIndex == 0, "Monitor overflow");
    const auto partial = ParseConfig(R"({"titleFont":"sans","accentColor":"red","hoverDelayMs":false})");
    Require(partial.user.titleFont == TitleFont::Sans && partial.user.hoverDelayMs == 150 &&
        partial.user.accentColor.r == kDefaultConfig.accentColor.r, "Per-field fallback");
    Require(ParseConfig("\xEF\xBB\xBF{\"titleFont\":\"s\\u0061ns\"}").user.titleFont == TitleFont::Sans,
        "UTF-8 BOM and JSON escapes");
    const auto start = AccentStart(custom.user.accentColor);
    Require(std::abs(start.r - (custom.user.accentColor.r + (1 - custom.user.accentColor.r) * 0.35f)) < 0.0001f,
        "Custom gradient start");
    Require(AccentStart(ParseConfig(R"({"accentColor":"#38BDF8"})").user.accentColor).g == Color::Aqua.g,
        "Default accent must preserve Aqua");
    for (unsigned delay : {0u, 800u, 1000u}) {
        HoverTracker hover(delay);
        hover.Pointer(true, 0.0);
        const double deadline = delay / 1000.0;
        Require(hover.NextDeadline() == deadline, "Configured hover deadline");
        Require(!hover.Tick(deadline - 0.001), "Hover fired early");
        Require(hover.Tick(deadline) == true, "Hover did not expand");
        hover.Pointer(false, 2.0);
        Require(hover.NextDeadline() == 2.25 && hover.Tick(2.25) == false, "Collapse delay changed");
    }
    Defaults(LoadConfig(path));
    Require(std::filesystem::exists(path), "Missing config not created");
    const auto defaults = Read(path);
    Defaults(ParseConfig(defaults));
    Require(defaults.find("\"version\": 1") != std::string::npos, "Generated schema");
    const std::string unknown = R"({"titleFont":"sans","future":{"untouched":true}})";
    Write(path, unknown);
    Require(LoadConfig(path).user.titleFont == TitleFont::Sans && Read(path) == unknown, "Unknown fields must survive");
    const std::string broken = "{\"titleFont\":\"sans\", broken";
    Write(path, broken);
    Defaults(LoadConfig(path));
    Require(Read(path) == broken, "Malformed user file was overwritten");
    const std::string oversized(65537, ' ');
    Write(path, oversized);
    Defaults(LoadConfig(path));
    Require(Read(path) == oversized, "Oversized user file was overwritten");
    Defaults(LoadConfig(path.parent_path())); // Directory/unreadable path must not be replaced.
    CheckFont(TitleFont::Serif, L"Georgia");
    CheckFont(TitleFont::Sans, L"Segoe UI Variable Display");
}
} // namespace

int main() {
    std::filesystem::path temporary;
    const HRESULT initialized = RoInitialize(RO_INIT_SINGLETHREADED);
    try {
        caecilia::Check(initialized, "Initialize test WinRT apartment");
        wchar_t folder[MAX_PATH]{}, name[MAX_PATH]{};
        caecilia::CheckWin32(GetTempPathW(MAX_PATH, folder) != 0, "Get test temp path");
        caecilia::CheckWin32(GetTempFileNameW(folder, L"ncf", 0, name) != 0, "Reserve test path");
        temporary = name;
        std::filesystem::remove(temporary);
        Run(temporary / L"NotchIsland" / L"config.json");
        std::filesystem::remove_all(temporary);
        RoUninitialize();
        std::puts("PASS: config parsing/defaults/preservation, hover delay, accent and title fonts");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        std::error_code ignored;
        if (!temporary.empty()) { std::filesystem::remove_all(temporary, ignored); }
        if (SUCCEEDED(initialized)) { RoUninitialize(); }
        return 1;
    }
}
