// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "xr_preview_feed.h"
#include <filesystem>
#include <fstream>
#include <string>
namespace mkwvk {
// Optional private, offscreen-only diagnostic: at most three stereo pairs and
// one subsequent menu. Caller owns mode/path validation; no game data enters CI.
class PreviewCapture {
    std::filesystem::path root_;uint64_t pairs_=0;unsigned written_=0;bool seen_pair_=false,menu_written_=false;
    bool ppm(const std::filesystem::path& path,const std::shared_ptr<const std::vector<uint8_t>>& pixels,uint32_t width,uint32_t height){
        if(!pixels||!width||!height||width>1920||height>1080||pixels->size()!=uint64_t(width)*height*4||std::filesystem::exists(path))return false;
        std::vector<uint8_t> rgb(uint64_t(width)*height*3);
        for(size_t i=0;i<rgb.size()/3;++i)for(unsigned c=0;c<3;++c)rgb[3*i+c]=(*pixels)[4*i+c];
        std::ofstream out(path,std::ios::binary);if(!out)return false;
        out<<"P6\n"<<width<<' '<<height<<"\n255\n";out.write(reinterpret_cast<const char*>(rgb.data()),std::streamsize(rgb.size()));return bool(out);
    }
public:
    explicit PreviewCapture(const std::string& root):root_(std::filesystem::u8path(root)){}
    bool offer(const PreviewFrame& frame){
        if(!frame.pixels||!frame.width||!frame.height||frame.width>1920||frame.height>1080||frame.pixels->size()!=uint64_t(frame.width)*frame.height*4||(frame.right_pixels&&frame.right_pixels->size()!=frame.pixels->size()))return false;
        if(root_.empty()||!std::filesystem::is_directory(root_)||std::filesystem::is_symlink(root_))return false;
        if(frame.right_pixels){
            seen_pair_=true;const auto pair=++pairs_;
            if(written_>=3||(pair-1)%60)return false;
            const auto stem="pair-"+std::to_string(++written_);
            if(std::filesystem::exists(root_/(stem+".json")))return false;
            const bool left=ppm(root_/(stem+"-left.ppm"),frame.pixels,frame.width,frame.height);
            const bool right=ppm(root_/(stem+"-right.ppm"),frame.right_pixels,frame.width,frame.height);
            std::ofstream meta(root_/(stem+".json"));
            if(meta)meta<<"{\"schema\":1,\"left_present\":"<<frame.game_present<<",\"left_serial\":"<<frame.serial<<",\"right_serial\":"<<frame.right_serial<<",\"temporal_stereo\":true,\"headset_capture\":false,\"horizontal_fov_rad\":"<<frame.immersive.fov[1]-frame.immersive.fov[0]<<",\"vertical_fov_rad\":"<<frame.immersive.fov[2]-frame.immersive.fov[3]<<"}\n";
            return left&&right&&bool(meta);
        }
        if(seen_pair_&&frame.immersive.menu&&!menu_written_){menu_written_=true;return ppm(root_/"menu.ppm",frame.pixels,frame.width,frame.height);}
        return false;
    }
};
}
