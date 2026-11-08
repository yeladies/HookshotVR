#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include "window_capture.h"
#define VK_NO_PROTOTYPES
#include "panel_color.h"

namespace {
struct Apartment {
    Apartment() { winrt::init_apartment(winrt::apartment_type::multi_threaded); }
    ~Apartment() { winrt::uninit_apartment(); }
};
struct Search { DWORD pid; HWND window = nullptr; unsigned count = 0; };
BOOL CALLBACK find_window(HWND hwnd, LPARAM param) {
    auto& s = *reinterpret_cast<Search*>(param);
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == s.pid && IsWindowVisible(hwnd) && GetWindow(hwnd, GW_OWNER) == nullptr) {
        RECT rc{};
        if (GetClientRect(hwnd, &rc) && rc.right > 0 && rc.bottom > 0) { s.window = hwnd; ++s.count; }
    }
    return TRUE;
}
}
struct WindowCapture::Impl {
    Apartment apartment; // Destroyed after all WinRT/COM members.
    HWND window = nullptr;
    winrt::com_ptr<ID3D11Device> device;
    winrt::com_ptr<ID3D11DeviceContext> context;
    winrt::com_ptr<ID3D11Texture2D> staging;
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem item{nullptr};
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool pool{nullptr};
    winrt::Windows::Graphics::Capture::GraphicsCaptureSession session{nullptr};
    winrt::Windows::Graphics::SizeInt32 size{};
    ~Impl() {
        try { if (session) session.Close(); if (pool) pool.Close(); } catch (...) {}
    }
    explicit Impl(uint32_t pid) {
        const HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!process) throw std::runtime_error("Cannot inspect capture process");
        wchar_t path[32768]; DWORD len = 32768;
        const BOOL ok = QueryFullProcessImageNameW(process, 0, path, &len);
        CloseHandle(process);
        if (!ok || _wcsicmp(std::filesystem::path(std::wstring(path, len)).filename().c_str(), L"hookshot.exe"))
            throw std::runtime_error("Capture target must be the explicitly selected hookshot.exe process");
        Search search{pid};
        EnumWindows(find_window, reinterpret_cast<LPARAM>(&search));
        if (search.count != 1) throw std::runtime_error("Expected exactly one visible Hookshot window; restart after it opens");
        window = search.window;
        using namespace winrt::Windows::Graphics::Capture;
        if (!GraphicsCaptureSession::IsSupported()) throw std::runtime_error("Windows Graphics Capture unsupported");
        auto factory = winrt::get_activation_factory<GraphicsCaptureItem, IGraphicsCaptureItemInterop>();
        winrt::check_hresult(factory->CreateForWindow(window, winrt::guid_of<GraphicsCaptureItem>(), winrt::put_abi(item)));
        size = item.Size();
        if (size.Width <= 0 || size.Height <= 0 || size.Width > 16384 || size.Height > 16384)
            throw std::runtime_error("Invalid capture dimensions");
        winrt::check_hresult(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                            D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION,
                            device.put(), nullptr, context.put()));
        const auto dxgi = device.as<IDXGIDevice>();
        winrt::com_ptr<IInspectable> inspectable;
        winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(dxgi.get(), inspectable.put()));
        auto direct = inspectable.as<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>();
        pool = Direct3D11CaptureFramePool::CreateFreeThreaded(direct,
                 winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized, 2, size);
        session = pool.CreateCaptureSession(item);
        session.IsCursorCaptureEnabled(false);
        session.StartCapture();
        std::printf("capture_source=Hookshot_window pid=%u size=%dx%d\n", pid, size.Width, size.Height);
    }
};
WindowCapture::WindowCapture(uint32_t pid) : impl(std::make_unique<Impl>(pid)) {}
WindowCapture::~WindowCapture() = default;
bool WindowCapture::copy_pixels(uint8_t* dst, uint32_t width, uint32_t height, bool bgra) {
    auto& c = *impl;
    if (!IsWindow(c.window)) throw std::runtime_error("Hookshot capture window closed");
    if (IsIconic(c.window)) throw std::runtime_error("Hookshot window minimized; restore it and restart the experiment");
    auto frame = c.pool.TryGetNextFrame();
    if (!frame) return false;
    const auto size = frame.ContentSize();
    if (size.Width != c.size.Width || size.Height != c.size.Height)
        throw std::runtime_error("Hookshot window resized; restart the bounded experiment");
    const auto access = frame.Surface().as<::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
    winrt::com_ptr<ID3D11Texture2D> texture;
    winrt::check_hresult(access->GetInterface(__uuidof(ID3D11Texture2D), texture.put_void()));
    D3D11_TEXTURE2D_DESC desc{};
    texture->GetDesc(&desc);
    if (!c.staging) {
        desc.BindFlags = 0; desc.MiscFlags = 0;
        desc.Usage = D3D11_USAGE_STAGING; desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        winrt::check_hresult(c.device->CreateTexture2D(&desc, nullptr, c.staging.put()));
    }
    c.context->CopyResource(c.staging.get(), texture.get());
    D3D11_MAPPED_SUBRESOURCE map{};
    winrt::check_hresult(c.context->Map(c.staging.get(), 0, D3D11_MAP_READ, 0, &map));
    std::memset(dst, 0, static_cast<size_t>(width) * height * 4);
    const double scale = std::min(double(width) / size.Width, double(height) / size.Height);
    const auto w = std::max(1u, static_cast<uint32_t>(size.Width * scale));
    const auto h = std::max(1u, static_cast<uint32_t>(size.Height * scale));
    const auto x0 = (width - w) / 2, y0 = (height - h) / 2;
    for (uint32_t y = 0; y < h; ++y) {
        const auto* row = static_cast<const uint8_t*>(map.pData) + (uint64_t(y) * size.Height / h) * map.RowPitch;
        for (uint32_t x = 0; x < w; ++x) {
            const auto* src = row + (uint64_t(x) * size.Width / w) * 4;
            auto* pixel = dst + (static_cast<size_t>(y + y0) * width + x + x0) * 4;
            copy_capture_pixel(pixel, src, bgra);
        }
    }
    c.context->Unmap(c.staging.get(), 0);
    return true;
}
