// SPDX-License-Identifier: GPL-3.0-or-later
// Authored metadata/opaque buffer only. Vulkan/XR are supplied by the fake loader.
#include "xr_gpu_eye_stream.h"
#include <mutex>
#include <cstring>
#include <cstdlib>
#include <string>
namespace mkwvk {
std::shared_ptr<const void> late_owner;std::weak_ptr<const void> late_observed;
namespace {std::mutex feed_mutex;std::shared_ptr<const XrGpuEyeFrame> owner;std::weak_ptr<const XrGpuEyeFrame> observed;bool disabled=false;
bool feed_mode(const char* suffix){const auto* mode=std::getenv("XR_FAKE_MODE");return mode&&std::string(mode)==std::string("preview_pair_")+suffix;}}
void gpu_test_feed_drop_owner(){std::lock_guard lock(feed_mutex);disabled=true;owner.reset();late_owner.reset();}
bool gpu_test_feed_alive(){std::lock_guard lock(feed_mutex);return !observed.expired()||!late_observed.expired();}
std::shared_ptr<const XrGpuEyeFrame> xr_gpu_eye_stream_acquire(VkDevice device,uint64_t origin,int64_t now,bool){
    std::lock_guard lock(feed_mutex);if(disabled||feed_mode("missing"))return {};if(!owner){auto f=std::make_shared<XrGpuEyeFrame>();f->device=device;f->buffer=reinterpret_cast<VkBuffer>(uintptr_t(900));f->bytes=512;f->state=1;f->source_state=11;f->generation=37;f->recording=17;f->width=f->height=8;f->queue_family=7;f->source_observed_ns=now-20000000;f->target.observed_ns=now-10000000;f->ready_ns=now-1000000;f->target.origin=origin;f->target.serial=13;
        for(unsigned eye=0;eye<2;++eye){auto& v=f->target.eyes[eye];v.orientation_valid=v.position_valid=true;v.orientation={0,0,0,1};v.position={eye?10.032f:9.968f,20,30};v.fov={-.72f,.78f,.88f,-.62f};}
        if(feed_mode("stale"))f->source_observed_ns=now-400000000;
        if(feed_mode("synthetic"))f->synthetic=true;if(feed_mode("bad_eye"))f->target.eyes[1].orientation={0,0,0,0};if(feed_mode("wrong_origin"))f->target.origin=origin+1;if(feed_mode("wrong_device"))f->device=reinterpret_cast<VkDevice>(uintptr_t(999));if(feed_mode("wrong_size"))f->width=16;
        owner=f;observed=f;
    }return owner;
}
}

#include "xr_gpu_scene_source.h"
namespace mkwvk {std::shared_ptr<const XrGpuSceneSource> xr_gpu_scene_source_acquire(VkDevice device,uint64_t origin,int64_t now,bool,bool){
    std::lock_guard lock(feed_mutex);const char* mode=std::getenv("XR_FAKE_MODE");const std::string name=mode?mode:"";auto is=[&](const char* suffix){return name==std::string("preview_pair_late_")+suffix;};
    if(disabled||is("missing"))return {};if(!late_owner){auto f=std::make_shared<XrGpuSceneSource>();f->device=device;f->buffer=reinterpret_cast<VkBuffer>(uintptr_t(901));f->address=0x20000;f->bytes=512;f->format=123;f->state=11;f->generation=37;f->recording=17;f->width=f->height=8;f->queue_family=7;f->ready_ns=now-1000000;f->pose.valid=true;f->pose.generation=origin;f->pose.observed_ns=now-20000000;f->projection[0]=f->projection[5]=.5f;f->projection[10]=-1.01f;f->projection[11]=-.2f;
    if(is("stale"))f->pose.observed_ns=now-400000000;if(is("synthetic"))f->synthetic_pose=true;if(is("wrong_origin"))f->pose.generation=origin+1;if(is("wrong_device"))f->device=reinterpret_cast<VkDevice>(uintptr_t(999));if(is("wrong_size"))f->width=16;late_owner=f;late_observed=f;}
    if(is("generation_change")){static unsigned calls=0;if(++calls==2){auto newer=std::make_shared<XrGpuSceneSource>(*std::static_pointer_cast<const XrGpuSceneSource>(late_owner));newer->generation=38;late_owner=newer;late_observed=newer;}}
    return std::static_pointer_cast<const XrGpuSceneSource>(late_owner);
}}
