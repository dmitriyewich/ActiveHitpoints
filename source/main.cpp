#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cstddef>
#include <cstdint>

static_assert(sizeof(void*) == 4, "ActiveHitpoints.asi must be built for Win32.");

extern "C" int _fltused = 0;

#pragma function(memcpy, memset)
extern "C" void* __cdecl memcpy(void* dst, const void* src, std::size_t size) {
    auto* out = static_cast<unsigned char*>(dst);
    const auto* in = static_cast<const unsigned char*>(src);
    for (std::size_t i = 0; i < size; ++i) {
        out[i] = in[i];
    }
    return dst;
}

extern "C" void* __cdecl memset(void* dst, int value, std::size_t size) {
    auto* out = static_cast<unsigned char*>(dst);
    const auto v = static_cast<unsigned char>(value);
    for (std::size_t i = 0; i < size; ++i) {
        out[i] = v;
    }
    return dst;
}

// RGB color for the triangle: 0x00RRGGBB (alpha preserved from game vertices).
// Default = green (25, 255, 25).
volatile DWORD g_triangleD3DColor = 0x0019FF19;

// Address of the original RwIm3DTransform used by the naked hook below.
static const std::uintptr_t kOriginalRwIm3DTransformAddr = 0x7EF450;

extern "C" void __declspec(naked) HookedRwIm3DTransformForTriangle() {
    static const std::uintptr_t kVtxColor0 = 0xC4D970;
    static const std::uintptr_t kVtxColor1 = 0xC4D994;
    static const std::uintptr_t kVtxColor2 = 0xC4D9B8;
    __asm {
        push eax
        push ecx
        push edx
        mov eax, dword ptr [g_triangleD3DColor]     // 0x00RRGGBB (no alpha)

        mov ecx, dword ptr [kVtxColor0]
        mov edx, dword ptr [ecx]                     // existing color with game alpha
        and edx, 0xFF000000                          // keep only alpha
        or  edx, eax                                 // merge our RGB
        mov dword ptr [ecx], edx

        mov ecx, dword ptr [kVtxColor1]
        mov edx, dword ptr [ecx]
        and edx, 0xFF000000
        or  edx, eax
        mov dword ptr [ecx], edx

        mov ecx, dword ptr [kVtxColor2]
        mov edx, dword ptr [ecx]
        and edx, 0xFF000000
        or  edx, eax
        mov dword ptr [ecx], edx

        pop edx
        pop ecx
        pop eax
        jmp dword ptr [kOriginalRwIm3DTransformAddr]
    }
}

namespace {

constexpr DWORD kGtaLoadStateAddress = 0x00C8D4C0;
constexpr std::uintptr_t kLocalPlayerPedPointerAddress = 0x00B6F5F0;
constexpr std::uint32_t kPlayerTargetedPedOffset = 0x79C;
constexpr std::uint32_t kPedHealthOffset = 0x540;

constexpr std::uintptr_t kTriangleRwIm3DTransformCall = 0x0060C034;

using IdFindFn = unsigned short(__thiscall*)(void*, const void*);

enum class SampVersion {
    Unknown,
    R1,
    R2,
    R3,
    R3_1,
    R4,
    R4_2,
    R5_1,
    DL_R1,
};

struct Color {
    std::uint8_t r;
    std::uint8_t g;
    std::uint8_t b;

    bool operator==(const Color& other) const {
        return r == other.r && g == other.g && b == other.b;
    }

