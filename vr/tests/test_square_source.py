# SPDX-License-Identifier: GPL-3.0-or-later
import argparse,os,subprocess,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
class SquareSource(unittest.TestCase):
 def test_actual_host_parser_opt_in_and_pixel_budget(self):
  source=(ROOT/'host/nn_applet.c').read_text(encoding='utf-8');body=source[source.index('int hos_output_parse('):source.index('static uint32_t output_w, output_h;')]
  fixture=r'''
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include "host/hos_wide_size.h"
void hos_log(const char*,...){}
'''+body+r'''
int main(int argc,char** argv){
#ifdef _WIN32
_putenv_s("HOS_VR_SQUARE_SOURCE",argv[1]);
#else
setenv("HOS_VR_SQUARE_SOURCE",argv[1],1);
#endif
uint32_t w,h;bool enabled=!strcmp(argv[1],"1");
assert(hos_output_parse("1440x1440",&w,&h)==enabled);if(enabled)assert(w==1440&&h==1440&&uint64_t(w)*h==1920ull*1080);
assert(hos_output_parse("1920x1920",&w,&h)==enabled);
assert(hos_output_parse("4096x4096",&w,&h)==enabled);
assert(hos_output_parse("640x640",&w,&h)==enabled);
for(const char* text:{"4104x4104","8192x8192","640x800","0x0"})assert(!hos_output_parse(text,&w,&h));
assert(hos_output_parse("1920x1080",&w,&h)&&w==1920&&h==1080);
}
'''
  with tempfile.TemporaryDirectory(prefix='vr-square-source-') as directory:
   root=Path(directory);cpp=root/'test.cpp';exe=root/('test.exe' if os.name=='nt' else 'test');cpp.write_text('#include <initializer_list>\n'+fixture,encoding='utf-8')
   command=[COMPILER,'-std=c++17','-I',str(ROOT),str(cpp),'-o',str(exe)];command+=['-static'] if os.name=='nt' else []
   result=subprocess.run(command,capture_output=True,text=True,timeout=30);self.assertEqual(result.returncode,0,result.stdout+result.stderr)
   for flag in ['', '0','1','true']:
    result=subprocess.run([str(exe),flag],capture_output=True,text=True,timeout=5);self.assertEqual(result.returncode,0,result.stdout+result.stderr)
if __name__=='__main__':
 parser=argparse.ArgumentParser();parser.add_argument('--compiler',required=True);args=parser.parse_args();COMPILER=args.compiler;unittest.main(argv=['test_square_source'])
