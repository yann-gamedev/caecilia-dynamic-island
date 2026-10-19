#include "app/Config.h"
#include "app/Errors.h"
#include <shlobj.h>
#include <windows.data.json.h>
#include <wrl/client.h>
#include <wrl/wrappers/corewrappers.h>
#include <fstream>
#include <limits>
#include <memory>

namespace caecilia {
namespace {
using namespace ABI::Windows::Data::Json;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Wrappers::HString;
using Microsoft::WRL::Wrappers::HStringReference;
constexpr std::size_t kMaxConfigBytes = 64 * 1024;

ComPtr<IJsonValue> Field(IJsonObject* object, const wchar_t* name, JsonValueType type) {
    ComPtr<IJsonValue> value;
    const HRESULT result = object->GetNamedValue(HStringReference(name).Get(), &value);
    // WEB_E_JSON_VALUE_NOT_FOUND is absent from the supplemental MinGW headers.
    if (result == static_cast<HRESULT>(0x83750009L)) { return {}; }
    Check(result, "Read JSON field");
    JsonValueType actual{};
    Check(value->get_ValueType(&actual), "Read JSON field type");
    return actual == type ? value : nullptr;
}
std::wstring String(IJsonObject* object, const wchar_t* name) {
    const auto value = Field(object, name, JsonValueType_String);
    if (!value) { return {}; }
    HString text;
    Check(value->GetString(text.GetAddressOf()), "Read JSON string");
    UINT32 length{};
    const auto* buffer = WindowsGetStringRawBuffer(text.Get(), &length);
    return {buffer, length};
}
double Number(IJsonObject* object, const wchar_t* name, double fallback) {
    const auto value = Field(object, name, JsonValueType_Number);
    if (!value) { return fallback; }
    double number{};
    Check(value->GetNumber(&number), "Read JSON number");
    return std::isfinite(number) && std::trunc(number) == number ? number : fallback;
}
int Hex(wchar_t c) {
    if (c >= L'0' && c <= L'9') { return c - L'0'; }
    if (c >= L'a' && c <= L'f') { return c - L'a' + 10; }
    if (c >= L'A' && c <= L'F') { return c - L'A' + 10; }
    return -1;
}
std::filesystem::path DefaultPath() {
    PWSTR raw{};
    Check(SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_DEFAULT, nullptr, &raw),
          "Locate RoamingAppData");
    const std::unique_ptr<wchar_t, decltype(&CoTaskMemFree)> owner(raw, CoTaskMemFree);
    return std::filesystem::path(raw) / L"NotchIsland" / L"config.json";
}
void CreateDefault(const std::filesystem::path& path) {
    if (path.has_parent_path()) { std::filesystem::create_directories(path.parent_path()); }
    const auto& config = NotchIsland::kDefaultConfig;
    char text[256]{};
    const int length = std::snprintf(text, sizeof(text),
        "{\n  \"version\": %d,\n  \"titleFont\": \"%s\",\n  \"accentColor\": \"#%02X%02X%02X\",\n"
        "  \"hoverDelayMs\": %u,\n  \"monitorIndex\": %d\n}\n", config.version,
        config.titleFont == NotchIsland::TitleFont::Sans ? "sans" : "serif",
        static_cast<unsigned>(std::lround(config.accentColor.r * 255)),
        static_cast<unsigned>(std::lround(config.accentColor.g * 255)),
        static_cast<unsigned>(std::lround(config.accentColor.b * 255)),
        config.hoverDelayMs, config.monitorIndex);
    if (length < 0 || static_cast<std::size_t>(length) >= sizeof(text)) {
        throw std::runtime_error("Cannot serialize default config");
    }
    // CREATE_NEW never truncates user data, including a file created by another process.
    const HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                    FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE && GetLastError() == ERROR_FILE_EXISTS) { return; }
    CheckWin32(file != INVALID_HANDLE_VALUE, "Create default config");
    try {
        DWORD written{};
        CheckWin32(WriteFile(file, text, static_cast<DWORD>(length), &written, nullptr), "Write default config");
        if (written != static_cast<DWORD>(length)) { throw std::runtime_error("Incomplete default config write"); }
        CheckWin32(FlushFileBuffers(file), "Flush default config");
    } catch (...) { CloseHandle(file); throw; }
    CheckWin32(CloseHandle(file), "Close default config");
}
} // namespace

