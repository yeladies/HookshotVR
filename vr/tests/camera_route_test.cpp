// SPDX-License-Identifier: GPL-3.0-or-later
#include "vk_xr.h"
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <cstdio>
#include <stdexcept>
namespace mkwvk{void logf(const char*,...) {}}
static void env(const char* key,const char* value){
#ifdef _WIN32
    _putenv_s(key,value);
#else
    if(*value)setenv(key,value,1);else unsetenv(key);
#endif
}
static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(){try{
    const char* keys[]={"RX_VK_XR_IMMERSIVE","RX_VK_XR_GPU_WORLD_PREVIEW","RX_VK_XR_GPU_LATE_REPROJECT",
        "HOOKSHOT_VR_IMMERSIVE_TEST","RX_VK_XR_TEMPORAL_STEREO","RX_VK_XR_WIDE_FOV","HOOKSHOT_VR_COHERENT_VIEW"};
    for(const auto* key:keys){env(key,"1");
        check(!mkwvk::xr_vulkan_prepare(false),"retired environment must fail even before XR negotiation");
        check(!mkwvk::xr_vulkan_active(),"retired environment must not create XR");
        float rows[12]={1,0,0,0,0,1,0,0,0,0,1,0},saved[12];std::memcpy(saved,rows,sizeof rows);
        check(!mkwvk::xr_immersive_head_camera(123,rows,1)&&!std::memcmp(rows,saved,sizeof rows),"retired override cannot mutate a view");
        float fov=1,aspect=2;check(!mkwvk::xr_immersive_head_lens(&fov,&aspect)&&fov==1&&aspect==2,"retired lens cannot write");
        env(key,"");
    }
    check(mkwvk::xr_vulkan_prepare(false),"default off still works");mkwvk::xr_vulkan_reset();
    std::puts("Production camera retirement checks passed");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());mkwvk::xr_vulkan_reset();return 1;}}
