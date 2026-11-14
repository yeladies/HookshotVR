// SPDX-License-Identifier: GPL-3.0-or-later
#include "xr_camera_payload_capture.h"
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace mkwvk;
void check(bool yes,const char* why){if(!yes)throw std::runtime_error(why);}
int main(){try{
    std::vector<uint32_t> words(13568/4,0x7fc00001);words[0]=0xffffffff;auto same=words;
    XrCameraPayloadEvidence e{1,2,9,0x1000000000ull,{9,3,4},1152,648,true,true,true,true};std::string output="unchanged";
    check(xr_camera_payload_json(words.data(),same.data(),13568,e,output),"complete source serializes after verification");
    check(output.find("4294967295,2143289345")!=std::string::npos&&output.find("NaN")==std::string::npos,"unknown words retain exact bits as integers");
    check(output.find("\"bytes\":13568")!=std::string::npos&&output.find("\"pool_generation\":3")!=std::string::npos,"scope and full extent recorded");
    for(int fault=0;fault<8;++fault){auto q=e;if(fault==0)q.completed=false;if(fault==1)q.aliases=false;if(fault==2)q.matrices=false;if(fault==3)q.indirect=false;if(fault==4)q.stamp.recording=8;if(fault==5)q.stamp.pool_generation=0;if(fault==6)q.guest++;if(fault==7)q.width=0;
        output="unchanged";check(!xr_camera_payload_json(words.data(),same.data(),13568,q,output)&&output=="unchanged","bad provenance leaves output intact");}
    same.back()^=1;check(!xr_camera_payload_json(words.data(),same.data(),13568,e,output),"mutation beyond prefix rejected");
    check(!xr_camera_payload_json(words.data(),words.data(),559,e,output),"short source rejected");
    std::vector<uint32_t> largest(65536/4,UINT32_MAX);check(xr_camera_payload_json(largest.data(),largest.data(),65536,e,output)&&output.size()<256*1024,"maximum source has bounded JSON size");
    check(!write_xr_camera_payload(nullptr,words.data(),words.data(),13568,e),"default-off export performs no I/O");
    std::cout<<"Completed camera serialization preserves full words and rejects stale, unverified or changed source\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
