// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cstdint>
namespace hookshot::vr {
struct EyeRootAliasParameters {
    uint64_t source=0,destination=0,source_pages=0,private_pages=0,counters=0;
    uint32_t count=0,page_bytes=0,mode=0,reserved=0,extra0=0,extra1=0;
};
static_assert(sizeof(EyeRootAliasParameters)==64);
// Address/extent validation only. Callers must prove these are actual live GPU
// allocations from the same captured generation and synchronize their queue use.
inline bool eye_root_alias_parameters_valid(const EyeRootAliasParameters& p,uint64_t source_bytes,uint64_t destination_bytes,
        uint64_t source_page_bytes,uint64_t private_page_bytes,uint64_t counter_bytes){
    if(!p.count||p.count>3145728||!p.page_bytes||p.page_bytes>64u*1024*1024||p.page_bytes%4096||p.mode>1||p.reserved||p.extra0||p.extra1||
       source_bytes<uint64_t(p.count)*8||destination_bytes<uint64_t(p.count)*8||source_page_bytes<p.page_bytes||private_page_bytes<p.page_bytes||counter_bytes<16)return false;
    const std::array<uint64_t,5> starts{p.source,p.destination,p.source_pages,p.private_pages,p.counters};
    const std::array<uint64_t,5> sizes{uint64_t(p.count)*8,uint64_t(p.count)*8,p.page_bytes,p.page_bytes,16};
    for(unsigned i=0;i<5;++i){if(!starts[i]||starts[i]%16||starts[i]>UINT64_MAX-sizes[i])return false;
        for(unsigned j=0;j<i;++j)if(starts[i]<starts[j]+sizes[j]&&starts[j]<starts[i]+sizes[i])return false;}
    return true;
}
// Mode 2 reads a guest camera only through entries inside owned private pages.
// Any missing/uncovered mapping writes zero and increments the rejection counter.
inline bool eye_root_lookup_parameters_valid(const EyeRootAliasParameters& p,uint64_t root_bytes,uint64_t output_bytes,uint64_t private_bytes,uint64_t counter_bytes){
    if(p.mode!=2||p.extra0||p.extra1||!p.reserved||p.reserved>65536||p.reserved%4||!p.count||p.count>3145728||
       !p.page_bytes||p.page_bytes>64u*1024*1024||p.page_bytes%4096||root_bytes<uint64_t(p.count)*8||output_bytes<p.reserved||private_bytes<p.page_bytes||counter_bytes<16||
       !p.source_pages||p.source_pages%4||p.source_pages>UINT64_MAX-p.reserved)return false;
    const auto end=p.source_pages+p.reserved;
    if(!((p.source_pages>=0x1000000000ull&&end<=0x1200000000ull)||(p.source_pages>=0x7f00000000ull&&end<=0x8000000000ull)))return false;
    const auto last=end-1;
    const uint64_t last_index=last>=0x7f00000000ull?2097152ull+((last-0x7f00000000ull)>>12):((last-0x1000000000ull)>>12);
    if(last_index>=p.count)return false;
    const std::array<uint64_t,4> starts{p.source,p.destination,p.private_pages,p.counters};
    const std::array<uint64_t,4> sizes{uint64_t(p.count)*8,p.reserved,p.page_bytes,16};
    for(unsigned i=0;i<4;++i){if(!starts[i]||starts[i]%16||starts[i]>UINT64_MAX-sizes[i])return false;
        for(unsigned j=0;j<i;++j)if(starts[i]<starts[j]+sizes[j]&&starts[j]<starts[i]+sizes[i])return false;}
    return true;
}

// Mode 3 writes a zero-count indirect command unless all GPU alias counters and
// every camera byte agree. private_pages packs first and vertex_offset here;
// extra0 is first_instance (zero only), extra1 packs minimum_pages and indexed.
inline bool eye_root_guard_parameters_valid(const EyeRootAliasParameters& p,uint64_t matrix_bytes,uint64_t lookup_bytes,uint64_t indirect_bytes,uint64_t counter_bytes){
    const auto minimum_pages=p.extra1>>1;
    if(p.mode!=3||p.reserved<560||p.reserved>65536||p.reserved%4||!p.count||p.count>1048576||!p.page_bytes||p.page_bytes>1024||uint64_t(p.count)*p.page_bytes>1048576||
       p.extra0||!minimum_pages||minimum_pages>16||uint64_t(uint32_t(p.private_pages))+p.count>UINT32_MAX||
       matrix_bytes<p.reserved||lookup_bytes<p.reserved||indirect_bytes<20||counter_bytes<16)return false;
    const std::array<uint64_t,4> starts{p.source,p.destination,p.source_pages,p.counters};
    const std::array<uint64_t,4> sizes{p.reserved,20,p.reserved,16};
    for(unsigned i=0;i<4;++i){if(!starts[i]||starts[i]%16||starts[i]>UINT64_MAX-sizes[i])return false;
        for(unsigned j=0;j<i;++j)if(starts[i]<starts[j]+sizes[j]&&starts[j]<starts[i]+sizes[i])return false;}
    return true;
}

}
