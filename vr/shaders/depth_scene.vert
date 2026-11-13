// SPDX-License-Identifier: GPL-3.0-or-later
#version 450
layout(location=0) in vec3 position;
layout(location=1) in vec3 color;
layout(location=0) out vec3 rgb;
layout(push_constant,row_major) uniform Camera {mat4 mvp;} camera;
void main(){gl_Position=camera.mvp*vec4(position,1);rgb=color;}
