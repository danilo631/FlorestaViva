#include "ktx2.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <vector>

namespace forest_assets {
namespace {
constexpr unsigned COMPRESSED_RGBA_ASTC_6x6 = 0x93B4u;
constexpr uint32_t VK_FORMAT_ASTC_6x6_UNORM_BLOCK=165u;
constexpr std::array<uint8_t,12> MAGIC{
    0xab,0x4b,0x54,0x58,0x20,0x32,0x30,0xbb,0x0d,0x0a,0x1a,0x0a};
uint32_t rd32(const uint8_t*ptr){
    return uint32_t(ptr[0])|(uint32_t(ptr[1])<<8)|
           (uint32_t(ptr[2])<<16)|(uint32_t(ptr[3])<<24);
}
uint64_t rd64(const uint8_t*ptr){
    return uint64_t(rd32(ptr))|(uint64_t(rd32(ptr+4))<<32);
}
bool readFile(AAssetManager*manager,const char*name,std::vector<uint8_t>&bytes){
    if(!manager||!name||std::strncmp(name,"ktx2/",5)!=0)return false;
    AAsset*file=AAssetManager_open(manager,name,AASSET_MODE_STREAMING);
    if(!file)return false;
    const off64_t n=AAsset_getLength64(file);
    if(n<128||n>48*1024*1024){AAsset_close(file);return false;}
    bytes.resize(size_t(n));
    size_t pos=0;
    while(pos<bytes.size()){
        int got=AAsset_read(file,bytes.data()+pos,
                            int(std::min<size_t>(65536,bytes.size()-pos)));
        if(got<=0)break;
        pos+=size_t(got);
    }
    AAsset_close(file);
    return pos==bytes.size();
}
}
bool gpuSupportsAstc(){
    GLint n=0;
    glGetIntegerv(GL_NUM_EXTENSIONS,&n);
    for(GLint i=0;i<n;i++){
        auto*str=reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS,GLuint(i)));
        if(!str)continue;
        if(!std::strcmp(str,"GL_KHR_texture_compression_astc_ldr") ||
           !std::strcmp(str,"GL_OES_texture_compression_astc"))
            return true;
    }
    return false;
}
bool loadAstcKtx2(AAssetManager*manager,const char*name,
                  GLuint&texture,uint64_t&residentBytes){
    if(!gpuSupportsAstc())return false;
    std::vector<uint8_t>data;
    if(!readFile(manager,name,data))return false;
    const uint8_t*src=data.data();
    if(!std::equal(MAGIC.begin(),MAGIC.end(),src))return false;
    // KTX2 header: magic(12), 13x4/2x8 bytes = 80 in total.
    const uint32_t vkFormat=rd32(src+12), typeSize=rd32(src+16);
    const uint32_t width=rd32(src+20),height=rd32(src+24);
    const uint32_t depth=rd32(src+28), layers=rd32(src+32);
    const uint32_t faces=rd32(src+36),levels=rd32(src+40),super=rd32(src+44);
    if(vkFormat!=VK_FORMAT_ASTC_6x6_UNORM_BLOCK || typeSize!=1 ||
       width<4||height<4||width>8192||height>8192||
       depth!=0||layers>1||faces!=1||levels<1||levels>14||super!=0)
        return false;
    if(data.size()<80u+size_t(levels)*24u)return false;
    struct Level{uint64_t off,size;int w,h;};
    std::vector<Level>validated;
    validated.reserve(levels);
    uint64_t totalBytes=0;
    for(uint32_t mip=0;mip<levels;mip++){
        const uint8_t*entry=src+80+size_t(mip)*24u;
        const uint64_t off=rd64(entry),length=rd64(entry+8);
        const uint32_t w=std::max(width>>mip,1u),h=std::max(height>>mip,1u);
        const uint64_t expected=uint64_t((w+5)/6)*uint64_t((h+5)/6)*16u;
        if(length!=expected || off>data.size() ||
           length>data.size()-size_t(off))return false;
        validated.push_back({off,length,int(w),int(h)});
        totalBytes+=length;
    }
    GLuint newTexture=0;
    while(glGetError()!=GL_NO_ERROR){} // isolate upload validation
    glGenTextures(1,&newTexture);
    if(!newTexture)return false;
    glBindTexture(GL_TEXTURE_2D,newTexture);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,
                    levels>1?GL_LINEAR_MIPMAP_LINEAR:GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,GLint(levels-1));
    for(uint32_t mip=0;mip<levels;mip++){
        const Level&m=validated[mip];
        glCompressedTexImage2D(GL_TEXTURE_2D,GLint(mip),
            COMPRESSED_RGBA_ASTC_6x6,m.w,m.h,0,
            GLsizei(m.size),src+size_t(m.off));
    }
    if(glGetError()!=GL_NO_ERROR){
        glDeleteTextures(1,&newTexture);
        return false;
    }
    texture=newTexture;residentBytes=totalBytes;
    return true;
}
}
