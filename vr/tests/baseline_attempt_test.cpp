// SPDX-License-Identifier: GPL-3.0-or-later
#include "xr_baseline_attempt.h"
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace mkwvk;
void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
int main(){try{
    using P=XrBaselineAttempt::Phase;using R=XrBaselineComparison::Result;
    XrBaselineAttempt a;
    check(!a.prepare(0,9,3,4,5)&&a.phase()==P::Idle,"invalid preparation leaves idle");
    check(a.prepare(1,9,3,4,5),"prepared source draw");
    check(!a.finish(2,9,3,4,5)&&!a.finish(1,10,3,4,5)&&!a.finish(1,9,6,4,5)&&!a.finish(1,9,3,6,5)&&!a.finish(1,9,3,4,6),"all source identities required");
    a.next_draw(1);check(a.phase()==P::Prepared,"same draw still valid");
    a.next_draw(2);check(a.phase()==P::Abandoned&&!a.finish(2,9,3,4,5),"skipped original cannot replay on next draw");
    check(!a.retire(4,8,true)&&a.phase()==P::Abandoned,"inflight abandoned resources retained");
    check(a.retire(4,9,true)&&a.phase()==P::Retired,"actual queue completion releases abandoned storage");
    check(!a.prepare(3,10,3,4,5),"attempt cannot silently reuse its serial");
    XrBaselineAttempt b;check(b.prepare(1,9,3,4,5)&&b.finish(1,9,3,4,5),"matching original completes");
    check(b.completed_draw()&&!b.retire(4,8,true)&&b.completed_draw(),"no early retirement");
    check(b.retire(4,10,true),"later same-device timeline retires");
    XrBaselineAttempt c;c.prepare(1,9,3,4,5);c.abandon();check(!c.retire(6,100,true)&&c.phase()==P::Retained&&!c.retire(4,100,true),"device change retains permanently");
    XrBaselineAttempt d;d.prepare(1,9,3,4,5);d.finish(1,9,3,4,5);check(!d.retire(4,100,false)&&d.phase()==P::Retained,"unknown completion retains storage");
    std::vector<unsigned char> pixels(4*4*4,0);
    check(compare_xr_baseline(pixels.data(),pixels.size(),2,2).result==R::Invisible,"unchanged source inconclusive");
    pixels[16]=pixels[32]=pixels[48]=1;
    auto result=compare_xr_baseline(pixels.data(),pixels.size(),2,2);
    check(result.result==R::Match&&result.changed_pixels==1&&!result.mismatched_pixels[0]&&!result.mismatched_pixels[1],"both exact outputs and visible source required");
    pixels[49]=2;result=compare_xr_baseline(pixels.data(),pixels.size(),2,2);check(result.result==R::Mismatch&&result.mismatched_pixels[1]==1,"one channel bit mismatch detected");
    pixels[0]=1;check(compare_xr_baseline(pixels.data(),pixels.size(),2,2).result==R::Mismatch,"invisible source cannot hide corruption");
    check(compare_xr_baseline(nullptr,64,2,2).result==R::Invalid&&compare_xr_baseline(pixels.data(),63,2,2).result==R::Invalid&&compare_xr_baseline(pixels.data(),64,0,2).result==R::Invalid&&compare_xr_baseline(pixels.data(),64,4097,2).result==R::Invalid,"bounded complete readback required");
    std::cout<<"Baseline attempts reject skipped draws and retain unknown GPU work; four-frame comparisons detect every pixel mismatch\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
