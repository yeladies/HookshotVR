// SPDX-License-Identifier: GPL-3.0-or-later
// Private frozen-file verification only; no game process or OpenXR session.
#define VK_NO_PROTOTYPES
#include "depth_pair_gpu_fixture.h"
#include "depth_stream_frame.h"
#include <iostream>
#include <fstream>
#include <filesystem>
using namespace hookshot::vr;
using namespace hookshot::vr::smoke;
namespace {
bool inside(const std::filesystem::path& root,const std::filesystem::path& path){auto a=root.begin(),b=path.begin();for(;a!=root.end();++a,++b)if(b==path.end()||*a!=*b)return false;return true;}
}
int main(int argc,char** argv){if(argc!=6||std::strcmp(argv[1],"--run")||std::strcmp(argv[2],"--capture")||std::strcmp(argv[4],"--output")){
    std::cerr<<"Private offscreen GPU check: --run --capture <work/frame.hsvrd> --output <work/new-pair.ppm>\n";return 2;
}
try{
    namespace fs=std::filesystem;
    const auto root=fs::weakly_canonical(fs::current_path()/"work");const auto input=fs::weakly_canonical(fs::u8path(argv[3]));
    const auto output=fs::weakly_canonical(fs::u8path(argv[5]));
    require(fs::is_directory(root)&&inside(root,input)&&inside(root,output)&&output.extension()==".ppm"&&!fs::exists(output)&&fs::is_directory(output.parent_path()),"capture/output must stay inside private project/work; output must be a fresh PPM");
    DepthStreamFrame source;require(read_depth_stream_frame(input,source),"invalid complete private color/depth snapshot");
    std::array<DepthPairParameters,2> cameras;
    for(unsigned eye=0;eye<2;++eye){auto& p=cameras[eye];p.source_from_eye={1,0,0,eye?.032f:-.032f,0,1,0,0,0,0,1,0};
        const auto& q=source.projection;p.eye_tangents={(q[2]-1)/q[0],(q[2]+1)/q[0],(q[6]+1)/q[5],(q[6]-1)/q[5]};
        p.projection={q[0],q[5],q[10],q[11]};p.optical={q[2],q[6],100000,1.25f};p.dimensions={source.width,source.height,source.width,source.height};p.settings={source.format,6,0,0};require(depth_pair_parameters_valid(p),"source-derived camera parameters invalid");
    }
    DepthPairGpuFixture gpu(source.width,source.height,source.width,source.height);
    const auto result=gpu.run(source.pixels,cameras,source.digest?source.digest:1);require(result.source==source.pixels,"original full GPU packet modified");
    const uint64_t pixels=uint64_t(source.width)*source.height;
    for(unsigned eye=0;eye<2;++eye){uint64_t total=0;for(auto n:result.counters[eye])total+=n;require(total==pixels,"GPU pair has incomplete pixel accounting");}
    for(uint32_t word:result.pixels)require((word>>24)==255,"GPU pair has unwritten alpha");
    std::ofstream file(output,std::ios::binary|std::ios::trunc);require(bool(file),"cannot write private PPM");file<<"P6\n"<<2*source.width<<' '<<source.height<<"\n255\n";
    for(uint32_t word:result.pixels){const char rgb[3]{char(word&255),char((word>>8)&255),char((word>>16)&255)};file.write(rgb,3);}file.close();require(bool(file),"private PPM write incomplete");
    std::cout<<"{\"schema\":1,\"width_per_eye\":"<<source.width<<",\"height\":"<<source.height<<",\"source_format\":"<<source.format
        <<",\"valid_pixels\":["<<result.counters[0][0]<<','<<result.counters[1][0]<<"],\"uncovered_pixels\":["<<result.counters[0][1]<<','<<result.counters[1][1]
        <<"],\"invalid_depth_pixels\":["<<result.counters[0][2]<<','<<result.counters[1][2]<<"],\"residual_rejected_pixels\":["<<result.counters[0][3]<<','<<result.counters[1][3]
        <<"],\"same_source_packet\":true,\"original_GPU_packet_unchanged\":true,\"source_file_CPU_upload\":true,\"intermediate_CPU_pixels\":false,\"eye_separation_source_units\":0.064,\"scale_calibrated\":false,\"XR\":false,\"live_game\":false}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
