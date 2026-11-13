// SPDX-License-Identifier: GPL-3.0-or-later
#define VK_NO_PROTOTYPES
#include "xr_descriptor_snapshot.h"
#include <iostream>
#include <stdexcept>
#include <type_traits>
using namespace mkwvk;
template<class T>T handle(uint64_t n){if constexpr(std::is_pointer_v<T>)return reinterpret_cast<T>(uintptr_t(n));else return T(n);}
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(){try{
    static_assert(!std::is_copy_constructible_v<XrDescriptorSnapshot>);
    VkDescriptorBufferInfo buffers[2]={{handle<VkBuffer>(1),256,4096},{handle<VkBuffer>(2),512,1024}};
    VkDescriptorImageInfo image{handle<VkSampler>(3),handle<VkImageView>(4),VK_IMAGE_LAYOUT_GENERAL};VkBufferView view=handle<VkBufferView>(5);
    std::vector<VkWriteDescriptorSet> writes(3);
    for(uint32_t i=0;i<3;++i){writes[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;writes[i].dstBinding=i;writes[i].descriptorCount=1;}
    writes[0].descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;writes[0].descriptorCount=2;writes[0].pBufferInfo=buffers;
    writes[1].descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;writes[1].pImageInfo=&image;
    writes[2].descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;writes[2].pTexelBufferView=&view;
    XrDescriptorSnapshot snapshot;check(snapshot.capture(writes)&&snapshot.matches(writes),"full bounded set copied");
    buffers[0].offset=768;image.imageView=handle<VkImageView>(6);view=handle<VkBufferView>(7);
    check(snapshot.writes()[0].pBufferInfo[0].offset==256&&snapshot.writes()[1].pImageInfo[0].imageView==handle<VkImageView>(4)&&snapshot.writes()[2].pTexelBufferView[0]==handle<VkBufferView>(5),"buffer/image/view payloads own their storage");
    check(!snapshot.matches(writes),"changed source descriptors detected");
    XrDescriptorSnapshot moved=std::move(snapshot);XrDescriptorSnapshot eye;check(eye.capture(moved.writes()),"deep eye copy from owning source");
    check(eye.set_uniform(0,1,{handle<VkBuffer>(8),0,1024}),"one array element replaced");
    check(eye.writes()[0].pBufferInfo[0].buffer==handle<VkBuffer>(1)&&eye.writes()[0].pBufferInfo[1].buffer==handle<VkBuffer>(8)&&moved.writes()[0].pBufferInfo[1].buffer==handle<VkBuffer>(2),"private replacement leaves original and other elements untouched");
    check(!eye.set_uniform(2,0,{handle<VkBuffer>(8),0,1024})&&!eye.set_uniform(0,2,{handle<VkBuffer>(8),0,1024}),"wrong type and array bounds reject");
    check(!eye.set_uniform(0,0,{handle<VkBuffer>(8),0,VK_WHOLE_SIZE}),"replacement range must be explicit");
    auto invalid=writes;invalid[0].pBufferInfo=nullptr;check(!eye.capture(invalid)&&eye.writes()[0].pBufferInfo[1].buffer==handle<VkBuffer>(8),"capture fails atomically");
    invalid=writes;invalid[1].dstBinding=0;check(!eye.capture(invalid),"duplicate binding rejected");
    invalid=writes;invalid[0].descriptorCount=33;check(!eye.capture(invalid),"total descriptor budget enforced before pointer reads");
    invalid=writes;invalid[0].descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;check(!eye.capture(invalid),"dynamic offsets require separate reviewed handling");
    invalid=writes;invalid[0].pNext=&image;check(!eye.capture(invalid),"unknown extension chain rejected");
    check(!eye.capture({}),"empty snapshot is not a full source set");
    std::cout<<"Owning descriptor snapshots preserve source state, array elements and pointers across moves\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
