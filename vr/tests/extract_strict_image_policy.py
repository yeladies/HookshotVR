# SPDX-License-Identifier: GPL-3.0-or-later
"""Extract the actual authored strict allocator body for the game-free stub test."""
import sys
from pathlib import Path
source=(Path(__file__).resolve().parents[2]/'gpu/vk/vk_vma.cpp').read_text(encoding='utf-8')
start=source.index('bool create_xr_private_image(');brace=source.index('{',start);depth=0
for end in range(brace,len(source)):
    if source[end]=='{':depth+=1
    elif source[end]=='}':
        depth-=1
        if depth==0:break
else:raise RuntimeError('Strict allocator body not complete')
Path(sys.argv[1]).write_text('// SPDX-License-Identifier: GPL-3.0-or-later\n'+source[start:end+1]+'\n',encoding='utf-8')
