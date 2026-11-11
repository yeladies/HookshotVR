// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <openxr/openxr.h>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace hookshot::vr {
struct MarkerRect { int32_t x, y; uint32_t width, height; float distance; };
// Project world-space billboards into one eye. XR looks down -Z, framebuffer Y points down.
inline bool marker_rect(const XrPosef& eye, const XrFovf& fov, XrVector3f world, float radius,
                        uint32_t width, uint32_t height, MarkerRect& out) {
    const auto q = eye.orientation;
    const float norm = q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w;
    if (!width || !height || !std::isfinite(norm) || std::abs(norm - 1.0f) > 0.01f ||
        !std::isfinite(radius) || radius <= 0) return false;
    const XrVector3f v{world.x-eye.position.x, world.y-eye.position.y, world.z-eye.position.z};
    // q^-1 * v * q, expanded without a matrix convention dependency.
    const XrVector3f t{2*(-q.y*v.z+q.z*v.y), 2*(-q.z*v.x+q.x*v.z), 2*(-q.x*v.y+q.y*v.x)};
    const XrVector3f p{v.x+q.w*t.x-q.y*t.z+q.z*t.y,
                      v.y+q.w*t.y-q.z*t.x+q.x*t.z,
                      v.z+q.w*t.z-q.x*t.y+q.y*t.x};
    const float d = -p.z;
    const float left=std::tan(fov.angleLeft), right=std::tan(fov.angleRight);
    const float down=std::tan(fov.angleDown), up=std::tan(fov.angleUp);
    if (!std::isfinite(d) || !std::isfinite(p.x) || !std::isfinite(p.y) || d <= 0.08f ||
        !std::isfinite(left) || !std::isfinite(right) || !std::isfinite(down) || !std::isfinite(up) ||
        right <= left || up <= down) return false;
    const float x = (p.x/d-left)/(right-left), y=(up-p.y/d)/(up-down);
    const float rx=radius/d/(right-left), ry=radius/d/(up-down);
    const float x0=std::clamp(x-rx, 0.f, 1.f), x1=std::clamp(x+rx, 0.f, 1.f);
    const float y0=std::clamp(y-ry, 0.f, 1.f), y1=std::clamp(y+ry, 0.f, 1.f);
    if (x1 <= x0 || y1 <= y0) return false;
    const auto ix0=static_cast<uint32_t>(x0*width), iy0=static_cast<uint32_t>(y0*height);
    const auto ix1=std::min(width, static_cast<uint32_t>(std::ceil(x1*width)));
    const auto iy1=std::min(height, static_cast<uint32_t>(std::ceil(y1*height)));
    if (ix1 <= ix0 || iy1 <= iy0) return false;
    out={static_cast<int32_t>(ix0),static_cast<int32_t>(iy0),ix1-ix0,iy1-iy0,d};
    return true;
}
inline bool pose_valid(XrSpaceLocationFlags flags) {
    constexpr XrSpaceLocationFlags need=XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
    return (flags&need)==need;
}
} // namespace hookshot::vr
