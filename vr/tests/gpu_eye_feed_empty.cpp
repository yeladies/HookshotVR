// SPDX-License-Identifier: GPL-3.0-or-later
#include "xr_gpu_eye_stream.h"
namespace mkwvk {
std::shared_ptr<const XrGpuEyeFrame> xr_gpu_eye_stream_acquire(VkDevice,uint64_t,int64_t,bool){return {};}
}

#include "xr_gpu_scene_source.h"
namespace mkwvk {std::shared_ptr<const XrGpuSceneSource> xr_gpu_scene_source_acquire(VkDevice,uint64_t,int64_t,bool,bool){return {};}}
