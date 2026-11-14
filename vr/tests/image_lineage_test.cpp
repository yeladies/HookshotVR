// SPDX-License-Identifier: GPL-3.0-or-later
#include "xr_image_lineage.h"
#include <iostream>
#include <stdexcept>
using namespace mkwvk;
void check(bool x,const char* why){if(!x)throw std::runtime_error(why);}
ImageWork work(ImageWorkKind kind,std::vector<uint64_t> reads,std::vector<uint64_t> writes,bool seed=false){ImageWork w{};w.kind=kind;w.reads=std::move(reads);w.writes=std::move(writes);w.camera_seed=seed;return w;}
int main(){try {
    FrameImageGraph graph;
    graph.observe(work(ImageWorkKind::Compute,{100},{10}));
    graph.observe(work(ImageWorkKind::Graphics,{10},{11,11},true));
    graph.observe(work(ImageWorkKind::Compute,{11},{12}));
    graph.observe(work(ImageWorkKind::Graphics,{12},{13}));
    graph.observe(work(ImageWorkKind::Graphics,{200},{13}));
    check(graph.dependent(13)&&!graph.dependent(10),"camera lineage propagates through compute and retained UI destination");
    check(graph.stats().events==5&&graph.stats().seeds==1&&graph.stats().shared==1&&graph.stats().dependent_graphics==3&&graph.stats().dependent_compute==1,"one record per emitted command");
    const auto& records=graph.records();check(records[1].outputs.size()==1,"duplicate attachment not double-versioned");
    check(records[4].outputs[0].version==2&&records[4].inputs.back().image==13&&records[4].inputs.back().version==1,"retained destination dependency refers to previous version, never itself");
    check(!graph.replay_ready(),"partial observations cannot authorize stereo replay");
    auto opaque=work(ImageWorkKind::Compute,{13},{14});opaque.opaque_buffers=true;graph.observe(opaque);
    check(graph.stats().opaque==1&&graph.dependent(14),"unresolved buffer dependencies remain visible");
    auto huge=work(ImageWorkKind::Compute,{},{});huge.reads.resize(65,11);const auto before=graph.records().size();graph.observe(huge);
    check(graph.stats().overflow&&graph.records().size()==before,"oversized descriptor lists rejected without partial record");
    graph.reset();graph.observe(work(ImageWorkKind::Graphics,{}, {1},true));
    graph.observe(work(ImageWorkKind::Transfer,{1},{2}));
    graph.observe(work(ImageWorkKind::Clear,{}, {2}));
    check(graph.dependent(2)&&graph.records()[2].inputs[0].version==1,"transfer follows source; clear conservatively retains partial destination");
    auto to_buffer=work(ImageWorkKind::Transfer,{2},{});to_buffer.buffers.push_back({{900,0,16},true});graph.observe(to_buffer);
    auto from_buffer=work(ImageWorkKind::Transfer,{}, {3});from_buffer.buffers.push_back({{900,0,16},false});graph.observe(from_buffer);
    check(graph.dependent(3),"texture-buffer-texture chain preserves eye dependence");    graph.reset();check(!graph.dependent(14)&&!graph.stats().overflow,"new frame has no stale image identity");
    for(size_t i=0;i<FrameImageGraph::max_events+1;++i)graph.observe(work(ImageWorkKind::Graphics,{},{1},true));
    check(graph.stats().overflow&&graph.records().size()==FrameImageGraph::max_events,"event storage bounded");
    graph.reset();for(uint64_t i=1;i<=FrameImageGraph::max_images;++i)graph.observe(work(ImageWorkKind::Compute,{}, {i}));
    auto extra=work(ImageWorkKind::Graphics,{}, {1,999999},true);graph.observe(extra);
    check(graph.stats().overflow&&!graph.dependent(1),"resource overflow does not partly modify existing versions");
    graph.reset();auto source=work(ImageWorkKind::Graphics,{}, {1},true);source.buffers.push_back({{500,0,16},true});graph.observe(source);
    auto overlap=work(ImageWorkKind::Compute,{}, {2});overlap.buffers.push_back({{500,8,4},false});graph.observe(overlap);
    auto disjoint=work(ImageWorkKind::Compute,{}, {3});disjoint.buffers.push_back({{500,32,16},false});graph.observe(disjoint);
    check(graph.dependent(2)&&!graph.dependent(3),"only overlapping ranges propagate camera dependence");
    check(graph.records()[1].buffer_inputs.size()==2&&graph.records()[1].buffer_inputs[1].version==1,"buffer read keeps prior writer and imported baseline");
    auto invalid=work(ImageWorkKind::Compute,{}, {4});invalid.buffers.push_back({{500,UINT64_MAX,2},true});graph.observe(invalid);
    check(graph.stats().overflow&&!graph.dependent(4),"invalid overflowing range rejected before writes");    graph.reset();ImageResourceDescription description{5,4096,37,32,32,1,1,1,1,1,1,false};graph.describe(description);
    description.allocation_bytes=8192;graph.describe(description);
    check(graph.descriptions().at(5).allocation_bytes==8192,"image inventory retains conservative allocation reference");
    description.width=64;graph.describe(description);check(graph.stats().overflow,"same image identity cannot silently change shape within frame");    std::cout<<"Conservative image versions, dependency propagation and bounds checked\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
