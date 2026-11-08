// Real process-snapshot test. Child fixtures only sleep; no game, XR or graphics.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "capture_guards.h"
#include <filesystem>
#include <functional>

namespace {
struct Child {
    PROCESS_INFORMATION process{};
    ~Child() {
        if (process.hProcess) {
            if (WaitForSingleObject(process.hProcess, 0) == WAIT_TIMEOUT) {
                TerminateProcess(process.hProcess, 0);
                WaitForSingleObject(process.hProcess, 5000);
            }
            CloseHandle(process.hProcess);
            CloseHandle(process.hThread);
        }
    }
    void start(const std::filesystem::path& exe) {
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        std::wstring command = L"\"" + exe.native() + L"\" --guard-child";
        if (!CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                            nullptr, exe.parent_path().c_str(), &startup, &process))
            throw std::runtime_error("Cannot start harmless process fixture");
    }
};
void expect_block(const std::function<void()>& action) {
    bool blocked = false;
    try { action(); } catch (const std::runtime_error&) { blocked = true; }
    if (!blocked) throw std::runtime_error("Unsafe capture state was accepted");
}
}
int main(int argc, char**) {
    if (argc > 1) { Sleep(20000); return 0; }
    try {
        const auto initial = capture_health(GetCurrentProcessId());
        if (initial.games) { std::puts("Skip: a real Hookshot process is running"); return 77; }
        expect_block([&] { require_capture_health(initial); });
        wchar_t name[32768];
        const auto n = GetModuleFileNameW(nullptr, name, 32768);
        if (!n || n >= 32768) throw std::runtime_error("Cannot locate test executable");
        const std::filesystem::path self(std::wstring(name, n));
        const auto dir = self.parent_path() / "guard-fixture";
        std::filesystem::create_directories(dir);
        const auto fixture = dir / "hookshot.exe";
        if (!CopyFileW(self.c_str(), fixture.c_str(), FALSE)) throw std::runtime_error("Cannot create fixture copy");
        {
            Child first;
            first.start(fixture);
            auto one = capture_health(first.process.dwProcessId);
            if (one.games != 1 || !one.target_found) throw std::runtime_error("First fixture not identified");
            require_capture_health(one);
            expect_block([&] { require_capture_health(capture_health(GetCurrentProcessId())); });
            auto low = one;
            low.physical_available = 0;
            expect_block([&] { require_capture_health(low); });
            low = one;
            low.commit_available = 0;
            expect_block([&] { require_capture_health(low); });
            {
                Child second;
                second.start(fixture);
                const auto two = capture_health(first.process.dwProcessId);
                if (two.games != 2) throw std::runtime_error("Duplicate fixture not identified");
                expect_block([&] { require_capture_health(two); });
            }
            require_capture_health(capture_health(first.process.dwProcessId));
        }
        if (capture_health(0).games) throw std::runtime_error("Fixture cleanup incomplete");
        std::puts("Process selection, duplicate rejection, memory pressure and cleanup passed; no XR invoked");
        return 0;
    } catch (const std::exception& e) { std::fprintf(stderr, "%s\n", e.what()); return 1; }
}
