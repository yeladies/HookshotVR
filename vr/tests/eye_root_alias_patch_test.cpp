// SPDX-License-Identifier: GPL-3.0-or-later
#include "eye_root_alias_patch.h"
#include <stdexcept>
#include <iostream>
using namespace hookshot::vr;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(){try{
    EyeRootAliasParameters p{0x1000,0x2000,0x10000,0x20000,0x30000,8,4096,0,0};
    auto valid=[&](auto v){return eye_root_alias_parameters_valid(v,64,64,8192,4096,16);};
    check(valid(p),"bounded disjoint patch ranges");auto q=p;q.mode=1;check(valid(q),"verification has same resource contract");
    for(int i=0;i<9;++i){q=p;if(i==0)q.source=0;if(i==1)q.destination=q.source;if(i==2)q.private_pages=q.source_pages;if(i==3)q.counters=q.destination;if(i==4)q.source++;if(i==5)q.count=9;if(i==6)q.page_bytes=4097;if(i==7)q.reserved=1;if(i==8)q.mode=2;check(!valid(q),"invalid patch contract rejected before recording");}
    q=p;q.source=UINT64_MAX-15;check(!valid(q),"overflowing physical range rejected");
    check(!eye_root_alias_parameters_valid(p,63,64,8192,4096,16),"short original table");
    check(!eye_root_alias_parameters_valid(p,64,64,8192,4095,16),"short private pages");
    check(!eye_root_alias_parameters_valid(p,64,64,8192,4096,15),"short counter region");
    EyeRootAliasParameters lookup{0x1000,0x2000,0x1000000000ull,0x10000,0x20000,8,4096,2,4096};
    auto readable=[&](auto v){return eye_root_lookup_parameters_valid(v,64,4096,4096,16);};
    check(readable(lookup),"guest range resolves through bounded private-only lookup");
    q=lookup;q.source_pages=0x7ffffff000ull;check(!readable(q),"guest table index must fit actual root extent");
    q=lookup;q.source_pages=0x11fffffffcul;q.reserved=8;check(!readable(q),"lookup cannot cross guest windows");
    q=lookup;q.source_pages++;check(!readable(q),"unaligned guest words rejected");
    q=lookup;q.destination=q.private_pages;check(!readable(q),"lookup cannot overwrite private source");
    q=lookup;q.mode=0;check(!readable(q),"lookup mode is explicit");
    q=lookup;q.source_pages=0x1200000000ull;check(!readable(q),"unmapped guest address rejected");
    EyeRootAliasParameters guard{0x10000,0x20000,0x30000,0,0x40000,36,1,3,560,0,2};
    auto guarded=[&](auto v){return eye_root_guard_parameters_valid(v,560,560,20,16);};
    check(guarded(guard),"bounded indirect draw guard");
    q=guard;q.extra1=5;q.private_pages=(uint64_t(uint32_t(-2))<<32)|7;check(guarded(q),"indexed geometry preserves signed vertex offset");
    for(int fault=0;fault<15;++fault){q=guard;
        if(fault==0)q.mode=2;if(fault==1)q.count=0;if(fault==2)q.count=1048577;
        if(fault==3)q.page_bytes=0;if(fault==4)q.page_bytes=1025;if(fault==5){q.count=1048576;q.page_bytes=2;}
        if(fault==6)q.reserved=556;if(fault==7)q.reserved=564;if(fault==8)q.extra0=1;
        if(fault==9)q.extra1=0;if(fault==10)q.extra1=34;if(fault==11)q.private_pages=UINT32_MAX;
        if(fault==12)q.source=q.source_pages;if(fault==13)q.destination=q.counters;if(fault==14)q.source++;
        check(!guarded(q),"invalid guard geometry, payload or address rejected before recording");}
    check(!eye_root_guard_parameters_valid(guard,560,560,19,16),"short indirect destination rejected");
    q=p;q.extra0=1;check(!valid(q),"patch rejects guard-only extras");
    q=lookup;q.extra1=2;check(!readable(q),"lookup rejects guard-only extras");
    std::cout<<"GPU alias patch and private-only lookup validate extents, modes, windows and nonaliasing\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
