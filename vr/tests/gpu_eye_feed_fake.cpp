// SPDX-License-Identifier: GPL-3.0-or-later
// Authored metadata/opaque buffer only. Vulkan/XR are supplied by the fake loader.
#include "xr_gpu_eye_stream.h"
#include <mutex>
#include <cstring>
#include <cstdlib>
#include <string>
namespace mkwvk {
namespace {std::mutex feed_mutex;std::shared_ptr<const XrGpuEyeFrame> owner;std::weak_ptr<const XrGpuEyeFrame> observed;bool disabled=false;
bool feed_mode(const char* suffix){const auto* mode=std::getenv("XR_FAKE_MODE");return mode&&std::string(mode)==std::string("preview_pair_")+suffix;}}
void gpu_test_feed_drop_owner(){std::lock_guard lock(feed_mutex);disabled=true;owner.reset();}
bool gpu_test_feed_alive(){std::lock_guard lock(feed_mutex);return !observed.expired();}
std::shared_ptr<const XrGpuEyeFrame> xr_gpu_eye_stream_acquire(VkDevice device,uint64_t origin,int64_t now,bool){
    std::lock_guard lock(feed_mutex);if(disabled||feed_mode("missing"))return {};if(!owner){auto f=std::make_shared<XrGpuEyeFrame>();f->device=device;f->buffer=reinterpret_cast<VkBuffer>(uintptr_t(900));f->bytes=512;f->state=1;f->source_state=11;f->generation=37;f->recording=17;f->width=f->height=8;f->queue_family=7;f->source_observed_ns=now-20000000;f->target.observed_ns=now-10000000;f->ready_ns=now-1000000;f->target.origin=origin;f->target.serial=13;
        for(unsigned eye=0;eye<2;++eye){auto& v=f->target.eyes[eye];v.orientation_valid=v.position_valid=true;v.orientation={0,0,0,1};v.position={eye?10.032f:9.968f,20,30};v.fov={-.72f,.78f,.88f,-.62f};}
        if(feed_mode("stale"))f->source_observed_ns=now-400000000;
        if(feed_mode("synthetic"))f->synthetic=true;if(feed_mode("bad_eye"))f->target.eyes[1].orientation={0,0,0,0};if(feed_mode("wrong_origin"))f->target.origin=origin+1;if(feed_mode("wrong_device"))f->device=reinterpret_cast<VkDevice>(uintptr_t(999));if(feed_mode("wrong_size"))f->width=16;
        owner=f;observed=f;
    }return owner;
}
}
