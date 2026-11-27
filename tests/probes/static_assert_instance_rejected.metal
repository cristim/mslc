// EXPECT: error static assertion failed: T must be small
// A function template instance whose static_assert does not hold, here g<float> with sizeof(T) <= 2.
#include <metal_stdlib>
using namespace metal;
static_assert(sizeof(int) == 4, "int");
inline float f(float x) { static_assert(sizeof(float) == 4, "float"); return x; }
template <typename T> inline T g(T x) { static_assert(sizeof(T) <= 2, "T must be small"); static_assert(sizeof(T) >= 1); return x; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) { static_assert(true, "k"); out[i] = f(1.0f) + g(2.0f); }
