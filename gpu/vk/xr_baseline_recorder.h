// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "xr_eye_image_factory.h"
#include "xr_descriptor_snapshot.h"
#include "xr_baseline_attempt.h"
#include <vector>
namespace mkwvk {
struct XrBaselineCameraSlot {uint32_t binding=0,element=0;};
struct XrBaselineGeometry {
    bool indexed=false;uint32_t count=0,instances=0,first=0,first_instance=0;int32_t vertex_offset=0;
};
// Internal recorder only. The caller must first prove current-process reviewed stage
// identities, direct geometry, exact target views and no feedback/hidden side effects.
// It must keep all original bindings alive on the ordinary recording timeline.
struct XrBaselineRecorderInput {
    uint64_t serial=0,recording=0,program=0;VkCommandBuffer command=VK_NULL_HANDLE;
    VkPipelineLayout layout=VK_NULL_HANDLE;EyeImageSource color{},depth{};
    VkDescriptorBufferInfo camera{};
    std::vector<XrBaselineCameraSlot> camera_slots;
    std::vector<VkWriteDescriptorSet> writes;
    XrBaselineGeometry geometry{};bool matrix_probe=false;
};
struct XrBaselineRecorder;
// Allocation/snapshot only: failure records no commands and leaves source state intact.
XrBaselineRecorder* xr_baseline_recorder_create(const XrBaselineRecorderInput&);
// Ends the previous ordinary pass and snapshots private images/uniforms. No draw yet.
bool xr_baseline_recorder_prepare(XrBaselineRecorder*);
// Called after the exact ordinary source draw; it renders two unchanged private copies
// and explicitly restores the complete original pushed set. Never calls draw().
bool xr_baseline_recorder_finish(XrBaselineRecorder*,uint64_t serial,uint64_t recording,uint64_t program,
                                VkCommandBuffer,const std::vector<VkWriteDescriptorSet>& live_writes);
// A skipped/changed source must abandon before another draw uses this recorder.
void xr_baseline_recorder_abandon(XrBaselineRecorder*);
// No wait. Returns false while in flight or on unknown/device-changed completion.
// On true all private allocations are retired and the pointer is cleared.
bool xr_baseline_recorder_retire(XrBaselineRecorder*&,XrBaselineComparison&);
}
