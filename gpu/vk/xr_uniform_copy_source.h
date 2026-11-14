// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>
namespace mkwvk {
enum class XrUniformSourceKind {Unsupported,Mirror};
struct XrUniformSourceStamp {uint64_t recording=0,pool_generation=0,capture_generation=0;};
struct XrGuestRootSource {uint64_t address=0,bytes=0;XrUniformSourceStamp stamp{};};
struct XrUniformCopySource {
    VkBuffer buffer=VK_NULL_HANDLE;uint64_t bytes=0;VkBufferUsageFlags usage=0;
    XrUniformSourceKind kind=XrUniformSourceKind::Unsupported;XrUniformSourceStamp stamp{};
};
inline bool xr_uniform_copy_source_valid(const XrUniformCopySource& source,VkBuffer expected,uint64_t offset,uint64_t bytes,
                                         const XrUniformSourceStamp& current){
    const auto needed=VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    return expected&&source.buffer==expected&&source.kind==XrUniformSourceKind::Mirror&&(source.usage&needed)==needed&&
        source.stamp.recording&&source.stamp.pool_generation&&source.stamp.capture_generation&&
        source.stamp.recording==current.recording&&source.stamp.pool_generation==current.pool_generation&&source.stamp.capture_generation==current.capture_generation&&
        bytes&&bytes<=65536&&!(offset%4)&&!(bytes%4)&&offset<=source.bytes&&bytes<=source.bytes-offset;
}
}
