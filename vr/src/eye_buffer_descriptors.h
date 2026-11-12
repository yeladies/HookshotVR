// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "eye_page_layout.h"
#include <vulkan/vulkan.h>
#include <array>
namespace hookshot::vr {
struct EyeDescriptorSource {uint64_t object=0;VkBuffer buffer=VK_NULL_HANDLE;uint64_t bytes=0;VkBufferUsageFlags usage=0;};
struct EyeDescriptorPrivate {VkBuffer buffer=VK_NULL_HANDLE;uint64_t bytes=0;VkBufferUsageFlags usage=0;};
struct EyeDescriptorRequest {
    uint64_t object=0,offset=0,bytes=0;VkDescriptorType type=VK_DESCRIPTOR_TYPE_MAX_ENUM;
    bool writable=false,shared_read_reviewed=false;
};
struct EyeDescriptorLimits {uint64_t uniform_alignment=0,storage_alignment=0,uniform_range=0,storage_range=0;};
struct EyeBufferDescriptors {
    std::array<std::vector<VkDescriptorBufferInfo>,2> eyes;EyePageStamp stamp{};
    bool replay_ready()const{return false;}
};
// Prepare copied binding records only. Native allocation ownership, alias closure and
// truthful access reviews must be established externally; no descriptors are pushed here.
inline bool prepare_eye_buffer_descriptors(const EyeBufferLayout& layout,const std::vector<EyeDescriptorSource>& sources,
    const std::vector<EyeDescriptorPrivate>& private_buffers,const std::vector<EyeDescriptorRequest>& requests,
    const EyePageStamp& stamp,const EyePageStamp& current,const EyeDescriptorLimits& limits,EyeBufferDescriptors& out){
    auto alignment=[](uint64_t n){return n&&!(n&(n-1));};
    if(!eye_page_stamp_matches(stamp,current)||sources.size()>256||requests.size()>1024||
        !alignment(limits.uniform_alignment)||!alignment(limits.storage_alignment)||!limits.uniform_range||!limits.storage_range||
        private_buffers.size()!=2*layout.allocations_per_eye().size())return false;
    for(size_t i=0;i<sources.size();++i){const auto& s=sources[i];if(!s.object||!s.buffer||!s.bytes)return false;
        for(size_t j=0;j<i;++j)if(s.object==sources[j].object||s.buffer==sources[j].buffer)return false;
    }
    for(size_t i=0;i<private_buffers.size();++i){const auto& p=private_buffers[i];
        if(!p.buffer||p.bytes<layout.allocations_per_eye()[i%layout.allocations_per_eye().size()].bytes)return false;
        for(const auto& s:sources)if(p.buffer==s.buffer)return false;
        for(size_t j=0;j<i;++j)if(p.buffer==private_buffers[j].buffer)return false;
    }
    EyeBufferDescriptors next;next.stamp=stamp;
    for(const auto& request:requests){
        const bool uniform=request.type==VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,storage=request.type==VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        if((!uniform&&!storage)||(uniform&&request.writable)||!request.bytes||request.bytes==VK_WHOLE_SIZE)return false;
        const auto usage=uniform?VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT:VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        const uint64_t align=uniform?limits.uniform_alignment:limits.storage_alignment,max_range=uniform?limits.uniform_range:limits.storage_range;
        auto source=std::find_if(sources.begin(),sources.end(),[&](const auto& s){return s.object==request.object;});
        if(source==sources.end()||!(source->usage&usage)||request.offset>source->bytes||request.bytes>source->bytes-request.offset||request.bytes>max_range)return false;
        for(unsigned eye=0;eye<2;++eye){auto route=layout.resolve(eye,request.object,request.offset,request.bytes,request.writable);
            VkDescriptorBufferInfo binding{};
            if(route.route==EyeBufferRoute::Private){
                if(route.allocation>=private_buffers.size())return false;const auto& p=private_buffers[route.allocation];
                if(!(p.usage&usage)||route.offset>p.bytes||request.bytes>p.bytes-route.offset)return false;
                binding={p.buffer,route.offset,request.bytes};
            }else if(route.route==EyeBufferRoute::SharedReadOnly&&request.shared_read_reviewed)binding={source->buffer,request.offset,request.bytes};
            else return false;
            if(binding.offset%align)return false;
            next.eyes[eye].push_back(binding);
        }
    }
    out=std::move(next);return true;
}
}
