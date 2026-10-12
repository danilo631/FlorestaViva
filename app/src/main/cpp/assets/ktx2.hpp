#pragma once
#include <cstdint>
#include <GLES3/gl3.h>
#include <android/asset_manager.h>
namespace forest_assets {
// Decodes only Vulkan-format ASTC_6x6_UNORM KTX2 with no supercompression.
// Returns a new GLES texture; caller owns it. No UASTC transcoding.
bool loadAstcKtx2(AAssetManager*manager,const char*name,
                  GLuint&texture,uint64_t&residentBytes);
bool gpuSupportsAstc();
}
