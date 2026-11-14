# SPDX-License-Identifier: GPL-3.0-or-later
"""Extract the actual private guest-root recorder for game-free lifecycle checks."""
from pathlib import Path
import sys
source=(Path(__file__).resolve().parents[2]/'gpu/vk/vk_xr_root_alias_probe.cpp').read_text(encoding='utf-8-sig')
Path(sys.argv[1]).write_text('// SPDX-License-Identifier: GPL-3.0-or-later\n'+source[source.index('namespace mkwvk {'):],encoding='utf-8')
