// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <cstddef>
#include <utility>
#include <cstdint>
#include <vector>
namespace hookshot::vr {
struct EyeBufferRegion {uint64_t object=0,offset=0,size=0;};
struct EyeBufferAllocation {EyeBufferRegion original;uint64_t prefix=0,bytes=0;};
enum class EyeBufferRoute { Reject, SharedReadOnly, Private };
struct EyeBufferBinding {EyeBufferRoute route=EyeBufferRoute::Reject;size_t allocation=0;uint64_t offset=0,size=0;};
// Address layout only. The owner must allocate separate native buffers for every allocation index.
class EyeBufferLayout {
    std::vector<EyeBufferAllocation> regions_;
public:
    static bool build(std::vector<EyeBufferRegion> regions,uint64_t alignment,EyeBufferLayout& out) {
        if(!alignment||(alignment&(alignment-1))||regions.size()>256)return false;
        std::sort(regions.begin(),regions.end(),[](auto a,auto b){return a.object!=b.object?a.object<b.object:a.offset<b.offset;});
        EyeBufferLayout next;uint64_t previous_object=0,previous_end=0;
        for(auto r:regions) {
            if(!r.object||!r.size||r.offset>UINT64_MAX-r.size)return false;
            if(previous_object==r.object&&r.offset<previous_end)return false;
            previous_object=r.object;previous_end=r.offset+r.size;
            const uint64_t prefix=r.offset&(alignment-1);
            if(r.size>UINT64_MAX-prefix)return false;
            const uint64_t needed=prefix+r.size;
            if(needed>UINT64_MAX-(alignment-1))return false;
            const uint64_t bytes=(needed+alignment-1)&~(alignment-1);
            next.regions_.push_back({r,prefix,bytes});
        }
        out=std::move(next);return true;
    }
    EyeBufferBinding resolve(unsigned eye,uint64_t object,uint64_t offset,uint64_t size,bool written)const {
        if(eye>1||!object||!size||offset>UINT64_MAX-size)return {};
        const uint64_t end=offset+size;
        for(size_t i=0;i<regions_.size();++i) {
            const auto& r=regions_[i];if(object!=r.original.object)continue;
            const uint64_t stop=r.original.offset+r.original.size;
            if(offset>=stop||r.original.offset>=end)continue;
            if(offset<r.original.offset||end>stop)return {}; // Never borrow the original for a partially covered range.
            return {EyeBufferRoute::Private,eye*regions_.size()+i,r.prefix+offset-r.original.offset,size};
        }
        if(written)return {}; // A per-eye write without a reviewed private range cannot use shared storage.
        return {EyeBufferRoute::SharedReadOnly,0,offset,size};
    }
    const std::vector<EyeBufferAllocation>& allocations_per_eye()const{return regions_;}
};
}