Config ParseConfig(std::string_view utf8) {
    const auto invalid = [] {
        Log("Warning: invalid config JSON/UTF-8; using defaults without overwriting the file");
        return Config{};
    };
    if (utf8.starts_with("\xEF\xBB\xBF")) { utf8.remove_prefix(3); }
    if (utf8.empty() || utf8.size() > kMaxConfigBytes) { return invalid(); }
    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(),
                                          static_cast<int>(utf8.size()), nullptr, 0);
    if (!length) { return invalid(); }
    std::wstring wide(static_cast<std::size_t>(length), L'\0');
    CheckWin32(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8.data(),
        static_cast<int>(utf8.size()), wide.data(), length) != 0, "Decode config UTF-8");
    HString input;
    Check(input.Set(wide.data(), static_cast<UINT32>(wide.size())), "Create JSON input string");
    ComPtr<IJsonValueStatics> parser;
    Check(RoGetActivationFactory(HStringReference(L"Windows.Data.Json.JsonValue").Get(),
        IID_PPV_ARGS(&parser)), "Activate Windows JSON parser");
    ComPtr<IJsonValue> root;
    boolean parsed{};
    Check(parser->TryParse(input.Get(), &root, &parsed), "Parse JSON");
    if (!parsed) { return invalid(); }
    JsonValueType type{};
    Check(root->get_ValueType(&type), "Read JSON root type");
    if (type != JsonValueType_Object) { return invalid(); }
    ComPtr<IJsonObject> object;
    Check(root->GetObject(&object), "Read JSON object");
    Config config;
    if (String(object.Get(), L"titleFont") == L"sans") { config.user.titleFont = NotchIsland::TitleFont::Sans; }
    const auto color = String(object.Get(), L"accentColor");
    if (color.size() == 7 && color[0] == L'#') {
        unsigned rgb{};
        bool valid = true;
        for (std::size_t i = 1; i < color.size(); ++i) {
            const int digit = Hex(color[i]);
            if (digit < 0) { valid = false; break; }
            rgb = rgb * 16 + static_cast<unsigned>(digit);
        }
        if (valid) {
            config.user.accentColor = {static_cast<float>((rgb >> 16) & 255) / 255.0f,
                static_cast<float>((rgb >> 8) & 255) / 255.0f, static_cast<float>(rgb & 255) / 255.0f, 1.0f};
        }
    }
    config.user.hoverDelayMs = static_cast<unsigned>(std::clamp(
        Number(object.Get(), L"hoverDelayMs", config.user.hoverDelayMs), 0.0, 1000.0));
    const double monitor = Number(object.Get(), L"monitorIndex", config.user.monitorIndex);
    if (monitor >= 0 && monitor <= (std::numeric_limits<int>::max)()) {
        config.user.monitorIndex = static_cast<int>(monitor); // Display-count validation is done by the window.
    }
    // Unknown/version fields are retained on disk; only schema version 1 is interpreted.
    if (const auto fps = Field(object.Get(), L"showDebugFps", JsonValueType_Boolean)) {
        boolean enabled{};
        Check(fps->GetBoolean(&enabled), "Read debug FPS option");
        config.showDebugFps = enabled != 0;
    }
    return config;
}

Config LoadConfig(const std::filesystem::path& requested) {
    try {
        const auto path = requested.empty() ? DefaultPath() : requested;
        if (!std::filesystem::exists(path)) { CreateDefault(path); }
        std::ifstream input(path, std::ios::binary);
        if (!input) { throw std::runtime_error("Cannot open config file"); }
        // ponytail: config is capped at 64 KiB; raise this only if the schema grows.
        std::string bytes(kMaxConfigBytes + 1, '\0');
        input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        if (input.bad()) { throw std::runtime_error("Config read failed"); }
        bytes.resize(static_cast<std::size_t>(input.gcount()));
        return ParseConfig(bytes);
    } catch (const std::exception& error) {
        Log(error.what());
        Log("Warning: config unavailable; using defaults without overwriting user data");
        return {};
    }
}
} // namespace caecilia
