// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include "xr_immersive_camera.h"
#include <memory>
#include <mutex>
#include <vector>
namespace mkwvk {
struct PreviewFrame {
    std::shared_ptr<const std::vector<uint8_t>> pixels;
    ImmersiveTag immersive;
    uint32_t width=0,height=0;
    uint64_t serial=0;
    int64_t game_present=0,received_ns=0;
    std::shared_ptr<const std::vector<uint8_t>> right_pixels;
    ImmersiveTag right_immersive;uint64_t right_serial=0;
};
class PreviewFeed {
    std::mutex mutex_;
    PreviewFrame frame_{};std::array<PreviewFrame,2> eyes_{};
    uint64_t serial_=0;
public:
    bool publish(std::shared_ptr<const std::vector<uint8_t>> pixels,uint32_t width,uint32_t height,int64_t present,int64_t now,const ImmersiveTag& immersive={}) {
        if(!pixels||!width||!height||width>1920||height>1080||pixels->size()!=uint64_t(width)*height*4||present<0||now<=0)return false;
        if(immersive.temporal_stereo&&(!immersive.valid||immersive.menu||immersive.eye>1||!immersive.generation))return false;
        std::lock_guard lock(mutex_);
        if(immersive.temporal_stereo){
            const auto& previous=eyes_[immersive.eye];
            if(previous.serial&&previous.immersive.generation==immersive.generation&&present<=previous.game_present)return false;
            const auto& other=eyes_[1-immersive.eye];
            if(other.serial&&other.immersive.generation==immersive.generation&&present==other.game_present)return false;
        }
        frame_={std::move(pixels),immersive,width,height,++serial_,present,now};
        if(immersive.temporal_stereo)eyes_[immersive.eye]=frame_;
        else eyes_={}; // Menus and mono images invalidate both old eye images.
        return true;
    }
    PreviewFrame latest(int64_t now) {
        std::lock_guard lock(mutex_);
        if(!frame_.serial||now<frame_.received_ns||now-frame_.received_ns>2000000000)return {};
        if(frame_.immersive.temporal_stereo){
            const auto& a=eyes_[0];const auto& b=eyes_[1];
            if(!a.pixels||!b.pixels||a.width!=b.width||a.height!=b.height||a.immersive.generation!=b.immersive.generation)return {};
            for(const auto* eye:{&a,&b})if(now<eye->received_ns||now-eye->received_ns>200000000||now<eye->immersive.observed_ns||now-eye->immersive.observed_ns>300000000)return {};
            const auto gap=a.game_present>b.game_present?a.game_present-b.game_present:b.game_present-a.game_present;
            if(gap>4)return {}; // Explicit temporal pair, never an arbitrarily old opposite eye.
            auto pair=a;pair.right_pixels=b.pixels;pair.right_immersive=b.immersive;pair.right_serial=b.serial;return pair;
        }
        return frame_;
    }
    void clear() {std::lock_guard lock(mutex_);frame_={};eyes_={};}
};
}