cmake_minimum_required(VERSION 3.22.1)
project(florestaviva LANGUAGES CXX)

add_library(florestaviva SHARED
    game.cpp
    vulkan_wind.cpp
    assets/ktx2.cpp
    terrain/terrain_shaders.cpp
    vegetation/vegetation_shaders.cpp
    sky/sky_shaders.cpp
    post/post_shaders.cpp
    renderer/shadow_shaders.cpp
    renderer/metrics.cpp
)
target_include_directories(florestaviva PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(florestaviva PRIVATE cxx_std_20)
target_compile_options(florestaviva PRIVATE -Wall -Wextra -O3)

find_library(gles-lib GLESv3 REQUIRED)
find_library(android-lib android REQUIRED)
find_library(jnigraphics-lib jnigraphics REQUIRED)
find_library(vulkan-lib vulkan REQUIRED)
find_library(log-lib log REQUIRED)

# AndroidBitmap_getInfo/lockPixels/unlockPixels live in libjnigraphics.
# Without this dependency the APK may compile but the native library
# cannot be loaded when MainActivity calls System.loadLibrary().
target_link_libraries(florestaviva
    ${gles-lib}
    ${android-lib}
    ${jnigraphics-lib}
    ${log-lib}
    ${vulkan-lib}
)
