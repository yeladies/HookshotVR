// SPDX-License-Identifier: GPL-3.0-or-later
#include "xr_preview_capture.h"
#include <chrono>
#include <iostream>
#include <stdexcept>
using namespace mkwvk;
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
int main(){namespace fs=std::filesystem;const auto parent=fs::temp_directory_path();const auto root=parent/("hookshot-private-capture-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));try{
    check(root.parent_path()==parent&&fs::create_directory(root),"own fresh temp directory");PreviewCapture capture(root.u8string());
    PreviewFrame frame;frame.width=2;frame.height=1;frame.pixels=std::make_shared<const std::vector<uint8_t>>(std::vector<uint8_t>{17,34,51,255,85,102,119,255});
    frame.right_pixels=std::make_shared<const std::vector<uint8_t>>(8,180);frame.immersive.valid=true;frame.immersive.temporal_stereo=true;frame.immersive.generation=1;
    for(int i=0;i<220;++i)capture.offer(frame);
    frame.right_pixels.reset();frame.immersive={};frame.immersive.menu=true;check(capture.offer(frame),"one post-world menu captured");check(!capture.offer(frame),"menu capture bounded");
    size_t count=0;for(const auto& entry:fs::directory_iterator(root)){(void)entry;++count;}check(count==10,"three pairs plus metadata and one menu only");
    std::ifstream in(root/"pair-1-left.ppm",std::ios::binary);std::string bytes((std::istreambuf_iterator<char>(in)),{});const std::string header="P6\n2 1\n255\n";
    check(bytes.size()==header.size()+6&&bytes.substr(0,header.size())==header,"bounded standard image header");
    check(uint8_t(bytes[header.size()])==17&&uint8_t(bytes[header.size()+5])==119,"exact original RGB channels, alpha omitted");
    in.close(); frame.right_pixels=std::make_shared<const std::vector<uint8_t>>(8,180); const auto original=fs::file_size(root/"pair-1-left.ppm");PreviewCapture again(root.u8string());check(!again.offer(frame)&&fs::file_size(root/"pair-1-left.ppm")==original,"no overwrite");
    fs::remove_all(root);std::cout<<"Private preview capture bounds, pixel identity and no-overwrite checks pass\n";return 0;
}catch(const std::exception& e){if(root.parent_path()==parent&&fs::exists(root))fs::remove_all(root);std::cerr<<e.what()<<'\n';return 1;}}
