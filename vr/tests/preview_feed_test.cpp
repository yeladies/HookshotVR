// SPDX-License-Identifier: GPL-3.0-or-later
#include "xr_preview_feed.h"
#include <stdexcept>
#include <iostream>
using namespace mkwvk;
void check(bool x){if(!x)throw std::runtime_error("preview feed");}
int main(){try{
 PreviewFeed feed;auto image=std::make_shared<std::vector<uint8_t>>(4*2*4,42);
 check(!feed.publish(image,0,2,0,1));check(feed.publish(image,4,2,10,100));
 auto a=feed.latest(101);check(a.serial==1&&a.pixels==image&&a.game_present==10);
 auto next=std::make_shared<std::vector<uint8_t>>(4*2*4,99);check(feed.publish(next,4,2,11,102));
 auto b=feed.latest(103);check(b.serial==2&&(*b.pixels)[0]==99&&(*a.pixels)[0]==42);
 check(!feed.latest(2000000200).pixels);check(!feed.latest(50).pixels);feed.clear();check(!feed.latest(104).pixels);
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}