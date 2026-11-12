// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "eye_page_layout.h"
#include <array>
namespace hookshot::vr {
struct EyePageDestination {uint64_t address=0,bytes=0;};
struct EyePagePatch {uint32_t index=0;uint64_t address=0;};
struct EyePagePatchSet {
    std::array<std::vector<EyePagePatch>,2> eyes;
    EyePageStamp stamp{};
    bool replay_ready()const{return false;}
};
// CPU encoding only. Caller supplies a frozen original root snapshot and actual
// allocated buffer addresses in EyeBufferLayout allocation-index order.
inline bool prepare_eye_page_patches(const EyeGuestPagePlan& plan,const EyePageStamp& current,
                                    const uint64_t* root,size_t root_count,
                                    const std::vector<EyePageDestination>& destinations,
                                    EyePagePatchSet& out) {
    if(!eye_page_stamp_matches(plan.stamp(),current)||!root||root_count!=kEyeGuestPageCount)return false;
    const auto& allocations=plan.buffers().allocations_per_eye();
    if(destinations.size()!=2*allocations.size())return false;
    // Index the snapshot once; avoid scanning a 24 MiB root for every allocation.
    std::vector<uint64_t> source_pages;
    for(size_t i=0;i<root_count;++i)if(root[i]){
        if(root[i]>UINT64_MAX-kEyeGuestPageBytes)return false;
        source_pages.push_back(root[i]);
    }
    std::sort(source_pages.begin(),source_pages.end());
    source_pages.erase(std::unique(source_pages.begin(),source_pages.end()),source_pages.end());
    auto overlap=[](uint64_t a,uint64_t bytes,uint64_t b,uint64_t size){return a<b+size&&b<a+bytes;};
    for(size_t i=0;i<destinations.size();++i){const auto& d=destinations[i];
        if(!d.address||d.address%16||!d.bytes||d.address>UINT64_MAX-d.bytes||
           d.bytes<allocations[i%allocations.size()].bytes)return false;
        for(size_t j=0;j<i;++j)if(overlap(d.address,d.bytes,destinations[j].address,destinations[j].bytes))return false;
        for(const auto& object:plan.objects())if(object.second.device_address&&
            overlap(d.address,d.bytes,object.second.device_address,object.second.bytes))return false;
        // Also protect root entries omitted from the supplied mapping inventory.
        auto after=std::lower_bound(source_pages.begin(),source_pages.end(),d.address+d.bytes);
        if(after!=source_pages.begin()&&*(after-1)+kEyeGuestPageBytes>d.address)return false;
    }
    EyePagePatchSet next;next.stamp=current;
    for(const auto& page:plan.pages())for(unsigned eye=0;eye<2;++eye){
        auto route=plan.resolve(eye,page.guest_address,false);
        if(route.route==EyePageRoute::Reject||root[route.table_index]!=route.original_address)return false;
        if(route.route==EyePageRoute::SharedReadOnlyCandidate)continue;
        if(route.allocation>=destinations.size())return false;
        const auto& d=destinations[route.allocation];
        if(route.offset>d.bytes||kEyeGuestPageBytes>d.bytes-route.offset)return false;
        next.eyes[eye].push_back({route.table_index,d.address+route.offset});
    }
    out=std::move(next);return true;
}
}
