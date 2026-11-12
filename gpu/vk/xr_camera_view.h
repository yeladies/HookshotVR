// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
namespace hookshot::vr {
struct CameraView {
    double eye[3]{};
    // Row-major world-to-eye affine matrix: right, up, backward, translation.
    double rows[3][4]{};
};
}
