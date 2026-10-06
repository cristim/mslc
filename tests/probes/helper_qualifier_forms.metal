// EXPECT: valid
#include <metal_stdlib>
using namespace metal;
static float a(float x) { return x; }
inline float b(float x) { return x; }
static inline float c(float x) { return x; }
inline static float d(float x) { return x; }
constexpr float e(float x) { return x; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = a(1.0f) + b(1.0f) + c(1.0f) + d(1.0f) + e(1.0f); }
