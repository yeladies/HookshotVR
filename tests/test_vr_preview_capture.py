"""Exercise the actual preview/file routing with authored pixels; no game, GPU or runtime."""
import argparse
from pathlib import Path
import subprocess
import tempfile
RUN_ALL_ARGS = ["--cxx", "{cxx}"]
ROOT = Path(__file__).resolve().parents[1]
def main():
    p=argparse.ArgumentParser();p.add_argument("--cxx",default="g++");a=p.parse_args()
    source=(ROOT/"gpu/vk/vk_replay.cpp").read_text(encoding="utf-8")
    start=source.index("            const bool from_pending=")
    end=source.index("            static int black_left",start)
    route=source[start:end]
    fixture=r'''
#include <cstdint>
#include <memory>
#include <vector>
#include <cstdlib>
#include <cstdio>
struct Pending {bool on=true;int64_t presents=3;bool preview=false,save=true;};
int published=0,saved=0;constexpr int kShotW=1280,kShotH=720;
void xr_preview_publish(std::shared_ptr<std::vector<uint8_t>> pixels,int w,int h,int64_t present){
 if(!pixels||(*pixels)[0]!=42||w!=1280||h!=720||present<0)std::exit(2);++published;}
void take(Pending pending,bool preview_frame,bool save_shot,int64_t at_present){
 auto px=std::make_shared<std::vector<uint8_t>>(1,42);
 /* ROUTE */
 ++saved;
}
int main(){
 take({true,3,false,true},false,true,3);if(published!=0||saved!=1)return 1;
 take({true,3,true,false},false,true,3);if(published!=1||saved!=1)return 3;
 take({true,3,true,true},false,true,3);if(published!=2||saved!=2)return 4;
 take({false,0,false,true},true,false,4);if(published!=3||saved!=2)return 5;
 take({true,2,true,false},false,true,3);if(published!=3||saved!=3)return 6;
}
'''.replace("/* ROUTE */",route)
    with tempfile.TemporaryDirectory(prefix="vr-preview-route-") as temp:
        cpp=Path(temp)/"fixture.cpp";exe=Path(temp)/"fixture.exe";cpp.write_text(fixture,encoding="utf-8")
        subprocess.run([a.cxx,"-std=c++17","-Wall","-Wextra","-Werror",str(cpp),"-o",str(exe)],check=True)
        subprocess.run([str(exe)],check=True)
if __name__=="__main__":main()