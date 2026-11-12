// SPDX-License-Identifier: GPL-3.0-or-later
#define VK_NO_PROTOTYPES
#include "eye_buffer_descriptors.h"
#include <iostream>
#include <stdexcept>
#include <type_traits>
using namespace hookshot::vr;
template<class T>T handle(uint64_t n){if constexpr(std::is_pointer_v<T>)return reinterpret_cast<T>(uintptr_t(n));else return T(n);}
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(){try{
    const auto usage=VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    EyeBufferLayout layout;check(EyeBufferLayout::build({{1,256,4096}},256,layout),"source layout");
    EyePageStamp stamp{1,2,3,4},current=stamp;EyeDescriptorLimits limits{256,16,65536,1<<20};
    std::vector<EyeDescriptorSource> sources{{1,handle<VkBuffer>(1),8192,usage}};
    std::vector<EyeDescriptorPrivate> buffers{{handle<VkBuffer>(2),4096,usage},{handle<VkBuffer>(3),4096,usage}};
    std::vector<EyeDescriptorRequest> requests{{1,256,4096,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,false,false},{1,256,4096,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,false,false}};
    EyeBufferDescriptors out;auto prepare=[&]{return prepare_eye_buffer_descriptors(layout,sources,buffers,requests,stamp,current,limits,out);};
    check(prepare()&&out.eyes[0].size()==2&&out.eyes[0][0].buffer==buffers[0].buffer&&out.eyes[1][0].buffer==buffers[1].buffer&&out.eyes[0][0].offset==0,"both buffer binding types route to distinct eyes");
    check(!out.replay_ready(),"routing alone does not authorize replay");
    requests[0].writable=true;check(prepare(),"reviewed private storage writes remain private");requests[0].writable=false;
    auto good=requests;requests.push_back({1,4608,256,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,false,false});
    check(!prepare()&&out.eyes[0].size()==2,"unreviewed shared read rejects atomically");
    requests.back().shared_read_reviewed=true;check(prepare()&&out.eyes[0].back().buffer==sources[0].buffer,"explicitly reviewed shared read");
    requests.back().writable=true;check(!prepare(),"shared write always rejected");requests=good;
    requests[0].offset=128;check(!prepare(),"partially covered private descriptor rejected");requests=good;
    requests[1].offset=272;requests[1].bytes=256;check(!prepare(),"private uniform offset alignment enforced");requests=good;
    buffers[1].buffer=buffers[0].buffer;check(!prepare(),"eye handle aliases rejected");buffers[1].buffer=handle<VkBuffer>(3);
    buffers[0].buffer=sources[0].buffer;check(!prepare(),"original storage cannot masquerade as private");buffers[0].buffer=handle<VkBuffer>(2);
    buffers[0].usage=VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;check(!prepare(),"required native usage checked");buffers[0].usage=usage;
    requests[1].writable=true;check(!prepare(),"uniform writes invalid");requests=good;
    requests[0].type=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;check(!prepare(),"dynamic descriptors need separate reviewed offset handling");requests=good;
    requests[0].bytes=VK_WHOLE_SIZE;check(!prepare(),"unknown descriptor range rejected");requests=good;
    requests[0].offset=UINT64_MAX-1;check(!prepare(),"source range overflow rejected");requests=good;
    ++current.pool_generation;check(!prepare(),"stale native pool generation rejected");current=stamp;
    limits.uniform_range=1024;check(!prepare(),"physical device range limit enforced");limits.uniform_range=65536;
    check(prepare(),"valid preparation after failed attempts");
    std::cout<<"Private UBO/SSBO descriptors preserve source identity, alignment, bounds and atomic failure\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
