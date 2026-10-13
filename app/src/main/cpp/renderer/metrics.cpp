#include "renderer/metrics.hpp"
#include <algorithm>
ForestRenderMetrics debugMetrics;
void countDraw(GLsizei indexCount,uint32_t instances){
    debugMetrics.drawCalls++;
    debugMetrics.triangles+=uint64_t(std::max(indexCount,0))/3*
                            uint64_t(instances);
}
