// SPDX-License-Identifier: GPL-3.0-or-later
#include "eye_resource_layout.h"
#include <iostream>
#include <stdexcept>
using namespace hookshot::vr;
void check(bool x,const char* why){if(!x)throw std::runtime_error(why);}
int main(){try {
    EyeBufferLayout layout;
    check(EyeBufferLayout::build({{50,10,100},{80,256,256}},16,layout),"layout built");
    auto left=layout.resolve(0,50,16,32,true),right=layout.resolve(1,50,16,32,true);
    check(left.route==EyeBufferRoute::Private&&right.route==EyeBufferRoute::Private&&left.allocation!=right.allocation,"eyes never name the same private allocation");
    check(left.offset==16&&left.size==32&&layout.allocations_per_eye()[0].bytes==112,"prefix preserves original descriptor alignment");
    for(uint64_t alignment:{1,16,64,256,4096}) {
        EyeBufferLayout test;check(EyeBufferLayout::build({{7,13,10000}},alignment,test),"valid power-of-two alignment");
        for(uint64_t off=13;off<10013;off+=17) {
            const auto binding=test.resolve(1,7,off,1,false);
            check(binding.route==EyeBufferRoute::Private&&binding.offset%alignment==off%alignment,"all original offset residues preserved");
            check(binding.offset+binding.size<=test.allocations_per_eye()[0].bytes,"mapped descriptor stays inside allocation");
        }
    }
    check(layout.resolve(0,50,0,20,false).route==EyeBufferRoute::Reject,"partially overlapping read cannot silently use shared storage");
    check(layout.resolve(0,50,0,4,false).route==EyeBufferRoute::SharedReadOnly,"unrelated immutable input can remain shared");
    check(layout.resolve(0,99,0,4,true).route==EyeBufferRoute::Reject,"unknown write cannot target original data");
    check(layout.resolve(2,50,16,4,false).route==EyeBufferRoute::Reject,"invalid eye rejected");
    check(layout.resolve(0,50,UINT64_MAX,4,false).route==EyeBufferRoute::Reject,"overflowing range rejected");
    const auto before=layout.allocations_per_eye().size();
    check(!EyeBufferLayout::build({{1,0,20},{1,10,20}},16,layout)&&layout.allocations_per_eye().size()==before,"overlap rejects entire replacement plan");
    check(!EyeBufferLayout::build({{1,0,20}},3,layout),"non-power-of-two alignment rejected");
    check(!EyeBufferLayout::build({{1,1,UINT64_MAX-8}},16,layout),"padding arithmetic checked");
    std::cout<<"Per-eye buffer identities, alias bounds and descriptor alignment checked\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
