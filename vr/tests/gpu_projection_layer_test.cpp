// SPDX-License-Identifier: GPL-3.0-or-later
#include "gpu_projection_layer.h"
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <type_traits>
using namespace hookshot::vr;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
template<class T>T handle(uint64_t n){if constexpr(std::is_pointer_v<T>)return reinterpret_cast<T>(uintptr_t(n));else return T(n);}
int main(){try{
    GpuProjectionPair good;good.source_state=12;good.device_generation=3;good.width=good.height=1024;good.source_observed_ns=1000000000;good.ready_ns=1020000000;good.immutable=good.complete=true;good.target.origin=9;good.target.serial=17;good.target.observed_ns=1010000000;
    for(unsigned eye=0;eye<2;++eye){auto& e=good.target.eyes[eye];e.orientation_valid=e.position_valid=true;e.orientation={0,.02f,0,float(std::sqrt(1-.0004))};e.position={eye?.032f:-.032f,1.6f,.1f};e.fov={eye?-.7f:-.75f,eye?.75f:.7f,.8f,-.72f};}
    const GpuProjectionSwapchain image{handle<XrSwapchain>(2),3,2048,1024,1,true,true};const auto local=handle<XrSpace>(1);GpuProjectionData data;
    check(prepare_gpu_projection_layer(good,image,local,9,1030000000,data),"one complete GPU pair forms a projection layer");auto layer=data.layer();check(layer.space==local&&layer.viewCount==2&&layer.views==data.views.data()&&layer.layerFlags==0,"opaque projection owns two current views");
    for(unsigned eye=0;eye<2;++eye){const auto& v=layer.views[eye];check(v.pose.position.x==good.target.eyes[eye].position[0]&&v.fov.angleLeft==good.target.eyes[eye].fov[0]&&v.fov.angleRight==good.target.eyes[eye].fov[1],"pose and full asymmetric FOV are the production sample's values");check(v.subImage.swapchain==image.chain&&v.subImage.imageRect.offset.x==int(eye*1024)&&v.subImage.imageRect.extent.width==1024&&v.subImage.imageRect.extent.height==1024,"each eye uses its complete GPU image rectangle");}
    auto copy=data;auto copy_layer=copy.layer();check(copy_layer.views==copy.views.data()&&copy_layer.views!=layer.views,"copied layer metadata never points into a temporary or original object");
    const auto accepted=data;auto reject=[&](auto pair,auto target){data=accepted;check(!prepare_gpu_projection_layer(pair,target,local,9,1030000000,data)&&std::memcmp(&accepted,&data,sizeof data)==0,"rejection preserves both views atomically");};
    auto p=good;p.temporal_source=true;reject(p,image);p=good;p.complete=false;reject(p,image);p=good;p.immutable=false;reject(p,image);p=good;p.target.origin=8;reject(p,image);p=good;p.target.serial=0;reject(p,image);p=good;p.ready_ns=1040000000;reject(p,image);p=good;p.source_observed_ns=1;reject(p,image);p=good;p.target.observed_ns=999999999;reject(p,image);p=good;p.target.eyes[1].position_valid=false;reject(p,image);p=good;p.target.eyes[1].orientation={0,0,0,0};reject(p,image);p=good;p.target.eyes[1].fov[0]=.5f;reject(p,image);
    auto t=image;t.device_generation=4;reject(good,t);t=image;t.acquired_and_waited=false;reject(good,t);t=image;t.pair_copy_completed=false;reject(good,t);t=image;t.array_size=2;reject(good,t);t=image;t.width=1024;reject(good,t);
    check(!prepare_gpu_projection_layer(good,image,local,10,1030000000,data),"recenter invalidates an old-origin pair");
    std::cout<<"Projection metadata preserves the GPU pair's exact pose/FOV, full eye rectangles, copy lifetime and origin/age/acquire scope\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
