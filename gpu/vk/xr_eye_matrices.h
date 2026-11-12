// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "xr_camera_view.h"
#include "xr_projection_math.h"
namespace hookshot::vr {
struct EyeMatrices {CameraView view;ProjectionMatrix projection{},world_to_clip{};};
}