    bool operator!=(const Color& other) const {
        return !(*this == other);
    }
};

struct SampOffsets {
    std::uint32_t sampInfoOffset;
    std::uint32_t poolsOffset;
    std::uint32_t playerPoolOffset;
    std::uint32_t remotePlayerArrayOffset;
    std::uint32_t remoteDataOffset;
    std::uint32_t remoteHealthOffset;
    std::uint32_t idFindOffset;
};

struct SampVersionInfo {
    DWORD entryPointRva;
    SampVersion version;
    const char* name;
    SampOffsets offsets;
};

constexpr Color kDefaultColor{ 25, 255, 25 };
constexpr int kInvalidPlayerId = 65535;
constexpr int kNumSupportedVersions = 8;

// Смещения: rz-bin (AddressOfEntryPoint) и rizin (pd на CPlayerPool::Find, /x на VA глобала *CNetGame).
// Значения выверены по samp.dll целевых сборок клиента (локальные копии с одного и того же дерева версий).
static const SampVersionInfo kSupportedVersions[kNumSupportedVersions] = {
    { 0x31DF13, SampVersion::R1, "R1", { 0x21A0F8, 0x03CD, 0x18, 0x002E, 0x00, 0x01BC, 0x010420 } },
    { 0x3195DD, SampVersion::R2, "R2", { 0x21A100, 0x03C5, 0x08, 0x0026, 0x0C, 0x01BC, 0x0104C0 } },
    { 0x0CC490, SampVersion::R3, "R3", { 0x26E8DC, 0x03DE, 0x08, 0x0004, 0x00, 0x01B0, 0x013570 } },
    { 0x0CC4D0, SampVersion::R3_1, "R3-1", { 0x26E8DC, 0x03DE, 0x08, 0x0004, 0x00, 0x01B0, 0x013570 } },
    { 0x0CBCB0, SampVersion::R4, "R4", { 0x26EA0C, 0x03DE, 0x08, 0x002E, 0x10, 0x01B0, 0x013890 } },
    { 0x0CBCD0, SampVersion::R4_2, "R4-2", { 0x26EA0C, 0x03DE, 0x04, 0x1F8A, 0x10, 0x01B0, 0x0138C0 } },
    { 0x0CBC90, SampVersion::R5_1, "R5-1", { 0x26EB94, 0x03DE, 0x04, 0x1F8A, 0x10, 0x01B0, 0x0138C0 } },
    { 0x0FDB60, SampVersion::DL_R1, "DL-R1", { 0x2ACA24, 0x03DE, 0x08, 0x0026, 0x08, 0x01B0, 0x0137C0 } },
};

volatile LONG g_stopRequested = 0;
bool g_triangleHookApplied = false;

static bool IsRangeReadable(const unsigned char* first, std::size_t size) {
    if (!first || size == 0) {
        return false;
    }
    const unsigned char* cur = first;
    const unsigned char* const limit = first + size;
    while (cur < limit) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (VirtualQuery(cur, &mbi, sizeof(mbi)) < sizeof(mbi)) {
            return false;
        }
        if (mbi.State != MEM_COMMIT) {
            return false;
        }
        if (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) {
            return false;
        }
        constexpr DWORD kReadMask = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ
            | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
        if ((mbi.Protect & kReadMask) == 0) {
            return false;
        }
        const auto* regBegin = reinterpret_cast<const unsigned char*>(mbi.BaseAddress);
        const auto* regEnd = regBegin + mbi.RegionSize;
        if (cur < regBegin || cur >= regEnd) {
            return false;
        }
        if (limit <= regEnd) {
            return true;
        }
        cur = regEnd;
    }
    return true;
}

static bool IsRangeExecutable(const unsigned char* first, std::size_t size) {
    if (!first || size == 0) {
        return false;
    }
    const unsigned char* cur = first;
    const unsigned char* const limit = first + size;
    while (cur < limit) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (VirtualQuery(cur, &mbi, sizeof(mbi)) < sizeof(mbi)) {
            return false;
        }
        if (mbi.State != MEM_COMMIT) {
            return false;
        }
        if (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) {
            return false;
        }
        constexpr DWORD kExecMask = PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
        if ((mbi.Protect & kExecMask) == 0) {
            return false;
        }
        const auto* regBegin = reinterpret_cast<const unsigned char*>(mbi.BaseAddress);
        const auto* regEnd = regBegin + mbi.RegionSize;
        if (cur < regBegin || cur >= regEnd) {
            return false;
        }
        if (limit <= regEnd) {
            return true;
        }
        cur = regEnd;
    }
    return true;
}

template <typename T>
bool SafeRead(std::uintptr_t address, T& value) {
    if (address == 0) {
        value = T{};
        return false;
    }
    const auto* p = reinterpret_cast<const unsigned char*>(address);
    if (!IsRangeReadable(p, sizeof(T))) {
        value = T{};
        return false;
    }
    value = *reinterpret_cast<const T*>(p);
    return true;
}

bool WriteMemory(std::uintptr_t address, const void* data, std::size_t size) {
    if (address == 0 || data == nullptr || size == 0) {
        return false;
    }

    DWORD oldProtect = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(address), size, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        return false;
    }

    memcpy(reinterpret_cast<void*>(address), data, size);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(address), size);

    DWORD restoredProtect = 0;
    VirtualProtect(reinterpret_cast<void*>(address), size, oldProtect, &restoredProtect);
    return true;
}

bool WriteRelativeCall(std::uintptr_t callAddress, const void* target) {
    const auto targetAddress = reinterpret_cast<std::uintptr_t>(target);
    const auto nextInstruction = callAddress + 5;
    const auto relative = static_cast<std::int64_t>(targetAddress) - static_cast<std::int64_t>(nextInstruction);

    if (relative < -0x80000000LL || relative > 0x7FFFFFFFLL) {
        return false;
    }

    unsigned char patch[5];
    patch[0] = 0xE8;
    const auto relative32 = static_cast<std::int32_t>(relative);
    memcpy(patch + 1, &relative32, sizeof(relative32));
    return WriteMemory(callAddress, patch, sizeof(patch));
}

