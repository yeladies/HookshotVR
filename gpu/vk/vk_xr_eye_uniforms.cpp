// SPDX-License-Identifier: GPL-3.0-or-later
#include "vk_internal.h"
#include "xr_eye_uniform_probe.h"
#include "xr_eye_uniform_copy.h"
#include "xr_first_person_uniform_copy.h"
#include "xr_camera_submission.h"
#include <chrono>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include "vk_work.h"
namespace mkwvk {
namespace {
struct Probe {
    std::mutex mutex;VkDevice device=VK_NULL_HANDLE;
    std::array<VkBuffer,2> buffers{};std::array<VmaAllocation,2> memory{};
    VkBuffer readback=VK_NULL_HANDLE;VmaAllocation readback_memory=nullptr;void* mapped=nullptr;
    std::array<std::vector<std::byte>,2> payloads;
    uint64_t value=0,allocated=0,last_poll=UINT64_MAX;size_t bytes=0;
    bool attempted=false,recorded=false,complete=false,validated=false,released=false;
    bool first_person=false,armed=false,waiting_reported=false;uint64_t arm_poll=UINT64_MAX,source_nvn_submit=0;
    int64_t game_present=-1;FirstPersonProjectionEvidence camera{};
};
Probe& probe(){static auto* state=new Probe;return *state;}
const char* output(){static const char* path=std::getenv("HOOKSHOT_VR_EYE_UNIFORM_PROBE_FILE");return path&&*path?path:nullptr;}
const char* first_person_marker(){static const char* path=std::getenv("HOOKSHOT_VR_FIRST_PERSON_EYE_UNIFORM_ARM_FILE");return path&&*path?path:nullptr;}
void report(const Probe& p,const char* status){
    std::ofstream file(std::filesystem::u8path(output()),std::ios::trunc);
    if(file)file<<"{\"schema\":1,\"status\":\""<<status<<"\",\"descriptor_bytes\":"<<p.bytes<<",\"allocated_bytes\":"<<p.allocated
        <<",\"copy_recorded\":"<<(p.recorded?"true":"false")<<",\"gpu_completion_verified\":"<<(p.complete?"true":"false")
        <<",\"full_payloads_validated\":"<<(p.validated?"true":"false")<<",\"resources_released\":"<<(p.released?"true":"false")
        <<",\"first_person_requested\":"<<(p.first_person?"true":"false")
        <<",\"first_person_packet_matched\":"<<(p.camera.available&&p.camera.center_matches?"true":"false")
        <<",\"native_depth_preserved\":"<<(p.camera.depth_exact?"true":"false")<<",\"tracking_fov_used\":"<<(p.camera.fov_matches?"true":"false")
        <<",\"synthetic_tracking\":"<<(p.camera.synthetic?"true":"false")<<",\"writer_game_present\":"<<p.game_present
        <<",\"source_nvn_submit\":"<<p.source_nvn_submit<<",\"tracking_sequence\":"<<p.camera.sequence<<",\"origin_generation\":"<<p.camera.origin
        <<",\"predicted_display_time\":"<<p.camera.predicted_time<<",\"observed_ns\":"<<p.camera.observed_ns
        <<",\"source_gpu_readback\":false,\"all_camera_aliases_patched\":false,\"complete_scene_shader_coverage\":false"
        <<",\"source_objects_replaced\":false,\"stereo_replay_enabled\":false}\n";
}
void release(Probe& p){
    for(unsigned eye=0;eye<2;++eye){if(p.buffers[eye])destroy_buffer(p.buffers[eye],p.memory[eye]);p.buffers[eye]=VK_NULL_HANDLE;p.memory[eye]=nullptr;}
    if(p.readback)destroy_buffer(p.readback,p.readback_memory);p.readback=VK_NULL_HANDLE;p.readback_memory=nullptr;p.mapped=nullptr;
    p.released=true;
}
bool payloads(Probe& p,const void* source){
    if(p.first_person){
        std::array<float,0x230/4> prefix{};std::memcpy(prefix.data(),source,sizeof prefix);
        const float w=prefix[0x120/4],h=prefix[0x124/4];
        if(!std::isfinite(w)||!std::isfinite(h)||w<1||h<1||w>4096||h>4096||std::floor(w)!=w||std::floor(h)!=h)return false;
        const auto snapshot=xr_scene_submission_current();
        const auto now=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        const auto match=match_submitted_uniform(snapshot.get(),now,uint32_t(w),uint32_t(h),prefix.data(),prefix.size());
        if(!match.matched||match.ambiguous||match.observation_present_ambiguous||match.observation_game_present<0||!xr_scene_submission_source_value())return false;
        FirstPersonProjectionPacket packet{};
        if(!xr_first_person_native_projection(match.observation_game_present,prefix.data()+0x70/4,&packet)||
           !prepare_first_person_primary_uniform_copies(source,p.bytes,packet,p.payloads))return false;
        p.game_present=packet.game_present;p.source_nvn_submit=xr_scene_submission_source_value();
        p.camera=first_person_projection_evidence(packet,prefix.data(),prefix.data()+0x70/4);return true;
    }
    using namespace hookshot::vr;
    float base[0xe0/4];std::memcpy(base,source,sizeof base);
    for(unsigned eye=0;eye<2;++eye){EyeMatrices matrices{};
        for(int r=0;r<3;++r)for(int col=0;col<4;++col)matrices.view.rows[r][col]=base[4*r+col];
        // Unsubmitted diagnostic displacement in game units; no world-scale claim.
        matrices.view.rows[0][3]+=eye?0.01:-0.01;
        float view[12],inverse[12];for(int i=0;i<12;++i)view[i]=float(matrices.view.rows[i/4][i%4]);
        uniform_detail::inverse(view,inverse);for(int r=0;r<3;++r)matrices.view.eye[r]=inverse[4*r+3];
        for(int i=0;i<16;++i)matrices.projection[i]=base[0x70/4+i];
        for(int r=0;r<4;++r)for(int col=0;col<4;++col)for(int k=0;k<4;++k)
            matrices.world_to_clip[4*r+col]+=matrices.projection[4*r+k]*(k==3?(col==3?1:0):matrices.view.rows[k][col]);
        if(!prepare_eye_uniform_copy(source,p.bytes,matrices,ClipDepth::MinusOneToOne,p.payloads[eye]))return false;
    }
    return p.payloads[0]!=p.payloads[1];
}
}
bool xr_eye_uniform_probe_wants(uint64_t frame){
    if(!output())return false;auto& p=probe();std::lock_guard lock(p.mutex);
    p.first_person=first_person_marker()!=nullptr;
    if(!p.attempted){
        if(p.first_person&&!p.armed&&p.arm_poll!=frame){p.arm_poll=frame;std::ifstream marker(std::filesystem::u8path(first_person_marker()));p.armed=bool(marker);}
        return !p.first_person||p.armed;
    }
    if(p.recorded&&!p.complete&&p.last_poll!=frame){p.last_poll=frame;
        if(p.device!=dev.device){report(p,"device_changed_resources_retained");return false;}
        if(vk_timeline_completed()>=p.value){p.complete=true;invalidate_mapped(p.readback_memory,2*p.bytes);
            p.validated=true;for(unsigned eye=0;eye<2;++eye)p.validated&=std::memcmp(static_cast<std::byte*>(p.mapped)+eye*p.bytes,p.payloads[eye].data(),p.bytes)==0;
            release(p);report(p,p.validated?"completed":"readback_mismatch");
            logf("vr-eye-uniforms: complete descriptor payloads %s (%llu bytes each)",p.validated?"validated":"mismatched",(unsigned long long)p.bytes);
        }
    }
    return false;
}
void xr_eye_uniform_probe_record(VkCommandBuffer command,uint64_t recording_value,const void* source,size_t bytes){
    if(!output())return;auto& p=probe();std::lock_guard lock(p.mutex);if(p.attempted)return;
    p.first_person=first_person_marker()!=nullptr;
    if(p.first_person&&!p.armed)return;
    p.device=dev.device;p.value=recording_value;p.bytes=bytes;
    if(!command||!p.value||vk_timeline_completed()>=p.value||!source||bytes<0x230||bytes>65536||bytes%4){p.attempted=true;report(p,"invalid_source_or_recording");return;}
    if(!payloads(p,source)){
        if(p.first_person){if(!p.waiting_reported){p.waiting_reported=true;report(p,"waiting_for_frozen_first_person_packet");}return;}
        p.attempted=true;report(p,"camera_payload_rejected");return;
    }
    p.attempted=true;
    constexpr uint64_t budget=1024*1024;
    for(unsigned eye=0;eye<2;++eye){
        if(!create_device_buffer(bytes,VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT|
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,p.buffers[eye],p.memory[eye],true)){release(p);report(p,"allocation_rejected");return;}
        const auto actual=allocation_size(p.memory[eye]);if(!actual||actual>budget-p.allocated){release(p);report(p,"budget_rejected");return;}p.allocated+=actual;
    }
    if(!create_buffer(2*bytes,VK_BUFFER_USAGE_TRANSFER_DST_BIT,true,p.readback,p.readback_memory,&p.mapped,true)||!p.mapped){release(p);report(p,"readback_allocation_rejected");return;}
    const auto actual=allocation_size(p.readback_memory);if(!actual||actual>budget-p.allocated){release(p);report(p,"budget_rejected");return;}p.allocated+=actual;
    p.recorded=true;
    for(unsigned eye=0;eye<2;++eye)vkCmdUpdateBuffer(command,p.buffers[eye],0,bytes,p.payloads[eye].data());
    VkMemoryBarrier2 before{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};before.srcStageMask=VK_PIPELINE_STAGE_2_TRANSFER_BIT;before.srcAccessMask=VK_ACCESS_2_TRANSFER_WRITE_BIT;
    before.dstStageMask=VK_PIPELINE_STAGE_2_TRANSFER_BIT;before.dstAccessMask=VK_ACCESS_2_TRANSFER_READ_BIT;
    VkDependencyInfo dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};dependency.memoryBarrierCount=1;dependency.pMemoryBarriers=&before;vkCmdPipelineBarrier2(command,&dependency);
    for(unsigned eye=0;eye<2;++eye){VkBufferCopy copy{0,eye*bytes,bytes};vkCmdCopyBuffer(command,p.buffers[eye],p.readback,1,&copy);}
    before.srcAccessMask=VK_ACCESS_2_TRANSFER_WRITE_BIT;before.dstStageMask=VK_PIPELINE_STAGE_2_HOST_BIT;before.dstAccessMask=VK_ACCESS_2_HOST_READ_BIT;
    vkCmdPipelineBarrier2(command,&dependency);report(p,"recorded");
}
}
