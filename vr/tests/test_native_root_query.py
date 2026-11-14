"""Execute the actual read-only native root query with authored metadata."""
import argparse
from pathlib import Path
import subprocess
import tempfile

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--cxx',required=True);parser.add_argument('--vulkan-include',required=True);args=parser.parse_args()
    root=Path(__file__).resolve().parents[2];source=(root/'gpu/vk/vk_memory.cpp').read_text(encoding='utf-8-sig')
    start=source.index('bool memory_xr_guest_root_source(');at=source.index('{',start)+1;depth=1
    while depth:
        depth+=(source[at]=='{')-(source[at]=='}');at+=1
    body=source[start:at]
    fixture=r'''
#define VK_NO_PROTOTYPES
#include "xr_uniform_copy_source.h"
#include <atomic>
#include <mutex>
#include <stdexcept>
#include <iostream>
using namespace mkwvk;
struct {VkDevice device=reinterpret_cast<VkDevice>(uintptr_t(1));} dev;
std::atomic<bool> par_active{false};std::atomic<uint64_t> xr_root_address{0},xr_root_device{0},xr_root_bytes{0},pools_gen{17};
std::mutex pools_mu;uint64_t mem_rec_value=9,capture_generation=19;unsigned created=0;
uint64_t memory_capture_view(){return capture_generation;}
uint64_t memory_guest_pages(){++created;return 0;}
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
QUERY
int main(){try{
    XrGuestRootSource out{123,456,{1,2,3}};
    auto rejected=[&](){out={123,456,{1,2,3}};check(!memory_xr_guest_root_source(9,out)&&out.address==123&&out.bytes==456,"query rejection leaves output intact");};
    rejected();check(created==0,"missing root never initializes a table");
    xr_root_address=0x1000;xr_root_bytes=25165824;xr_root_device=2;rejected();
    xr_root_device=1;par_active=true;rejected();par_active=false;
    mem_rec_value=8;rejected();mem_rec_value=9;pools_gen=0;rejected();pools_gen=17;
    capture_generation=0;rejected();capture_generation=19;xr_root_bytes=0;rejected();xr_root_bytes=25165824;
    check(memory_xr_guest_root_source(9,out)&&out.address==0x1000&&out.bytes==25165824&&out.stamp.recording==9&&out.stamp.pool_generation==17&&out.stamp.capture_generation==19,"published root scoped to current recording and capture");
    check(!memory_xr_guest_root_source(0,out),"zero recording rejected");
    dev.device=reinterpret_cast<VkDevice>(uintptr_t(2));rejected();
    check(created==0,"native metadata query never creates, imports or synchronizes a GPU table");
    std::cout<<"Native root query is read-only, device scoped and recording scoped\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
'''.replace('QUERY',body)
    with tempfile.TemporaryDirectory(prefix='vr-root-query-') as directory:
        cpp=Path(directory)/'test.cpp';exe=Path(directory)/'test.exe';cpp.write_text(fixture,encoding='utf-8')
        command=[args.cxx,'-std=c++20','-O2','-I',str(root/'gpu/vk'),'-I',args.vulkan_include,str(cpp),'-o',str(exe)]
        if 'mingw' in args.cxx.lower():command+=['-static']
        subprocess.run(command,check=True);subprocess.run([str(exe)],check=True)
if __name__=='__main__':main()
