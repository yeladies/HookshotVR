// SPDX-License-Identifier: GPL-3.0-or-later
#include "eye_page_patches.h"
#include <iostream>
#include <stdexcept>
using namespace hookshot::vr;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(){try{
    constexpr uint64_t a=0x1000000000ull,b=0x7f00000000ull;EyePageStamp stamp{1,2,3,4};
    EyeGuestPagePlan plan;
    check(EyeGuestPagePlan::build({{1,16,4}},{},{{a,1,0},{b,1,0},{a+4096,1,4096}},{{1,8192,0x100000}},stamp,stamp,256,8192,plan),"source plan");
    std::vector<uint64_t> root(kEyeGuestPageCount);root[0]=root[*eye_guest_page_index(b)]=0x100000;root[1]=0x101000;
    auto original=root;std::vector<EyePageDestination> destinations{{0x200000,4096},{0x300000,4096}};EyePagePatchSet patches;
    auto prepare=[&]{return prepare_eye_page_patches(plan,stamp,root.data(),root.size(),destinations,patches);};
    check(prepare()&&root==original,"encode without changing root");
    check(patches.eyes[0].size()==2&&patches.eyes[1].size()==2,"private mappings only");
    check(patches.eyes[0][0].address==0x200000&&patches.eyes[0][1].address==0x200000&&patches.eyes[1][0].address==0x300000,"same-eye aliases and different-eye addresses");
    check(!patches.replay_ready(),"encoding does not authorize replay");
    destinations[1]=destinations[0];check(!prepare()&&patches.eyes[1][0].address==0x300000,"reject eye alias atomically");
    destinations[1]={0x100000,4096};check(!prepare(),"reject original storage");
    destinations[1]={0x300000,4095};check(!prepare(),"reject insufficient allocation");
    destinations[1]={UINT64_MAX-15,4096};check(!prepare(),"reject overflow");
    destinations[1]={0x300000,4096};root[2]=0x300800;check(!prepare(),"reject original page omitted from inventory");root[2]=0;
    root[1]=0;check(!prepare(),"shared mapping changed after capture");root[1]=0x101000;
    ++stamp.submit;check(!prepare(),"reject stale submission");--stamp.submit;
    check(!prepare_eye_page_patches(plan,stamp,root.data(),root.size()-1,destinations,patches),"reject incomplete root");
    check(prepare(),"failure does not poison subsequent valid preparation");
    std::cout<<"Eye page patches preserve root, aliases and submission identity; overlapping storage rejected\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
