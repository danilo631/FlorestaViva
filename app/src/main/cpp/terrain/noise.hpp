#pragma once
#include <cstdint>
// Deterministic cell noise shared by terrain, ecology, and PCG.
// World seed formulas remain unchanged for save-file terrain parity.
uint32_t hash32(uint32_t x);
float randCell(int x,int z);
float noise(float x,float z);
