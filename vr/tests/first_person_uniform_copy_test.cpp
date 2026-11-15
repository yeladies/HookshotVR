// SPDX-License-Identifier: GPL-3.0-or-later
#include "xr_first_person_uniform_copy.h"
#include <cstdio>
#include <stdexcept>
static void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
int main(){try{
    MkwVkFirstPersonInput input{};input.size=sizeof input;input.actor_id=42;input.game_present=7;
    input.root_position[0]=10;input.root_position[1]=20;input.root_position[2]=30;input.eye_height=1.5;input.world_units_per_metre=2;
    mkwvk::XrFrameSnapshot tracking{};tracking.sequence=8;tracking.origin_generation=1;tracking.predicted_display_time=200;tracking.observed_at_monotonic_ns=100;
    tracking.running=tracking.focused=tracking.should_render=true;tracking.head.flags=3;tracking.head.orientation[3]=1;
    for(unsigned eye=0;eye<2;++eye){auto& e=tracking.eyes[eye];e.pose=tracking.head;e.pose.position[0]=eye?.032f:-.032f;
        e.fov[0]=-.7f;e.fov[1]=.8f;e.fov[2]=.9f;e.fov[3]=-.6f;}
    mkwvk::FirstPersonFrameOwner owner;MkwVkFirstPersonFrame head{};head.size=sizeof head;
    check(owner.build(input,tracking,false,head),"build immutable packet");mkwvk::FirstPersonProjectionPacket packet{};
    check(owner.project(7,.125,1234,packet),"reference projection");
    double native[16];for(unsigned k=0;k<16;++k)native[k]=float(packet.camera.eyes[0].projection[k]);native[0]=.9f;native[5]=1.2f;
    check(owner.project(7,0,0,packet,native),"exact native depth projection");
    std::vector<std::byte> source(0xb00);for(size_t k=0;k<source.size();++k)source[k]=std::byte(k*37%256);
    float prefix[0xe0/4]{};std::memcpy(prefix,head.head_view,sizeof head.head_view);
    for(unsigned k=0;k<16;++k)prefix[0x70/4+k]=float(native[k]);
    for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c){double v=0;for(unsigned k=0;k<4;++k)v+=native[4*r+k]*(k==3?(c==3?1:0):head.head_view[4*k+c]);prefix[0x30/4+4*r+c]=float(v);}
    hookshot::vr::uniform_detail::inverse(prefix,prefix+0xb0/4);std::memcpy(source.data(),prefix,sizeof prefix);const auto original=source;
    std::array<std::vector<std::byte>,2> copies;
    check(mkwvk::prepare_first_person_primary_uniform_copies(source.data(),source.size(),packet,copies),"new eye copies");
    check(copies[0]!=copies[1]&&source==original,"separate eyes and immutable source");
    for(unsigned eye=0;eye<2;++eye){
        check(copies[eye].size()==source.size()&&!std::memcmp(copies[eye].data()+0xe0,source.data()+0xe0,source.size()-0xe0),"all unknown and temporal bytes preserved");
        check(!std::memcmp(copies[eye].data()+0x70+10*4,source.data()+0x70+10*4,8),"native float depth retained exactly");
        float copied[12];std::memcpy(copied,copies[eye].data(),sizeof copied);
        for(unsigned k=0;k<12;++k)check(copied[k]==float(packet.camera.eyes[eye].view[k]),"frozen located eye view, no artificial shift");
    }
    const auto saved=copies;auto bad=packet;bad.native_depth_preserved=false;
    check(!mkwvk::prepare_first_person_primary_uniform_copies(source.data(),source.size(),bad,copies)&&copies==saved,"native depth provenance required atomically");
    bad=packet;bad.tracking.eyes[1].fov[0]-=.1f;
    check(!mkwvk::prepare_first_person_primary_uniform_copies(source.data(),source.size(),bad,copies)&&copies==saved,"tracking FOV cannot be relabeled");
    bad=packet;bad.camera.world_from_head[3]+=1;
    check(!mkwvk::prepare_first_person_primary_uniform_copies(source.data(),source.size(),bad,copies)&&copies==saved,"wrong center cannot borrow source");
    bad=packet;bad.camera.eyes[1].view[0]*=2;
    check(!mkwvk::prepare_first_person_primary_uniform_copies(source.data(),source.size(),bad,copies)&&copies==saved,"invalid eye fails atomically");
    check(!mkwvk::prepare_first_person_primary_uniform_copies(copies[0].data(),copies[0].size(),packet,copies)&&copies==saved,"in-place alias rejected");
    std::puts("Frozen first-person primary-prefix copies preserve native depth and all unknown bytes");return 0;
}catch(const std::exception& error){std::fprintf(stderr,"%s\n",error.what());return 1;}}
