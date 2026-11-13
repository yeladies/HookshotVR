"""Execute the actual opt-in preview wait/routing code with an authored GPU model."""
import argparse
from pathlib import Path
import subprocess
import tempfile
import sys
RUN_ALL_ARGS=['--cxx','{cxx}']
ROOT=Path(__file__).resolve().parents[1]
def block(text,marker):
    start=text.index(marker);at=text.index('{',start)+1;depth=1
    while depth:
        depth+=(text[at]=='{')-(text[at]=='}');at+=1
    return text[start:at]
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--cxx',required=True);args=parser.parse_args()
    source=(ROOT/'gpu/vk/vk_replay.cpp').read_text(encoding='utf-8-sig')
    wait=block(source,'VkResult preview_readback_wait(')
    start=source.index('        bool preview_taken=false;')
    route=source[start:source.index('        if (shot && made_src && made_map)',start)]
    fixture=r'''
#include <cstdint>
#include <stdexcept>
#include <iostream>
using VkResult=int;constexpr int VK_SUCCESS=0,VK_NOT_READY=1,VK_TIMEOUT=2,VK_ERROR_DEVICE_LOST=-4;
constexpr int VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO=1,FR_VALUE=2;
struct VkSemaphoreWaitInfo {int sType=0;void* next=nullptr;uint32_t flags=0,semaphoreCount=0;const uint64_t* pSemaphores=nullptr;const uint64_t* pValues=nullptr;};
uint64_t timeline=9;struct {int device=7;}dev;
int waited=0,flushed=0,published=0;bool complete=false,enabled=true;
VkResult result=VK_SUCCESS;bool preview_frame=true,save_shot=false;void* made_src=nullptr;
uint64_t value=100;int64_t presents=12;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
void queue_flush_for(int reason){check(reason==FR_VALUE,"flush reason");++flushed;}
VkResult vkWaitSemaphores(int device,const VkSemaphoreWaitInfo* info,uint64_t timeout){
 check(device==7&&info->semaphoreCount==1&&*info->pSemaphores==9&&*info->pValues==100&&timeout==20000000,"exact current transfer and bounded 20ms wait");++waited;complete=result==VK_SUCCESS;return result;
}
bool env_on(const char*){return enabled;}
void logf(const char*,...){}
void take(uint64_t v,int64_t p){check(complete&&v==100&&p==12,"never publish before current GPU completion or with old metadata");++published;}
'''+wait+'\nbool attempt(){\n'+route+'\nreturn preview_taken;\n}\n'+r'''
void reset(){waited=flushed=published=0;complete=false;enabled=preview_frame=true;save_shot=false;made_src=nullptr;value=100;timeline=9;}
int main(){try{
 reset();result=VK_SUCCESS;check(attempt()&&published==1&&waited==1&&flushed==1,"ready transfer published exactly once");
 for(int state:{VK_TIMEOUT,VK_NOT_READY}){reset();result=state;check(!attempt()&&!published&&waited==1,"unfinished transfer keeps deferred ownership path");}
 reset();timeline=0;check(!attempt()&&!waited&&!published,"missing timeline is not read");
 reset();value=0;check(!attempt()&&!waited&&!published,"invalid queue value is not read");
 reset();result=VK_ERROR_DEVICE_LOST;check(attempt()&&!published&&waited==1,"failed transfer discarded without mapping or unsafe pending read");
 for(int mode=0;mode<4;++mode){reset();if(mode==0)enabled=false;if(mode==1)save_shot=true;if(mode==2)made_src=reinterpret_cast<void*>(1);if(mode==3)preview_frame=false;check(!attempt()&&!waited&&!flushed&&!published,"normal screenshots, made frames and default-off path untouched");}
 std::cout<<"Actual preview routing preserves deferred ownership and never reads before bounded completion\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
'''
    with tempfile.TemporaryDirectory(prefix='vr-preview-wait-') as tmp:
        path=Path(tmp);cpp=path/'test.cpp';exe=path/('test.exe' if sys.platform=='win32' else 'test');cpp.write_text(fixture,encoding='utf-8')
        subprocess.run([args.cxx,'-std=c++17','-Wall','-Wextra','-Werror',*(['-static'] if sys.platform=='win32' else []),str(cpp),'-o',str(exe)],check=True)
        subprocess.run([str(exe)],check=True)
if __name__=='__main__':main()
