# SPDX-License-Identifier: GPL-3.0-or-later
"""Compile the actual GPU source pool with a game-free Vulkan transport double."""
from pathlib import Path
import sys
source=(Path(__file__).resolve().parents[2]/'gpu/vk/vk_xr_gpu_scene_source.cpp').read_text(encoding='utf-8')
Path(sys.argv[1]).write_text('// SPDX-License-Identifier: GPL-3.0-or-later\n'+source[source.index('namespace mkwvk {'):],encoding='utf-8')
