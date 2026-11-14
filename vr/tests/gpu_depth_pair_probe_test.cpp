// SPDX-License-Identifier: GPL-3.0-or-later
#define main source_fixture_main
#define env_on source_fixture_env_on
#include "gpu_scene_source_test.cpp"
#undef env_on
#undef main
#include "xr_gpu_depth_pair_probe.h"
#include "xr_depth_pair_parameters.h"
#include "xr_depth_pair_reproject_spv.h"
#include <set>
namespace mkwvk {
bool env_on(const char* name){return std::strcmp(name,"RX_VK_XR_GPU_DEPTH_PAIR_PROBE")==0;}
struct PairFake {std::set<VkShaderModule> shaders;std::set<VkPipelineLayout> layouts;std::set<VkPipeline> pipelines;hookshot::vr::DepthPairDispatch dispatch;uint64_t shared_source=0;unsigned dispatches=0;bool corrupt=false,uniform=false;} pair_fake;
VkResult vkCreateShaderModule(VkDevice,const VkShaderModuleCreateInfo* c,const VkAllocationCallbacks*,VkShaderModule* out){check(c->codeSize==sizeof kDepthPairReprojectSpv,"actual authored shader only");if(fail())return VK_ERROR_OUT_OF_DEVICE_MEMORY;*out=handle<VkShaderModule>(++fake.next);pair_fake.shaders.insert(*out);return VK_SUCCESS;}
void vkDestroyShaderModule(VkDevice,VkShaderModule s,const VkAllocationCallbacks*){check(pair_fake.shaders.erase(s)==1,"owned shader");}
VkResult vkCreatePipelineLayout(VkDevice,const VkPipelineLayoutCreateInfo* c,const VkAllocationCallbacks*,VkPipelineLayout* out){check(c->pushConstantRangeCount==1&&c->pPushConstantRanges->size==32,"bounded compute pushes");if(fail())return VK_ERROR_OUT_OF_DEVICE_MEMORY;*out=handle<VkPipelineLayout>(++fake.next);pair_fake.layouts.insert(*out);return VK_SUCCESS;}
void vkDestroyPipelineLayout(VkDevice,VkPipelineLayout s,const VkAllocationCallbacks*){check(pair_fake.layouts.erase(s)==1,"owned layout");}
VkResult vkCreateComputePipelines(VkDevice,VkPipelineCache,uint32_t count,const VkComputePipelineCreateInfo*,const VkAllocationCallbacks*,VkPipeline* out){check(count==1,"one compute pipeline");if(fail())return VK_ERROR_OUT_OF_DEVICE_MEMORY;*out=handle<VkPipeline>(++fake.next);pair_fake.pipelines.insert(*out);return VK_SUCCESS;}
void vkDestroyPipeline(VkDevice,VkPipeline s,const VkAllocationCallbacks*){check(pair_fake.pipelines.erase(s)==1,"owned pipeline");}
void note_work(){}
void vkCmdBindPipeline(VkCommandBuffer,VkPipelineBindPoint point,VkPipeline pipeline){check(point==VK_PIPELINE_BIND_POINT_COMPUTE&&pair_fake.pipelines.count(pipeline),"private compute only");}
void vkCmdPushConstants(VkCommandBuffer,VkPipelineLayout layout,VkShaderStageFlags stages,uint32_t offset,uint32_t bytes,const void* data){check(pair_fake.layouts.count(layout)&&stages==VK_SHADER_STAGE_COMPUTE_BIT&&offset==0&&bytes==32,"owned dispatch metadata");std::memcpy(&pair_fake.dispatch,data,32);}
void vkCmdUpdateBuffer(VkCommandBuffer,VkBuffer b,VkDeviceSize offset,VkDeviceSize bytes,const void* data){auto& dst=fake.memory.at(fake.bindings.at(b));check(bytes+offset<=dst.size(),"bounded metadata update");std::memcpy(dst.data()+offset,data,bytes);}
void vkCmdFillBuffer(VkCommandBuffer,VkBuffer b,VkDeviceSize offset,VkDeviceSize bytes,uint32_t word){check(word==0&&offset%4==0&&bytes%4==0,"zeroed diagnostic counters");auto& dst=fake.memory.at(fake.bindings.at(b));check(bytes+offset<=dst.size(),"bounded counter clear");std::memset(dst.data()+offset,0,bytes);}
unsigned char* address(uint64_t a,uint64_t bytes){const auto b=handle<VkBuffer>(a>>20);const auto offset=a&((1<<20)-1);auto& dst=fake.memory.at(fake.bindings.at(b));check(offset+bytes<=dst.size(),"bounded device address");return dst.data()+offset;}
// Transport/lifetime double only; the real shader has a separate GPU oracle.
void vkCmdDispatch(VkCommandBuffer,uint32_t x,uint32_t y,uint32_t z){check(x==1&&y==1&&z==1,"bounded fixture dispatch");const auto& d=pair_fake.dispatch;hookshot::vr::DepthPairParameters camera;std::memcpy(&camera,address(d.parameters,128),128);check(camera.dimensions[0]==4&&camera.dimensions[1]==4,"source shape retained");
    if(pair_fake.dispatches%2==0)pair_fake.shared_source=d.source;else check(pair_fake.shared_source==d.source,"both eyes read the same GPU source address");address(d.source,128);uint32_t pixels[16];for(auto& p:pixels)p=pair_fake.uniform||camera.source_from_eye[3]<0?0xff123456:0xff654321;std::memcpy(address(d.destination,64),pixels,64);uint32_t counts[4]={pair_fake.corrupt?15u:16u,0,0,0};std::memcpy(address(d.counters,16),counts,16);++pair_fake.dispatches;
}
}
#include "gpu_depth_pair_probe_function.inc"
using namespace mkwvk;
void reset_pair(){auto& p=depth_probe();p.~DepthProbe();new(&p)DepthProbe;pair_fake={};reset();}
void record_pair(uint64_t frame=1){capture(frame);complete(frame);xr_gpu_depth_pair_probe_network(handle<VkCommandBuffer>(3),frame*10+5,0xfd029a317a9a4b94ull);}
void assert_clean(){check(pair_fake.pipelines.empty()&&pair_fake.layouts.empty()&&pair_fake.shaders.empty()&&depth_probe().allocated==0,"all optional resources unwind");}
int main(){try{
    for(int failure=1;failure<=19;++failure){reset_pair();capture(1);complete(1);fake.fail=fake.calls+failure;xr_gpu_depth_pair_probe_network(handle<VkCommandBuffer>(3),15,0xfd029a317a9a4b94ull);check(depth_probe().stopped&&depth_probe().failures==1&&pair_fake.dispatches==0,"each setup failure records no partial pair");assert_clean();check(fake.memory.size()==1&&xr_gpu_scene_source_stop(),"setup unwind leaves original source intact");}
    reset_pair();record_pair();check(depth_probe().pending&&depth_probe().source&&pair_fake.dispatches==2,"pair retains one source through both dispatches");xr_gpu_depth_pair_probe_tick(2);check(depth_probe().completed==0,"no early output publication");check(!xr_gpu_depth_pair_probe_stop()&&depth_probe().source&&!pair_fake.pipelines.empty(),"shutdown retains in-flight consumer and source lease");fake.completed=15;check(xr_gpu_depth_pair_probe_stop(),"known completion permits consumer release");assert_clean();check(xr_gpu_scene_source_stop()&&fake.memory.empty(),"source storage retires after consumer release");
    reset_pair();record_pair();pair_fake.corrupt=true;fake.completed=15; // Corrupt the completed readback counter, not merely future emulation.
    auto& p=depth_probe();uint32_t wrong=15;std::memcpy(static_cast<unsigned char*>(p.readback.mapped)+128,&wrong,4);xr_gpu_depth_pair_probe_tick(3);check(p.stopped&&p.failures==1&&!p.complete,"incomplete pixel counter rejects pair publication");check(xr_gpu_depth_pair_probe_stop(),"completed rejected work can retire");assert_clean();
    reset_pair();pair_fake.uniform=true;record_pair();fake.completed=15;xr_gpu_depth_pair_probe_tick(3);check(!depth_probe().complete&&!depth_probe().stopped&&depth_probe().completed==0&&depth_probe().background_rejections==1&&!depth_probe().source,"uniform background is not accepted and source lease is released for retry");check(xr_gpu_depth_pair_probe_stop(),"completed background attempt retires");assert_clean();
    reset_pair();pair_fake.uniform=true;for(uint64_t frame=1;frame<24;frame+=2){depth_probe().not_before_ns=0;record_pair(frame);fake.completed=frame*10+5;xr_gpu_depth_pair_probe_tick(frame+2);}check(depth_probe().stopped&&depth_probe().failures==1&&depth_probe().recorded==12&&depth_probe().background_rejections==12&&!depth_probe().complete,"uniform outputs stop at twelve attempts rather than looping indefinitely");check(xr_gpu_depth_pair_probe_stop(),"bounded rejected attempts release completed resources");assert_clean();
    reset_pair();record_pair();dev.device=handle<VkDevice>(2);xr_gpu_depth_pair_probe_tick(2);dev.device=handle<VkDevice>(1);fake.completed=15;check(!xr_gpu_depth_pair_probe_stop()&&depth_probe().source&&!pair_fake.pipelines.empty(),"device handle ABA retains GPU consumer and lease permanently");
    reset_pair();const auto path=std::filesystem::temp_directory_path()/("vr-authored-pair-"+std::to_string(depth_probe_now())+".ppm");
#ifdef _WIN32
    _putenv_s("HOOKSHOT_VR_GPU_DEPTH_PAIR_IMAGE_FILE",path.string().c_str());
#else
    setenv("HOOKSHOT_VR_GPU_DEPTH_PAIR_IMAGE_FILE",path.string().c_str(),1);
#endif
    for(uint64_t frame:{1ull,3ull,5ull}){record_pair(frame);fake.completed=frame*10+5;xr_gpu_depth_pair_probe_tick(frame+2);}
    check(depth_probe().complete&&depth_probe().completed==3&&pair_fake.dispatches==6,"three different live sources complete paired dispatches with reused allocations");check(depth_probe().source_states[0]<depth_probe().source_states[1]&&depth_probe().source_states[1]<depth_probe().source_states[2],"strictly advancing scene states");check(std::filesystem::file_size(path)==11+4*4*6,"authored side-by-side diagnostic image extent");std::filesystem::remove(path);check(xr_gpu_depth_pair_probe_stop(),"completed consumer releases");assert_clean();check(xr_gpu_scene_source_stop()&&fake.memory.empty(),"all completed source/consumer allocations released");
    std::cout<<"Actual native GPU depth-pair consumer: shared source, 19 setup failures, timeline gating, reused storage and uncertain-work retention passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
