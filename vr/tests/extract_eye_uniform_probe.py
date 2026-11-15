"""Compile the actual GPU probe body against bounded authored Vulkan stubs."""
import sys
from pathlib import Path
source=Path(sys.argv[1]);fixture=Path(sys.argv[2]);out=Path(sys.argv[3])
text=source.read_text()
for include in ('#include "vk_internal.h"','#include "vk_work.h"'):
    if text.count(include)!=1:raise SystemExit('probe include boundary changed')
    text=text.replace(include,'')
template=fixture.read_text()
if template.count('/* ACTUAL_GPU_UNIFORM_PROBE */')!=1:raise SystemExit('missing actual probe insertion marker')
out.write_text(template.replace('/* ACTUAL_GPU_UNIFORM_PROBE */',text),encoding='utf-8')
