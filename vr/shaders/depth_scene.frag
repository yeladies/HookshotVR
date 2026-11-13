// SPDX-License-Identifier: GPL-3.0-or-later
#version 450
layout(location=0) in vec3 rgb;
layout(location=0) out vec4 color;
void main(){color=vec4(rgb,1);}
