# SPDX-License-Identifier: GPL-3.0-or-later
"""Extract the real read-only pool query for a game-free mutation/guard test."""
import sys
from pathlib import Path
source=(Path(__file__).resolve().parents[2]/"gpu/vk/vk_memory.cpp").read_text(encoding="utf-8")
start=source.index("bool memory_xr_uniform_copy_source(");brace=source.index("{",start);depth=0
for end in range(brace,len(source)):
    if source[end]=="{":depth+=1
    elif source[end]=="}":
        depth-=1
        if depth==0:break
else:raise RuntimeError("Incomplete mirror query")
Path(sys.argv[1]).write_text("// SPDX-License-Identifier: GPL-3.0-or-later\n"+source[start:end+1]+"\n",encoding="utf-8")