DWORD ColorToD3D(const Color& color) {
    return (static_cast<DWORD>(color.r) << 16) | (static_cast<DWORD>(color.g) << 8) | static_cast<DWORD>(color.b);
}

bool ApplyTriangleHook() {
    if (g_triangleHookApplied) {
        return true;
    }

    if (!WriteRelativeCall(kTriangleRwIm3DTransformCall,
            reinterpret_cast<const void*>(&HookedRwIm3DTransformForTriangle))) {
        return false;
    }

    g_triangleHookApplied = true;
    return true;
}

void SetTriangleColor(const Color& color) {
    InterlockedExchange(reinterpret_cast<volatile LONG*>(&g_triangleD3DColor),
        static_cast<LONG>(ColorToD3D(color)));
}

float Clamp01(float value) {
    if (value < 0.0f) {
        return 0.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

std::uint8_t RoundToByte(float value) {
    if (value < 0.0f) {
        value = 0.0f;
    } else if (value > 255.0f) {
        value = 255.0f;
    }

    return static_cast<std::uint8_t>(value + 0.5f);
}

Color LerpColor(float t, float fromR, float fromG, float fromB, float toR, float toG, float toB) {
    const float amount = Clamp01(t);
    return {
        RoundToByte(fromR + ((toR - fromR) * amount)),
        RoundToByte(fromG + ((toG - fromG) * amount)),
        RoundToByte(fromB + ((toB - fromB) * amount)),
    };
}

Color GetColorByHealth(float health) {
    if (health >= 100.0f) {
        return kDefaultColor;                                              // зелёный
    }
    if (health > 50.0f) {
        return LerpColor((health - 50.0f) / 50.0f,                        // 50->99: жёлтый->зелёный
            255.0f, 255.0f, 0.0f, 25.0f, 255.0f, 25.0f);
    }
    if (health > 10.0f) {
        return LerpColor((health - 10.0f) / 40.0f,                        // 10->50: красный->жёлтый
            255.0f, 0.0f, 0.0f, 255.0f, 255.0f, 0.0f);
    }
    if (health > 0.0f) {
        return LerpColor(health / 10.0f,                                   // 0->10: чёрный->красный
            0.0f, 0.0f, 0.0f, 255.0f, 0.0f, 0.0f);
    }
    return { 0, 0, 0 };                                                    // 0: чёрный
}

const SampVersionInfo* DetectSampVersion(HMODULE sampModule) {
    if (sampModule == nullptr) {
        return nullptr;
    }

    const auto* dosHeader = reinterpret_cast<const IMAGE_DOS_HEADER*>(sampModule);
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        return nullptr;
    }

    const auto* ntHeaders = reinterpret_cast<const IMAGE_NT_HEADERS32*>(
        reinterpret_cast<const std::uint8_t*>(sampModule) + dosHeader->e_lfanew);
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) {
        return nullptr;
    }

    const DWORD entryPoint = ntHeaders->OptionalHeader.AddressOfEntryPoint;
    for (int i = 0; i < kNumSupportedVersions; ++i) {
        if (kSupportedVersions[i].entryPointRva == entryPoint) {
            return &kSupportedVersions[i];
        }
    }

    return nullptr;
}

bool ResolvePlayerPool(HMODULE sampModule, const SampVersionInfo& version, std::uint32_t& playerPool) {
    playerPool = 0;
    if (sampModule == nullptr) {
        return false;
    }

    const auto base = reinterpret_cast<std::uintptr_t>(sampModule);

    std::uint32_t sampInfo = 0;
    if (!SafeRead(base + version.offsets.sampInfoOffset, sampInfo) || sampInfo == 0) {
        return false;
    }

    std::uint32_t pools = 0;
    if (!SafeRead(sampInfo + version.offsets.poolsOffset, pools) || pools == 0) {
        return false;
    }

    return SafeRead(pools + version.offsets.playerPoolOffset, playerPool) && playerPool != 0;
}

int FindPlayerId(HMODULE sampModule, const SampVersionInfo& version, std::uint32_t playerPool, const void* gtaPed) {
    if (sampModule == nullptr || playerPool == 0 || gtaPed == nullptr) {
        return -1;
    }

    const auto base = reinterpret_cast<std::uintptr_t>(sampModule);
    const auto findIdAddr = base + version.offsets.idFindOffset;
    const auto* findIdBytes = reinterpret_cast<const unsigned char*>(findIdAddr);
    if (!IsRangeExecutable(findIdBytes, 16)) {
        return -1;
    }

    const auto findId = reinterpret_cast<IdFindFn>(findIdAddr);
    const unsigned short playerId = findId(reinterpret_cast<void*>(playerPool), gtaPed);

    return playerId != static_cast<unsigned short>(kInvalidPlayerId) ? static_cast<int>(playerId) : -1;
}

