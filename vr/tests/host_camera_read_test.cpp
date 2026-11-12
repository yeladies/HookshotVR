// SPDX-License-Identifier: GPL-3.0-or-later
#include "vr_camera_read.h"
#include <map>
#include <vector>
#include <stdexcept>
#include <iostream>
#include <limits>
using Memory=std::map<uint64_t,std::vector<unsigned char>>;
struct Fixture {Memory memory;int roots=0;bool change=false;};
extern "C" int host_vr_c_primary(HostVrRead,void*,HostVrPrimary*);
void check(bool x,const char* why){if(!x)throw std::runtime_error(why);}
template<class T>void put(Fixture& f,uint64_t base,size_t offset,T value){std::memcpy(f.memory[base].data()+offset,&value,sizeof value);}
int read(uint64_t address,void* out,size_t size,void* context){auto& f=*static_cast<Fixture*>(context);
    if(address==0x84ca6bc0ULL&&++f.roots==2&&f.change)put<uint64_t>(f,4000,120,6000);
    for(const auto& [base,bytes]:f.memory)if(address>=base&&address-base<=bytes.size()&&size<=bytes.size()-(address-base)) {
        std::memcpy(out,bytes.data()+(address-base),size);return 1;}
    return 0;}
Fixture fixture(){Fixture f;f.memory[0x84ca6bc0ULL].resize(8);for(uint64_t base:{1000,2000,3000,4000,5000})f.memory[base].resize(256);
    put<uint64_t>(f,0x84ca6bc0ULL,0,1000);put<uint64_t>(f,1000,0,2000);put<uint32_t>(f,2000,32,1);put<uint64_t>(f,2000,40,3000);
    put<uint64_t>(f,3000,0,4000);put<uint64_t>(f,4000,120,5000);put<uint64_t>(f,5000,0,0x84c18a88ULL);
    float rows[12]{};rows[0]=rows[5]=rows[10]=1;rows[3]=-2;std::memcpy(f.memory[5000].data()+8,rows,sizeof rows);
    double eye[3]{2,0,0};std::memcpy(f.memory[5000].data()+96,eye,sizeof eye);return f;}
int main(){try{
    auto f=fixture();auto before=f.memory;HostVrPrimary out{};
    check(host_vr_c_primary(read,&f,&out)&&out.camera==5000&&out.rows[3]==-2&&out.eye[0]==2,"actual C reader returns guarded primary view");
    check(f.memory==before,"read-only guest access");const auto saved=out;
    f=fixture();put<uint64_t>(f,5000,0,123);check(!host_vr_c_primary(read,&f,&out)&&out.camera==saved.camera,"wrong vtable rejects without partial output");
    f=fixture();put<uint32_t>(f,2000,32,0);check(!host_vr_c_primary(read,&f,&out),"no gameplay camera rejects cached state");
    f=fixture();f.change=true;check(!host_vr_c_primary(read,&f,&out),"camera selection changed between reads rejects sample");
    f=fixture();put<float>(f,5000,8,std::numeric_limits<float>::quiet_NaN());check(!host_vr_c_primary(read,&f,&out),"invalid matrix rejected");
    f=fixture();put<uint64_t>(f,1000,0,UINT64_MAX-8);check(!host_vr_c_primary(read,&f,&out),"invalid chain does not wrap to a valid low address");
    f=fixture();check(host_vr_c_primary(read,&f,&out),"fresh unchanged camera can be read again without a rebuild hook");
    HostVrCameraCache cache{};host_vr_cache_primary(&cache,out.camera,out.rows,0);host_vr_cache_uniform(&cache,9000,out.rows);
    check(cache.uniform==9000,"main-view uniform cached");auto other=out;other.rows[3]+=5;host_vr_cache_uniform(&cache,10000,other.rows);
    check(cache.uniform==9000,"auxiliary view does not replace main-view provenance");host_vr_cache_primary(&cache,6000,out.rows,0);
    check(cache.uniform==0,"camera identity change clears prior uniform");host_vr_cache_primary(&cache,6000,out.rows,1);check(cache.primary==0,"debug camera excluded");
    std::cout<<"Guarded live camera refresh, race rejection and uniform provenance checked\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
