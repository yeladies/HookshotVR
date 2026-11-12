// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>
namespace mkwvk {
struct PreviewFrame {
    std::shared_ptr<const std::vector<uint8_t>> pixels;
    uint32_t width=0,height=0;
    uint64_t serial=0;
    int64_t game_present=0,received_ns=0;
};
class PreviewFeed {
    std::mutex mutex_;
    PreviewFrame frame_{};
    uint64_t serial_=0;
public:
    bool publish(std::shared_ptr<const std::vector<uint8_t>> pixels,uint32_t width,uint32_t height,int64_t present,int64_t now) {
        if(!pixels||!width||!height||width>1920||height>1080||pixels->size()!=uint64_t(width)*height*4||present<0||now<=0)return false;
        std::lock_guard lock(mutex_);frame_={std::move(pixels),width,height,++serial_,present,now};return true;
    }
    PreviewFrame latest(int64_t now) {
        std::lock_guard lock(mutex_);
        if(!frame_.serial||now<frame_.received_ns||now-frame_.received_ns>2000000000)return {};
        return frame_;
    }
    void clear() {std::lock_guard lock(mutex_);frame_={};}
};
}