// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
#include "xr_uniform_camera_scan.h"
namespace mkwvk {
struct ShaderUniformAuditBinding {
    uint64_t module=0;
    uint32_t nvn_stage=0,vk_stage=0,binding=0,array_element=0,bank=0,range_bytes=0;
    bool primary_camera_binding=false;
    UniformCameraScan camera_candidates;
};
struct ShaderUniformAuditStage {
    uint64_t module=0;uint32_t nvn_stage=0,vk_stage=0;
    bool uses_global_memory=false,writes_storage_buffers=false,writes_storage_images=false,writes_texel_buffers=false;
    bool stores_global_memory=false;
};
bool xr_shader_uniform_audit_enabled();
bool xr_shader_uniform_audit_compute_only();
bool xr_shader_uniform_audit_module(uint64_t module,const uint32_t* words,size_t count);
bool xr_shader_uniform_audit_wants_program(uint64_t program);
bool xr_shader_uniform_audit_identity(uint64_t module,uint64_t& fingerprint,size_t& words);
bool xr_shader_uniform_audit_program(uint64_t program,const std::vector<ShaderUniformAuditBinding>&,
                                    const std::vector<ShaderUniformAuditStage>& stages={});
}
