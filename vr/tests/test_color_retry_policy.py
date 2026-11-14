"""Compile actual native color-retry selection/gate with authored state only."""
import argparse
from pathlib import Path
import subprocess
import tempfile

def block(source, marker):
    start=source.index(marker);at=source.index('{',start)+1;depth=1
    while depth:
        depth+=(source[at]=='{')-(source[at]=='}');at+=1
    return source[start:at]

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--cxx',required=True);args=parser.parse_args()
    root=Path(__file__).resolve().parents[2]
    source=(root/'gpu/vk/vk_xr_baseline_draw.cpp').read_text(encoding='utf-8-sig')
    selection=block(source,'bool xr_baseline_draw_selected(')
    retry=block(source,'if(result.result==XrBaselineComparison::Result::Invisible&&result.view_color_requested')
    fixture=r'''
#include "xr_baseline_attempt.h"
#include <map>
#include <set>
#include <mutex>
#include <stdexcept>
#include <iostream>
using namespace mkwvk;
struct Program {uint64_t key;bool reviewed=true;};
struct State {std::mutex mutex;std::map<uint64_t,unsigned> visits,previous_visits,invisible_retries;std::set<uint64_t> attempted;bool stopped=false;void* current=nullptr;unsigned started=0;uint64_t current_program=7;} shared;
State& state(){return shared;}bool enabled=true;bool xr_baseline_draw_enabled(){return enabled;}
bool selected(const State&,const Program& program){return program.reviewed;}
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
SELECTION
void completion(const XrBaselineComparison& result){auto& p=state();RETRY}
int main(){try{
    auto& p=state();Program program{7};
    p.attempted.insert(7);XrBaselineComparison result;result.result=XrBaselineComparison::Result::Invisible;result.view_color_requested=true;result.matrix_probe_validated=true;
    completion(result);check(p.attempted.count(7)&&p.invisible_retries.empty(),"changed/unverified source never authorizes retry");
    result.matrix_source_unchanged=true;result.matrix_probe_validated=false;completion(result);check(p.attempted.count(7),"unverified patch cannot retry");
    result.matrix_probe_validated=true;result.result=XrBaselineComparison::Result::Mismatch;completion(result);check(p.attempted.count(7),"mismatched baseline cannot retry");
    result.result=XrBaselineComparison::Result::Match;completion(result);check(p.attempted.count(7),"visible failed raster is not retried by invisible policy");
    result.result=XrBaselineComparison::Result::Invisible;completion(result);check(!p.attempted.count(7)&&p.invisible_retries[7]==1,"only invisible validated color draw becomes retryable");
    p.previous_visits[7]=4;
    check(!xr_baseline_draw_selected(program),"retry skips first eligible ordinal");
    check(xr_baseline_draw_selected(program),"retry can select next eligible draw");
    p.current=&p;check(!xr_baseline_draw_selected(program)&&p.visits[7]==3,"in-flight draw blocks new allocation while observations continue");p.current=nullptr;
    p.started=8;check(!xr_baseline_draw_selected(program),"total budget includes repeated program attempts");
    p.started=0;p.visits.clear();p.previous_visits.clear();p.invisible_retries[7]=3;
    check(xr_baseline_draw_selected(program),"one-draw frame never waits for an unreachable ordinal");
    p.stopped=true;check(!xr_baseline_draw_selected(program),"stopped probe cannot restart");p.stopped=false;
    check(!xr_baseline_draw_selected({8,false}),"unreviewed shaders still rejected");
    p.current=&p;p.visits.clear();for(unsigned i=1;i<=256;++i)xr_baseline_draw_selected({i});check(p.visits.size()==64,"observation map remains bounded");
    enabled=false;check(!xr_baseline_draw_selected(program),"disabled mode cannot select");
    std::cout<<"Native invisible-color retry requires intact controls and a bounded total budget\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
'''.replace('SELECTION',selection).replace('RETRY',retry)
    with tempfile.TemporaryDirectory(prefix='vr-color-retry-') as directory:
        cpp=Path(directory)/'test.cpp';exe=Path(directory)/'test.exe';cpp.write_text(fixture,encoding='utf-8')
        command=[args.cxx,'-std=c++20','-O2','-I',str(root/'gpu/vk'),str(cpp),'-o',str(exe)]
        if 'mingw' in args.cxx.lower():command+=['-static']
        subprocess.run(command,check=True);subprocess.run([str(exe)],check=True)
if __name__=='__main__':main()
