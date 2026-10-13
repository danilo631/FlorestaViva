#pragma once
// Budgeted, reproducible quality policy, independent of JNI and GLES.
struct ForestQuality {
    const char* name;
    float nearMeters,midMeters,grassMeters,maxMeters;
    float shrubMeters,fernMeters,microMeters,litterMeters;
    float reedMeters,flowerMeters,wildlifeMeters;
    bool shadows,postFX,godRays,legacy;
    float shadowFilterQuality,waterQuality;
    int targetFps;
};
const ForestQuality& forestQuality(int preset);
