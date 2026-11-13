// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <vulkan/vulkan.h>
#include <vector>
namespace mkwvk {
// Owning, move-only snapshot of a bounded full pushed set. Raw data pointers always
// refer to this snapshot's own vectors; no pointer into a mutable renderer writer.
class XrDescriptorSnapshot {
    std::vector<VkWriteDescriptorSet> writes_;
    std::vector<VkDescriptorBufferInfo> buffers_;
    std::vector<VkDescriptorImageInfo> images_;
    std::vector<VkBufferView> views_;
    static int kind(VkDescriptorType type){
        switch(type){
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:return 1;
        case VK_DESCRIPTOR_TYPE_SAMPLER:case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:return 2;
        case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:return 3;
        default:return 0;
        }
    }
public:
    XrDescriptorSnapshot()=default;
    XrDescriptorSnapshot(const XrDescriptorSnapshot&)=delete;XrDescriptorSnapshot& operator=(const XrDescriptorSnapshot&)=delete;
    XrDescriptorSnapshot(XrDescriptorSnapshot&&)=default;XrDescriptorSnapshot& operator=(XrDescriptorSnapshot&&)=default;
    bool capture(const std::vector<VkWriteDescriptorSet>& source){
        if(source.empty()||source.size()>32)return false;size_t total=0,nb=0,ni=0,nv=0;
        for(size_t index=0;index<source.size();++index){const auto& w=source[index];const int k=kind(w.descriptorType);
            if(w.sType!=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET||w.pNext||w.dstArrayElement||!w.descriptorCount||w.descriptorCount>32-total||!k)return false;
            for(size_t previous=0;previous<index;++previous)if(source[previous].dstBinding==w.dstBinding)return false;
            if((k==1&&!w.pBufferInfo)||(k==2&&!w.pImageInfo)||(k==3&&!w.pTexelBufferView))return false;
            total+=w.descriptorCount;if(k==1)nb+=w.descriptorCount;if(k==2)ni+=w.descriptorCount;if(k==3)nv+=w.descriptorCount;
        }
        XrDescriptorSnapshot next;next.writes_.reserve(source.size());next.buffers_.reserve(nb);next.images_.reserve(ni);next.views_.reserve(nv);
        for(auto w:source){const int k=kind(w.descriptorType);w.dstSet=VK_NULL_HANDLE;
            if(k==1){const auto first=next.buffers_.size();next.buffers_.insert(next.buffers_.end(),w.pBufferInfo,w.pBufferInfo+w.descriptorCount);w.pBufferInfo=next.buffers_.data()+first;}
            if(k==2){const auto first=next.images_.size();next.images_.insert(next.images_.end(),w.pImageInfo,w.pImageInfo+w.descriptorCount);w.pImageInfo=next.images_.data()+first;}
            if(k==3){const auto first=next.views_.size();next.views_.insert(next.views_.end(),w.pTexelBufferView,w.pTexelBufferView+w.descriptorCount);w.pTexelBufferView=next.views_.data()+first;}
            next.writes_.push_back(w);
        }
        *this=std::move(next);return true;
    }
    bool set_uniform(uint32_t binding,uint32_t element,const VkDescriptorBufferInfo& value){
        if(!value.buffer||!value.range||value.range==VK_WHOLE_SIZE)return false;
        for(const auto& w:writes_)if(w.dstBinding==binding&&w.descriptorType==VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER&&element<w.descriptorCount){
            buffers_[size_t(w.pBufferInfo-buffers_.data())+element]=value;return true;
        }
        return false;
    }
    bool matches(const std::vector<VkWriteDescriptorSet>& source)const{
        if(source.size()!=writes_.size())return false;
        for(size_t i=0;i<source.size();++i){const auto& a=writes_[i];const auto& b=source[i];
            if(b.sType!=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET||b.pNext||a.dstBinding!=b.dstBinding||a.dstArrayElement!=b.dstArrayElement||a.descriptorCount!=b.descriptorCount||a.descriptorType!=b.descriptorType)return false;
            const int k=kind(a.descriptorType);
            if((k==1&&!b.pBufferInfo)||(k==2&&!b.pImageInfo)||(k==3&&!b.pTexelBufferView))return false;
            for(uint32_t item=0;item<a.descriptorCount;++item){
                if(k==1){const auto& x=a.pBufferInfo[item];const auto& y=b.pBufferInfo[item];if(x.buffer!=y.buffer||x.offset!=y.offset||x.range!=y.range)return false;}
                if(k==2){const auto& x=a.pImageInfo[item];const auto& y=b.pImageInfo[item];if(x.sampler!=y.sampler||x.imageView!=y.imageView||x.imageLayout!=y.imageLayout)return false;}
                if(k==3&&a.pTexelBufferView[item]!=b.pTexelBufferView[item])return false;
            }
        }
        return true;
    }
    const std::vector<VkWriteDescriptorSet>& writes()const{return writes_;}
};
}
