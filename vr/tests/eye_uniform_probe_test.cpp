// SPDX-License-Identifier: GPL-3.0-or-later
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "xr_first_person_uniform_copy.h"
#include "xr_camera_submission.h"
#include <map>
#include <fstream>
#include <filesystem>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <chrono>
static void verify(bool value,const char* why){if(!value)throw std::runtime_error(why);}
namespace mkwvk {
struct TestAllocation{uint64_t bytes;};using VmaAllocation=TestAllocation*;
struct TestDevice{VkDevice device=reinterpret_cast<VkDevice>(uintptr_t(1));}dev;
std::map<VkBuffer,std::vector<std::byte>> test_buffers;
uint64_t test_next=100,test_completed=0;unsigned test_allocations=0,test_destroys=0,test_updates=0,test_copies=0,test_barriers=0,test_invalidates=0,test_fail_at=0;
bool test_oversize=false;VkBuffer test_readback=VK_NULL_HANDLE;
SceneSubmissionPtr test_snapshot;FirstPersonFrameOwner test_owner;
void logf(const char*,...){}
uint64_t vk_timeline_completed(){return test_completed;}
bool create_device_buffer(uint64_t bytes,VkBufferUsageFlags,VkBuffer& buffer,VmaAllocation& allocation,bool){
    if(++test_allocations==test_fail_at)return false;
    buffer=reinterpret_cast<VkBuffer>(uintptr_t(++test_next));allocation=new TestAllocation{bytes};test_buffers[buffer].resize(bytes);return true;
}
bool create_buffer(uint64_t bytes,VkBufferUsageFlags usage,bool,VkBuffer& buffer,VmaAllocation& allocation,void** mapped,bool){
    if(!create_device_buffer(bytes,usage,buffer,allocation,true))return false;
    *mapped=test_buffers.at(buffer).data();test_readback=buffer;return true;
}
uint64_t allocation_size(VmaAllocation allocation){return test_oversize?2*1024*1024:allocation->bytes;}
void destroy_buffer(VkBuffer buffer,VmaAllocation allocation){verify(test_buffers.erase(buffer)==1,"double release");delete allocation;++test_destroys;}
void invalidate_mapped(VmaAllocation allocation,uint64_t bytes){verify(bytes<=allocation->bytes,"invalidate range");++test_invalidates;}
void vkCmdUpdateBuffer(VkCommandBuffer command,VkBuffer buffer,VkDeviceSize offset,VkDeviceSize bytes,const void* data){
    verify(command&&offset+bytes<=test_buffers.at(buffer).size(),"update outside private buffer");std::memcpy(test_buffers.at(buffer).data()+offset,data,bytes);++test_updates;
}
void vkCmdPipelineBarrier2(VkCommandBuffer,const VkDependencyInfo* info){
    verify(info->memoryBarrierCount==1&&info->pMemoryBarriers,"probe memory barrier");const auto& b=*info->pMemoryBarriers;
    verify(b.srcAccessMask==VK_ACCESS_2_TRANSFER_WRITE_BIT,"transfer writes ordered");
    verify((test_barriers==0&&test_updates==2&&test_copies==0&&b.dstAccessMask==VK_ACCESS_2_TRANSFER_READ_BIT)||
           (test_barriers==1&&test_copies==2&&b.dstAccessMask==VK_ACCESS_2_HOST_READ_BIT),"transfer/host barrier sequence");++test_barriers;
}
void vkCmdCopyBuffer(VkCommandBuffer,VkBuffer from,VkBuffer to,uint32_t count,const VkBufferCopy* copy){
    verify(count==1&&test_barriers==1,"copy ordered after upload barrier");auto& src=test_buffers.at(from);auto& dst=test_buffers.at(to);
    verify(copy->srcOffset+copy->size<=src.size()&&copy->dstOffset+copy->size<=dst.size(),"copy bounds");
    std::memcpy(dst.data()+copy->dstOffset,src.data()+copy->srcOffset,copy->size);++test_copies;
}
SceneSubmissionPtr xr_scene_submission_current(){return test_snapshot;}
uint64_t xr_scene_submission_source_value(){return 1234;}
bool xr_first_person_native_projection(int64_t present,const float projection[16],FirstPersonProjectionPacket* out,FirstPersonProjectionLookup* lookup){
    double native[16];for(unsigned k=0;k<16;++k)native[k]=projection[k];return test_owner.project(present,0,0,*out,native,lookup);
}
}
/* ACTUAL_GPU_UNIFORM_PROBE */
static void environment(const char* key,const std::string& value){
#ifdef _WIN32
    _putenv_s(key,value.c_str());
#else
    setenv(key,value.c_str(),1);
#endif
}
static std::string read_text(const std::filesystem::path& path){std::ifstream file(path);return std::string(std::istreambuf_iterator<char>(file),{});}
int main(int argc,char** argv){try{
    using namespace mkwvk;verify(argc==3,"case and private report path required");const std::string mode=argv[1];
    const bool first=mode!="legacy",waiting=mode=="waiting";const auto output=std::filesystem::u8path(argv[2]);
    std::filesystem::create_directories(output.parent_path());auto marker=output;marker+=".armed";std::filesystem::remove(marker);
    environment("HOOKSHOT_VR_EYE_UNIFORM_PROBE_FILE",output.u8string());environment("HOOKSHOT_VR_FIRST_PERSON_EYE_UNIFORM_ARM_FILE",first?marker.u8string():"");
    MkwVkFirstPersonInput input{};input.size=sizeof input;input.actor_id=42;input.game_present=7;input.flags=MKWVK_FP_SYNTHETIC_INPUT;input.eye_height=1.5;input.world_units_per_metre=1;
    XrFrameSnapshot frame{};frame.sequence=8;frame.origin_generation=1;frame.predicted_display_time=200;frame.observed_at_monotonic_ns=100;
    frame.running=frame.focused=frame.should_render=true;frame.head.orientation[3]=1;frame.head.flags=3;
    for(unsigned eye=0;eye<2;++eye){auto& e=frame.eyes[eye];e.pose=frame.head;e.pose.position[0]=eye?.032f:-.032f;e.fov[0]=-.7f;e.fov[1]=.8f;e.fov[2]=.9f;e.fov[3]=-.6f;}
    FirstPersonFrameOwner source_owner;MkwVkFirstPersonFrame head{};head.size=sizeof head;verify(source_owner.build(input,frame,true,head),"source owner");
    if(!waiting)test_owner=source_owner;
    std::vector<std::byte> source(0xb00);float prefix[0x230/4]{};std::memcpy(prefix,head.head_view,sizeof head.head_view);
    float projection[16]{};projection[0]=1;projection[5]=1.7f;projection[10]=-1.0002f;projection[11]=-.20002f;projection[14]=-1;
    std::memcpy(prefix+0x70/4,projection,sizeof projection);
    for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c){double value=0;for(unsigned k=0;k<4;++k)value+=double(projection[4*r+k])*(k==3?(c==3?1:0):head.head_view[4*k+c]);prefix[0x30/4+4*r+c]=float(value);}
    hookshot::vr::uniform_detail::inverse(prefix,prefix+0xb0/4);prefix[0x120/4]=1280;prefix[0x124/4]=720;std::memcpy(source.data(),prefix,sizeof prefix);
    for(size_t k=0x230;k<source.size();++k)source[k]=std::byte(k*37%256);const auto before=source;
    const auto now=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    auto snapshot=std::make_shared<SceneSubmissionSnapshot>();snapshot->captured_ns=now;snapshot->count=1;snapshot->cameras[0].received_ns=now;
    auto& pair=snapshot->cameras[0].pair;pair.revision=1;pair.primary.camera_id=42;pair.primary.sequence=1;pair.primary.game_present=7;
    std::memcpy(pair.primary.view,head.head_view,sizeof head.head_view);pair.uniforms=pair.primary;pair.uniforms.camera_id=99;pair.uniforms.sequence=2;
    std::memcpy(pair.uniforms.projection,projection,sizeof projection);test_snapshot=snapshot;
    if(first){verify(!xr_eye_uniform_probe_wants(1),"first-person probe cannot arm during startup");std::ofstream armed(marker);armed<<"armed";armed.close();}
    verify(xr_eye_uniform_probe_wants(2),"probe admission");const auto command=reinterpret_cast<VkCommandBuffer>(uintptr_t(2));
    if(waiting){xr_eye_uniform_probe_record(command,10,source.data(),source.size());verify(test_buffers.empty()&&xr_eye_uniform_probe_wants(3),"missing frame allocates nothing and may retry");test_owner=source_owner;}
    if(mode=="allocation")test_fail_at=3;if(mode=="budget")test_oversize=true;
    xr_eye_uniform_probe_record(command,10,source.data(),source.size());verify(source==before,"source bytes unchanged");
    if(mode=="allocation"||mode=="budget"){
        verify(test_buffers.empty()&&test_updates==0,"failed allocation releases only unsubmitted private buffers");
        const auto report=read_text(output);verify(report.find("\"resources_released\":true")!=std::string::npos,"allocation rejection reports release");return 0;
    }
    verify(test_buffers.size()==3&&test_updates==2&&test_copies==2&&test_barriers==2,"private upload/copy work");
    xr_eye_uniform_probe_wants(4);verify(test_buffers.size()==3&&test_invalidates==0,"no release or host read before fence");
    if(mode=="mismatch")test_buffers.at(test_readback)[0]^=std::byte(1);
    test_completed=10;
    if(mode=="device"){
        const auto original=dev.device;dev.device=reinterpret_cast<VkDevice>(uintptr_t(99));xr_eye_uniform_probe_wants(5);
        verify(test_buffers.size()==3&&test_destroys==0,"different device retains pending resources");dev.device=original;
    }
    xr_eye_uniform_probe_wants(6);verify(test_buffers.empty()&&test_destroys==3&&test_invalidates==1,"release follows completed destination readback");
    const auto report=read_text(output);const bool mismatch=mode=="mismatch";
    verify(report.find(mismatch?"\"status\":\"readback_mismatch\"":"\"status\":\"completed\"")!=std::string::npos,"readback result");
    verify(report.find("\"gpu_completion_verified\":true")!=std::string::npos&&report.find("\"resources_released\":true")!=std::string::npos,"completion proof");
    if(first)verify(report.find("\"first_person_packet_matched\":true")!=std::string::npos&&report.find("\"source_nvn_submit\":1234")!=std::string::npos&&report.find("\"writer_game_present\":7")!=std::string::npos,"writer pose and NVN source identity retained");
    verify(report.find("\"stereo_replay_enabled\":false")!=std::string::npos&&report.find("\"all_camera_aliases_patched\":false")!=std::string::npos,"no complete stereo claim");
    std::puts("Actual GPU uniform probe admission, fences, readback and lifetime passed");return 0;
}catch(const std::exception& error){std::fprintf(stderr,"%s\n",error.what());return 1;}}