bool ReadRemotePlayerHealth(
    std::uint32_t playerPool,
    const SampVersionInfo& version,
    int playerId,
    float& health,
    std::uint32_t* remotePlayerOut,
    std::uint32_t* remoteDataOut) {
    health = 0.0f;
    if (remotePlayerOut != nullptr) {
        *remotePlayerOut = 0;
    }
    if (remoteDataOut != nullptr) {
        *remoteDataOut = 0;
    }

    if (playerPool == 0 || playerId < 0 || playerId > 1003) {
        return false;
    }

    std::uint32_t remotePlayer = 0;
    if (!SafeRead(playerPool + version.offsets.remotePlayerArrayOffset + (static_cast<std::uint32_t>(playerId) * 4), remotePlayer)
        || remotePlayer == 0) {
        return false;
    }
    if (remotePlayerOut != nullptr) {
        *remotePlayerOut = remotePlayer;
    }

    std::uint32_t remoteData = 0;
    if (!SafeRead(remotePlayer + version.offsets.remoteDataOffset, remoteData) || remoteData == 0) {
        return false;
    }
    if (remoteDataOut != nullptr) {
        *remoteDataOut = remoteData;
    }

    return SafeRead(remoteData + version.offsets.remoteHealthOffset, health);
}

bool ReadLocalPlayerPed(std::uint32_t& localPed) {
    localPed = 0;
    return SafeRead(kLocalPlayerPedPointerAddress, localPed) && localPed != 0;
}

bool ResolveTargetHealth(
    HMODULE sampModule,
    const SampVersionInfo& version,
    std::uint32_t playerPool,
    std::uint32_t localPed,
    std::uint32_t targetPed,
    float& health) {
    health = 0.0f;

    if (targetPed == 0) {
        return false;
    }

    if (targetPed == localPed) {
        return SafeRead(localPed + kPedHealthOffset, health);
    }

    const int playerId = FindPlayerId(sampModule, version, playerPool, reinterpret_cast<const void*>(targetPed));
    if (playerId < 0) {
        return false;
    }

    return ReadRemotePlayerHealth(playerPool, version, playerId, health, nullptr, nullptr);
}

void WaitForGameLoad() {
    const auto* gtaLoadState = reinterpret_cast<volatile DWORD*>(kGtaLoadStateAddress);
    while (InterlockedCompareExchange(&g_stopRequested, 0, 0) == 0 && *gtaLoadState < 9) {
        Sleep(10);
    }
}

DWORD WINAPI InitializePlugin(void*) {
    WaitForGameLoad();

    if (!ApplyTriangleHook()) {
        return 0;
    }

    HMODULE cachedSampModule = nullptr;
    const SampVersionInfo* cachedVersion = nullptr;

    while (InterlockedCompareExchange(&g_stopRequested, 0, 0) == 0) {
        std::uint32_t localPed = 0;
        if (!ReadLocalPlayerPed(localPed)) {
            SetTriangleColor(kDefaultColor);
            Sleep(50);
            continue;
        }

        HMODULE sampModule = GetModuleHandleA("samp.dll");
        if (sampModule != cachedSampModule) {
            cachedSampModule = sampModule;
            cachedVersion = DetectSampVersion(sampModule);
        }

        if (sampModule == nullptr || cachedVersion == nullptr) {
            SetTriangleColor(kDefaultColor);
            Sleep(sampModule == nullptr ? 100 : 250);
            continue;
        }

        std::uint32_t playerPool = 0;
        if (!ResolvePlayerPool(sampModule, *cachedVersion, playerPool)) {
            SetTriangleColor(kDefaultColor);
            Sleep(50);
            continue;
        }

        std::uint32_t targetPed = 0;
        if (!SafeRead(localPed + kPlayerTargetedPedOffset, targetPed) || targetPed == 0) {
            SetTriangleColor(kDefaultColor);
            Sleep(1);
            continue;
        }

        float health = 0.0f;
        if (ResolveTargetHealth(sampModule, *cachedVersion, playerPool, localPed, targetPed, health)) {
            const Color color = GetColorByHealth(health);
            SetTriangleColor(color);
        } else {
            // This matches the Lua behavior: non-SA:MP targets keep the previous color.
        }

        Sleep(1);
    }

    return 0;
}

}  // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);

        HANDLE thread = CreateThread(nullptr, 0, &InitializePlugin, nullptr, 0, nullptr);
        if (thread != nullptr) {
            CloseHandle(thread);
        }
    } else if (reason == DLL_PROCESS_DETACH) {
        InterlockedExchange(&g_stopRequested, 1);
    }

    return TRUE;
}
