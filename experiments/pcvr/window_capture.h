#pragma once
#include <cstdint>
#include <memory>

// Windows Graphics Capture targets one explicitly selected Hookshot process/window.
// Never captures a monitor; does not store images or send them over a network.
class WindowCapture {
public:
    explicit WindowCapture(uint32_t pid);
    ~WindowCapture();
    WindowCapture(const WindowCapture&) = delete;
    WindowCapture& operator=(const WindowCapture&) = delete;
    bool copy_pixels(uint8_t* dst, uint32_t width, uint32_t height, bool bgra);
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
