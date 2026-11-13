// SPDX-License-Identifier: GPL-3.0-or-later
#version 450
layout(set=0,binding=0,std430) readonly buffer Capture {uint data[];} source;
layout(location=0) out vec3 rgb;
layout(push_constant,row_major) uniform Camera {mat4 mvp;vec4 projection;vec4 optical;uvec4 dimensions;} camera;
const uvec2 corners[6]=uvec2[6](uvec2(0,0),uvec2(0,1),uvec2(1,0),uvec2(1,0),uvec2(0,1),uvec2(1,1));
vec3 point(uvec2 pixel,out bool valid){
    uint index=pixel.y*camera.dimensions.x+pixel.x;
    float depth=uintBitsToFloat(source.data[camera.dimensions.x*camera.dimensions.y+index]);
    float denominator=2*depth-1+camera.projection.z;
    valid=!isnan(depth)&&!isinf(depth)&&depth>=0&&depth<.999999&&abs(denominator)>1e-12;
    float z=valid?-camera.projection.w/denominator:0;
    vec2 ndc=vec2(2*(float(pixel.x)+.5)/camera.dimensions.x-1,1-2*(float(pixel.y)+.5)/camera.dimensions.y);
    vec3 value=vec3(-z*(ndc+camera.optical.xy)/camera.projection.xy,z)*camera.optical.z;
    valid=valid&&z<0&&length(value)<=80;return value;
}
float channel(uint bits,uint mantissa){uint exponent=bits>>mantissa;uint fraction=bits&((1u<<mantissa)-1);if(exponent==31)return 0;return exponent==0?float(fraction)*exp2(float(-14-int(mantissa))):(1+float(fraction)/float(1u<<mantissa))*exp2(float(int(exponent)-15));}
vec3 decode(uint bits){if(camera.dimensions.w==123)return vec3(bits&511u,(bits>>9)&511u,(bits>>18)&511u)*exp2(float(int(bits>>27)-24));return vec3(channel(bits&2047u,6),channel((bits>>11)&2047u,6),channel(bits>>22,5));}
void main(){
    uint step=camera.dimensions.z;uvec2 cells=(camera.dimensions.xy-uvec2(1)+step-1)/step;
    uint cell=uint(gl_VertexIndex)/6;uvec2 origin=uvec2(cell%cells.x,cell/cells.x)*step;
    uint triangle=(uint(gl_VertexIndex)%6)/3;vec3 values[3];bool valid=true;float nearest=1e20,farthest=0;
    for(uint i=0;i<3;++i){bool ok;uvec2 pixel=min(origin+corners[triangle*3+i]*step,camera.dimensions.xy-uvec2(1));values[i]=point(pixel,ok);valid=valid&&ok;nearest=min(nearest,length(values[i]));farthest=max(farthest,length(values[i]));}
    valid=valid&&farthest<=nearest*1.15;
    if(!valid){gl_Position=vec4(2,2,2,1);rgb=vec3(0);return;}
    uvec2 pixel=min(origin+corners[uint(gl_VertexIndex)%6]*step,camera.dimensions.xy-uvec2(1));uint bits=source.data[pixel.y*camera.dimensions.x+pixel.x];
    vec3 encoded=decode(bits);
    rgb=pow(clamp(encoded,vec3(0),vec3(1)),vec3(2));gl_Position=camera.mvp*vec4(values[uint(gl_VertexIndex)%3],1);
}
