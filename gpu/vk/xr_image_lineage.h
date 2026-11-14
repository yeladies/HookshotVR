// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <algorithm>
#include <cstddef>
#include <utility>
#include <set>
#include <cstdint>
#include <vector>
#include <map>
namespace mkwvk {
enum class ImageWorkKind { Graphics, Compute, Replacement, Transfer, Clear };
struct BufferSpan {
    uint64_t object=0,offset=0,size=0;
    bool valid()const{return object&&size&&offset<=UINT64_MAX-size;}
};
inline bool buffer_overlap(const BufferSpan& a,const BufferSpan& b){return a.valid()&&b.valid()&&a.object==b.object&&a.offset<b.offset+b.size&&b.offset<a.offset+a.size;}
struct BufferUse {BufferSpan range;bool written=false;};
struct BufferVersion {BufferSpan range;uint64_t version=0;bool dependent=false;};
struct ImageWork {
    ImageWorkKind kind=ImageWorkKind::Graphics;
    uint64_t program=0;
    bool camera_seed=false,opaque_buffers=false,truncated=false;
    uint32_t width=0,height=0;
    std::vector<uint64_t> reads,writes;
    std::vector<BufferUse> buffers;
};
struct ImageResourceDescription {
    uint64_t image=0,allocation_bytes=0;
    uint32_t format=0,width=0,height=0,depth=0,levels=0,layers=0,samples=0,aspect=0,type=0;
    bool sparse=false;
};
struct ImageVersion {uint64_t image=0,version=0;bool dependent=false;};
struct ImageWorkRecord {ImageWork work;bool dependent=false;std::vector<ImageVersion> inputs,outputs;std::vector<BufferVersion> buffer_inputs,buffer_outputs;};
struct ImageGraphStats {uint64_t events=0,seeds=0,dependent_graphics=0,dependent_compute=0,shared=0,opaque=0;bool overflow=false;};
// Conservative whole-image lineage. Does not prove shader access, subresource coverage or replay safety.
class FrameImageGraph {
    std::map<uint64_t,ImageVersion> images_;
    std::map<uint64_t,ImageResourceDescription> descriptions_;
    std::vector<ImageWorkRecord> records_;
    ImageGraphStats stats_{};
    std::vector<BufferVersion> buffer_writes_;uint64_t buffer_version_=0;
public:
    static constexpr size_t max_events=4096,max_images=2048,max_bindings=64,max_buffer_writes=4096,max_buffer_inputs=512;
    void reset(){descriptions_.clear();images_.clear();records_.clear();stats_={};buffer_writes_.clear();buffer_version_=0;}
    void describe(ImageResourceDescription description) {
        if(!description.image||!description.format||!description.width||!description.height||!description.depth||!description.levels||!description.layers||!description.samples) {
            stats_.overflow=true;return;
        }
        if(!descriptions_.count(description.image)&&descriptions_.size()>=max_images){stats_.overflow=true;return;}
        auto it=descriptions_.find(description.image);
        if(it!=descriptions_.end()) {
            const auto& old=it->second;
            if(old.format!=description.format||old.width!=description.width||old.height!=description.height||old.depth!=description.depth||
               old.layers!=description.layers||old.levels!=description.levels||old.samples!=description.samples||old.aspect!=description.aspect||old.type!=description.type) {
                stats_.overflow=true;return;
            }
            description.allocation_bytes=std::max(description.allocation_bytes,old.allocation_bytes);
        }
        descriptions_[description.image]=description;
    }
    const std::map<uint64_t,ImageResourceDescription>& descriptions()const{return descriptions_;}    void observe(ImageWork work) {
        if(records_.size()>=max_events||work.reads.size()>max_bindings||work.writes.size()>max_bindings||work.buffers.size()>max_bindings||work.truncated) {
            stats_.overflow=true;return;
        }
        size_t new_buffers=0;
        for(const auto& use:work.buffers){if(!use.range.valid()){stats_.overflow=true;return;}if(use.written)++new_buffers;}
        if(buffer_writes_.size()+new_buffers>max_buffer_writes){stats_.overflow=true;return;}
        std::set<uint64_t> new_images;
        for(uint64_t id:work.writes)if(id&&!images_.count(id))new_images.insert(id);
        if(images_.size()+new_images.size()>max_images){stats_.overflow=true;return;}
        ImageWorkRecord record{};record.dependent=work.camera_seed;record.work=std::move(work);
        for(uint64_t id:record.work.reads)if(id) {
            auto it=images_.find(id);const auto prior=it==images_.end()?ImageVersion{id,0,false}:it->second;
            record.inputs.push_back(prior);record.dependent|=prior.dependent;
        }
        // Attachment blending and partial storage writes may retain earlier destination pixels.
        // Read-modify-write is conservative for written storage ranges too. Keep imported baseline explicit.
        std::set<uint64_t> seen_buffer_versions;
        for(const auto& use:record.work.buffers) {
            record.buffer_inputs.push_back({use.range,0,false});
            for(const auto& prior:buffer_writes_)if(buffer_overlap(use.range,prior.range)&&seen_buffer_versions.insert(prior.version).second) {
                record.buffer_inputs.push_back(prior);record.dependent|=prior.dependent;
                if(record.buffer_inputs.size()>max_buffer_inputs){stats_.overflow=true;return;}
            }
        }
        for(uint64_t id:record.work.writes)if(id) {
            auto it=images_.find(id);if(it!=images_.end()) {
                record.dependent|=it->second.dependent;
                bool found=false;for(const auto& prior:record.inputs)if(prior.image==id&&prior.version==it->second.version)found=true;
                if(!found)record.inputs.push_back(it->second);
            }
        }
        for(uint64_t id:record.work.writes)if(id) {
            bool duplicate=false;for(const auto& out:record.outputs)if(out.image==id)duplicate=true;
            if(duplicate)continue;
            if(!images_.count(id)&&images_.size()>=max_images){stats_.overflow=true;return;}
            auto& latest=images_[id];latest={id,latest.version+1,record.dependent};record.outputs.push_back(latest);
        }
        for(const auto& use:record.work.buffers)if(use.written) {
            BufferVersion version{use.range,++buffer_version_,record.dependent};buffer_writes_.push_back(version);record.buffer_outputs.push_back(version);
        }
        ++stats_.events;if(record.work.camera_seed)++stats_.seeds;
        if(record.work.opaque_buffers)++stats_.opaque;
        if(!record.dependent)++stats_.shared;
        else if(record.work.kind==ImageWorkKind::Graphics)++stats_.dependent_graphics;
        else ++stats_.dependent_compute;
        records_.push_back(std::move(record));
    }
    bool dependent(uint64_t image) const {auto it=images_.find(image);return it!=images_.end()&&it->second.dependent;}
    const ImageGraphStats& stats() const{return stats_;}
    const std::vector<ImageWorkRecord>& records()const{return records_;}
    // Copies, aliases, hidden pointers and subresources are not completely covered by this observer.
    bool replay_ready()const{return false;}
};
bool xr_image_lineage_enabled();
void xr_image_lineage_work(uint64_t frame,ImageWork);
void xr_image_lineage_resource(uint64_t frame,ImageResourceDescription);
void xr_image_lineage_present(uint64_t frame,uint64_t image);
}
