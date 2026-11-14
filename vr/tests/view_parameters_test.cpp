// SPDX-License-Identifier: GPL-3.0-or-later
#include "../../host/vr_view_parameters.h"
#include "../../gpu/vk/xr_synthetic_head.h"
#include <iostream>
#include <stdexcept>
#include <cstring>
void check(bool v,const char* why){if(!v)throw std::runtime_error(why);}
bool near(double a,double b){return std::abs(a-b)<1e-4;}
int main(){try{
    float base[12]={1,0,0,-10,0,1,0,-20,0,0,1,-30};
    HostVrViewParameters input{{10,20,30},{10,20,25},{10,20,30,10,20,25,0,1,0}},output{};
    for(const char* axis:{"WideYaw","WidePitch"})for(double time:{8.,18.}){
        mkwvk::ImmersiveCamera camera;mkwvk::ImmersivePose pose;pose.generation=1;pose.observed_ns=1000000000;
        float view[12];std::memcpy(view,base,sizeof view);check(camera.apply(1,view,pose,pose.observed_ns),"centre anchor");
        mkwvk::synthetic_head_pose(axis,time,pose);pose.observed_ns+=10000000;pose.p[0]=.1f;
        check(camera.apply(1,view,pose,pose.observed_ns),"45 degree tracked camera");
        check(host_vr_view_parameters(base,view,&input,&output),"valid derived view fields accepted");
        double length=0;for(int k=0;k<3;++k){
            const double d=output.eye[k]-output.target[k];length+=d*d;
            check(near(output.cached[k],output.eye[k])&&near(output.cached[3+k],output.target[k]),"float/double caches agree");
            check(near(d/5,view[8+k])&&near(output.cached[6+k],view[4+k]),"look and up match tracked rotation");
        }
        check(near(std::sqrt(length),5),"focus distance preserved");
        for(int r=0;r<3;++r){double position=view[4*r+3];for(int k=0;k<3;++k)position+=view[4*r+k]*output.eye[k];check(near(position,0),"eye matches inverse tracked view");}
        HostVrViewParameters again=output;for(int repeat=0;repeat<1000;++repeat){HostVrViewParameters next;check(host_vr_view_parameters(view,view,&again,&next),"repeated update remains coherent");again=next;}
        for(int k=0;k<3;++k)check(near(output.eye[k],again.eye[k])&&near(output.target[k],again.target[k]),"repeated update does not accumulate");
    }
    const auto before=output;
    for(unsigned bad=0;bad<6;++bad){auto wrong=input;float tracked[12];std::memcpy(tracked,base,sizeof tracked);
        if(bad==0)wrong.cached[0]+=1;if(bad==1)wrong.eye[0]+=1;if(bad==2)wrong.target[0]+=1;if(bad==3)wrong.cached[7]=0;if(bad==4)tracked[0]=2;if(bad==5)tracked[0]=NAN;
        check(!host_vr_view_parameters(base,tracked,&wrong,&output),"unknown or corrupt view rejected");check(!std::memcmp(&before,&output,sizeof output),"failure is atomic");
    }
    check(near(mkwvk::synthetic_head_angle("WideYaw",0),0)&&near(mkwvk::synthetic_head_angle("WidePitch",3),0),"wide sweep anchors at centre");
    check(near(mkwvk::synthetic_head_angle("WideYaw",8),.7853981633974483)&&near(mkwvk::synthetic_head_angle("WideYaw",18),-.7853981633974483),"both 45 degree extremes reached");
    check(near(mkwvk::synthetic_head_angle("Small",2),.12*std::sin(1.4)),"existing small diagnostic preserved");
    std::cout<<"Coherent eye/target/up at +/-45 degree yaw and pitch; invalid layouts fail atomically\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
