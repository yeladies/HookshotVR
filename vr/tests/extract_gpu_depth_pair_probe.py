# SPDX-License-Identifier: GPL-3.0-or-later
from pathlib import Path
import sys
source=(Path(__file__).resolve().parents[2]/'gpu/vk/vk_xr_gpu_depth_pair_probe.cpp').read_text(encoding='utf-8')
Path(sys.argv[1]).write_text('// SPDX-License-Identifier: GPL-3.0-or-later\n'+source[source.index('namespace mkwvk {'):],encoding='utf-8')
