// SPDX-License-Identifier: GPL-3.0-or-later
#include "eye_page_layout.h"
#include <iostream>
#include <stdexcept>
using namespace hookshot::vr;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(){try {
    constexpr uint64_t a=0x1000000000ull,b=0x7f00000000ull;EyePageStamp stamp{1,2,3,4};
    check(eye_guest_page_index(a)==0&&eye_guest_page_index(b)==(8ull<<30>>12),"both backend windows");
    check(eye_guest_page_index(b+(4ull<<30)-4096)==kEyeGuestPageCount-1,"last valid page");
    check(!eye_guest_page_index(a+1)&&!eye_guest_page_index(a+(8ull<<30))&&!eye_guest_page_index(0),"alignment/window bounds");
    check(kEyeGuestTableBytes==24ull*1024*1024,"current full guest root table reference");
    std::vector<EyePageObject> objects{{10,16384,0x100000}};
    std::vector<EyeMappedGuestPage> pages{{a,10,0},{a+4096,10,4096},{a+8192,10,8192},{b,10,0}};
    EyeGuestPagePlan plan;
    check(EyeGuestPagePlan::build({{10,4090,20}},{},pages,objects,stamp,stamp,256,16384,plan),"cross-page write plan");
    check(plan.bytes_per_eye_reference()==8192,"only intersecting pages copied");
    auto left=plan.resolve(0,a,true),right=plan.resolve(1,a+4096,true),alias=plan.resolve(0,b,true);
    check(left.route==EyePageRoute::PrivateCandidate&&right.route==left.route&&left.allocation!=right.allocation&&right.offset==4096,"different eyes and complete page spans");
    check(alias.allocation==left.allocation&&alias.offset==left.offset,"guest aliases retain the same private bytes");
    check(plan.resolve(0,a+8192,false).route==EyePageRoute::SharedReadOnlyCandidate&&plan.resolve(0,a+8192,true).route==EyePageRoute::Reject,"unplanned shared writes rejected");
    check(plan.resolve(0,a+12288,false).route==EyePageRoute::Reject&&plan.resolve(2,a,false).route==EyePageRoute::Reject,"unknown mappings/eyes rejected");
    check(!plan.replay_ready(),"no semantic replay authority");
    check(EyeGuestPagePlan::build({{10,4090,20}},{{10,8000,1000}},pages,objects,stamp,stamp,256,16384,plan)&&plan.bytes_per_eye_reference()==12288,"read-descriptor/page closure reaches fixed point");
    check(EyeGuestPagePlan::build({{10,4096,1}},{},pages,objects,stamp,stamp,256,16384,plan)&&plan.bytes_per_eye_reference()==4096&&plan.resolve(0,a,false).route==EyePageRoute::SharedReadOnlyCandidate,"adjacent pages do not spread private ownership");
    auto biased=std::vector<EyeMappedGuestPage>{{a,10,128}};
    check(EyeGuestPagePlan::build({{10,256,4}},{},biased,objects,stamp,stamp,256,16384,plan)&&plan.bytes_per_eye_reference()==4352&&plan.resolve(0,a,true).offset==128,"nonzero native page offset keeps alignment prefix");
    auto overlapping=objects;overlapping.push_back({11,4096,0x100800});
    check(!EyeGuestPagePlan::build({{10,256,4}},{},biased,overlapping,stamp,stamp,256,16384,plan),"ambiguous native object aliases rejected");
    const auto old_bytes=plan.bytes_per_eye_reference();auto stale=stamp;++stale.capture_generation;
    check(!EyeGuestPagePlan::build({{10,256,4}},{},biased,objects,stale,stamp,256,16384,plan)&&plan.bytes_per_eye_reference()==old_bytes,"stale capture cannot replace output");
    auto duplicate=pages;duplicate.push_back(pages.front());
    check(!EyeGuestPagePlan::build({{10,1,4}},{},duplicate,objects,stamp,stamp,256,16384,plan),"duplicate root entries are ambiguous");
    check(!EyeGuestPagePlan::build({{10,4090,20}},{},pages,objects,stamp,stamp,256,8191,plan),"budget uses expanded page allocation");
    check(!EyeGuestPagePlan::build({{10,16000,500}},{},pages,objects,stamp,stamp,256,16384,plan),"source buffer end checked");
    biased[0].offset=14000;check(!EyeGuestPagePlan::build({{10,1,4}},{},biased,objects,stamp,stamp,256,16384,plan),"partial source page rejected");
    std::cout<<"Guest-page candidates close aliases/read ranges, preserve stamp/alignment and reject shared writes\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
