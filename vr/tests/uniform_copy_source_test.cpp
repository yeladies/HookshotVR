// SPDX-License-Identifier: GPL-3.0-or-later
#define VK_NO_PROTOTYPES
#include "xr_uniform_copy_source.h"
#include <atomic>
#include <map>
#include <mutex>
#include <iostream>
#include <stdexcept>
#include <type_traits>
using namespace mkwvk;
template<class T>T handle(uint64_t n){if constexpr(std::is_pointer_v<T>)return reinterpret_cast<T>(uintptr_t(n));else return T(n);}
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
namespace mkwvk {
struct Pool {
    enum {MIRROR_NONE,MIRROR_WANTED,MIRROR_ON};
    VkBuffer buffer=VK_NULL_HANDLE,mirror=VK_NULL_HANDLE;uintptr_t mirror_alloc=0;
    uint64_t lo=0,hi=0,last_value=0,synced_epoch=0,sync_seg=0,retired_value=0;
    int mstate=MIRROR_NONE;bool inherit_pending=false;
};
std::map<uint64_t,Pool> imported;std::mutex pools_mu;std::atomic<bool> par_active{false};
std::atomic<uint64_t> pools_gen{3};uint64_t mem_rec_value=7,mirror_epoch=9,capture_value=5,segment_value=1;
constexpr auto kUsage=VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
uint64_t memory_capture_view(){return capture_value;}uint64_t texture_segment(){return segment_value;}
#include "uniform_copy_source_function.inc"
}
int main(){try{
    Pool pool;pool.lo=4096;pool.hi=12288;pool.buffer=handle<VkBuffer>(1);pool.mirror=handle<VkBuffer>(2);pool.mirror_alloc=1;
    pool.mstate=Pool::MIRROR_ON;pool.last_value=7;pool.synced_epoch=9;pool.sync_seg=1;imported[pool.lo]=pool;
    XrUniformCopySource out;check(memory_xr_uniform_copy_source(pool.mirror,7,out),"bound current mirror described");
    check(out.bytes==8192&&out.stamp.recording==7&&out.stamp.pool_generation==3&&out.stamp.capture_generation==5,"logical size and submission generations copied");
    check(imported.size()==1&&imported.begin()->second.last_value==7&&mem_rec_value==7&&mirror_epoch==9&&pools_gen==3&&capture_value==5,"query does not stamp or mutate source state");
    check(xr_uniform_copy_source_valid(out,pool.mirror,256,560,out.stamp),"bounded aligned uniform transfer");
    auto current=out.stamp;++current.capture_generation;check(!xr_uniform_copy_source_valid(out,pool.mirror,0,560,current),"stale capture rejected");
    check(!xr_uniform_copy_source_valid(out,pool.mirror,UINT64_MAX-3,8,out.stamp)&&!xr_uniform_copy_source_valid(out,pool.mirror,8192,4,out.stamp),"end and overflow rejected");
    check(!xr_uniform_copy_source_valid(out,pool.mirror,1,560,out.stamp)&&!xr_uniform_copy_source_valid(out,pool.mirror,0,559,out.stamp),"transfer alignment required");
    check(!xr_uniform_copy_source_valid(out,pool.mirror,0,0,out.stamp)&&!xr_uniform_copy_source_valid(out,pool.mirror,0,65540,out.stamp),"uniform copy size cap");
    auto invalid=out;invalid.usage=VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;check(!xr_uniform_copy_source_valid(invalid,pool.mirror,0,560,out.stamp),"transfer source usage required");
    invalid=out;invalid.kind=XrUniformSourceKind::Unsupported;check(!xr_uniform_copy_source_valid(invalid,pool.mirror,0,560,out.stamp),"no guessed direct-host fallback");
    auto rejected=[&]{out.bytes=777;check(!memory_xr_uniform_copy_source(pool.mirror,7,out)&&out.bytes==777,"failed query preserves output");};
    par_active=true;rejected();par_active=false;
    mem_rec_value=8;rejected();mem_rec_value=7;
    capture_value=0;rejected();capture_value=5;
    pools_gen=0;rejected();pools_gen=3;
    for(unsigned reason=0;reason<7;++reason){auto& p=imported.begin()->second;p=pool;
        if(reason==0)p.mstate=Pool::MIRROR_WANTED;if(reason==1)p.synced_epoch=8;if(reason==2)p.sync_seg=2;
        if(reason==3)p.retired_value=7;if(reason==4)p.inherit_pending=true;if(reason==5)p.last_value=6;if(reason==6)p.mirror_alloc=0;
        rejected();
    }
    imported.begin()->second=pool;imported[20000]=pool;rejected();imported.erase(20000);
    check(!memory_xr_uniform_copy_source(pool.buffer,7,out),"direct-host import is not silently accepted");
    check(memory_xr_uniform_copy_source(pool.mirror,7,out),"valid query after rejected attempts");
    std::cout<<"Actual mirror query preserves state and rejects stale, mutable or unbounded copy sources\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
