// SPDX-License-Identifier: GPL-3.0-or-later
// Reuse the recorder's Vulkan/allocation fixture; exercise the actual root module.
#define main baseline_fixture_main
#define xr_root_alias_probe_create baseline_stub_root_create
#define xr_root_alias_probe_record baseline_stub_root_record
#define xr_root_alias_probe_indirect_command baseline_stub_root_indirect_command
#define xr_root_alias_probe_retire baseline_stub_root_retire
#define memory_xr_uniform_copy_source baseline_stub_memory_query
#define vkGetBufferDeviceAddress baseline_stub_buffer_address
#define vkCmdPushConstants baseline_stub_push_constants
#define vkCmdDispatch baseline_stub_dispatch
#include "baseline_recorder_test.cpp"
#undef main
#undef xr_root_alias_probe_create
#undef xr_root_alias_probe_record
#undef xr_root_alias_probe_indirect_command
#undef xr_root_alias_probe_retire
#undef memory_xr_uniform_copy_source
#undef vkGetBufferDeviceAddress
#undef vkCmdPushConstants
#undef vkCmdDispatch
#include "xr_root_alias_parameters.h"
#include "xr_root_alias_patch_spv.h"
namespace mkwvk {
using hookshot::vr::EyeRootAliasParameters;
EyeRootAliasParameters root_parameters{};
int root_dispatches=0;
uint64_t root_address(VkBuffer b){return uint64_t(reinterpret_cast<uintptr_t>(b))<<32;}
unsigned char* root_span(uint64_t address,uint64_t bytes){
    for(auto& entry:fake.buffers){const auto base=root_address(entry.first);if(address>=base&&address-base<=entry.second.size()&&bytes<=entry.second.size()-(address-base))return entry.second.data()+(address-base);}
    throw std::runtime_error("GPU double access outside an owned buffer");
}
bool memory_xr_uniform_copy_source(VkBuffer b,uint64_t recording,XrUniformCopySource& out){
    if(b!=handle<VkBuffer>(10))return false;
    out={b,8192,VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT|VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,XrUniformSourceKind::Mirror,{recording,fake.generation,1}};return true;
}
bool memory_xr_guest_root_source(uint64_t recording,XrGuestRootSource& out){out={root_address(handle<VkBuffer>(9)),3145728ull*8,{recording,fake.generation,1}};return true;}
VkDeviceAddress vkGetBufferDeviceAddress(VkDevice,const VkBufferDeviceAddressInfo* info){return root_address(info->buffer);}
void vkCmdFillBuffer(VkCommandBuffer,VkBuffer b,VkDeviceSize offset,VkDeviceSize bytes,uint32_t word){auto* p=root_span(root_address(b)+offset,bytes);for(uint64_t i=0;i<bytes;i+=4)std::memcpy(p+i,&word,4);}
void vkCmdPushConstants(VkCommandBuffer,VkPipelineLayout layout,VkShaderStageFlags stages,uint32_t offset,uint32_t bytes,const void* data){check(fake.layouts.count(layout)&&stages==VK_SHADER_STAGE_COMPUTE_BIT&&!offset&&bytes==sizeof root_parameters,"root parameters preserve compute layout");std::memcpy(&root_parameters,data,bytes);}
// Only model transport and alias lookup. Numerical shader correctness is tested
// separately by the real GPU smoke, using authored input without a game dump.
void vkCmdDispatch(VkCommandBuffer,uint32_t x,uint32_t y,uint32_t z){
    ++root_dispatches;const auto& p=root_parameters;check(y==1&&z==1,"bounded root dispatch");
    auto* counters=reinterpret_cast<uint32_t*>(root_span(p.counters,16));
    if(p.mode==3){check(x==1,"bounded indirect guard dispatch");
        bool valid=!counters[0]&&!counters[2]&&!counters[3]&&counters[1]>=(p.extra1>>1);
        valid=valid&&!std::memcmp(root_span(p.source,p.reserved),root_span(p.source_pages,p.reserved),p.reserved);
        const bool indexed=p.extra1&1;const uint32_t arguments[5]{valid?p.count:0,p.page_bytes,uint32_t(p.private_pages),indexed?uint32_t(p.private_pages>>32):p.extra0,indexed?p.extra0:0};
        std::memcpy(root_span(p.destination,sizeof arguments),arguments,sizeof arguments);return;
    }
    const auto* root=reinterpret_cast<const uint64_t*>(root_span(p.source,uint64_t(p.count)*8));
    if(p.mode<=1){check(x==(p.count+255)/256,"complete root dispatch");auto* target=reinterpret_cast<uint64_t*>(root_span(p.destination,uint64_t(p.count)*8));
        for(uint32_t i=0;i<p.count;++i){const auto original=root[i];const bool overflow=original>UINT64_MAX-4096;
            const bool misaligned=original&&(original&3);const bool overlap=!overflow&&original&&original<p.source_pages+p.page_bytes&&original+4096>p.source_pages;
            const bool covered=overlap&&!misaligned&&original>=p.source_pages&&original+4096<=p.source_pages+p.page_bytes;
            const auto expected=covered?p.private_pages+original-p.source_pages:original;
            if(!p.mode)target[i]=expected;else{counters[0]+=target[i]!=expected;counters[1]+=covered;counters[2]+=overlap&&!covered;counters[3]+=overflow||misaligned;}
        }
    }else{check(p.mode==2&&x==(p.reserved/4+255)/256,"bounded camera lookup");auto* output=root_span(p.destination,p.reserved);
        for(uint32_t i=0;i<p.reserved;i+=4){const auto guest=p.source_pages+i;const auto index=guest>=0x7f00000000ull?2097152+((guest-0x7f00000000ull)>>12):((guest-0x1000000000ull)>>12);
            check(index<p.count,"guest fits published root");const auto page=root[index];uint32_t value=0;
            if(!(page&3)&&page>=p.private_pages&&page<=UINT64_MAX-4096&&page+4096<=p.private_pages+p.page_bytes)std::memcpy(&value,root_span(page+(guest&4095),4),4);else ++counters[3];
            std::memcpy(output+i,&value,4);
        }
    }
}
}
#include "root_alias_probe_function.inc"
XrRootAliasProbeInput root_input(){
    fake={};dev.device=handle<VkDevice>(1);root_dispatches=0;
    fake.buffers[handle<VkBuffer>(9)].resize(3145728ull*8);
    auto* roots=reinterpret_cast<uint64_t*>(fake.buffers.at(handle<VkBuffer>(9)).data());
    for(auto index:{0u,2097152u}){roots[index]=root_address(handle<VkBuffer>(10));roots[index+1]=roots[index]+4096;}
    fake.buffers[handle<VkBuffer>(10)].resize(8192,23);
    fake.buffers[handle<VkBuffer>(20)].resize(560,37);fake.buffers[handle<VkBuffer>(21)].resize(560,51);
    for(unsigned i=0;i<560;++i){fake.buffers[handle<VkBuffer>(20)][i]=static_cast<unsigned char>((i+37)%251);fake.buffers[handle<VkBuffer>(21)][i]=static_cast<unsigned char>((i+51)%251);}
    XrRootAliasProbeInput in;in.command=handle<VkCommandBuffer>(2);in.recording=9;in.guest_camera=0x1000000f00ull;
    in.camera={handle<VkBuffer>(10),3840,560};in.matrices={handle<VkBuffer>(20),handle<VkBuffer>(21)};in.count=36;in.instances=1;in.first=7;in.vertex_offset=-2;return in;
}
void root_cleaned(){check(fake.memory.empty()&&fake.shaders.empty()&&fake.layouts.empty()&&fake.pipelines.empty()&&fake.buffers.size()==4,"only original buffers remain after root retirement");}
int main(){try{
    XrRootAliasProbeResult result;const std::array<const unsigned char*,2> no_expected{};
    for(int failure=1;failure<=8;++failure){auto in=root_input();fake.fail=failure;check(!xr_root_alias_probe_create(in),"root setup failure rejects");root_cleaned();check(!fake.ends&&!root_dispatches,"setup failure records no GPU work");}
    auto in=root_input();fake.throw_at=5;check(!xr_root_alias_probe_create(in),"allocation exception contained");root_cleaned();
    in=root_input();auto* p=xr_root_alias_probe_create(in);check(p,"allocate root fixture");++fake.generation;
    check(!xr_root_alias_probe_record(p)&&!root_dispatches&&!fake.ends,"changed pool rejected before GPU work");check(xr_root_alias_probe_retire(p,0,no_expected,0,result)&&!p&&!result.recorded,"unrecorded resources retire immediately");root_cleaned();
    for(bool indexed:{false,true})for(bool corrupt:{false,true}){in=root_input();in.indexed=indexed;const auto source=fake.buffers.at(handle<VkBuffer>(10));if(corrupt){auto* roots=reinterpret_cast<uint64_t*>(fake.buffers.at(handle<VkBuffer>(9)).data());roots[0]+=16;}
        p=xr_root_alias_probe_create(in);check(p&&xr_root_alias_probe_record(p)&&root_dispatches==8,"private root copy, verification and lookup per eye");
        XrRootIndirectCommand command{handle<VkBuffer>(99),99};
        check(!xr_root_alias_probe_indirect_command(p,in.command,8,0,command)&&command.buffer==handle<VkBuffer>(99)&&command.offset==99,"wrong recording leaves borrowed command untouched");
        check(!xr_root_alias_probe_indirect_command(p,handle<VkCommandBuffer>(3),9,0,command)&&!xr_root_alias_probe_indirect_command(p,in.command,9,2,command),"command and eye identity required");
        check(xr_root_alias_probe_indirect_command(p,in.command,9,0,command)&&command.buffer&&command.offset%16==0,"same-recording eye may borrow the GPU argument range without waiting");
        ++fake.generation;check(!xr_root_alias_probe_indirect_command(p,in.command,9,1,command),"stale source generation cannot borrow arguments");--fake.generation;
        const std::array<const unsigned char*,2> expected{fake.buffers.at(handle<VkBuffer>(20)).data(),fake.buffers.at(handle<VkBuffer>(21)).data()};
        check(!xr_root_alias_probe_retire(p,8,expected,560,result)&&p&&!fake.memory.empty(),"in-flight work retains all backing allocations");
        check(xr_root_alias_probe_retire(p,9,expected,560,result)&&!p&&result.recorded&&result.verified==!corrupt,"completed cross-page camera readback detects wrong alias offsets");
        check(result.indirect_guard_verified==!corrupt&&result.indirect_counts[0]==(corrupt?0:36)&&result.indirect_counts[1]==(corrupt?0:36),"actual recorder transports zero-count guards on bad lookup and preserves indexed/nonindexed geometry");
        check(result.aliases[0]==4&&result.aliases[1]==4&&!result.uncovered[0]&&!result.uncovered[1],"all low and high window aliases visited");
        check((result.errors[0]>0)==corrupt&&(result.errors[1]>0)==corrupt,"payload mismatch contributes an explicit error");
        check(fake.buffers.at(handle<VkBuffer>(10))==source,"original camera bytes unchanged");root_cleaned();
    }
    // A device change makes ownership permanently indeterminate. Do not let a
    // recycled handle later free resources belonging to the abandoned device.
    in=root_input();p=xr_root_alias_probe_create(in);check(p&&xr_root_alias_probe_record(p),"record device retention case");dev.device=handle<VkDevice>(2);
    check(!xr_root_alias_probe_retire(p,9,no_expected,0,result)&&p,"changed device retains storage");dev.device=handle<VkDevice>(1);
    check(!xr_root_alias_probe_retire(p,9,no_expected,0,result)&&p&&!xr_root_alias_probe_record(p),"restored handle never revives indeterminate ownership");
    std::cout<<"Actual root recorder rejects allocation/stale-state failures, checks cross-page aliases and retains in-flight or device-abandoned resources\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
