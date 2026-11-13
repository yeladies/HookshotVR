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
struct PreviewFeedStats {uint64_t committed=0,held=0,unavailable=0,gap_requests=0,unassociated=0,pose_expired=0,image_expired=0,max_pose_age_ns=0;};
class PreviewFeed {
    std::mutex mutex_;
    PreviewFrame frame_{},committed_pair_{};std::array<PreviewFrame,2> eyes_{};
    PreviewFeedStats stats_;
    uint64_t serial_=0;int64_t committed_right_received_=0;
public:
    bool publish(std::shared_ptr<const std::vector<uint8_t>> pixels,uint32_t width,uint32_t height,int64_t present,int64_t now,const ImmersiveTag& immersive={}) {
        if(!pixels||!width||!height||width>1920||height>1080||pixels->size()!=uint64_t(width)*height*4||present<0||now<=0)return false;
        if(immersive.temporal_stereo&&(!immersive.valid||immersive.menu||immersive.eye>1||!immersive.generation))return false;
        std::lock_guard lock(mutex_);
        if(!immersive.valid&&!immersive.menu&&(eyes_[0].pixels||eyes_[1].pixels)){++stats_.unassociated;return false;}
        if(immersive.temporal_stereo){
            if(committed_pair_.pixels&&(committed_pair_.immersive.generation!=immersive.generation||committed_pair_.width!=width||committed_pair_.height!=height))committed_pair_={};
            const auto& previous=eyes_[immersive.eye];
            if(previous.serial&&previous.immersive.generation==immersive.generation&&present<=previous.game_present)return false;
            const auto& other=eyes_[1-immersive.eye];
            if(other.serial&&other.immersive.generation==immersive.generation&&present==other.game_present)return false;
        }
        frame_={std::move(pixels),immersive,width,height,++serial_,present,now};
        if(immersive.temporal_stereo)eyes_[immersive.eye]=frame_;
        else {eyes_={};committed_pair_={};} // Menus and mono images invalidate both old eye images.
        return true;
    }
    PreviewFrame latest(int64_t now) {
        std::lock_guard lock(mutex_);
        if(!frame_.serial||now<frame_.received_ns||now-frame_.received_ns>2000000000)return {};
        if(frame_.immersive.temporal_stereo){
            const auto fresh=[&](const PreviewFrame& eye){
                if(!eye.pixels||now<eye.received_ns||now<eye.immersive.observed_ns)return false;
                stats_.max_pose_age_ns=std::max(stats_.max_pose_age_ns,uint64_t(now-eye.immersive.observed_ns));
                if(now-eye.received_ns>200000000){++stats_.image_expired;return false;}
                if(now-eye.immersive.observed_ns>300000000){++stats_.pose_expired;return false;}return true;
            };
            const auto& a=eyes_[0];const auto& b=eyes_[1];
            if(a.pixels&&b.pixels&&a.width==b.width&&a.height==b.height&&a.immersive.generation==b.immersive.generation){
                const auto gap=a.game_present>b.game_present?a.game_present-b.game_present:b.game_present-a.game_present;
                if(gap>4)++stats_.gap_requests;
                if(gap<=4&&fresh(a)&&fresh(b)){if(committed_pair_.serial!=a.serial||committed_pair_.right_serial!=b.serial)++stats_.committed;committed_pair_=a;committed_pair_.right_pixels=b.pixels;committed_pair_.right_immersive=b.immersive;committed_pair_.right_serial=b.serial;committed_right_received_=b.received_ns;}
            }
            // Publish complete pairs atomically. A lone new eye must not blank a
            // still-fresh accepted pair. Original age/origin/gap limits stay intact.
            const auto& pair=committed_pair_;
            if(!pair.pixels||!pair.right_pixels||!fresh(pair)||pair.immersive.generation!=frame_.immersive.generation){++stats_.unavailable;return {};}
            // Right-eye capture/pose times are retained separately from the left.
            if(now<pair.right_immersive.observed_ns||now-pair.right_immersive.observed_ns>300000000){++stats_.pose_expired;++stats_.unavailable;return {};}
            // Find its receive time by identity; a later replacement may no longer
            // be the committed right eye, so save the original timestamp at commit.
            if(now<committed_right_received_||now-committed_right_received_>200000000){++stats_.image_expired;++stats_.unavailable;return {};}
            if(pair.serial!=a.serial||pair.right_serial!=b.serial)++stats_.held;
            return pair;
        }
        return frame_;
    }
    PreviewFeedStats stats(){std::lock_guard lock(mutex_);return stats_;}
    void clear(bool reset_stats=false) {std::lock_guard lock(mutex_);frame_={};eyes_={};committed_pair_={};committed_right_received_=0;if(reset_stats)stats_={};}
};
}