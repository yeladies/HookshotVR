// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
namespace mkwvk {
struct ShaderUniformAuditBinding {
    uint64_t module=0;
    uint32_t nvn_stage=0,vk_stage=0,binding=0,array_element=0,bank=0,range_bytes=0;
    bool primary_camera_binding=false;
};
bool xr_shader_uniform_audit_enabled();
bool xr_shader_uniform_audit_module(uint64_t module,const uint32_t* words,size_t count);
bool xr_shader_uniform_audit_wants_program(uint64_t program);
bool xr_shader_uniform_audit_program(uint64_t program,const std::vector<ShaderUniformAuditBinding>&);
}
