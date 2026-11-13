// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <vector>
#include <filesystem>
#include <fstream>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <utility>
namespace hookshot::vr {
struct DepthStreamFrame {
    uint32_t width=0,height=0,format=0;std::array<float,16> projection{};std::array<float,12> view{};
    std::vector<uint32_t> pixels;uint64_t digest=0;
};
inline bool read_depth_stream_frame(const std::filesystem::path& path,DepthStreamFrame& out){
    std::ifstream file(path,std::ios::binary|std::ios::ate);if(!file)return false;const auto bytes=file.tellg();
    if(bytes<132||bytes>16*1024*1024+132)return false;file.seekg(0);char magic[8];DepthStreamFrame next;
    file.read(magic,8);file.read(reinterpret_cast<char*>(&next.width),4);file.read(reinterpret_cast<char*>(&next.height),4);file.read(reinterpret_cast<char*>(&next.format),4);
    file.read(reinterpret_cast<char*>(next.projection.data()),64);file.read(reinterpret_cast<char*>(next.view.data()),48);
    if(!file||std::memcmp(magic,"HSVRD2\0\0",8)||next.width<2||next.height<2||next.width>1920||next.height>1080||(next.format!=122&&next.format!=123)||bytes!=std::streamoff(132+uint64_t(next.width)*next.height*8))return false;
    const auto& p=next.projection;const auto& v=next.view;
    for(float value:p)if(!std::isfinite(value))return false;for(float value:v)if(!std::isfinite(value))return false;
    if(p[0]<.0001f||p[5]<.0001f||p[0]>10000||p[5]>10000||std::abs(p[2])>4||std::abs(p[6])>4||p[10]>-1||p[11]>=0||std::abs(p[14]+1)>1e-5)return false;
    for(int index:{1,3,4,7,8,9,12,13,15})if(std::abs(p[index])>1e-5)return false;
    for(int r=0;r<3;++r)for(int s=0;s<3;++s){double dot=0;for(int c=0;c<3;++c)dot+=double(v[r*4+c])*v[s*4+c];if(std::abs(dot-(r==s))>.003)return false;}
    const double det=v[0]*(double(v[5])*v[10]-double(v[6])*v[9])-v[1]*(double(v[4])*v[10]-double(v[6])*v[8])+v[2]*(double(v[4])*v[9]-double(v[5])*v[8]);
    if(std::abs(det-1)>.003||v[8]*v[8]+v[10]*v[10]<.0001)return false;
    next.pixels.resize(uint64_t(next.width)*next.height*2);file.read(reinterpret_cast<char*>(next.pixels.data()),std::streamsize(next.pixels.size()*4));if(!file)return false;
    next.digest=14695981039346656037ull;for(auto value:next.pixels){next.digest^=value;next.digest*=1099511628211ull;}
    out=std::move(next);return true;
}
inline std::array<float,16> depth_stream_rotation(const std::array<float,12>& view){
    const float yaw=std::atan2(view[8],view[10]),co=std::cos(yaw),si=std::sin(yaw);std::array<float,16> result{};result[15]=1;
    for(int c=0;c<3;++c){result[c]=co*view[4*c]-si*view[4*c+2];result[4+c]=view[4*c+1];result[8+c]=si*view[4*c]+co*view[4*c+2];}return result;
}
}
