#include <IPluginInterface.h>
#include <Logging.h>
#include <bcrypt.h>

#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {
constexpr char ExpectedExecutableSha256[] =
    "005412cf59daa1aadfba8deb05f097f5418aec57ddc98216483d93bf5b0119b1";

struct PatchSite {
    const char* name;
    size_t rva;
    std::vector<unsigned char> expected;
    size_t byteOffset;
    unsigned char doorValue;
    unsigned char originalKeyValue;
    bool applied = false;
    unsigned char appliedValue = 0;
};

// These are the input-update and prompt-binding methods of the non-VR
// KeyboardDragging and GamepadDragging classes, identified through MSVC RTTI.
// Keep eGameInputDumpBody (72) intact; replace only its low-level input binding.
std::array<PatchSite, 4> Sites() {
    return {{
        {"keyboard input", 0x10757c,
         {0x45,0x0f,0xb6,0x82,0x49,0x08,0x00,0x00,0xba,0x48,0x00,0x00,0x00}, 4,0x3f,0x48},
        {"gamepad input", 0xff066,
         {0x45,0x0f,0xb6,0x82,0xf4,0x07,0x00,0x00,0xba,0x48,0x00,0x00,0x00}, 4,0xf0,0xf0},
        {"keyboard prompt", 0x107489,
         {0xba,0x48,0x00,0x00,0x00,0x48,0x8b,0xcb,0x44,0x8d,0x42,0x75}, 11,0x6b,0x74},
        {"gamepad prompt", 0xfef25,
         {0xba,0x48,0x00,0x00,0x00,0x48,0x8b,0xcb,0x44,0x8d,0x42,0x20}, 11,0x1c,0x1c}
    }};
}

std::string ExecutableSha256() {
    std::array<wchar_t, 32768> path{};
    const auto pathLength = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (!pathLength || pathLength >= path.size()) return {};
    std::ifstream file(std::filesystem::path(path.data()), std::ios::binary);
    if (!file) return {};

    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) return {};
    if (BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) < 0) {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return {};
    }
    bool success = true;
    std::array<unsigned char, 65536> buffer{};
    while (file) {
        file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
        const auto count = file.gcount();
        if (count > 0 && BCryptHashData(hash, buffer.data(), static_cast<ULONG>(count), 0) < 0) {
            success = false;
            break;
        }
    }
    if (!file.eof()) success = false;
    std::array<unsigned char, 32> digest{};
    if (success && BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) < 0) success = false;
    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    if (!success) return {};
    constexpr char hex[] = "0123456789abcdef";
    std::string result;
    result.reserve(64);
    for (const auto byte : digest) {
        result += hex[byte >> 4];
        result += hex[byte & 15];
    }
    return result;
}

bool WriteByte(unsigned char* address, unsigned char value) {
    DWORD protection = 0;
    if (!VirtualProtect(address, 1, PAGE_EXECUTE_READWRITE, &protection)) return false;
    *address = value;
    FlushInstructionCache(GetCurrentProcess(), address, 1);
    DWORD ignored = 0;
    if (!VirtualProtect(address, 1, protection, &ignored)) {
        Logger::Warn("[NoHoldBodyDump] Could not restore page protection (error {}).", GetLastError());
    }
    return true;
}
}

class NoHoldBodyDump : public IPluginInterface {
public:
    ~NoHoldBodyDump() override { Restore(); }

    // Patch before engine initialization so prompt bindings are registered from
    // the changed instructions. Install/enable with the game closed, then start.
    void Init() override {
        if (initialized_) return;
        initialized_ = true;
        const bool enabled = GetSettingBool("bodydump", "enabled", true);
        const bool door = GetSettingBool("bodydump", "use_open_door_key", true);
        if (!HasSetting("bodydump", "enabled")) SetSettingBool("bodydump", "enabled", enabled);
        if (!HasSetting("bodydump", "use_open_door_key")) SetSettingBool("bodydump", "use_open_door_key", door);
        if (!enabled) {
            Logger::Info("[NoHoldBodyDump] Disabled in INI; no changes applied.");
            return;
        }
        const auto digest = ExecutableSha256();
        if (digest != ExpectedExecutableSha256) {
            Logger::Warn("[NoHoldBodyDump] Unsupported executable SHA-256 {}; no changes applied.", digest);
            return;
        }
        base_ = reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
        const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base_);
        const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base_ + dos->e_lfanew);
        const size_t imageSize = nt->OptionalHeader.SizeOfImage;
        // Verify every site before writing anything, including possible conflicts
        // with another mod that has already changed these instructions in memory.
        for (const auto& site : sites_) {
            if (site.rva + site.expected.size() > imageSize ||
                std::memcmp(base_ + site.rva, site.expected.data(), site.expected.size()) != 0) {
                Logger::Warn("[NoHoldBodyDump] Unexpected bytes at {}; no changes applied.", site.name);
                return;
            }
        }
        for (auto& site : sites_) {
            const auto replacement = door ? site.doorValue : site.originalKeyValue;
            if (!WriteByte(base_ + site.rva + site.byteOffset, replacement)) {
                Logger::Warn("[NoHoldBodyDump] Failed to apply {}; restoring previous sites.", site.name);
                Restore();
                return;
            }
            site.applied = true;
            site.appliedValue = replacement;
            Logger::Info("[NoHoldBodyDump] Applied {}.", site.name);
        }
        Logger::Info("[NoHoldBodyDump] Single-press body dump enabled: keyboard {}, gamepad interact downedge. Prototype needs in-game validation.",
                     door ? "Open Door binding" : "original Activate Prop binding");
    }

private:
    void Restore() {
        if (!base_) return;
        for (auto& site : sites_) {
            if (!site.applied) continue;
            auto* address = base_ + site.rva + site.byteOffset;
            const auto current = *address;
            // Leave a later mod's change alone if it replaced our byte.
            if (current == site.appliedValue) {
                if (!WriteByte(address, site.expected[site.byteOffset])) {
                    Logger::Warn("[NoHoldBodyDump] Could not restore {}.", site.name);
                }
            }
            site.applied = false;
        }
    }
    std::array<PatchSite, 4> sites_ = Sites();
    unsigned char* base_ = nullptr;
    bool initialized_ = false;
};

DEFINE_ZHM_PLUGIN(NoHoldBodyDump);
