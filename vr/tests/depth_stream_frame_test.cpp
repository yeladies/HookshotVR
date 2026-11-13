// SPDX-License-Identifier: GPL-3.0-or-later
#include "depth_stream_frame.h"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <limits>
using namespace hookshot::vr;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
void write_frame(const std::filesystem::path& path,uint32_t format=123,bool bad_projection=false,bool bad_view=false){
 std::ofstream file(path,std::ios::binary|std::ios::trunc);const char magic[8]={'H','S','V','R','D','2',0,0};uint32_t w=2,h=2;std::array<float,16> p{};p[0]=p[5]=1;p[10]=-1.002f;p[11]=-.2002f;p[14]=-1;
 std::array<float,12> v{1,0,0,0,0,1,0,0,0,0,1,0};if(bad_projection)p[0]=std::numeric_limits<float>::quiet_NaN();if(bad_view)v[0]=2;
 std::array<uint32_t,8> data{1,2,3,4,5,6,7,8};file.write(magic,8);for(auto value:{w,h,format})file.write(reinterpret_cast<const char*>(&value),4);file.write(reinterpret_cast<const char*>(p.data()),64);file.write(reinterpret_cast<const char*>(v.data()),48);file.write(reinterpret_cast<const char*>(data.data()),32);
}
int main(){const auto root=std::filesystem::temp_directory_path()/std::filesystem::u8path("vr-stream-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directory(root);
 try{const auto path=root/"frame.hsvrd";DepthStreamFrame frame;frame.width=77;check(!read_depth_stream_frame(path,frame)&&frame.width==77,"missing frame fails atomically");
 for(uint32_t format:{122u,123u}){write_frame(path,format);check(read_depth_stream_frame(path,frame)&&frame.width==2&&frame.height==2&&frame.pixels.size()==8&&frame.format==format,"both explicit color views accepted");}
 const auto original=frame.digest;
 for(int bad=0;bad<3;++bad){write_frame(path,bad==0?999:123,bad==1,bad==2);check(!read_depth_stream_frame(path,frame)&&frame.digest==original,"bad format/projection/view preserve previous frame");}
 write_frame(path);{std::ofstream file(path,std::ios::binary|std::ios::app);file.put('x');}check(!read_depth_stream_frame(path,frame)&&frame.digest==original,"trailing bytes rejected");
 write_frame(path);{std::fstream file(path,std::ios::binary|std::ios::in|std::ios::out);file.seekp(132);uint32_t value=42;file.write(reinterpret_cast<const char*>(&value),4);}check(read_depth_stream_frame(path,frame)&&frame.digest!=original,"content changes have a different digest");
 const auto identity=depth_stream_rotation({1,0,0,0,0,1,0,0,0,0,1,0});check(identity[0]==1&&identity[5]==1&&identity[10]==1&&identity[15]==1,"identity source orientation");
 const auto yaw=depth_stream_rotation({0,0,-1,0,0,1,0,0,1,0,0,0});check(std::abs(yaw[0]-1)<1e-6&&std::abs(yaw[10]-1)<1e-6,"source yaw is levelled into the preview reference");
 std::filesystem::remove(path);std::filesystem::remove(root);std::cout<<"Bounded complete frames, typed formats, rigid cameras and source orientation verified\n";return 0;
 }catch(const std::exception& e){std::filesystem::remove(root/"frame.hsvrd");std::filesystem::remove(root);std::cerr<<e.what()<<'\n';return 1;}}
