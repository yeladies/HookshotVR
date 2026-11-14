// SPDX-License-Identifier: GPL-3.0-or-later
#include "xr_baseline_selection.h"
#include <iostream>
#include <stdexcept>
using namespace mkwvk;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(int argc,char** argv){try{
    if(argc==4&&std::string(argv[1])=="--check"){
        std::string text;char c;while(std::cin.get(c)){if(text.size()>=8192)throw std::runtime_error("selection size bound");text+=c;}
        XrBaselineSelection parsed;check(parse_xr_baseline_selection(text,argv[2],argv[3],parsed),"invalid generated baseline selection");
    std::cout<<"Baseline protocol verified; programs="<<parsed.programs.size()<<'\n';return 0;
    }
    check(argc==1,"unsupported protocol checker arguments");
    const std::string nonce(32,'a'),source="1234abcd";
    const std::string header="HSVR_BASELINE_DRAW_V1\n"+nonce+" "+source+" ";
    const std::string body="0000000000000007 2\n0 1 0000000000000011 100\n1 16 0000000000000022 200\n";
    XrBaselineSelection out;
    auto parse=[&](std::string text){return parse_xr_baseline_selection(text,nonce,source,out);};
    check(parse(header+"1\n"+body)&&out.programs.size()==1&&out.programs[0].stages.size()==2,"bounded complete stage selection parsed");
    check(out.programs[0].stages[1].vk_stage==16&&out.programs[0].stages[1].words==200,"stage identity fields retained");
    auto reject=[&](std::string text){check(!parse(text)&&out.programs.size()==1&&out.programs[0].program==7,"invalid selection leaves output unchanged");};
    reject(header+"0\n");reject(header+"33\n");reject(header+"1\n"+body+"extra");reject(std::string(8193,'x'));
    reject(header+"2\n"+body+body);
    reject(header+"1\n0000000000000000 2\n0 1 0000000000000011 100\n1 16 0000000000000022 200\n");
    reject(header+"1\n0000000000000007 2\n0 1 0000000000000011 100\n1 1 0000000000000022 200\n");
    reject(header+"1\n0000000000000007 2\n0 1 0000000000000011 100\n1 16 0000000000000022 2097153\n");
    reject(header+"1\n0000000000000007 2\n0 1 0000000000000011 100\n1 16 0000000000000022 -1\n");
    reject(header+"1\n0000000000000007 1\n0 1 0000000000000011 100\n");
    reject(header+"1\nzzzzzzzzzzzzzzzz 2\n0 1 0000000000000011 100\n1 16 0000000000000022 200\n");
    check(!parse_xr_baseline_selection(header+"1\n"+body,std::string(32,'b'),source,out),"wrong run nonce rejected");
    check(!parse_xr_baseline_selection(header+"1\n"+body,nonce,"abcdef12",out),"wrong build source rejected");
    check(parse(header+"1\r\n"+body),"normal Windows line endings accepted");
    const auto lens=std::string("HSVR_LENS_SHIFT_DRAW_V1\n")+nonce+" "+source+" 1\n"+body;
    check(!parse(lens),"baseline mode rejects lens-shift selection");
    check(parse_xr_baseline_selection(lens,nonce,source,out,true),"explicit lens mode accepts its own protocol");
    check(!parse_xr_baseline_selection(header+"1\n"+body,nonce,source,out,true),"lens mode rejects unchanged baseline selection");
        const auto view=std::string("HSVR_VIEW_SHIFT_DRAW_V1\n")+nonce+" "+source+" 1\n"+body;
    check(parse_xr_baseline_selection(view,nonce,source,out,true,true),"explicit translated mode accepts its own protocol");
    check(!parse_xr_baseline_selection(view,nonce,source,out,true),"lens mode rejects translated protocol");
    check(!parse_xr_baseline_selection(lens,nonce,source,out,true,true),"translated mode rejects lens protocol");
    const auto color=std::string("HSVR_VIEW_COLOR_DRAW_V1\n")+nonce+" "+source+" 1\n"+body;
    check(parse_xr_baseline_selection(color,nonce,source,out,true,true,true),"color view mode accepts its own protocol");
    check(!parse_xr_baseline_selection(color,nonce,source,out,true,true),"depth view mode rejects color mode");
    check(!parse_xr_baseline_selection(view,nonce,source,out,true,true,true),"color mode rejects depth view selection");
    std::cout<<"Baseline selection is bounded, run/build scoped and atomic on malformed identities\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
