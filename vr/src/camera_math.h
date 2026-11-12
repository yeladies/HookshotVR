// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <openxr/openxr.h>
#include <cmath>

namespace hookshot::vr {
// Explicit mapping chosen by the caller, not a measured Hookshot calibration.
// Both spaces are right-handed, +Y up, looking along -Z at identity.
struct TrackingOrigin {
    double world_position[3]{};
    XrVector3f local_reference{};
    double yaw_radians = 0;
    double world_units_per_metre = 1;
};
struct CameraView {
    double eye[3]{};
    // Row-major world-to-eye affine matrix: right, up, backward, translation.
    double rows[3][4]{};
};
inline bool tracked_camera_view(const TrackingOrigin& origin, const XrPosef& pose,
                                XrSpaceLocationFlags flags, CameraView& out) {
    constexpr auto needed = XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
    if ((flags & needed) != needed) return false;
    const auto q = pose.orientation;
    const double norm = double(q.x)*q.x + double(q.y)*q.y + double(q.z)*q.z + double(q.w)*q.w;
    if (!std::isfinite(norm) || std::abs(norm-1) > 0.001 ||
        !std::isfinite(origin.yaw_radians) || !std::isfinite(origin.world_units_per_metre) ||
        origin.world_units_per_metre <= 0) return false;
    const double inv_norm = 1/std::sqrt(norm);
    const double x=q.x*inv_norm, y=q.y*inv_norm, z=q.z*inv_norm, w=q.w*inv_norm;
    const double local[3] = {double(pose.position.x)-origin.local_reference.x,
                             double(pose.position.y)-origin.local_reference.y,
                             double(pose.position.z)-origin.local_reference.z};
    const double sy = std::sin(origin.yaw_radians), cy = std::cos(origin.yaw_radians);
    CameraView candidate{};
    const double rotated[3] = {cy*local[0]+sy*local[2], local[1], -sy*local[0]+cy*local[2]};
    for (int i=0; i<3; ++i) {
        candidate.eye[i] = origin.world_position[i] + origin.world_units_per_metre*rotated[i];
        if (!std::isfinite(candidate.eye[i])) return false;
    }
    // Columns of the pose's local-to-tracking rotation are the eye's basis vectors.
    const double basis[3][3] = {
        {1-2*(y*y+z*z), 2*(x*y+w*z), 2*(x*z-w*y)},
        {2*(x*y-w*z), 1-2*(x*x+z*z), 2*(y*z+w*x)},
        {2*(x*z+w*y), 2*(y*z-w*x), 1-2*(x*x+y*y)}
    };
    for (int row=0; row<3; ++row) {
        candidate.rows[row][0] = cy*basis[row][0]+sy*basis[row][2];
        candidate.rows[row][1] = basis[row][1];
        candidate.rows[row][2] = -sy*basis[row][0]+cy*basis[row][2];
        for (int col=0; col<3; ++col)
            candidate.rows[row][3] -= candidate.rows[row][col]*candidate.eye[col];
        if (!std::isfinite(candidate.rows[row][3])) return false;
    }
    out = candidate; // Invalid samples leave the caller's last value untouched.
    return true;
}
} // namespace hookshot::vr