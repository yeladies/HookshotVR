#pragma once
#include <windows.h>
#include <tlhelp32.h>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>

struct CaptureHealth {
    unsigned games = 0;
    bool target_found = false;
    uint64_t physical_available = 0;
    uint64_t commit_available = 0;
};

inline CaptureHealth capture_health(uint32_t target) {
    CaptureHealth health;
    MEMORYSTATUSEX memory{};
    memory.dwLength = sizeof(memory);
    if (!GlobalMemoryStatusEx(&memory)) throw std::runtime_error("Cannot check system memory; capture not started");
    health.physical_available = memory.ullAvailPhys;
    health.commit_available = memory.ullAvailPageFile;
    HANDLE snapshot = INVALID_HANDLE_VALUE;
    for (int attempt = 0; attempt < 3 && snapshot == INVALID_HANDLE_VALUE; ++attempt)
        snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot enumerate game processes; capture not started");
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (!Process32FirstW(snapshot, &entry)) {
        CloseHandle(snapshot);
        throw std::runtime_error("Cannot read process snapshot; capture not started");
    }
    do {
        if (!_wcsicmp(entry.szExeFile, L"hookshot.exe")) {
            ++health.games;
            health.target_found |= entry.th32ProcessID == target;
        }
    } while (Process32NextW(snapshot, &entry));
    const DWORD result = GetLastError();
    CloseHandle(snapshot);
    if (result != ERROR_NO_MORE_FILES) throw std::runtime_error("Incomplete process snapshot; capture not started");
    return health;
}

inline void require_capture_health(const CaptureHealth& health) {
    if (health.games != 1 || !health.target_found)
        throw std::runtime_error("Capture requires exactly one Hookshot process matching the selected PID; found " +
                                 std::to_string(health.games) + ". Close duplicate/preparation instances first");
    // A small diagnostic reserve, not a guarantee that the game or driver cannot run out of memory.
    constexpr uint64_t MiB = 1024ULL * 1024;
    if (health.physical_available < 768 * MiB || health.commit_available < 1024 * MiB)
        throw std::runtime_error("Low system memory: capture requires 768 MiB available RAM and 1024 MiB commit headroom");
}

inline void check_capture_health(uint32_t target, bool report = false) {
    const auto health = capture_health(target);
    if (report)
        std::printf("capture_preflight games=%u target_found=%s available_ram_mib=%llu commit_headroom_mib=%llu\n",
                    health.games, health.target_found ? "true" : "false",
                    static_cast<unsigned long long>(health.physical_available >> 20),
                    static_cast<unsigned long long>(health.commit_available >> 20));
    require_capture_health(health);
}
