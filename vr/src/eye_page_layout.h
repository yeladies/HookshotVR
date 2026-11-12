// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "eye_resource_layout.h"
#include <map>
#include <iterator>
#include <optional>
namespace hookshot::vr {
constexpr uint64_t kEyeGuestPageBytes=4096;
constexpr uint64_t kEyeGuestPageCount=(12ull<<30)/kEyeGuestPageBytes;
constexpr uint64_t kEyeGuestTableBytes=kEyeGuestPageCount*sizeof(uint64_t);
// Current backend topology; changes to memory/bank/SPIR-V contracts require review.
inline std::optional<uint32_t> eye_guest_page_index(uint64_t address) {
    if(address&(kEyeGuestPageBytes-1))return {};
    if(address>=0x1000000000ull&&address<0x1000000000ull+(8ull<<30))return uint32_t((address-0x1000000000ull)>>12);
    if(address>=0x7f00000000ull&&address<0x7f00000000ull+(4ull<<30))return uint32_t((8ull<<30>>12)+((address-0x7f00000000ull)>>12));
    return {};
}
struct EyePageStamp {uint64_t session=0,submit=0,pool_generation=0,capture_generation=0;};
inline bool eye_page_stamp_matches(const EyePageStamp& a,const EyePageStamp& b){return a.session&&a.submit&&a.pool_generation&&a.capture_generation&&
    a.session==b.session&&a.submit==b.submit&&a.pool_generation==b.pool_generation&&a.capture_generation==b.capture_generation;}
struct EyePageObject {uint64_t object=0,bytes=0,device_address=0;};
struct EyeMappedGuestPage {uint64_t guest_address=0,object=0,offset=0;};
enum class EyePageRoute {Reject,SharedReadOnlyCandidate,PrivateCandidate};
struct EyePageBinding {EyePageRoute route=EyePageRoute::Reject;uint32_t table_index=0;size_t allocation=0;uint64_t offset=0,original_address=0;};
// Diagnostic planning only. Input must describe the same immutable source capture.
// This computes closure for supplied mappings/read ranges, not unknown shader writes
// or omitted aliases. It never allocates, patches a live page table, or authorizes replay.
class EyeGuestPagePlan {
    EyeBufferLayout layout_;std::map<uint64_t,EyePageObject> objects_;
    std::vector<EyeMappedGuestPage> pages_;EyePageStamp stamp_{};uint64_t bytes_=0;
public:
    static bool build(const std::vector<EyeBufferRegion>& writes,const std::vector<EyeBufferRegion>& reads,
                      std::vector<EyeMappedGuestPage> pages,const std::vector<EyePageObject>& objects,
                      const EyePageStamp& stamp,const EyePageStamp& expected,uint64_t alignment,uint64_t budget,
                      EyeGuestPagePlan& out) {
        if(!eye_page_stamp_matches(stamp,expected)||writes.size()>65536||reads.size()>65536||pages.size()>131072||objects.size()>256||
           !alignment||(alignment&(alignment-1))||alignment<16||budget>512ull*1024*1024)return false;
        EyeGuestPagePlan candidate;candidate.stamp_=stamp;
        for(auto object:objects){if(!object.object||!object.bytes||object.device_address>UINT64_MAX-object.bytes||
            (object.device_address&&object.device_address%16)||!candidate.objects_.emplace(object.object,object).second)return false;}
        // Different identities with overlapping native addresses need explicit canonicalization.
        // Reject them instead of silently treating their writes as independent storage.
        for(auto a=candidate.objects_.begin();a!=candidate.objects_.end();++a)
            for(auto b=std::next(a);b!=candidate.objects_.end();++b)
                if(a->second.device_address&&b->second.device_address&&
                   a->second.device_address<b->second.device_address+b->second.bytes&&
                   b->second.device_address<a->second.device_address+a->second.bytes)return false;
        struct Span {uint64_t object,first,end;bool selected;};std::vector<Span> spans;spans.reserve(writes.size()+reads.size()+pages.size());
        auto add=[&](EyeBufferRegion r,bool selected){auto object=candidate.objects_.find(r.object);
            if(object==candidate.objects_.end()||!r.size||r.offset>object->second.bytes||r.size>object->second.bytes-r.offset)return false;
            spans.push_back({r.object,r.offset,r.offset+r.size,selected});return true;};
        for(auto r:writes)if(!add(r,true))return false;
        for(auto r:reads)if(!add(r,false))return false;
        std::sort(pages.begin(),pages.end(),[](auto a,auto b){return a.guest_address<b.guest_address;});
        for(size_t i=0;i<pages.size();++i){const auto& page=pages[i];auto object=candidate.objects_.find(page.object);
            if(!eye_guest_page_index(page.guest_address)||(i&&pages[i-1].guest_address==page.guest_address)||
               object==candidate.objects_.end()||!object->second.device_address||page.offset%16||
               !add({page.object,page.offset,kEyeGuestPageBytes},false))return false;
        }
        std::sort(spans.begin(),spans.end(),[](auto a,auto b){return a.object!=b.object?a.object<b.object:a.first!=b.first?a.first<b.first:a.end<b.end;});
        std::vector<EyeBufferRegion> private_regions;
        auto keep=[&](Span span){if(!span.selected)return;
            if(!private_regions.empty()){auto& previous=private_regions.back();
                if(previous.object==span.object&&previous.offset+previous.size==span.first){previous.size=span.end-previous.offset;return;}}
            private_regions.push_back({span.object,span.first,span.end-span.first});};
        if(!spans.empty()){auto current=spans.front();for(size_t i=1;i<spans.size();++i){auto next=spans[i];
            // Adjacency alone is not aliasing: do not promote every page of a pool.
            if(next.object==current.object&&next.first<current.end){current.end=std::max(current.end,next.end);current.selected|=next.selected;}
            else {keep(current);current=next;}}
            keep(current);
        }
        if(!EyeBufferLayout::build(std::move(private_regions),alignment,candidate.layout_))return false;
        for(const auto& allocation:candidate.layout_.allocations_per_eye()){
            if(allocation.bytes>budget-candidate.bytes_)return false;candidate.bytes_+=allocation.bytes;
        }
        candidate.pages_=std::move(pages);out=std::move(candidate);return true;
    }
    EyePageBinding resolve(unsigned eye,uint64_t guest_page,bool written)const {
        if(eye>1)return {};
        auto page=std::lower_bound(pages_.begin(),pages_.end(),guest_page,[](auto a,uint64_t address){return a.guest_address<address;});
        if(page==pages_.end()||page->guest_address!=guest_page)return {};
        const auto source=objects_.find(page->object);if(source==objects_.end())return {};
        auto binding=layout_.resolve(eye,page->object,page->offset,kEyeGuestPageBytes,written);
        if(binding.route==EyeBufferRoute::Reject)return {};
        return {binding.route==EyeBufferRoute::Private?EyePageRoute::PrivateCandidate:EyePageRoute::SharedReadOnlyCandidate,
                *eye_guest_page_index(guest_page),binding.allocation,binding.offset,source->second.device_address+page->offset};
    }
    const EyeBufferLayout& buffers()const{return layout_;}
    uint64_t bytes_per_eye_reference()const{return bytes_;}
    const EyePageStamp& stamp()const{return stamp_;}
    bool replay_ready()const{return false;}
};
}
