#pragma once
#include <cstdint>
#include <GLES3/gl3.h>
// CPU-side instrumentation; GPU elapsed time requires disjoint timer queries.
struct ForestRenderMetrics {
    uint32_t drawCalls=0;
    uint64_t triangles=0,textureBytes=0;
    float fps=0.f,cpuSubmissionMs=0.f;
    float smoothFps=0.f,smoothCpuMs=0.f;
    uint32_t frames=0;
};
extern ForestRenderMetrics debugMetrics;
void countDraw(GLsizei indexCount,uint32_t instances=1);
