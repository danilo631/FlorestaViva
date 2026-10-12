# Graphics 10 — Four-layer Forest PCG & Atmospheric Lighting

## Technical benchmark

Reference: https://github.com/Rytelier/Godot-4-forest-benchmark
License: MIT. We inspected its \`Plant.gdshader\`, \`Blend8 splat.gdshader\`,
\`Groundcover/Forest plants.tres\`, materials and project description.

We independently implemented their broad principles in native C++17/GLES3:
- dense canopy, shading-aware medium understory, fern/grass/seedling layer,
  and a separate, close-range litter/twig/fungi forest-floor layer;
- height/texture-driven PBR from photographs and per-biome scattering;
- leaf transmission, wind with GPU vertex deformations, approximate single
  scattering and quality-dependent ray marching.

No Godot scene, shader text or meshes from the reference were copied.

## Changes

### 1. Canopy-driven plant ecology

In \`game.cpp\`, seedling / fern / shrub distributions derive shade estimates
from the actual nearby procedural tree locations and biome-moisture noise.
Flower meadows prefer open sun; ferns and fungi prefer shaded damp areas.
This is more coherent than scattering equal-density sprites everywhere.

### 2. Real 3D ground-level details

\`forest_floor_details.hpp\` generates polygonal split dry branches, mushroom
stems and caps. Geometry is GPU-instanced within 12 m (ECO), 22 m (ALTA)
or 30 m (ULTRA). The foliage atlas still provides fine leaf litter.

### 3. Player-driven interactive wind

The instance vertex shader bends grass and ferns within roughly 2 m of the
player. Root vertices remain anchored; tips move using deterministic
spatial falloff and sine movement, with no expensive CPU collision mesh.
Vulkan compute wind field remains optional with C++ fallback.

### 4. Physically motivated illumination

Foliage backlighting uses adjusted wrap diffuse, alpha-based apparent
thickness, and a chlorophyll-inspired warm green transmission curve.
This is an **approximation of subsurface scattering**, not a multiple-scatter
material with measured leaf optical parameters.

In ULTRA, the screen-space post-process samples **four points** along each
camera ray through the real foliage shadow map to approximate atmospheric
single scattering (god rays). It is a lightweight view-ray approximation,
not a full-resolution 3D volumetric fog texture or Unreal's Lumen.
ECO and ALTA avoid this extra per-pixel cost to preserve FPS.

### 5. Real PBR rock materials

Build pipeline downloads the **Mossy Rock** PBR texture (asset ID `mossy_rock`) from Poly Haven, CC0,
with independent 2K color, tangent-space OpenGL normal, and roughness maps.
These replace synthetic rocks while preserving existing meshes and
the single installable ARM64 APK.
Source: https://polyhaven.com/a/mossy_rock

### 6. Validation and size

Actions validates: scanned material dimensions, per-slot JNI texture list,
embedded GLES shader compilation including shadow sampler, SPIR-V compilation,
native tests of trees, undergrowth, fungi and split branches, river hydrology,
ELF64 package and complete debug-signed APK.

**Explicit non-goals:** Unreal Engine conversion, proprietary Quixel Megascans
distribution, raytracing, physically exact SSS, full volumetric voxel fog and
new Vulkan scene backend. Each would require more memory, licensing work
and physical-GPU benchmarking. This is an original GLES3 engine.

## Controls, profiles and testing

ECO: close-range groundcover 12m; no expensive shadow volumetrics.
ALTA: groundcover 22m and improved foliage shading.
ULTRA: groundcover 30m and four shadow-map god-ray samples per post pixel.

Compare Graphics 9 and 10 at the same spot and the same setting.
Verify shader loads, visual foliage density, frame timing, battery and thermals
on physical Mali hardware. A green CI build alone cannot establish visual
quality or guaranteed 60 FPS on a particular device.
